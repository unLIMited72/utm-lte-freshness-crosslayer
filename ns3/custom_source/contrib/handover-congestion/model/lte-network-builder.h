// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#ifndef HANDOVER_CONGESTION_LTE_NETWORK_BUILDER_H
#define HANDOVER_CONGESTION_LTE_NETWORK_BUILDER_H

#include "experiment-config.h"
#include "scenario-builder.h"
#include "ns3/ipv4-address.h"
#include "ns3/lte-helper.h"
#include "ns3/net-device-container.h"
#include "ns3/node.h"
#include "ns3/point-to-point-epc-helper.h"

#include <map>

namespace ns3::handover_congestion
{

struct NetworkArtifacts
{
    Ptr<LteHelper> lteHelper;
    Ptr<PointToPointEpcHelper> epcHelper;
    Ptr<Node> pgw;
    Ptr<Node> remoteHost;
    Ipv4Address remoteHostAddress;
    NetDeviceContainer enbDevices;
    NetDeviceContainer ueDevices;
    std::map<uint16_t, uint32_t> cellIdToEnbIndex;
    int64_t lteStreamBase{-1};
    int64_t lteStreamsAssigned{0};
    int64_t remoteInternetStreamBase{-1};
    int64_t remoteInternetStreamsAssigned{0};
    int64_t ueInternetStreamBase{-1};
    int64_t ueInternetStreamsAssigned{0};
};

class LteNetworkBuilder
{
  public:
    explicit LteNetworkBuilder(const ExperimentConfig& config);
    void ConfigurePreDeviceBaseline();
    NetworkArtifacts InstallDevicesAndNetwork(const ScenarioNodes& nodes);
    void AddX2AndAttach(const ScenarioNodes& nodes, NetworkArtifacts& artifacts) const;

  private:
    const ExperimentConfig& m_config;
    Ptr<LteHelper> m_lteHelper;
    Ptr<PointToPointEpcHelper> m_epcHelper;
    bool m_preconfigured{false};
};

} // namespace ns3::handover_congestion
#endif
