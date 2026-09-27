// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#include "ns3/experiment-config.h"
#include "ns3/detailed-handover-event-collector.h"
#include "ns3/inet-socket-address.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/lte-mac-sap.h"
#include "ns3/lte-rlc-sap.h"
#include "ns3/lte-rlc-um.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/seq-ts-header.h"
#include "ns3/simulator.h"
#include "ns3/test.h"
#include "ns3/udp-client-server-helper.h"

#include <algorithm>
#include <sstream>
#include <vector>

using namespace ns3::handover_congestion;

class PacketIdentityTraceCase : public ns3::TestCase
{
  public:
    PacketIdentityTraceCase():TestCase("UdpClient header-bearing successful trace"){}
  private:
    void LegacyTx(ns3::Ptr<const ns3::Packet> packet)
    {
        m_legacySizes.push_back(packet->GetSize());
    }
    void HeaderTx(ns3::Ptr<const ns3::Packet> packet, const ns3::Address&, const ns3::Address&)
    {
        const auto originalSize = packet->GetSize();
        auto copy = packet->Copy();
        ns3::SeqTsHeader header;
        copy->RemoveHeader(header);
        m_txSequences.push_back(header.GetSeq());
        m_txGenerationNs.push_back(header.GetTs().GetNanoSeconds());
        m_txSizes.push_back(originalSize);
        NS_TEST_ASSERT_MSG_EQ(packet->GetSize(),originalSize,"TX parsing must not mutate packet");
    }
    void Rx(ns3::Ptr<const ns3::Packet> packet, const ns3::Address&, const ns3::Address&)
    {
        const auto originalSize = packet->GetSize();
        auto copy = packet->Copy();
        ns3::SeqTsHeader header;
        copy->RemoveHeader(header);
        m_rxSequences.push_back(header.GetSeq());
        m_rxGenerationNs.push_back(header.GetTs().GetNanoSeconds());
        NS_TEST_ASSERT_MSG_EQ(packet->GetSize(),originalSize,"RX parsing must not mutate packet");
    }
    void DoRun() override
    {
        ns3::NodeContainer nodes;nodes.Create(2);
        ns3::InternetStackHelper internet;internet.Install(nodes);
        ns3::PointToPointHelper link;
        auto devices=link.Install(nodes);
        ns3::Ipv4AddressHelper addresses;addresses.SetBase("10.1.1.0","255.255.255.0");
        auto interfaces=addresses.Assign(devices);
        constexpr uint16_t port=9000;
        ns3::PacketSinkHelper sink("ns3::UdpSocketFactory",ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(),port));
        auto sinks=sink.Install(nodes.Get(1));sinks.Start(ns3::Seconds(0));sinks.Stop(ns3::Seconds(1));
        ns3::UdpClientHelper client(interfaces.GetAddress(1),port);
        client.SetAttribute("PacketSize",ns3::UintegerValue(120));
        client.SetAttribute("MaxPackets",ns3::UintegerValue(2));
        client.SetAttribute("Interval",ns3::TimeValue(ns3::MilliSeconds(10)));
        auto clients=client.Install(nodes.Get(0));clients.Start(ns3::Seconds(0.1));clients.Stop(ns3::Seconds(1));
        clients.Get(0)->TraceConnectWithoutContext("Tx",ns3::MakeCallback(&PacketIdentityTraceCase::LegacyTx,this));
        clients.Get(0)->TraceConnectWithoutContext("TxWithSeqTs",ns3::MakeCallback(&PacketIdentityTraceCase::HeaderTx,this));
        sinks.Get(0)->TraceConnectWithoutContext("RxWithAddresses",ns3::MakeCallback(&PacketIdentityTraceCase::Rx,this));
        ns3::Simulator::Stop(ns3::Seconds(1));ns3::Simulator::Run();ns3::Simulator::Destroy();
        NS_TEST_ASSERT_MSG_EQ(m_legacySizes.size(),2,"legacy trace count");
        NS_TEST_ASSERT_MSG_EQ(m_legacySizes[0],108,"legacy trace remains pre-header");
        NS_TEST_ASSERT_MSG_EQ(m_txSizes[0],120,"configured datagram remains 120 B");
        NS_TEST_ASSERT_MSG_EQ(m_txSequences.size(),2,"successful TX count");
        NS_TEST_ASSERT_MSG_EQ(m_rxSequences.size(),2,"RX count");
        for(std::size_t i=0;i<2;++i)
        {
            NS_TEST_ASSERT_MSG_EQ(m_txSequences[i],i,"TX sequence monotonic");
            NS_TEST_ASSERT_MSG_EQ(m_rxSequences[i],m_txSequences[i],"RX sequence matches TX");
            NS_TEST_ASSERT_MSG_EQ(m_rxGenerationNs[i],m_txGenerationNs[i],"generation timestamp survives delivery");
        }
    }
    std::vector<uint32_t> m_legacySizes,m_txSizes,m_txSequences,m_rxSequences;
    std::vector<int64_t> m_txGenerationNs,m_rxGenerationNs;
};

class DetailedHandoverPairingCase : public ns3::TestCase
{
  public:
    DetailedHandoverPairingCase():TestCase("detailed handover pairing state machine"){}
  private:
    void DoRun() override
    {
        ExperimentConfig config;config.experimentId="fixture";config.runId="run-a";
        std::ostringstream output;
        DetailedHandoverEventCollector collector(config,output);
        collector.ObserveStart(0,11,1,101,2,100);
        collector.ObserveEnd(11,2,202,150,true);
        collector.ObserveStart(0,11,2,202,3,200);
        collector.ObserveEnd(11,2,202,230,false);
        collector.ObserveStart(1,12,1,301,3,300);
        collector.ObserveStart(1,12,2,302,3,320);
        collector.Finalize();
        const std::string rows=output.str();
        NS_TEST_ASSERT_MSG_NE(rows.find("handover-events/1.0,fixture,run-a,1,0,11,1,2,2,100,150,OK,50,101,202"),std::string::npos,"OK row");
        NS_TEST_ASSERT_MSG_NE(rows.find("handover-events/1.0,fixture,run-a,2,0,11,2,3,2,200,230,ERROR,30,202,202"),std::string::npos,"ERROR row");
        NS_TEST_ASSERT_MSG_NE(rows.find("handover-events/1.0,fixture,run-a,1,1,12,1,3,2,300,320,SUPERSEDED,20,301,302"),std::string::npos,"superseded row");
        NS_TEST_ASSERT_MSG_NE(rows.find("handover-events/1.0,fixture,run-a,2,1,12,2,3,,320,,INCOMPLETE,,302,"),std::string::npos,"incomplete row");
        NS_TEST_ASSERT_MSG_EQ(std::count(rows.begin(),rows.end(),'\n'),4,"one row per paired/flushed event");
    }
};

class PassiveMacProvider : public ns3::LteMacSapProvider
{
  public:
    void TransmitPdu(TransmitPduParameters) override {}
    void ReportBufferStatus(ReportBufferStatusParameters) override {}
};

class RlcQueueStateCase : public ns3::TestCase
{
  public:
    RlcQueueStateCase():TestCase("RLC UM passive queue post-state and segmentation HOL"){}
  private:
    struct Row
    {
        int64_t timeNs;
        uint8_t reason;
        uint32_t bytes;
        uint32_t count;
        uint64_t holNs;
        bool empty;
        uint32_t affected;
    };
    void Queue(uint16_t,uint8_t,uint8_t reason,uint32_t bytes,uint32_t count,uint64_t hol,bool empty,uint32_t affected)
    {
        m_rows.push_back({ns3::Simulator::Now().GetNanoSeconds(),reason,bytes,count,hol,empty,affected});
    }
    void Send(uint32_t size)
    {
        ns3::LteRlcSapProvider::TransmitPdcpPduParameters p;
        p.pdcpPdu=ns3::Create<ns3::Packet>(size);p.rnti=1;p.lcid=3;
        m_rlc->GetLteRlcSapProvider()->TransmitPdcpPdu(p);
    }
    void Grant(uint32_t bytes)
    {
        m_rlc->GetLteMacSapUser()->NotifyTxOpportunity({bytes,0,0,0,1,3});
    }
    void SetSmallBufferAndOverflow()
    {
        m_rlc->SetAttribute("MaxTxBufferSize",ns3::UintegerValue(10));
        Send(20);
    }
    void DoRun() override
    {
        m_rlc=ns3::CreateObject<ns3::LteRlcUm>();m_rlc->SetRnti(1);m_rlc->SetLcId(3);
        m_rlc->SetLteMacSapProvider(&m_mac);
        m_rlc->TraceConnectWithoutContext("QueueState",ns3::MakeCallback(&RlcQueueStateCase::Queue,this));
        ns3::Simulator::Schedule(ns3::MilliSeconds(100),&RlcQueueStateCase::Send,this,26);
        ns3::Simulator::Schedule(ns3::MilliSeconds(150),&RlcQueueStateCase::Grant,this,10);
        ns3::Simulator::Schedule(ns3::MilliSeconds(200),&RlcQueueStateCase::Grant,this,30);
        ns3::Simulator::Schedule(ns3::MilliSeconds(300),&RlcQueueStateCase::SetSmallBufferAndOverflow,this);
        ns3::Simulator::Run();ns3::Simulator::Destroy();
        NS_TEST_ASSERT_MSG_EQ(m_rows.size(),4,"one selected row per state transition");
        NS_TEST_ASSERT_MSG_EQ(m_rows[0].reason,ns3::LteRlcUm::QUEUE_ENQUEUE,"enqueue reason");
        NS_TEST_ASSERT_MSG_EQ(m_rows[0].bytes,26,"post-enqueue bytes");
        NS_TEST_ASSERT_MSG_EQ(m_rows[0].count,1,"post-enqueue container count");
        NS_TEST_ASSERT_MSG_EQ(m_rows[0].holNs,0,"same-time enqueue HOL");
        NS_TEST_ASSERT_MSG_EQ(m_rows[1].reason,ns3::LteRlcUm::QUEUE_TX_OPPORTUNITY,"partial grant reason");
        NS_TEST_ASSERT_MSG_EQ(m_rows[1].bytes,18,"segmentation remainder bytes");
        NS_TEST_ASSERT_MSG_EQ(m_rows[1].count,1,"segmentation remainder container");
        NS_TEST_ASSERT_MSG_EQ(m_rows[1].holNs,50000000,"remainder retains original 100 ms arrival");
        NS_TEST_ASSERT_MSG_EQ(m_rows[1].affected,8,"partial grant affected bytes");
        NS_TEST_ASSERT_MSG_EQ(m_rows[2].bytes,0,"final drain bytes");
        NS_TEST_ASSERT_MSG_EQ(m_rows[2].count,0,"final drain containers");
        NS_TEST_ASSERT_MSG_EQ(m_rows[2].empty,true,"final drain empty");
        NS_TEST_ASSERT_MSG_EQ(m_rows[3].reason,ns3::LteRlcUm::QUEUE_DROP_OVERFLOW,"overflow reason");
        NS_TEST_ASSERT_MSG_EQ(m_rows[3].bytes,0,"overflow does not mutate queue");
        NS_TEST_ASSERT_MSG_EQ(m_rows[3].affected,20,"overflow affected bytes");
        m_rlc=nullptr;
    }
    PassiveMacProvider m_mac;
    ns3::Ptr<ns3::LteRlcUm> m_rlc;
    std::vector<Row> m_rows;
};

class BaselineDefaultsCase : public ns3::TestCase
{
  public:
    BaselineDefaultsCase():TestCase("effective baseline defaults"){}
  private:
    void DoRun() override
    {
        ExperimentConfig c;
        NS_TEST_ASSERT_MSG_EQ(c.schedulerType,"ns3::PfFfMacScheduler","scheduler");
        NS_TEST_ASSERT_MSG_EQ(c.useIdealRrc,true,"ideal RRC");
        NS_TEST_ASSERT_MSG_EQ(c.dlBandwidthRb,25,"DL RB");NS_TEST_ASSERT_MSG_EQ(c.ulBandwidthRb,25,"UL RB");
        NS_TEST_ASSERT_MSG_EQ(c.dlEarfcn,100,"DL EARFCN");NS_TEST_ASSERT_MSG_EQ(c.ulEarfcn,18100,"UL EARFCN");
        NS_TEST_ASSERT_MSG_EQ_TOL(c.enbTxPowerDbm,30.0,1e-12,"eNB power");
        NS_TEST_ASSERT_MSG_EQ_TOL(c.ueTxPowerDbm,10.0,1e-12,"UE power");
        NS_TEST_ASSERT_MSG_EQ_TOL(c.warmupSec,15.0,1e-12,"final warm-up");
        NS_TEST_ASSERT_MSG_EQ(c.queuePolicy,"FIFO","FIFO must remain the default policy");
        NS_TEST_ASSERT_MSG_EQ(c.enablePolicyDecisions,false,"decision trace default must be off");
        NS_TEST_ASSERT_MSG_EQ(c.crnStreamBase,-1,"legacy implicit streams remain default");
        c.numEnbs=99;c.backgroundStopSec=200;c.NormalizeLegacySemantics();
        NS_TEST_ASSERT_MSG_EQ(c.numEnbs,5,"legacy eNB clamp");
        NS_TEST_ASSERT_MSG_EQ_TOL(c.backgroundStopSec,120.0,1e-12,"legacy bg stop clamp");
    }
};
class HandoverCongestionConfigSuite : public ns3::TestSuite
{
  public:HandoverCongestionConfigSuite():TestSuite("handover-congestion-config",Type::UNIT)
  {
      AddTestCase(new BaselineDefaultsCase,Duration::QUICK);
      AddTestCase(new PacketIdentityTraceCase,Duration::QUICK);
      AddTestCase(new DetailedHandoverPairingCase,Duration::QUICK);
      AddTestCase(new RlcQueueStateCase,Duration::QUICK);
  }
};
static HandoverCongestionConfigSuite g_suite;
