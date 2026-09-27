// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#ifndef HANDOVER_CONGESTION_EXPERIMENT_CONFIG_H
#define HANDOVER_CONGESTION_EXPERIMENT_CONFIG_H

#include "ns3/command-line.h"

#include <cstdint>
#include <string>

namespace ns3::handover_congestion
{

struct ExperimentConfig
{
    std::string scenarioTag{"mobile_ho"};
    std::string outputPrefix{"results/handover-congestion/baseline"};
    uint32_t rngSeed{1};
    uint32_t rngRun{1};
    int64_t crnStreamBase{-1};
    std::string experimentId{"baseline"};
    std::string runId{"run-1"};
    bool enablePacketEvents{false};
    bool enableDetailedHandoverEvents{false};
    bool enableRlcQueueEvents{false};
    bool enablePacketLineage{false};
    bool enableRlcLineageEvents{false};
    std::string queuePolicy{"FIFO"};
    bool enablePolicyDecisions{false};
    double simTimeSec{120.0};
    double cleanupTimeSec{2.0};
    double appStartSec{1.0};
    double warmupSec{15.0};

    uint32_t numEnbs{3};
    double enbSpacingM{500.0};
    double enbHeightM{25.0};
    uint32_t nUes{35};

    double hoHysteresisDb{3.0};
    uint32_t hoTttMs{256};

    std::string schedulerType{"ns3::PfFfMacScheduler"};
    bool useIdealRrc{true};
    uint16_t dlBandwidthRb{25};
    uint16_t ulBandwidthRb{25};
    uint32_t dlEarfcn{100};
    uint32_t ulEarfcn{18100};
    double enbTxPowerDbm{30.0};
    double ueTxPowerDbm{10.0};

    std::string channelMode{"logdist"};
    bool enableFading{true};
    std::string fadingTrace{"src/lte/model/fading-traces/fading_trace_EVA_60kmph.fad"};
    std::string fadingTraceSha256{"22ea331d2e77d1e1790df96003378cace72c8cbee50b666989e06b5979077aca"};
    double pathlossExponent{2.6};
    double refLossDb{45.0};
    double refDistanceM{1.0};
    double shadowSigmaOutdoorDb{7.0};

    std::string mobilityMode{"linear"};
    double linearMarginM{50.0};
    double xMinM{-1000.0};
    double xMaxM{1000.0};
    double yMinM{-500.0};
    double yMaxM{500.0};
    double zMinM{80.0};
    double zMaxM{150.0};
    double gmTimeStepSec{1.0};
    double gmAlpha{0.85};
    double speedMinMps{12.0};
    double speedMaxMps{22.0};
    double dirMinRad{0.0};
    double dirMaxRad{6.283185307};
    double pitchMinRad{-0.08};
    double pitchMaxRad{0.08};
    double normalVelocityVariance{1.0};
    double normalDirectionVariance{0.05};
    double normalPitchVariance{0.01};

    uint32_t statusIntervalMs{200};
    uint32_t statusPacketSize{120};
    bool enableBackground{false};
    double backgroundStartSec{1.0};
    double backgroundStopSec{120.0};
    uint32_t backgroundIntervalMs{100};
    uint32_t backgroundPacketSize{300};
    bool enableBurst{false};
    double burstStartSec{20.0};
    double burstDurationSec{5.0};
    uint32_t burstIntervalMs{20};
    uint32_t burstPacketSize{400};

    bool saveXml{true};
    bool saveCsv{true};
    bool xmlHistograms{true};
    bool xmlProbes{false};
    bool saveAssociationCsv{false};
    bool saveMeasurementCsv{false};
    bool enableAnimation{false};
    double associationSamplePeriodSec{1.0};
    double delayBinWidthSec{0.001};
    double jitterBinWidthSec{0.001};
    double packetSizeBinWidth{20.0};

    static ExperimentConfig Parse(int argc, char* argv[]);
    void Register(CommandLine& commandLine);
    void NormalizeLegacySemantics();
    void Validate() const;
    std::string FilePrefix() const;
};

} // namespace ns3::handover_congestion

#endif
