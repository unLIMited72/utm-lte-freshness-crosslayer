// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#ifndef HANDOVER_CONGESTION_RESULT_WRITER_H
#define HANDOVER_CONGESTION_RESULT_WRITER_H

#include "baseline-trace-collector.h"
#include "experiment-config.h"
#include "flow-metrics.h"
#include "lte-network-builder.h"

#include <fstream>

namespace ns3::handover_congestion
{
class ResultWriter
{
  public:
    explicit ResultWriter(const ExperimentConfig& config);
    void Prepare();
    std::ostream* AssociationStream();
    std::ostream* UeCellStream();
    std::ostream* MeasurementStream();
    std::ostream* PacketEventStream();
    std::ostream* HandoverEventStream();
    std::ostream* RlcQueueEventStream();
    std::ostream* PacketLineageEventStream();
    std::ostream* RlcLineageEventStream();
    std::ostream* PolicyDecisionStream();
    void WriteCrnStreamManifest(const ScenarioNodes&, const NetworkArtifacts&) const;
    void Write(const RunMetrics&, const RunState&, const NetworkArtifacts&, FlowMonitorArtifacts&);

  private:
    void WriteEffectiveConfig() const;
    void WriteRunMetadata() const;
    void WriteSummary(const RunMetrics&) const;
    void WritePerUe(const RunMetrics&, const RunState&, const NetworkArtifacts&) const;
    const ExperimentConfig& m_config;
    std::ofstream m_association;
    std::ofstream m_ueCell;
    std::ofstream m_measurement;
    std::ofstream m_packetEvents;
    std::ofstream m_handoverEvents;
    std::ofstream m_rlcQueueEvents;
    std::ofstream m_packetLineageEvents;
    std::ofstream m_rlcLineageEvents;
    std::ofstream m_policyDecisions;
};
} // namespace ns3::handover_congestion
#endif
