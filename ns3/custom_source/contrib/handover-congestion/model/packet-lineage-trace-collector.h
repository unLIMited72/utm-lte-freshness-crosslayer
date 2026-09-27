// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#ifndef HANDOVER_CONGESTION_PACKET_LINEAGE_TRACE_COLLECTOR_H
#define HANDOVER_CONGESTION_PACKET_LINEAGE_TRACE_COLLECTOR_H

#include "experiment-config.h"
#include "traffic-installer.h"
#include "ns3/address.h"
#include "ns3/packet.h"

#include <map>
#include <ostream>
#include <set>
#include <tuple>

namespace ns3::handover_congestion
{

class PacketLineageTraceCollector
{
  public:
    PacketLineageTraceCollector(const ExperimentConfig& config, std::ostream& stream);
    void Connect(const TrafficArtifacts& traffic);

  private:
    using SequenceKey = std::tuple<uint64_t, uint8_t, uint32_t>;
    struct SuccessfulTx
    {
        uint64_t lineageId{0};
        int64_t txTimeNs{0};
    };

    static void AttemptAdapter(PacketLineageTraceCollector*, const ApplicationBinding*,
                               Ptr<const Packet>, const Address&, const Address&);
    static void SuccessAdapter(PacketLineageTraceCollector*, const ApplicationBinding*,
                               Ptr<const Packet>, const Address&, const Address&);
    static void RxAdapter(PacketLineageTraceCollector*, const ApplicationBinding*,
                          Ptr<const Packet>, const Address&, const Address&);
    void Attempt(const ApplicationBinding&, Ptr<const Packet>);
    void Success(const ApplicationBinding&, Ptr<const Packet>);
    void Receive(const ApplicationBinding&, Ptr<const Packet>);
    void Write(const ApplicationBinding&, uint64_t lineageId, const char* eventType,
               bool hasSequence, uint32_t sequence, int64_t generationTimeNs,
               bool hasTxTime, int64_t txTimeNs, uint32_t packetSizeBytes);
    static uint8_t ClassCode(TrafficClass trafficClass);

    const ExperimentConfig& m_config;
    std::ostream& m_stream;
    uint64_t m_nextLineageId{0};
    uint64_t m_eventOrder{0};
    std::map<SequenceKey, SuccessfulTx> m_successful;
    std::set<SequenceKey> m_received;
};

} // namespace ns3::handover_congestion

#endif
