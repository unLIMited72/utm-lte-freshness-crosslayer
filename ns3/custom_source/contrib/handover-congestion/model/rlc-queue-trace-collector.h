// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#ifndef HANDOVER_CONGESTION_RLC_QUEUE_TRACE_COLLECTOR_H
#define HANDOVER_CONGESTION_RLC_QUEUE_TRACE_COLLECTOR_H

#include "experiment-config.h"
#include "lte-network-builder.h"
#include "ns3/lte-rlc-um.h"
#include "ns3/lte-ue-rrc.h"

#include <cstdint>
#include <list>
#include <map>
#include <ostream>

namespace ns3::handover_congestion
{

class RlcQueueTraceCollector
{
  public:
    RlcQueueTraceCollector(const ExperimentConfig& config, std::ostream& stream);
    void Connect(const NetworkArtifacts& network);
    uint64_t GetSubscriptionCount() const;

  private:
    struct UeBinding
    {
        uint32_t ueId{0};
        uint64_t imsi{0};
        Ptr<LteUeRrc> rrc;
    };
    struct RlcBinding
    {
        UeBinding* ue{nullptr};
        uint8_t lcid{0};
    };

    static void DrbCreatedAdapter(RlcQueueTraceCollector*, UeBinding*, uint64_t, uint16_t,
                                  uint16_t, uint8_t);
    static void QueueAdapter(RlcQueueTraceCollector*, RlcBinding*, uint16_t, uint8_t, uint8_t,
                             uint32_t, uint32_t, uint64_t, bool, uint32_t);
    void ConnectDrb(UeBinding&, uint8_t drbIdentity);
    void Write(const RlcBinding&, uint16_t rnti, uint8_t lcid, uint8_t reason,
               uint32_t queueBytes, uint32_t containerCount, uint64_t holDelayNs,
               bool queueEmpty, uint32_t affectedBytes);

    const ExperimentConfig& m_config;
    std::ostream& m_stream;
    std::map<uint64_t, UeBinding> m_ues;
    std::list<RlcBinding> m_rlcBindings;
    uint64_t m_eventOrder{0};
    uint64_t m_subscriptionCount{0};
};

} // namespace ns3::handover_congestion
#endif
