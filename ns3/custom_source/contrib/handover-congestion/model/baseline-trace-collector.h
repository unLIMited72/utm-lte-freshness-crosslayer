// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#ifndef HANDOVER_CONGESTION_BASELINE_TRACE_COLLECTOR_H
#define HANDOVER_CONGESTION_BASELINE_TRACE_COLLECTOR_H

#include "experiment-config.h"
#include "lte-network-builder.h"
#include "ns3/nstime.h"

#include <cstdint>
#include <limits>
#include <map>
#include <ostream>

namespace ns3::handover_congestion
{

struct UeRuntime
{
    uint64_t imsi{0};
    uint32_t nodeId{0};
    uint16_t currentCell{0};
    uint16_t lastPolledCell{0};
    uint16_t targetCell{0};
    uint16_t rnti{0};
    bool hoActive{false};
    Time hoStart{Seconds(0)};
    uint32_t hoCount{0};
    uint32_t associationChangeCount{0};
    Time hoInterruptionSum{Seconds(0)};
    Time hoInterruptionMax{Seconds(0)};
    double lastServingRsrpDbm{std::numeric_limits<double>::quiet_NaN()};
    double lastServingRsrqDb{std::numeric_limits<double>::quiet_NaN()};
    double lastServingSinrDb{std::numeric_limits<double>::quiet_NaN()};
    double rsrpSum{0};
    double rsrqSum{0};
    double sinrSum{0};
    uint64_t rsrpCount{0};
    uint64_t rsrqCount{0};
    uint64_t sinrCount{0};
};

using RunState = std::map<uint64_t, UeRuntime>;

class BaselineTraceCollector
{
  public:
    explicit BaselineTraceCollector(const ExperimentConfig& config);
    void Connect(const NetworkArtifacts& network);
    void SetSampleStreams(std::ostream* association, std::ostream* ueCell, std::ostream* measurement);
    void ScheduleAssociationSampling();
    const RunState& GetRunState() const;

  private:
    static void ConnectionAdapter(BaselineTraceCollector*, uint64_t, uint32_t, uint64_t, uint16_t, uint16_t);
    static void HandoverStartAdapter(BaselineTraceCollector*, uint64_t, uint32_t, uint64_t, uint16_t, uint16_t, uint16_t);
    static void HandoverEndAdapter(BaselineTraceCollector*, uint64_t, uint32_t, uint64_t, uint16_t, uint16_t);
    static void MeasurementAdapter(BaselineTraceCollector*, uint64_t, uint32_t, uint16_t, uint16_t, double, double, bool, uint8_t);
    static void RsrpSinrAdapter(BaselineTraceCollector*, uint64_t, uint32_t, uint16_t, uint16_t, double, double, uint8_t);
    void SampleAssociations();

    const ExperimentConfig& m_config;
    RunState m_state;
    NetDeviceContainer m_ueDevices;
    std::map<uint16_t, uint32_t> m_cellMap;
    std::ostream* m_association{nullptr};
    std::ostream* m_ueCell{nullptr};
    std::ostream* m_measurement{nullptr};
};

} // namespace ns3::handover_congestion
#endif
