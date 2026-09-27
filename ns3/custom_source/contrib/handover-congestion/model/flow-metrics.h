// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#ifndef HANDOVER_CONGESTION_FLOW_METRICS_H
#define HANDOVER_CONGESTION_FLOW_METRICS_H

#include "baseline-trace-collector.h"
#include "experiment-config.h"
#include "lte-network-builder.h"
#include "traffic-installer.h"
#include "ns3/flow-monitor-helper.h"

#include <limits>
#include <memory>
#include <vector>

namespace ns3::handover_congestion
{
struct FlowAggregate
{
    uint64_t txPackets{0}, rxPackets{0}, lostPackets{0};
    double delaySumSec{0}, jitterSumSec{0};
    double minDelaySec{std::numeric_limits<double>::infinity()}, maxDelaySec{0};
    double histogramBinWidthSec{0};
    std::vector<uint64_t> delayHistogramCounts;
};
struct RadioHandoverAggregate
{
    uint32_t totalHoCount{0};
    double avgHoCountPerUe{0}, avgHoInterruptionMs{0}, maxHoInterruptionMs{0};
    double avgAssociationChangePerUe{0}, avgServingRsrpDbm{0}, avgServingRsrqDb{0}, avgServingSinrDb{0};
};
struct RunMetrics
{
    FlowAggregate status, background, burst;
    std::vector<FlowAggregate> statusPerUe, backgroundPerUe, burstPerUe;
    RadioHandoverAggregate radio;
};
struct FlowMonitorArtifacts
{
    std::unique_ptr<FlowMonitorHelper> helper;
    Ptr<FlowMonitor> monitor;
};

class FlowMetrics
{
  public:
    explicit FlowMetrics(const ExperimentConfig& config);
    FlowMonitorArtifacts InstallMonitor() const;
    RunMetrics Collect(FlowMonitorArtifacts& flow, const NetworkArtifacts&, const TrafficArtifacts&, const RunState&) const;
    static double AvgDelayMs(const FlowAggregate&);
    static double AvgJitterMs(const FlowAggregate&);
    static double Pdr(const FlowAggregate&);
    static double Plr(const FlowAggregate&);
    static double P95DelayMs(const FlowAggregate&);
    static double MinDelayMs(const FlowAggregate&);
    static double MaxDelayMs(const FlowAggregate&);

  private:
    static void Accumulate(FlowAggregate&, const FlowMonitor::FlowStats&);
    const ExperimentConfig& m_config;
};
} // namespace ns3::handover_congestion
#endif
