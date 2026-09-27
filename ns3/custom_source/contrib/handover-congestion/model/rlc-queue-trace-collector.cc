// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#include "rlc-queue-trace-collector.h"

#include "ns3/lte-radio-bearer-info.h"
#include "ns3/lte-ue-net-device.h"
#include "ns3/object-map.h"
#include "ns3/simulator.h"

namespace ns3::handover_congestion
{
namespace
{
const char* ReasonName(uint8_t reason)
{
    switch (reason)
    {
    case LteRlcUm::QUEUE_ENQUEUE: return "ENQUEUE";
    case LteRlcUm::QUEUE_TX_OPPORTUNITY: return "TX_OPPORTUNITY";
    case LteRlcUm::QUEUE_DROP_OVERFLOW: return "DROP_OVERFLOW";
    }
    return "UNKNOWN";
}
} // namespace

RlcQueueTraceCollector::RlcQueueTraceCollector(const ExperimentConfig& config,
                                               std::ostream& stream)
    : m_config(config),
      m_stream(stream)
{
}

void
RlcQueueTraceCollector::Connect(const NetworkArtifacts& network)
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
            MakeBoundCallback(&RlcQueueTraceCollector::DrbCreatedAdapter, this, &binding));
    }
}

void
RlcQueueTraceCollector::DrbCreatedAdapter(RlcQueueTraceCollector* self,
                                          UeBinding* binding,
                                          uint64_t,
                                          uint16_t,
                                          uint16_t,
                                          uint8_t drbIdentity)
{
    self->ConnectDrb(*binding, drbIdentity);
}

void
RlcQueueTraceCollector::ConnectDrb(UeBinding& ue, uint8_t drbIdentity)
{
    ObjectMapValue bearers;
    ue.rrc->GetAttribute("DataRadioBearerMap", bearers);
    auto object = bearers.Get(drbIdentity);
    auto bearer = DynamicCast<LteDataRadioBearerInfo>(object);
    NS_ABORT_MSG_IF(!bearer, "DrbCreated did not resolve a UE data-radio-bearer object");
    auto rlc = DynamicCast<LteRlcUm>(bearer->m_rlc);
    NS_ABORT_MSG_IF(!rlc, "instrumented UE data bearer is not LteRlcUm");
    m_rlcBindings.push_back(RlcBinding{&ue, bearer->m_logicalChannelIdentity});
    auto& binding = m_rlcBindings.back();
    rlc->TraceConnectWithoutContext(
        "QueueState",
        MakeBoundCallback(&RlcQueueTraceCollector::QueueAdapter, this, &binding));
    ++m_subscriptionCount;
}

void
RlcQueueTraceCollector::QueueAdapter(RlcQueueTraceCollector* self,
                                     RlcBinding* binding,
                                     uint16_t rnti,
                                     uint8_t lcid,
                                     uint8_t reason,
                                     uint32_t queueBytes,
                                     uint32_t containerCount,
                                     uint64_t holDelayNs,
                                     bool queueEmpty,
                                     uint32_t affectedBytes)
{
    self->Write(*binding, rnti, lcid, reason, queueBytes, containerCount, holDelayNs,
                queueEmpty, affectedBytes);
}

void
RlcQueueTraceCollector::Write(const RlcBinding& binding,
                              uint16_t rnti,
                              uint8_t lcid,
                              uint8_t reason,
                              uint32_t queueBytes,
                              uint32_t containerCount,
                              uint64_t holDelayNs,
                              bool queueEmpty,
                              uint32_t affectedBytes)
{
    NS_ABORT_MSG_IF(binding.lcid != lcid, "RLC QueueState LCID changed after DRB binding");
    NS_ABORT_MSG_IF(queueEmpty != (queueBytes == 0 && containerCount == 0),
                    "inconsistent RLC QueueState empty flag");
    m_stream << "rlc-queue-events/1.0," << m_config.experimentId << ',' << m_config.runId << ','
             << Simulator::Now().GetNanoSeconds() << ',' << ++m_eventOrder << ','
             << binding.ue->ueId << ',' << binding.ue->imsi << ",UL," << rnti << ','
             << binding.ue->rrc->GetCellId() << ',' << static_cast<uint32_t>(lcid) << ','
             << ReasonName(reason) << ',' << queueBytes << ',' << containerCount << ','
             << holDelayNs << ',' << (queueEmpty ? 1 : 0) << ',' << affectedBytes << '\n';
}

uint64_t
RlcQueueTraceCollector::GetSubscriptionCount() const
{
    return m_subscriptionCount;
}

} // namespace ns3::handover_congestion
