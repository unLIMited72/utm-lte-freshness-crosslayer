// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#include "scenario-builder.h"

#include "ns3/buildings-helper.h"
#include "ns3/double.h"
#include "ns3/gauss-markov-mobility-model.h"
#include "ns3/mobility-helper.h"
#include "ns3/position-allocator.h"
#include "ns3/random-variable-stream.h"
#include "ns3/string.h"
#include "ns3/waypoint-mobility-model.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace ns3::handover_congestion
{
namespace
{
std::string Uniform(double min, double max)
{
    std::ostringstream out;
    out << "ns3::UniformRandomVariable[Min=" << min << "|Max=" << max << ']';
    return out.str();
}
std::string Normal(double mean, double variance, double bound)
{
    std::ostringstream out;
    out << "ns3::NormalRandomVariable[Mean=" << mean << "|Variance=" << variance << "|Bound=" << bound << ']';
    return out.str();
}
} // namespace

ScenarioBuilder::ScenarioBuilder(const ExperimentConfig& config)
    : m_config(config)
{
}

ScenarioNodes
ScenarioBuilder::Build() const
{
    ScenarioNodes nodes;
    nodes.enbNodes.Create(m_config.numEnbs);
    nodes.ueNodes.Create(m_config.nUes);

    Ptr<ListPositionAllocator> enbPositions = CreateObject<ListPositionAllocator>();
    const double center = 0.5 * static_cast<double>(m_config.numEnbs - 1) * m_config.enbSpacingM;
    for (uint32_t i = 0; i < m_config.numEnbs; ++i)
    {
        enbPositions->Add(Vector(i * m_config.enbSpacingM - center, 0.0, m_config.enbHeightM));
    }
    MobilityHelper enbMobility;
    enbMobility.SetPositionAllocator(enbPositions);
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.Install(nodes.enbNodes);

    MobilityHelper ueMobility;
    if (m_config.mobilityMode == "linear")
    {
        ueMobility.SetMobilityModel("ns3::WaypointMobilityModel");
        ueMobility.Install(nodes.ueNodes);

        // Construction and per-UE draw order intentionally match legacy: y, z, speed.
        Ptr<UniformRandomVariable> yRv = CreateObject<UniformRandomVariable>();
        yRv->SetAttribute("Min", DoubleValue(m_config.yMinM));
        yRv->SetAttribute("Max", DoubleValue(m_config.yMaxM));
        Ptr<UniformRandomVariable> zRv = CreateObject<UniformRandomVariable>();
        zRv->SetAttribute("Min", DoubleValue(m_config.zMinM));
        zRv->SetAttribute("Max", DoubleValue(m_config.zMaxM));
        Ptr<UniformRandomVariable> speedRv = CreateObject<UniformRandomVariable>();
        speedRv->SetAttribute("Min", DoubleValue(m_config.speedMinMps));
        speedRv->SetAttribute("Max", DoubleValue(m_config.speedMaxMps));
        if (m_config.crnStreamBase >= 0)
        {
            yRv->SetStream(m_config.crnStreamBase);
            zRv->SetStream(m_config.crnStreamBase + 1);
            speedRv->SetStream(m_config.crnStreamBase + 2);
            nodes.mobilityStreamBase = m_config.crnStreamBase;
            nodes.mobilityStreamsAssigned = 3;
        }

        const double left = m_config.xMinM + m_config.linearMarginM;
        const double right = m_config.xMaxM - m_config.linearMarginM;
        const double horizon = m_config.simTimeSec + m_config.cleanupTimeSec;
        for (uint32_t u = 0; u < m_config.nUes; ++u)
        {
            Ptr<WaypointMobilityModel> waypoint = nodes.ueNodes.Get(u)->GetObject<WaypointMobilityModel>();
            const double y = yRv->GetValue();
            const double z = zRv->GetValue();
            const double speed = std::max(1.0, speedRv->GetValue());
            double current = (u % 2 == 0) ? left : right;
            double target = (u % 2 == 0) ? right : left;
            nodes.mobilityIdentity.push_back({u, current, y, z, speed});
            double time = 0.0;
            waypoint->AddWaypoint(Waypoint(Seconds(time), Vector(current, y, z)));
            while (time < horizon)
            {
                time += std::abs(target - current) / speed;
                waypoint->AddWaypoint(Waypoint(Seconds(time), Vector(target, y, z)));
                std::swap(current, target);
            }
        }
    }
    else
    {
        Ptr<RandomBoxPositionAllocator> positions = CreateObject<RandomBoxPositionAllocator>();
        positions->SetAttribute("X", StringValue(Uniform(m_config.xMinM, m_config.xMaxM)));
        positions->SetAttribute("Y", StringValue(Uniform(m_config.yMinM, m_config.yMaxM)));
        positions->SetAttribute("Z", StringValue(Uniform(m_config.zMinM, m_config.zMaxM)));
        ueMobility.SetPositionAllocator(positions);
        ueMobility.SetMobilityModel("ns3::GaussMarkovMobilityModel",
                                    "Bounds", BoxValue(Box(m_config.xMinM, m_config.xMaxM, m_config.yMinM, m_config.yMaxM, m_config.zMinM, m_config.zMaxM)),
                                    "TimeStep", TimeValue(Seconds(m_config.gmTimeStepSec)),
                                    "Alpha", DoubleValue(m_config.gmAlpha),
                                    "MeanVelocity", StringValue(Uniform(m_config.speedMinMps, m_config.speedMaxMps)),
                                    "MeanDirection", StringValue(Uniform(m_config.dirMinRad, m_config.dirMaxRad)),
                                    "MeanPitch", StringValue(Uniform(m_config.pitchMinRad, m_config.pitchMaxRad)),
                                    "NormalVelocity", StringValue(Normal(0.0, m_config.normalVelocityVariance, 10.0)),
                                    "NormalDirection", StringValue(Normal(0.0, m_config.normalDirectionVariance, 2.0)),
                                    "NormalPitch", StringValue(Normal(0.0, m_config.normalPitchVariance, 0.5)));
        ueMobility.Install(nodes.ueNodes);
    }
    if (m_config.channelMode == "buildings")
    {
        BuildingsHelper::Install(nodes.enbNodes);
        BuildingsHelper::Install(nodes.ueNodes);
    }
    return nodes;
}

} // namespace ns3::handover_congestion
