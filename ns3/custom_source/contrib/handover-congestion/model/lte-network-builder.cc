// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#include "lte-network-builder.h"

#include "ns3/abort.h"
#include "ns3/boolean.h"
#include "ns3/buildings-helper.h"
#include "ns3/config.h"
#include "ns3/data-rate.h"
#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/lte-enb-net-device.h"
#include "ns3/lte-enb-rrc.h"
#include "ns3/lte-rlc-um.h"
#include "ns3/lte-ue-net-device.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"

namespace ns3::handover_congestion
{

LteNetworkBuilder::LteNetworkBuilder(const ExperimentConfig& config)
    : m_config(config)
{
}

void
LteNetworkBuilder::ConfigurePreDeviceBaseline()
{
    m_lteHelper = CreateObject<LteHelper>();
    m_epcHelper = CreateObject<PointToPointEpcHelper>();
    m_lteHelper->SetEpcHelper(m_epcHelper);

    m_lteHelper->SetSchedulerType(m_config.schedulerType);
    m_lteHelper->SetAttribute("UseIdealRrc", BooleanValue(m_config.useIdealRrc));
    Config::SetDefault("ns3::LteEnbRrc::EpsBearerToRlcMapping",
                       EnumValue(LteEnbRrc::RLC_UM_ALWAYS));
    Config::SetDefault("ns3::LteEnbNetDevice::DlBandwidth", UintegerValue(m_config.dlBandwidthRb));
    Config::SetDefault("ns3::LteEnbNetDevice::UlBandwidth", UintegerValue(m_config.ulBandwidthRb));
    Config::SetDefault("ns3::LteEnbNetDevice::DlEarfcn", UintegerValue(m_config.dlEarfcn));
    Config::SetDefault("ns3::LteEnbNetDevice::UlEarfcn", UintegerValue(m_config.ulEarfcn));
    Config::SetDefault("ns3::LteEnbPhy::TxPower", DoubleValue(m_config.enbTxPowerDbm));
    Config::SetDefault("ns3::LteUePhy::TxPower", DoubleValue(m_config.ueTxPowerDbm));
    Config::SetDefault("ns3::LteRlcUm::TxSduSelectionPolicy",
                       EnumValue(m_config.queuePolicy == "FIFO"
                                     ? LteRlcUm::FIFO
                                     : LteRlcUm::STATUS_PRIORITY_NON_DROPPING));

    m_lteHelper->SetHandoverAlgorithmType("ns3::A3RsrpHandoverAlgorithm");
    m_lteHelper->SetHandoverAlgorithmAttribute("Hysteresis", DoubleValue(m_config.hoHysteresisDb));
    m_lteHelper->SetHandoverAlgorithmAttribute("TimeToTrigger", TimeValue(MilliSeconds(m_config.hoTttMs)));

    if (m_config.channelMode == "buildings")
    {
        m_lteHelper->SetAttribute("PathlossModel", StringValue("ns3::HybridBuildingsPropagationLossModel"));
        m_lteHelper->SetPathlossModelAttribute("ShadowSigmaOutdoor", DoubleValue(m_config.shadowSigmaOutdoorDb));
        m_lteHelper->SetPathlossModelAttribute("ShadowSigmaIndoor", DoubleValue(8.0));
        m_lteHelper->SetPathlossModelAttribute("ShadowSigmaExtWalls", DoubleValue(0.0));
    }
    else
    {
        m_lteHelper->SetAttribute("PathlossModel", StringValue("ns3::LogDistancePropagationLossModel"));
        m_lteHelper->SetPathlossModelAttribute("Exponent", DoubleValue(m_config.pathlossExponent));
        m_lteHelper->SetPathlossModelAttribute("ReferenceDistance", DoubleValue(m_config.refDistanceM));
        m_lteHelper->SetPathlossModelAttribute("ReferenceLoss", DoubleValue(m_config.refLossDb));
    }
    if (m_config.enableFading)
    {
        m_lteHelper->SetAttribute("FadingModel", StringValue("ns3::TraceFadingLossModel"));
        m_lteHelper->SetFadingModelAttribute("TraceFilename", StringValue(m_config.fadingTrace));
        m_lteHelper->SetFadingModelAttribute("TraceLength", TimeValue(Seconds(10.0)));
        m_lteHelper->SetFadingModelAttribute("SamplesNum", UintegerValue(10000));
        m_lteHelper->SetFadingModelAttribute("WindowSize", TimeValue(Seconds(0.5)));
        m_lteHelper->SetFadingModelAttribute("RbNum", UintegerValue(100));
    }
    m_preconfigured = true;
}

NetworkArtifacts
LteNetworkBuilder::InstallDevicesAndNetwork(const ScenarioNodes& nodes)
{
    NS_ABORT_MSG_IF(!m_preconfigured, "ConfigurePreDeviceBaseline must run before scenario/device installation");
    NetworkArtifacts a;
    a.lteHelper = m_lteHelper;
    a.epcHelper = m_epcHelper;
    a.pgw = m_epcHelper->GetPgwNode();

    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    a.remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);
    if (m_config.crnStreamBase >= 0)
    {
        a.remoteInternetStreamBase = m_config.crnStreamBase + 500000;
        a.remoteInternetStreamsAssigned =
            internet.AssignStreams(remoteHostContainer, a.remoteInternetStreamBase);
    }

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Gb/s")));
    pointToPoint.SetDeviceAttribute("Mtu", UintegerValue(1500));
    pointToPoint.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
    NetDeviceContainer coreDevices = pointToPoint.Install(a.pgw, a.remoteHost);
    Ipv4AddressHelper address;
    address.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer interfaces = address.Assign(coreDevices);
    a.remoteHostAddress = interfaces.GetAddress(1);

    Ipv4StaticRoutingHelper routing;
    routing.GetStaticRouting(a.remoteHost->GetObject<Ipv4>())
        ->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    a.enbDevices = m_lteHelper->InstallEnbDevice(nodes.enbNodes);
    a.ueDevices = m_lteHelper->InstallUeDevice(nodes.ueNodes);
    internet.Install(nodes.ueNodes);
    if (m_config.crnStreamBase >= 0)
    {
        a.ueInternetStreamBase = m_config.crnStreamBase + 600000;
        a.ueInternetStreamsAssigned =
            internet.AssignStreams(nodes.ueNodes, a.ueInternetStreamBase);
        NetDeviceContainer allLteDevices;
        allLteDevices.Add(a.enbDevices);
        allLteDevices.Add(a.ueDevices);
        a.lteStreamBase = m_config.crnStreamBase + 1000;
        a.lteStreamsAssigned = m_lteHelper->AssignStreams(allLteDevices, a.lteStreamBase);
        NS_ABORT_MSG_IF(a.lteStreamBase + a.lteStreamsAssigned > a.remoteInternetStreamBase,
                        "explicit LTE CRN stream range overlaps remote Internet range");
        NS_ABORT_MSG_IF(a.remoteInternetStreamBase + a.remoteInternetStreamsAssigned >
                            a.ueInternetStreamBase,
                        "explicit remote and UE Internet CRN stream ranges overlap");
    }
    for (uint32_t u = 0; u < nodes.ueNodes.GetN(); ++u)
    {
        m_epcHelper->AssignUeIpv4Address(NetDeviceContainer(a.ueDevices.Get(u)));
        routing.GetStaticRouting(nodes.ueNodes.Get(u)->GetObject<Ipv4>())
            ->SetDefaultRoute(m_epcHelper->GetUeDefaultGatewayAddress(), 1);
    }
    for (uint32_t i = 0; i < a.enbDevices.GetN(); ++i)
    {
        auto enb = a.enbDevices.Get(i)->GetObject<LteEnbNetDevice>();
        a.cellIdToEnbIndex[enb->GetCellId()] = i;
    }
    return a;
}

void
LteNetworkBuilder::AddX2AndAttach(const ScenarioNodes& nodes, NetworkArtifacts& a) const
{
    if (m_config.numEnbs > 1)
    {
        m_lteHelper->AddX2Interface(nodes.enbNodes);
    }
    m_lteHelper->AttachToClosestEnb(a.ueDevices, a.enbDevices);
}

} // namespace ns3::handover_congestion
