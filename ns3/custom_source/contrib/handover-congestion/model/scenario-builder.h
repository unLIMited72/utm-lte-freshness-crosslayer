// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#ifndef HANDOVER_CONGESTION_SCENARIO_BUILDER_H
#define HANDOVER_CONGESTION_SCENARIO_BUILDER_H

#include "experiment-config.h"
#include "ns3/node-container.h"

#include <vector>

namespace ns3::handover_congestion
{

struct CrnUeMobilityIdentity
{
    uint32_t ueId{0};
    double initialX{0};
    double y{0};
    double z{0};
    double speed{0};
};

struct ScenarioNodes
{
    NodeContainer enbNodes;
    NodeContainer ueNodes;
    int64_t mobilityStreamBase{-1};
    int64_t mobilityStreamsAssigned{0};
    std::vector<CrnUeMobilityIdentity> mobilityIdentity;
};

class ScenarioBuilder
{
  public:
    explicit ScenarioBuilder(const ExperimentConfig& config);
    ScenarioNodes Build() const;

  private:
    const ExperimentConfig& m_config;
};

} // namespace ns3::handover_congestion
#endif
