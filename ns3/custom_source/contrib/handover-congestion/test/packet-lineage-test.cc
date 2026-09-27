// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#include "ns3/experiment-config.h"
#include "ns3/enum.h"
#include "ns3/inet-socket-address.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/lte-mac-sap.h"
#include "ns3/lte-rlc-sap.h"
#include "ns3/lte-rlc-sdu-lineage-tag.h"
#include "ns3/lte-rlc-um.h"
#include "ns3/packet-lineage-trace-collector.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/simulator.h"
#include "ns3/test.h"
#include "ns3/udp-client-server-helper.h"

#include <algorithm>
#include <set>
#include <sstream>
#include <vector>

using namespace ns3;
using namespace ns3::handover_congestion;

namespace
{

class PassiveMacProvider : public LteMacSapProvider
{
  public:
    void TransmitPdu(TransmitPduParameters params) override
    {
        transmitted.push_back(params.pdu->GetSize());
    }
    void ReportBufferStatus(ReportBufferStatusParameters) override
    {
    }
    std::vector<uint32_t> transmitted;
};

struct LineageRow
{
    int64_t timeNs;
    uint8_t event;
    uint64_t lineage;
    uint32_t segment;
    uint32_t served;
    uint32_t remaining;
    uint64_t pdu;
    uint8_t disposition;
};

struct DecisionRow
{
    uint8_t policy;
    uint8_t action;
    uint64_t lineage;
    uint8_t trafficClass;
    uint32_t queueBytes;
    uint32_t statusCount;
    uint32_t otherCount;
};

class LineageHarness
{
  public:
    void Create()
    {
        rlc = CreateObject<LteRlcUm>();
        rlc->SetRnti(1);
        rlc->SetLcId(3);
        rlc->SetLteMacSapProvider(&mac);
        rlc->TraceConnectWithoutContext("SduLineage",
                                       MakeCallback(&LineageHarness::Trace, this));
        rlc->TraceConnectWithoutContext("SduSelection",
                                       MakeCallback(&LineageHarness::Decision, this));
    }

    void SetPolicy(LteRlcUm::TxSduSelectionPolicy policy)
    {
        rlc->SetAttribute("TxSduSelectionPolicy", EnumValue(policy));
    }

    void Send(uint64_t lineage, uint32_t bytes, uint8_t trafficClass = 1)
    {
        auto packet = ns3::Create<Packet>(bytes);
        packet->AddPacketTag(LteRlcSduLineageTag(lineage,
                                                trafficClass,
                                                Simulator::Now().GetNanoSeconds(),
                                                Simulator::Now().GetNanoSeconds()));
        LteRlcSapProvider::TransmitPdcpPduParameters params;
        params.pdcpPdu = packet;
        params.rnti = 1;
        params.lcid = 3;
        rlc->GetLteRlcSapProvider()->TransmitPdcpPdu(params);
    }

    void Grant(uint32_t bytes)
    {
        rlc->GetLteMacSapUser()->NotifyTxOpportunity({bytes, 0, 0, 0, 1, 3});
    }

    void Trace(uint16_t,
               uint8_t,
               uint8_t event,
               uint64_t lineage,
               uint8_t,
               int64_t,
               int64_t,
               int64_t,
               uint32_t,
               uint32_t segment,
               uint32_t served,
               uint32_t remaining,
               uint64_t pdu,
               uint8_t disposition)
    {
        rows.push_back({Simulator::Now().GetNanoSeconds(),
                        event,
                        lineage,
                        segment,
                        served,
                        remaining,
                        pdu,
                        disposition});
    }

    void Decision(uint16_t,
                  uint8_t,
                  uint8_t policy,
                  uint8_t action,
                  uint64_t lineage,
                  uint8_t trafficClass,
                  int64_t,
                  int64_t,
                  uint32_t,
                  uint32_t queueBytes,
                  uint32_t statusCount,
                  uint32_t,
                  uint32_t otherCount,
                  uint32_t)
    {
        decisions.push_back({policy,
                             action,
                             lineage,
                             trafficClass,
                             queueBytes,
                             statusCount,
                             otherCount});
    }

    PassiveMacProvider mac;
    Ptr<LteRlcUm> rlc;
    std::vector<LineageRow> rows;
    std::vector<DecisionRow> decisions;
};

std::vector<uint64_t>
ServiceOrder(const LineageHarness& harness)
{
    std::vector<uint64_t> order;
    for (const auto& row : harness.rows)
    {
        if (row.event == LteRlcUm::LINEAGE_SERVICE_FRAGMENT)
        {
            order.push_back(row.lineage);
        }
    }
    return order;
}

class TagNeutralityCase : public TestCase
{
  public:
    TagNeutralityCase()
        : TestCase("lineage tag is byte-neutral and survives copies and fragments")
    {
    }

  private:
    void DoRun() override
    {
        auto packet = Create<Packet>(120);
        const uint32_t original = packet->GetSize();
        LteRlcSduLineageTag tag(9, LteRlcSduLineageTag::STATUS, 11, 12);
        packet->AddPacketTag(tag);
        NS_TEST_ASSERT_MSG_EQ(packet->GetSize(), original, "tag changed serialized size");
        auto copy = packet->Copy();
        auto fragment = packet->CreateFragment(10, 20);
        LteRlcSduLineageTag observed;
        NS_TEST_ASSERT_MSG_EQ(copy->PeekPacketTag(observed), true, "copy lost tag");
        NS_TEST_ASSERT_MSG_EQ(fragment->PeekPacketTag(observed), true, "fragment lost tag");
        NS_TEST_ASSERT_MSG_EQ(observed.GetLineageId(), 9, "lineage value changed");
        NS_TEST_ASSERT_MSG_EQ(fragment->GetSize(), 20, "fragment bytes changed");
    }
};

class SegmentationCase : public TestCase
{
  public:
    SegmentationCase()
        : TestCase("one lineage survives RLC UM segmentation with byte conservation")
    {
    }

  private:
    void DoRun() override
    {
        LineageHarness h;
        h.Create();
        Simulator::Schedule(MilliSeconds(100), &LineageHarness::Send, &h, 10, 26, 1);
        Simulator::Schedule(MilliSeconds(150), &LineageHarness::Grant, &h, 10);
        Simulator::Schedule(MilliSeconds(200), &LineageHarness::Grant, &h, 30);
        Simulator::Run();
        uint32_t sum = 0;
        uint32_t fragments = 0;
        uint32_t terminal = 0;
        std::set<uint64_t> pdus;
        for (const auto& row : h.rows)
        {
            if (row.event == LteRlcUm::LINEAGE_SERVICE_FRAGMENT)
            {
                sum += row.segment;
                ++fragments;
                pdus.insert(row.pdu);
            }
            if (row.event == LteRlcUm::LINEAGE_FULLY_SERVED)
            {
                ++terminal;
                NS_TEST_ASSERT_MSG_EQ(row.remaining, 0, "terminal remainder");
                NS_TEST_ASSERT_MSG_EQ(row.served, 26, "terminal served total");
            }
        }
        NS_TEST_ASSERT_MSG_EQ(fragments, 2, "fragment event count");
        NS_TEST_ASSERT_MSG_EQ(pdus.size(), 2, "segmentation should span two RLC PDUs");
        NS_TEST_ASSERT_MSG_EQ(sum, 26, "fragment byte conservation");
        NS_TEST_ASSERT_MSG_EQ(terminal, 1, "one terminal FULLY_SERVED");
        h.rlc = nullptr;
        Simulator::Destroy();
    }
};

class SingleServiceCase : public TestCase
{
  public:
    SingleServiceCase()
        : TestCase("one SDU has one enqueue fragment and full-service terminal")
    {
    }

  private:
    void DoRun() override
    {
        LineageHarness h;
        h.Create();
        h.Send(5, 20);
        h.Grant(40);
        std::vector<uint8_t> events;
        for (const auto& row : h.rows)
        {
            events.push_back(row.event);
        }
        const std::vector<uint8_t> expected{LteRlcUm::LINEAGE_ENQUEUE,
                                            LteRlcUm::LINEAGE_SERVICE_FRAGMENT,
                                            LteRlcUm::LINEAGE_FULLY_SERVED};
        NS_TEST_ASSERT_MSG_EQ(events == expected, true, "single-service lineage sequence");
        h.rlc = nullptr;
        Simulator::Destroy();
    }
};

class ConcatenationCase : public TestCase
{
  public:
    ConcatenationCase()
        : TestCase("one RLC PDU preserves multiple constituent lineage identities")
    {
    }

  private:
    void DoRun() override
    {
        LineageHarness h;
        h.Create();
        h.Send(21, 10, 1);
        h.Send(22, 12, 2);
        h.Grant(40);
        std::set<uint64_t> lineages;
        std::set<uint64_t> pdus;
        std::vector<uint64_t> serviceOrder;
        for (const auto& row : h.rows)
        {
            if (row.event == LteRlcUm::LINEAGE_SERVICE_FRAGMENT)
            {
                lineages.insert(row.lineage);
                pdus.insert(row.pdu);
                serviceOrder.push_back(row.lineage);
            }
        }
        NS_TEST_ASSERT_MSG_EQ(lineages.size(), 2, "constituent identities collapsed");
        NS_TEST_ASSERT_MSG_EQ(pdus.size(), 1, "constituents should share one RLC PDU id");
        NS_TEST_ASSERT_MSG_EQ(serviceOrder == std::vector<uint64_t>({21, 22}), true,
                              "mixed traffic FIFO order changed");
        h.rlc = nullptr;
        Simulator::Destroy();
    }
};

class MultipleStatusFifoCase : public TestCase
{
  public:
    MultipleStatusFifoCase()
        : TestCase("multiple STATUS SDUs retain distinct FIFO lineage order")
    {
    }

  private:
    void DoRun() override
    {
        LineageHarness h;
        h.Create();
        h.Send(10, 10);
        h.Send(11, 10);
        h.Send(12, 10);
        h.Grant(60);
        std::vector<uint64_t> order;
        for (const auto& row : h.rows)
        {
            if (row.event == LteRlcUm::LINEAGE_SERVICE_FRAGMENT)
            {
                order.push_back(row.lineage);
            }
        }
        NS_TEST_ASSERT_MSG_EQ(order == std::vector<uint64_t>({10, 11, 12}), true,
                              "STATUS FIFO order changed");
        h.rlc = nullptr;
        Simulator::Destroy();
    }
};

class DispositionCase : public TestCase
{
  public:
    DispositionCase()
        : TestCase("overflow and simulation-end censor are explicit terminal dispositions")
    {
    }

  private:
    void DoRun() override
    {
        LineageHarness h;
        h.Create();
        h.rlc->SetAttribute("MaxTxBufferSize", UintegerValue(20));
        h.Send(31, 10);
        h.Send(32, 30);
        h.rlc->Dispose();
        uint32_t overflow = 0;
        uint32_t censor = 0;
        for (const auto& row : h.rows)
        {
            overflow += row.event == LteRlcUm::LINEAGE_DROP_OVERFLOW;
            censor += row.event == LteRlcUm::LINEAGE_CENSORED_SIM_END;
        }
        NS_TEST_ASSERT_MSG_EQ(overflow, 1, "explicit overflow terminal");
        NS_TEST_ASSERT_MSG_EQ(censor, 1, "explicit simulation-end censor terminal");
        h.rlc = nullptr;
        Simulator::Destroy();
    }
};

class ApplicationMappingCase : public TestCase
{
  public:
    ApplicationMappingCase()
        : TestCase("application attempt, successful sequence mapping, and RX lineage")
    {
    }

  private:
    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(2);
        InternetStackHelper internet;
        internet.Install(nodes);
        PointToPointHelper link;
        auto devices = link.Install(nodes);
        Ipv4AddressHelper addresses;
        addresses.SetBase("10.2.1.0", "255.255.255.0");
        auto interfaces = addresses.Assign(devices);
        constexpr uint16_t port = 9100;
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), port));
        auto sinks = sinkHelper.Install(nodes.Get(1));
        UdpClientHelper clientHelper(interfaces.GetAddress(1), port);
        clientHelper.SetAttribute("PacketSize", UintegerValue(120));
        clientHelper.SetAttribute("MaxPackets", UintegerValue(2));
        clientHelper.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
        auto clients = clientHelper.Install(nodes.Get(0));
        auto client = DynamicCast<UdpClient>(clients.Get(0));
        auto sink = DynamicCast<PacketSink>(sinks.Get(0));
        TrafficArtifacts traffic;
        traffic.bindings.push_back({0, 77, TrafficClass::STATUS, port, client, sink});
        ExperimentConfig config;
        config.experimentId = "fixture";
        config.runId = "app-map";
        std::ostringstream output;
        PacketLineageTraceCollector collector(config, output);
        collector.Connect(traffic);
        sinks.Start(Seconds(0));
        sinks.Stop(Seconds(1));
        clients.Start(Seconds(0.1));
        clients.Stop(Seconds(1));
        Simulator::Stop(Seconds(1));
        Simulator::Run();
        Simulator::Destroy();
        const std::string rows = output.str();
        NS_TEST_ASSERT_MSG_EQ(std::count(rows.begin(), rows.end(), '\n'), 6,
                              "two lineages should each have attempt/TX/RX");
        NS_TEST_ASSERT_MSG_NE(rows.find(",1,0,77,1,,100000000,,ATTEMPT_TAGGED,108"),
                              std::string::npos,
                              "first pre-header attempt row");
        NS_TEST_ASSERT_MSG_NE(rows.find(",1,0,77,1,0,100000000,100000000,TX_SUCCESS,120"),
                              std::string::npos,
                              "first successful sequence mapping");
        NS_TEST_ASSERT_MSG_NE(rows.find(",1,0,77,1,0,100000000,100000000,RX_FIRST,120"),
                              std::string::npos,
                              "first RX lineage mapping");
    }
};

class StatusPriorityBasicCase : public TestCase
{
  public:
    StatusPriorityBasicCase()
        : TestCase("STATUS priority is FIFO within class and applies at every grant boundary")
    {
    }

  private:
    void DoRun() override
    {
        LineageHarness h;
        h.Create();
        h.SetPolicy(LteRlcUm::STATUS_PRIORITY_NON_DROPPING);
        h.Send(101, 10, LteRlcSduLineageTag::BACKGROUND);
        h.Send(102, 10, LteRlcSduLineageTag::STATUS);
        h.Send(103, 10, LteRlcSduLineageTag::BACKGROUND);
        h.Send(104, 10, LteRlcSduLineageTag::STATUS);
        h.Grant(60);
        NS_TEST_ASSERT_MSG_EQ(ServiceOrder(h) == std::vector<uint64_t>({102, 104, 101, 103}),
                              true,
                              "STATUS priority or within-class FIFO failed");
        NS_TEST_ASSERT_MSG_EQ(h.decisions.at(0).action,
                              LteRlcUm::SELECT_STATUS_PRIORITY,
                              "first selection was not STATUS priority");
        NS_TEST_ASSERT_MSG_EQ(h.decisions.at(0).statusCount, 2, "STATUS count at selection");
        h.rlc = nullptr;
        Simulator::Destroy();
    }
};

class NonStatusOrderCase : public TestCase
{
  public:
    NonStatusOrderCase()
        : TestCase("STATUS policy preserves original ordering among non-STATUS SDUs")
    {
    }

  private:
    void DoRun() override
    {
        LineageHarness h;
        h.Create();
        h.SetPolicy(LteRlcUm::STATUS_PRIORITY_NON_DROPPING);
        h.Send(201, 10, LteRlcSduLineageTag::BACKGROUND);
        h.Send(202, 10, LteRlcSduLineageTag::BURST);
        h.Send(203, 10, LteRlcSduLineageTag::BACKGROUND);
        h.Grant(50);
        NS_TEST_ASSERT_MSG_EQ(ServiceOrder(h) == std::vector<uint64_t>({201, 202, 203}),
                              true,
                              "non-STATUS FIFO order changed");
        h.rlc = nullptr;
        Simulator::Destroy();
    }
};

class StatusBeforeUnstartedCase : public TestCase
{
  public:
    StatusBeforeUnstartedCase()
        : TestCase("waiting STATUS precedes an unstarted background SDU")
    {
    }

  private:
    void DoRun() override
    {
        LineageHarness h;
        h.Create();
        h.SetPolicy(LteRlcUm::STATUS_PRIORITY_NON_DROPPING);
        h.Send(301, 20, LteRlcSduLineageTag::BACKGROUND);
        h.Send(302, 20, LteRlcSduLineageTag::STATUS);
        h.Grant(25);
        NS_TEST_ASSERT_MSG_EQ(ServiceOrder(h).front(), 302, "unstarted background was preferred");
        h.rlc = nullptr;
        Simulator::Destroy();
    }
};

class PartialNonPreemptionCase : public TestCase
{
  public:
    PartialNonPreemptionCase()
        : TestCase("partially served non-STATUS SDU cannot be preempted by STATUS")
    {
    }

  private:
    void DoRun() override
    {
        LineageHarness h;
        h.Create();
        h.SetPolicy(LteRlcUm::STATUS_PRIORITY_NON_DROPPING);
        h.Send(401, 26, LteRlcSduLineageTag::BACKGROUND);
        h.Grant(10);
        h.Send(402, 10, LteRlcSduLineageTag::STATUS);
        h.Grant(40);
        const auto order = ServiceOrder(h);
        NS_TEST_ASSERT_MSG_EQ(order == std::vector<uint64_t>({401, 401, 402}),
                              true,
                              "STATUS interleaved inside a partially served SDU");
        NS_TEST_ASSERT_MSG_EQ(h.decisions.at(1).action,
                              LteRlcUm::CONTINUE_PARTIAL_SDU,
                              "partial continuation was not explicitly selected");
        NS_TEST_ASSERT_MSG_EQ(h.decisions.at(1).lineage, 401, "wrong partial lineage continued");
        NS_TEST_ASSERT_MSG_EQ(h.decisions.at(2).action,
                              LteRlcUm::SELECT_STATUS_PRIORITY,
                              "STATUS was not selected after partial completion");
        h.rlc = nullptr;
        Simulator::Destroy();
    }
};

class GrantAndNoDropCase : public TestCase
{
  public:
    GrantAndNoDropCase()
        : TestCase("policy changes ordering without grant manipulation or policy drop")
    {
    }

  private:
    static std::pair<std::vector<uint32_t>, uint32_t> RunPolicy(LteRlcUm::TxSduSelectionPolicy policy)
    {
        LineageHarness h;
        h.Create();
        h.SetPolicy(policy);
        h.Send(501, 10, LteRlcSduLineageTag::BACKGROUND);
        h.Send(502, 10, LteRlcSduLineageTag::STATUS);
        h.Grant(30);
        uint32_t policyDrops = 0;
        for (const auto& row : h.rows)
        {
            policyDrops += row.event != LteRlcUm::LINEAGE_FULLY_SERVED &&
                           row.disposition != LteRlcUm::DISPOSITION_NONE &&
                           row.disposition != LteRlcUm::DISPOSITION_DROP_OVERFLOW &&
                           row.disposition != LteRlcUm::DISPOSITION_CENSORED_SIM_END;
        }
        auto grants = h.mac.transmitted;
        h.rlc = nullptr;
        Simulator::Destroy();
        return {grants, policyDrops};
    }

    void DoRun() override
    {
        const auto fifo = RunPolicy(LteRlcUm::FIFO);
        const auto priority = RunPolicy(LteRlcUm::STATUS_PRIORITY_NON_DROPPING);
        NS_TEST_ASSERT_MSG_EQ(fifo.first == priority.first, true, "transmitted grant use changed");
        NS_TEST_ASSERT_MSG_EQ(priority.second, 0, "policy-specific drop disposition observed");
    }
};

class PacketLineageSuite : public TestSuite
{
  public:
    PacketLineageSuite()
        : TestSuite("handover-congestion-packet-lineage", Type::UNIT)
    {
        AddTestCase(new TagNeutralityCase, Duration::QUICK);
        AddTestCase(new ApplicationMappingCase, Duration::QUICK);
        AddTestCase(new SingleServiceCase, Duration::QUICK);
        AddTestCase(new SegmentationCase, Duration::QUICK);
        AddTestCase(new ConcatenationCase, Duration::QUICK);
        AddTestCase(new MultipleStatusFifoCase, Duration::QUICK);
        AddTestCase(new DispositionCase, Duration::QUICK);
        AddTestCase(new StatusPriorityBasicCase, Duration::QUICK);
        AddTestCase(new NonStatusOrderCase, Duration::QUICK);
        AddTestCase(new StatusBeforeUnstartedCase, Duration::QUICK);
        AddTestCase(new PartialNonPreemptionCase, Duration::QUICK);
        AddTestCase(new GrantAndNoDropCase, Duration::QUICK);
    }
};

PacketLineageSuite g_packetLineageSuite;

} // namespace
