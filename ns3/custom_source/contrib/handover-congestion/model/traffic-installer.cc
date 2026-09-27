// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#include "traffic-installer.h"

#include "ns3/inet-socket-address.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/lte-ue-net-device.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/uinteger.h"

#include <algorithm>

namespace ns3::handover_congestion
{
TrafficInstaller::TrafficInstaller(const ExperimentConfig& config)
    : m_config(config)
{
}

TrafficArtifacts
TrafficInstaller::Install(const ScenarioNodes& nodes, const NetworkArtifacts& network) const
{
    TrafficArtifacts a;
    for (uint32_t u = 0; u < m_config.nUes; ++u)
    {
        auto installClass = [&](uint16_t port,
                                TrafficClass trafficClass,
                                uint32_t intervalMs,
                                uint32_t packetSize,
                                uint32_t maxPackets,
                                ApplicationContainer& clients) {
            PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
            ApplicationContainer sinkApps = sink.Install(network.remoteHost);
            a.sinkApps.Add(sinkApps);
            UdpClientHelper client(network.remoteHostAddress, port);
            client.SetAttribute("Interval", TimeValue(MilliSeconds(intervalMs)));
            client.SetAttribute("PacketSize", UintegerValue(packetSize));
            client.SetAttribute("MaxPackets", UintegerValue(maxPackets));
            ApplicationContainer clientApps = client.Install(nodes.ueNodes.Get(u));
            clients.Add(clientApps);
            ApplicationBinding binding;
            binding.ueId = u;
            binding.imsi = network.ueDevices.Get(u)->GetObject<LteUeNetDevice>()->GetImsi();
            binding.trafficClass = trafficClass;
            binding.port = port;
            binding.client = DynamicCast<UdpClient>(clientApps.Get(0));
            binding.sink = DynamicCast<PacketSink>(sinkApps.Get(0));
            a.bindings.push_back(binding);
        };

        const uint32_t statusMax = static_cast<uint32_t>(((m_config.simTimeSec - m_config.appStartSec) * 1000.0) / m_config.statusIntervalMs) + 100;
        installClass(a.statusPortBase + u, TrafficClass::STATUS, m_config.statusIntervalMs, m_config.statusPacketSize, statusMax, a.statusApps);
        if (m_config.enableBackground)
        {
            const uint32_t max = static_cast<uint32_t>(((m_config.backgroundStopSec - m_config.backgroundStartSec) * 1000.0) / m_config.backgroundIntervalMs) + 100;
            installClass(a.backgroundPortBase + u, TrafficClass::BACKGROUND, m_config.backgroundIntervalMs, m_config.backgroundPacketSize, max, a.backgroundApps);
        }
        if (m_config.enableBurst)
        {
            const uint32_t max = static_cast<uint32_t>((m_config.burstDurationSec * 1000.0) / m_config.burstIntervalMs) + 100;
            installClass(a.burstPortBase + u, TrafficClass::BURST, m_config.burstIntervalMs, m_config.burstPacketSize, max, a.burstApps);
        }
    }
    a.sinkApps.Start(Seconds(0.1));
    a.sinkApps.Stop(Seconds(m_config.simTimeSec));
    a.statusApps.Start(Seconds(m_config.appStartSec));
    a.statusApps.Stop(Seconds(m_config.simTimeSec));
    if (m_config.enableBackground)
    {
        a.backgroundApps.Start(Seconds(m_config.backgroundStartSec));
        a.backgroundApps.Stop(Seconds(m_config.backgroundStopSec));
    }
    if (m_config.enableBurst)
    {
        a.burstApps.Start(Seconds(m_config.burstStartSec));
        a.burstApps.Stop(Seconds(std::min(m_config.simTimeSec, m_config.burstStartSec + m_config.burstDurationSec)));
    }
    return a;
}

} // namespace ns3::handover_congestion
