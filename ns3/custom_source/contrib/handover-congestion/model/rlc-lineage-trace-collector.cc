// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#include "rlc-lineage-trace-collector.h"

#include "ns3/abort.h"
#include "ns3/lte-radio-bearer-info.h"
#include "ns3/lte-ue-net-device.h"
#include "ns3/object-map.h"
#include "ns3/simulator.h"

namespace ns3::handover_congestion
{
namespace
{
const char*
EventName(uint8_t event)
{
    switch (event)
    {
    case LteRlcUm::LINEAGE_ENQUEUE: return "ENQUEUE";
    case LteRlcUm::LINEAGE_SERVICE_FRAGMENT: return "SERVICE_FRAGMENT";
    case LteRlcUm::LINEAGE_FULLY_SERVED: return "FULLY_SERVED";
    case LteRlcUm::LINEAGE_DROP_OVERFLOW: return "DROP_OVERFLOW";
    case LteRlcUm::LINEAGE_CENSORED_SIM_END: return "CENSORED_SIM_END";
    case LteRlcUm::LINEAGE_CENSORED_RLC_DISPOSE: return "CENSORED_RLC_DISPOSE";
    }
    return "UNKNOWN";
}

const char*
DispositionName(uint8_t disposition)
{
    switch (disposition)
    {
    case LteRlcUm::DISPOSITION_FULLY_SERVED: return "FULLY_SERVED";
    case LteRlcUm::DISPOSITION_DROP_OVERFLOW: return "DROP_OVERFLOW";
    case LteRlcUm::DISPOSITION_CENSORED_SIM_END: return "CENSORED_SIM_END";
    case LteRlcUm::DISPOSITION_CENSORED_RLC_DISPOSE: return "CENSORED_RLC_DISPOSE";
    default: return "";
    }
}
} // namespace

RlcLineageTraceCollector::RlcLineageTraceCollector(const ExperimentConfig& config,
                                                   std::ostream& stream)
    : m_config(config),
      m_stream(stream)
{
}

void
RlcLineageTraceCollector::Connect(const NetworkArtifacts& network)
{
    for (uint32_t ueId = 0; ueId < network.ueDevices.GetN(); ++ueId)
    {
        auto device = network.ueDevices.Get(ueId)->GetObject<LteUeNetDevice>();
        auto [it, inserted] = m_ues.emplace(device->GetImsi(), UeBinding{});
        auto& binding = it->second;
        binding.ueId = ueId;
        binding.imsi = device->GetImsi();
        binding.rrc = device->GetRrc();
        binding.rrc->TraceConnectWithoutContext(
            "DrbCreated",
            MakeBoundCallback(&RlcLineageTraceCollector::DrbCreatedAdapter, this, &binding));
    }
}

void
RlcLineageTraceCollector::DrbCreatedAdapter(RlcLineageTraceCollector* self,
                                            UeBinding* binding,
                                            uint64_t,
                                            uint16_t,
                                            uint16_t,
                                            uint8_t drbIdentity)
{
    self->ConnectDrb(*binding, drbIdentity);
}

void
RlcLineageTraceCollector::ConnectDrb(UeBinding& ue, uint8_t drbIdentity)
{
    ObjectMapValue bearers;
    ue.rrc->GetAttribute("DataRadioBearerMap", bearers);
    auto bearer = DynamicCast<LteDataRadioBearerInfo>(bearers.Get(drbIdentity));
    NS_ABORT_MSG_IF(!bearer, "DrbCreated did not resolve an RLC lineage bearer");
    auto rlc = DynamicCast<LteRlcUm>(bearer->m_rlc);
    NS_ABORT_MSG_IF(!rlc, "lineage-instrumented UE bearer is not LteRlcUm");
    m_rlcBindings.push_back(RlcBinding{&ue, bearer->m_logicalChannelIdentity});
    auto& binding = m_rlcBindings.back();
    rlc->TraceConnectWithoutContext(
        "SduLineage",
        MakeBoundCallback(&RlcLineageTraceCollector::LineageAdapter, this, &binding));
    ++m_subscriptionCount;
}

void
RlcLineageTraceCollector::LineageAdapter(RlcLineageTraceCollector* self,
                                         RlcBinding* binding,
                                         uint16_t rnti,
                                         uint8_t lcid,
                                         uint8_t event,
                                         uint64_t lineageId,
                                         uint8_t trafficClass,
                                         int64_t generationTimeNs,
                                         int64_t applicationAttemptTimeNs,
                                         int64_t enqueueTimeNs,
                                         uint32_t originalSduBytes,
                                         uint32_t segmentBytes,
                                         uint32_t servedTotalBytes,
                                         uint32_t remainingBytes,
                                         uint64_t rlcPduId,
                                         uint8_t disposition)
{
    uint64_t tracePduId = 0;
    if (rlcPduId != 0)
    {
        auto [found, inserted] = binding->tracePduIds.emplace(rlcPduId, 0);
        if (inserted)
        {
            found->second = ++self->m_nextTracePduId;
        }
        tracePduId = found->second;
    }
    self->Write(*binding, rnti, lcid, event, lineageId, trafficClass, generationTimeNs,
                applicationAttemptTimeNs, enqueueTimeNs, originalSduBytes, segmentBytes,
                servedTotalBytes, remainingBytes, tracePduId, disposition);
}

void
RlcLineageTraceCollector::Write(RlcBinding& binding,
                                uint16_t rnti,
                                uint8_t lcid,
                                uint8_t event,
                                uint64_t lineageId,
                                uint8_t trafficClass,
                                int64_t generationTimeNs,
                                int64_t applicationAttemptTimeNs,
                                int64_t enqueueTimeNs,
                                uint32_t originalSduBytes,
                                uint32_t segmentBytes,
                                uint32_t servedTotalBytes,
                                uint32_t remainingBytes,
                                uint64_t rlcPduId,
                                uint8_t disposition)
{
    NS_ABORT_MSG_IF(binding.lcid != lcid, "RLC SduLineage LCID changed after binding");
    // DoDispose is used both for bearer replacement (notably handover) and at
    // Simulator teardown.  The generic LTE core cannot safely distinguish
    // those contexts, so classify the terminal boundary here using the frozen
    // experiment horizon known by the collector.
    if (event == LteRlcUm::LINEAGE_CENSORED_RLC_DISPOSE &&
        Simulator::Now() >= Seconds(m_config.simTimeSec + m_config.cleanupTimeSec))
    {
        event = LteRlcUm::LINEAGE_CENSORED_SIM_END;
        disposition = LteRlcUm::DISPOSITION_CENSORED_SIM_END;
    }
    m_stream << "rlc-status-lineage/1.0," << m_config.experimentId << ',' << m_config.runId
             << ',' << Simulator::Now().GetNanoSeconds() << ',' << ++m_eventOrder << ','
             << lineageId << ',' << binding.ue->ueId << ',' << binding.ue->imsi << ",UL,"
             << rnti << ',' << binding.ue->rrc->GetCellId() << ','
             << static_cast<uint32_t>(lcid) << ',' << static_cast<uint32_t>(trafficClass) << ','
             << EventName(event) << ',';
    if (rlcPduId != 0)
    {
        m_stream << rlcPduId;
    }
    m_stream << ',' << originalSduBytes << ',';
    if (event == LteRlcUm::LINEAGE_SERVICE_FRAGMENT)
    {
        m_stream << segmentBytes;
    }
    m_stream << ',' << servedTotalBytes << ',' << remainingBytes << ',';
    if (event != LteRlcUm::LINEAGE_DROP_OVERFLOW)
    {
        m_stream << enqueueTimeNs;
    }
    m_stream << ',' << generationTimeNs << ',' << applicationAttemptTimeNs << ','
             << DispositionName(disposition) << '\n';
}

uint64_t
RlcLineageTraceCollector::GetSubscriptionCount() const
{
    return m_subscriptionCount;
}

} // namespace ns3::handover_congestion
