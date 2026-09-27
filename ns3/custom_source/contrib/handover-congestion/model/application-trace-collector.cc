// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#include "application-trace-collector.h"

#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/seq-ts-header.h"
#include "ns3/simulator.h"

namespace ns3::handover_congestion
{
namespace
{
const char* ClassName(TrafficClass value)
{
    switch (value)
    {
    case TrafficClass::STATUS: return "STATUS";
    case TrafficClass::BACKGROUND: return "BACKGROUND";
    case TrafficClass::BURST: return "BURST";
    }
    return "UNKNOWN";
}

uint16_t Port(const Address& address)
{
    if (InetSocketAddress::IsMatchingType(address))
    {
        return InetSocketAddress::ConvertFrom(address).GetPort();
    }
    if (Inet6SocketAddress::IsMatchingType(address))
    {
        return Inet6SocketAddress::ConvertFrom(address).GetPort();
    }
    return 0;
}
} // namespace

ApplicationTraceCollector::ApplicationTraceCollector(const ExperimentConfig& config,
                                                     std::ostream& stream)
    : m_config(config),
      m_stream(stream)
{
}

void
ApplicationTraceCollector::Connect(const TrafficArtifacts& traffic)
{
    for (const auto& binding : traffic.bindings)
    {
        binding.client->TraceConnectWithoutContext(
            "TxWithSeqTs",
            MakeBoundCallback(&ApplicationTraceCollector::TxAdapter, this, &binding));
        binding.sink->TraceConnectWithoutContext(
            "RxWithAddresses",
            MakeBoundCallback(&ApplicationTraceCollector::RxAdapter, this, &binding));
    }
}

void
ApplicationTraceCollector::TxAdapter(ApplicationTraceCollector* self,
                                     const ApplicationBinding* binding,
                                     Ptr<const Packet> packet,
                                     const Address& from,
                                     const Address& to)
{
    self->Write(*binding, packet, from, to, "TX");
}

void
ApplicationTraceCollector::RxAdapter(ApplicationTraceCollector* self,
                                     const ApplicationBinding* binding,
                                     Ptr<const Packet> packet,
                                     const Address& from,
                                     const Address& to)
{
    self->Write(*binding, packet, from, to, "RX");
}

void
ApplicationTraceCollector::Write(const ApplicationBinding& binding,
                                 Ptr<const Packet> packet,
                                 const Address& from,
                                 const Address& to,
                                 const char* eventType)
{
    Ptr<Packet> copy = packet->Copy();
    SeqTsHeader header;
    if (copy->RemoveHeader(header) != header.GetSerializedSize())
    {
        return;
    }
    m_stream << "packet-events/1.0," << m_config.experimentId << ',' << m_config.runId << ','
             << Simulator::Now().GetNanoSeconds() << ',' << binding.ueId << ',' << binding.imsi
             << ',' << ClassName(binding.trafficClass) << ',' << header.GetSeq() << ',' << eventType
             << ',' << header.GetTs().GetNanoSeconds() << ',' << packet->GetSize() << ','
             << Port(from) << ',' << Port(to) << '\n';
}

} // namespace ns3::handover_congestion
