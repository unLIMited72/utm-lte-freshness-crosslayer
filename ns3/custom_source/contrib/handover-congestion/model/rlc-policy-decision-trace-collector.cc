// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#include "rlc-policy-decision-trace-collector.h"

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
PolicyName(uint8_t policy)
{
    return policy == LteRlcUm::FIFO ? "FIFO" : "STATUS_PRIORITY_NON_DROPPING";
}

const char*
ActionName(uint8_t action)
{
    switch (action)
    {
    case LteRlcUm::SELECT_FIFO: return "SELECT_FIFO";
    case LteRlcUm::SELECT_STATUS_PRIORITY: return "SELECT_STATUS_PRIORITY";
    case LteRlcUm::CONTINUE_PARTIAL_SDU: return "CONTINUE_PARTIAL_SDU";
    }
    return "UNKNOWN";
}

const char*
ReasonName(uint8_t action)
{
    switch (action)
    {
    case LteRlcUm::SELECT_FIFO: return "FIFO_POLICY_OR_NO_WAITING_STATUS";
    case LteRlcUm::SELECT_STATUS_PRIORITY: return "OLDEST_WAITING_STATUS";
    case LteRlcUm::CONTINUE_PARTIAL_SDU: return "PARTIAL_SDU_IN_PROGRESS";
    }
    return "UNKNOWN";
}
} // namespace

RlcPolicyDecisionTraceCollector::RlcPolicyDecisionTraceCollector(const ExperimentConfig& config,
                                                                 std::ostream& stream)
    : m_config(config),
      m_stream(stream)
{
}

void
RlcPolicyDecisionTraceCollector::Connect(const NetworkArtifacts& network)
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
            MakeBoundCallback(&RlcPolicyDecisionTraceCollector::DrbCreatedAdapter,
                              this,
                              &binding));
    }
}

void
RlcPolicyDecisionTraceCollector::DrbCreatedAdapter(RlcPolicyDecisionTraceCollector* self,
                                                   UeBinding* binding,
                                                   uint64_t,
                                                   uint16_t,
                                                   uint16_t,
                                                   uint8_t drbIdentity)
{
    self->ConnectDrb(*binding, drbIdentity);
}

void
RlcPolicyDecisionTraceCollector::ConnectDrb(UeBinding& ue, uint8_t drbIdentity)
{
    ObjectMapValue bearers;
    ue.rrc->GetAttribute("DataRadioBearerMap", bearers);
    auto bearer = DynamicCast<LteDataRadioBearerInfo>(bearers.Get(drbIdentity));
    NS_ABORT_MSG_IF(!bearer, "DrbCreated did not resolve a policy-decision bearer");
    auto rlc = DynamicCast<LteRlcUm>(bearer->m_rlc);
    NS_ABORT_MSG_IF(!rlc, "policy-decision bearer is not LteRlcUm");
    m_rlcBindings.push_back(RlcBinding{&ue, bearer->m_logicalChannelIdentity});
    auto& binding = m_rlcBindings.back();
    rlc->TraceConnectWithoutContext(
        "SduSelection",
        MakeBoundCallback(&RlcPolicyDecisionTraceCollector::DecisionAdapter, this, &binding));
}

void
RlcPolicyDecisionTraceCollector::DecisionAdapter(RlcPolicyDecisionTraceCollector* self,
                                                  RlcBinding* binding,
                                                  uint16_t rnti,
                                                  uint8_t lcid,
                                                  uint8_t policy,
                                                  uint8_t action,
                                                  uint64_t lineageId,
                                                  uint8_t trafficClass,
                                                  int64_t generationTimeNs,
                                                  int64_t enqueueTimeNs,
                                                  uint32_t remainingBytes,
                                                  uint32_t queueBytes,
                                                  uint32_t statusCount,
                                                  uint32_t statusBytes,
                                                  uint32_t otherCount,
                                                  uint32_t otherBytes)
{
    self->Write(*binding, rnti, lcid, policy, action, lineageId, trafficClass,
                generationTimeNs, enqueueTimeNs, remainingBytes, queueBytes, statusCount,
                statusBytes, otherCount, otherBytes);
}

void
RlcPolicyDecisionTraceCollector::Write(const RlcBinding& binding,
                                       uint16_t rnti,
                                       uint8_t lcid,
                                       uint8_t policy,
                                       uint8_t action,
                                       uint64_t lineageId,
                                       uint8_t trafficClass,
                                       int64_t generationTimeNs,
                                       int64_t enqueueTimeNs,
                                       uint32_t remainingBytes,
                                       uint32_t queueBytes,
                                       uint32_t statusCount,
                                       uint32_t statusBytes,
                                       uint32_t otherCount,
                                       uint32_t otherBytes)
{
    NS_ABORT_MSG_IF(binding.lcid != lcid, "RLC policy-decision LCID changed after binding");
    const int64_t nowNs = Simulator::Now().GetNanoSeconds();
    NS_ABORT_MSG_IF(enqueueTimeNs > nowNs, "selected SDU enqueue time is in the future");
    m_stream << "policy-decisions/1.0," << m_config.experimentId << ',' << m_config.runId
             << ',' << nowNs << ',' << ++m_eventOrder << ',' << binding.ue->ueId << ','
             << binding.ue->imsi << ',' << PolicyName(policy) << ','
             << ActionName(action) << ',' << ReasonName(action) << ',';
    if (lineageId != 0)
    {
        m_stream << lineageId;
    }
    m_stream << ',' << static_cast<uint32_t>(trafficClass) << ',';
    if (generationTimeNs != 0)
    {
        m_stream << generationTimeNs;
    }
    m_stream << ',' << enqueueTimeNs << ',' << nowNs - enqueueTimeNs << ',' << remainingBytes
             << ',' << queueBytes << ',' << statusCount << ',' << statusBytes << ','
             << otherCount << ',' << otherBytes << ',' << rnti << ','
             << binding.ue->rrc->GetCellId() << ',' << static_cast<uint32_t>(lcid)
             << ",\n"; // ho_active is intentionally observational NA in Phase 2.
}

} // namespace ns3::handover_congestion
