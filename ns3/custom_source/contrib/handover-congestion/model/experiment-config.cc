// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#include "experiment-config.h"

#include "ns3/abort.h"

#include <algorithm>
#include <sstream>

namespace ns3::handover_congestion
{

ExperimentConfig
ExperimentConfig::Parse(int argc, char* argv[])
{
    ExperimentConfig config;
    CommandLine commandLine(__FILE__);
    config.Register(commandLine);
    commandLine.Parse(argc, argv);
    config.NormalizeLegacySemantics();
    config.Validate();
    return config;
}

void
ExperimentConfig::Register(CommandLine& c)
{
#define ADD(name, help, field) c.AddValue(name, help, field)
    ADD("scenarioTag", "Scenario tag [string, always]", scenarioTag);
    ADD("outputPrefix", "Output prefix [path, always]", outputPrefix);
    ADD("rngSeed", "Global RNG seed [uint32, always]", rngSeed);
    ADD("rngRun", "RNG run number [uint32, always]", rngRun);
    ADD("crnStreamBase", "Explicit CRN stream base; -1 preserves legacy implicit streams", crnStreamBase);
    ADD("experimentId", "Instrumentation experiment identifier [string]", experimentId);
    ADD("runId", "Instrumentation run identifier [string]", runId);
    ADD("enablePacketEvents", "Write application packet TX/RX events [bool]", enablePacketEvents);
    ADD("enableDetailedHandoverEvents", "Write paired UE handover events [bool]", enableDetailedHandoverEvents);
    ADD("enableRlcQueueEvents", "Write UE uplink RLC UM queue transitions [bool]", enableRlcQueueEvents);
    ADD("enablePacketLineage", "Attach and write application SDU lineage [bool]", enablePacketLineage);
    ADD("enableRlcLineageEvents", "Write constituent RLC SDU lineage events [bool]", enableRlcLineageEvents);
    ADD("queuePolicy", "RLC SDU selection: FIFO|STATUS_PRIORITY_NON_DROPPING", queuePolicy);
    ADD("enablePolicyDecisions", "Write RLC SDU policy decisions [bool]", enablePolicyDecisions);
    ADD("simTime", "Application horizon [s, always]", simTimeSec);
    ADD("cleanupTime", "Post-application cleanup [s, always]", cleanupTimeSec);
    ADD("appStart", "Status application start [s, always]", appStartSec);
    ADD("warmupSec", "FlowMonitor start [s, monitor]", warmupSec);
    ADD("numEnbs", "Number of eNBs [uint32, always; legacy clamps 1..5]", numEnbs);
    ADD("enbSpacing", "eNB spacing [m, always]", enbSpacingM);
    ADD("enbHeight", "eNB height [m, always]", enbHeightM);
    ADD("nUes", "Number of UAV UEs [uint32, always]", nUes);
    ADD("hoHysteresisDb", "A3 hysteresis [dB, always]", hoHysteresisDb);
    ADD("hoTtTMs", "A3 time-to-trigger [ms, always]", hoTttMs);
    ADD("channelMode", "Channel mode: logdist|buildings [enum, always]", channelMode);
    ADD("enableFading", "Enable EVA trace fading [bool, always]", enableFading);
    ADD("fadingTrace", "Fading trace path [path, fading]", fadingTrace);
    ADD("pathlossExponent", "LogDistance exponent [double, logdist]", pathlossExponent);
    ADD("refLossDb", "Reference loss [dB, logdist]", refLossDb);
    ADD("refDistanceM", "Reference distance [m, logdist]", refDistanceM);
    ADD("shadowSigmaOutdoorDb", "Outdoor shadow sigma [dB, buildings]", shadowSigmaOutdoorDb);
    ADD("mobilityMode", "Mobility: linear|gaussmarkov [enum, always]", mobilityMode);
    ADD("linearMargin", "Linear endpoint margin [m, linear]", linearMarginM);
    ADD("xMin", "Mobility x minimum [m, always]", xMinM);
    ADD("xMax", "Mobility x maximum [m, always]", xMaxM);
    ADD("yMin", "Mobility y minimum [m, always]", yMinM);
    ADD("yMax", "Mobility y maximum [m, always]", yMaxM);
    ADD("zMin", "Mobility z minimum [m, always]", zMinM);
    ADD("zMax", "Mobility z maximum [m, always]", zMaxM);
    ADD("gmTimeStep", "Gauss-Markov timestep [s, gaussmarkov]", gmTimeStepSec);
    ADD("gmAlpha", "Gauss-Markov alpha [double, gaussmarkov]", gmAlpha);
    ADD("speedMin", "Speed minimum [m/s, both mobility modes]", speedMinMps);
    ADD("speedMax", "Speed maximum [m/s, both mobility modes]", speedMaxMps);
    ADD("dirMin", "Direction minimum [rad, gaussmarkov]", dirMinRad);
    ADD("dirMax", "Direction maximum [rad, gaussmarkov]", dirMaxRad);
    ADD("pitchMin", "Pitch minimum [rad, gaussmarkov]", pitchMinRad);
    ADD("pitchMax", "Pitch maximum [rad, gaussmarkov]", pitchMaxRad);
    ADD("normalVelVar", "Normal velocity variance [gaussmarkov]", normalVelocityVariance);
    ADD("normalDirVar", "Normal direction variance [gaussmarkov]", normalDirectionVariance);
    ADD("normalPitchVar", "Normal pitch variance [gaussmarkov]", normalPitchVariance);
    ADD("statusIntervalMs", "Status interval [ms, always]", statusIntervalMs);
    ADD("statusPacketSize", "Status packet size [B, always]", statusPacketSize);
    ADD("enableBackground", "Enable background traffic [bool, always]", enableBackground);
    ADD("bgStart", "Background start [s, background]", backgroundStartSec);
    ADD("bgStop", "Background stop [s, background; clamped to simTime]", backgroundStopSec);
    ADD("bgIntervalMs", "Background interval [ms, background]", backgroundIntervalMs);
    ADD("bgPacketSize", "Background packet size [B, background]", backgroundPacketSize);
    ADD("enableBurst", "Enable burst traffic [bool, always]", enableBurst);
    ADD("burstStart", "Burst start [s, burst]", burstStartSec);
    ADD("burstDuration", "Burst duration [s, burst]", burstDurationSec);
    ADD("burstIntervalMs", "Burst interval [ms, burst]", burstIntervalMs);
    ADD("burstPacketSize", "Burst packet size [B, burst]", burstPacketSize);
    ADD("saveXml", "Write FlowMonitor XML [bool, output]", saveXml);
    ADD("saveCsv", "Write summary CSV [bool, output]", saveCsv);
    ADD("xmlHist", "Include XML histograms [bool, XML]", xmlHistograms);
    ADD("xmlProbes", "Include XML probes [bool, XML]", xmlProbes);
    ADD("saveAssocCsv", "Write association samples [bool, sampling]", saveAssociationCsv);
    ADD("saveMeasCsv", "Write UE measurements [bool, tracing]", saveMeasurementCsv);
    ADD("enableAnim", "Legacy animation switch; baseline must remain false", enableAnimation);
    ADD("assocSamplePeriodSec", "Association period [s, sampling]", associationSamplePeriodSec);
    ADD("delayBinWidthSec", "Flow delay histogram width [s, monitor]", delayBinWidthSec);
    ADD("jitterBinWidthSec", "Flow jitter histogram width [s, monitor]", jitterBinWidthSec);
    ADD("packetSizeBinWidth", "Packet-size histogram width [B, monitor]", packetSizeBinWidth);
#undef ADD
}

void
ExperimentConfig::NormalizeLegacySemantics()
{
    numEnbs = std::max(1u, std::min(5u, numEnbs));
    if (backgroundStopSec > simTimeSec)
    {
        backgroundStopSec = simTimeSec;
    }
}

void
ExperimentConfig::Validate() const
{
    NS_ABORT_MSG_IF(scenarioTag.empty() || outputPrefix.empty(), "scenarioTag/outputPrefix must be non-empty");
    NS_ABORT_MSG_IF(experimentId.empty() || runId.empty(), "experimentId/runId must be non-empty");
    NS_ABORT_MSG_IF(nUes == 0, "nUes must be positive");
    NS_ABORT_MSG_IF(simTimeSec <= 0 || cleanupTimeSec < 0, "invalid simulation times");
    NS_ABORT_MSG_IF(appStartSec < 0 || appStartSec > simTimeSec, "invalid appStart");
    NS_ABORT_MSG_IF(warmupSec < 0 || warmupSec > simTimeSec, "invalid warmupSec");
    NS_ABORT_MSG_IF(xMinM >= xMaxM || yMinM > yMaxM || zMinM > zMaxM, "invalid mobility bounds");
    NS_ABORT_MSG_IF(mobilityMode != "linear" && mobilityMode != "gaussmarkov", "invalid mobilityMode");
    NS_ABORT_MSG_IF(channelMode != "logdist" && channelMode != "buildings", "invalid channelMode");
    NS_ABORT_MSG_IF(speedMinMps > speedMaxMps || speedMaxMps <= 0, "invalid speed bounds");
    NS_ABORT_MSG_IF(statusIntervalMs == 0 || statusPacketSize == 0, "invalid status traffic");
    NS_ABORT_MSG_IF(enableBackground && (backgroundIntervalMs == 0 || backgroundStopSec < backgroundStartSec), "invalid background traffic");
    NS_ABORT_MSG_IF(enableBurst && (burstIntervalMs == 0 || burstDurationSec < 0), "invalid burst traffic");
    NS_ABORT_MSG_IF(associationSamplePeriodSec <= 0, "association sample period must be positive");
    NS_ABORT_MSG_IF(enableAnimation, "animation is intentionally unsupported in the reproduction baseline");
    NS_ABORT_MSG_IF(crnStreamBase < -1, "crnStreamBase must be -1 or non-negative");
    NS_ABORT_MSG_IF(crnStreamBase >= 0 && mobilityMode != "linear",
                    "explicit pilot CRN currently supports frozen linear mobility only");
    NS_ABORT_MSG_IF(enableRlcLineageEvents && !enablePacketLineage,
                    "RLC lineage events require application packet lineage");
    NS_ABORT_MSG_IF(queuePolicy != "FIFO" && queuePolicy != "STATUS_PRIORITY_NON_DROPPING",
                    "invalid queuePolicy");
    NS_ABORT_MSG_IF(queuePolicy != "FIFO" &&
                        (!enablePacketLineage || !enableRlcLineageEvents || !enablePolicyDecisions),
                    "STATUS priority requires packet lineage, RLC lineage, and decision tracing");
}

std::string
ExperimentConfig::FilePrefix() const
{
    std::ostringstream out;
    out << outputPrefix << '_' << scenarioTag << "_e" << numEnbs << "_u" << nUes << "_si"
        << statusIntervalMs << "_sp" << statusPacketSize << "_bg" << (enableBackground ? 1 : 0)
        << "_bu" << (enableBurst ? 1 : 0) << "_run" << rngRun;
    return out.str();
}

} // namespace ns3::handover_congestion
