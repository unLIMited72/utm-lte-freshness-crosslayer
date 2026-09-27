// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#include "packet-lineage-trace-collector.h"

#include "ns3/abort.h"
#include "ns3/lte-rlc-sdu-lineage-tag.h"
#include "ns3/seq-ts-header.h"
#include "ns3/simulator.h"

namespace ns3::handover_congestion
{

PacketLineageTraceCollector::PacketLineageTraceCollector(const ExperimentConfig& config,
                                                         std::ostream& stream)
    : m_config(config),
      m_stream(stream)
{
}

uint8_t
PacketLineageTraceCollector::ClassCode(TrafficClass trafficClass)
{
    switch (trafficClass)
    {
    case TrafficClass::STATUS: return LteRlcSduLineageTag::STATUS;
    case TrafficClass::BACKGROUND: return LteRlcSduLineageTag::BACKGROUND;
    case TrafficClass::BURST: return LteRlcSduLineageTag::BURST;
    }
    return LteRlcSduLineageTag::UNINSTRUMENTED;
}

void
PacketLineageTraceCollector::Connect(const TrafficArtifacts& traffic)
{
    for (const auto& binding : traffic.bindings)
    {
        binding.client->TraceConnectWithoutContext(
            "TxWithAddresses",
            MakeBoundCallback(&PacketLineageTraceCollector::AttemptAdapter, this, &binding));
        binding.client->TraceConnectWithoutContext(
            "TxWithSeqTs",
            MakeBoundCallback(&PacketLineageTraceCollector::SuccessAdapter, this, &binding));
        binding.sink->TraceConnectWithoutContext(
            "RxWithAddresses",
            MakeBoundCallback(&PacketLineageTraceCollector::RxAdapter, this, &binding));
    }
}

void
PacketLineageTraceCollector::AttemptAdapter(PacketLineageTraceCollector* self,
                                            const ApplicationBinding* binding,
                                            Ptr<const Packet> packet,
                                            const Address&,
                                            const Address&)
{
    self->Attempt(*binding, packet);
}

void
PacketLineageTraceCollector::SuccessAdapter(PacketLineageTraceCollector* self,
                                            const ApplicationBinding* binding,
                                            Ptr<const Packet> packet,
                                            const Address&,
                                            const Address&)
{
    self->Success(*binding, packet);
}

void
PacketLineageTraceCollector::RxAdapter(PacketLineageTraceCollector* self,
                                       const ApplicationBinding* binding,
                                       Ptr<const Packet> packet,
                                       const Address&,
                                       const Address&)
{
    self->Receive(*binding, packet);
}

void
PacketLineageTraceCollector::Attempt(const ApplicationBinding& binding, Ptr<const Packet> packet)
{
    const int64_t nowNs = Simulator::Now().GetNanoSeconds();
    const uint64_t lineageId = ++m_nextLineageId;
    LteRlcSduLineageTag tag(lineageId, ClassCode(binding.trafficClass), nowNs, nowNs);
    packet->AddPacketTag(tag);
    Write(binding, lineageId, "ATTEMPT_TAGGED", false, 0, nowNs, false, 0, packet->GetSize());
}

void
PacketLineageTraceCollector::Success(const ApplicationBinding& binding, Ptr<const Packet> packet)
{
    LteRlcSduLineageTag tag;
    NS_ABORT_MSG_IF(!packet->PeekPacketTag(tag), "successful application TX lacks lineage tag");
    Ptr<Packet> copy = packet->Copy();
    SeqTsHeader header;
    NS_ABORT_MSG_IF(copy->RemoveHeader(header) != header.GetSerializedSize(),
                    "successful application TX lacks SeqTsHeader");
    const int64_t generationNs = header.GetTs().GetNanoSeconds();
    NS_ABORT_MSG_IF(generationNs != tag.GetGenerationTimeNs(),
                    "lineage generation time differs from SeqTsHeader");
    SequenceKey key{binding.imsi, ClassCode(binding.trafficClass), header.GetSeq()};
    NS_ABORT_MSG_IF(m_successful.count(key) != 0, "duplicate successful sequence mapping");
    const int64_t txTimeNs = Simulator::Now().GetNanoSeconds();
    m_successful.emplace(key, SuccessfulTx{tag.GetLineageId(), txTimeNs});
    Write(binding,
          tag.GetLineageId(),
          "TX_SUCCESS",
          true,
          header.GetSeq(),
          generationNs,
          true,
          txTimeNs,
          packet->GetSize());
}

void
PacketLineageTraceCollector::Receive(const ApplicationBinding& binding, Ptr<const Packet> packet)
{
    Ptr<Packet> copy = packet->Copy();
    SeqTsHeader header;
    NS_ABORT_MSG_IF(copy->RemoveHeader(header) != header.GetSerializedSize(),
                    "application RX lacks SeqTsHeader");
    SequenceKey key{binding.imsi, ClassCode(binding.trafficClass), header.GetSeq()};
    auto found = m_successful.find(key);
    NS_ABORT_MSG_IF(found == m_successful.end(), "application RX has no successful lineage mapping");
    const bool first = m_received.insert(key).second;
    Write(binding,
          found->second.lineageId,
          first ? "RX_FIRST" : "RX_DUPLICATE",
          true,
          header.GetSeq(),
          header.GetTs().GetNanoSeconds(),
          true,
          found->second.txTimeNs,
          packet->GetSize());
}

void
PacketLineageTraceCollector::Write(const ApplicationBinding& binding,
                                   uint64_t lineageId,
                                   const char* eventType,
                                   bool hasSequence,
                                   uint32_t sequence,
                                   int64_t generationTimeNs,
                                   bool hasTxTime,
                                   int64_t txTimeNs,
                                   uint32_t packetSizeBytes)
{
    m_stream << "packet-lineage-events/1.0," << m_config.experimentId << ',' << m_config.runId
             << ',' << Simulator::Now().GetNanoSeconds() << ',' << ++m_eventOrder << ','
             << lineageId << ',' << binding.ueId << ',' << binding.imsi << ','
             << static_cast<uint32_t>(ClassCode(binding.trafficClass)) << ',';
    if (hasSequence)
    {
        m_stream << sequence;
    }
    m_stream << ',' << generationTimeNs << ',';
    if (hasTxTime)
    {
        m_stream << txTimeNs;
    }
    m_stream << ',' << eventType << ',' << packetSizeBytes << '\n';
}

} // namespace ns3::handover_congestion
