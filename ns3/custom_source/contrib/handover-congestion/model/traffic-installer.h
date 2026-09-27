// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#ifndef HANDOVER_CONGESTION_TRAFFIC_INSTALLER_H
#define HANDOVER_CONGESTION_TRAFFIC_INSTALLER_H

#include "experiment-config.h"
#include "lte-network-builder.h"
#include "scenario-builder.h"
#include "ns3/application-container.h"
#include "ns3/packet-sink.h"
#include "ns3/udp-client.h"

#include <vector>

namespace ns3::handover_congestion
{

enum class TrafficClass
{
    STATUS,
    BACKGROUND,
    BURST
};

struct ApplicationBinding
{
    uint32_t ueId{0};
    uint64_t imsi{0};
    TrafficClass trafficClass{TrafficClass::STATUS};
    uint16_t port{0};
    Ptr<UdpClient> client;
    Ptr<PacketSink> sink;
};

struct TrafficArtifacts
{
    uint16_t statusPortBase{4000};
    uint16_t backgroundPortBase{5000};
    uint16_t burstPortBase{6000};
    ApplicationContainer sinkApps;
    ApplicationContainer statusApps;
    ApplicationContainer backgroundApps;
    ApplicationContainer burstApps;
    std::vector<ApplicationBinding> bindings;
};

class TrafficInstaller
{
  public:
    explicit TrafficInstaller(const ExperimentConfig& config);
    TrafficArtifacts Install(const ScenarioNodes& nodes, const NetworkArtifacts& network) const;

  private:
    const ExperimentConfig& m_config;
};

} // namespace ns3::handover_congestion
#endif
