// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#include "flow-metrics.h"

#include "ns3/double.h"
#include "ns3/ipv4-flow-classifier.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace ns3::handover_congestion
{
FlowMetrics::FlowMetrics(const ExperimentConfig& config) : m_config(config) {}
FlowMonitorArtifacts FlowMetrics::InstallMonitor() const
{
    FlowMonitorArtifacts a;
    a.helper = std::make_unique<FlowMonitorHelper>();
    a.helper->SetMonitorAttribute("DelayBinWidth", DoubleValue(m_config.delayBinWidthSec));
    a.helper->SetMonitorAttribute("JitterBinWidth", DoubleValue(m_config.jitterBinWidthSec));
    a.helper->SetMonitorAttribute("PacketSizeBinWidth", DoubleValue(m_config.packetSizeBinWidth));
    a.monitor = a.helper->InstallAll();
    if (m_config.warmupSec > 0) a.monitor->Start(Seconds(m_config.warmupSec));
    return a;
}
void FlowMetrics::Accumulate(FlowAggregate& a, const FlowMonitor::FlowStats& s)
{
    a.txPackets += s.txPackets; a.rxPackets += s.rxPackets; a.lostPackets += s.lostPackets;
    a.delaySumSec += s.delaySum.GetSeconds(); a.jitterSumSec += s.jitterSum.GetSeconds();
    if (s.rxPackets > 0) { a.minDelaySec = std::min(a.minDelaySec, s.minDelay.GetSeconds()); a.maxDelaySec = std::max(a.maxDelaySec, s.maxDelay.GetSeconds()); }
    const uint32_t bins = s.delayHistogram.GetNBins();
    if (bins == 0) return;
    if (a.histogramBinWidthSec <= 0) a.histogramBinWidthSec = s.delayHistogram.GetBinWidth(0);
    if (a.delayHistogramCounts.size() < bins) a.delayHistogramCounts.resize(bins, 0);
    for (uint32_t i = 0; i < bins; ++i) a.delayHistogramCounts[i] += s.delayHistogram.GetBinCount(i);
}
RunMetrics FlowMetrics::Collect(FlowMonitorArtifacts& flow, const NetworkArtifacts& network, const TrafficArtifacts& traffic, const RunState& state) const
{
    flow.monitor->CheckForLostPackets();
    RunMetrics out;
    out.statusPerUe.resize(m_config.nUes); out.backgroundPerUe.resize(m_config.nUes); out.burstPerUe.resize(m_config.nUes);
    auto classifier = DynamicCast<Ipv4FlowClassifier>(flow.helper->GetClassifier());
    for (const auto& [id, stats] : flow.monitor->GetFlowStats())
    {
        auto t = classifier->FindFlow(id);
        if (t.protocol != 17 || t.destinationAddress != network.remoteHostAddress) continue;
        if (t.destinationPort >= traffic.statusPortBase && t.destinationPort < traffic.statusPortBase + m_config.nUes)
        { auto i = t.destinationPort - traffic.statusPortBase; Accumulate(out.status, stats); Accumulate(out.statusPerUe[i], stats); }
        else if (m_config.enableBackground && t.destinationPort >= traffic.backgroundPortBase && t.destinationPort < traffic.backgroundPortBase + m_config.nUes)
        { auto i = t.destinationPort - traffic.backgroundPortBase; Accumulate(out.background, stats); Accumulate(out.backgroundPerUe[i], stats); }
        else if (m_config.enableBurst && t.destinationPort >= traffic.burstPortBase && t.destinationPort < traffic.burstPortBase + m_config.nUes)
        { auto i = t.destinationPort - traffic.burstPortBase; Accumulate(out.burst, stats); Accumulate(out.burstPerUe[i], stats); }
    }
    double hoSum=0, durationMeanSum=0, assocSum=0, rsrpSum=0, rsrqSum=0, sinrSum=0;
    uint32_t rsrpN=0, rsrqN=0, sinrN=0;
    for (const auto& [imsi, s] : state)
    {
        out.radio.totalHoCount += s.hoCount; hoSum += s.hoCount; assocSum += s.associationChangeCount;
        durationMeanSum += s.hoCount ? s.hoInterruptionSum.GetSeconds()*1000.0/s.hoCount : 0;
        out.radio.maxHoInterruptionMs = std::max(out.radio.maxHoInterruptionMs, s.hoInterruptionMax.GetSeconds()*1000.0);
        if (s.rsrpCount) { rsrpSum += s.rsrpSum/s.rsrpCount; ++rsrpN; }
        if (s.rsrqCount) { rsrqSum += s.rsrqSum/s.rsrqCount; ++rsrqN; }
        if (s.sinrCount) { sinrSum += s.sinrSum/s.sinrCount; ++sinrN; }
    }
    const double n = state.size();
    if (n) { out.radio.avgHoCountPerUe=hoSum/n; out.radio.avgHoInterruptionMs=durationMeanSum/n; out.radio.avgAssociationChangePerUe=assocSum/n; }
    out.radio.avgServingRsrpDbm = rsrpN ? rsrpSum/rsrpN : 0;
    out.radio.avgServingRsrqDb = rsrqN ? rsrqSum/rsrqN : 0;
    out.radio.avgServingSinrDb = sinrN ? sinrSum/sinrN : 0;
    return out;
}
double FlowMetrics::AvgDelayMs(const FlowAggregate& s){return s.rxPackets?s.delaySumSec/s.rxPackets*1000:0;}
double FlowMetrics::AvgJitterMs(const FlowAggregate& s){return s.rxPackets>1?s.jitterSumSec/(s.rxPackets-1)*1000:0;}
double FlowMetrics::Pdr(const FlowAggregate& s){return s.txPackets?double(s.rxPackets)/s.txPackets:0;}
double FlowMetrics::Plr(const FlowAggregate& s){return s.txPackets?double(s.txPackets-s.rxPackets)/s.txPackets:0;}
double FlowMetrics::MinDelayMs(const FlowAggregate& s){return std::isfinite(s.minDelaySec)?s.minDelaySec*1000:0;}
double FlowMetrics::MaxDelayMs(const FlowAggregate& s){return s.maxDelaySec*1000;}
double FlowMetrics::P95DelayMs(const FlowAggregate& s)
{
    if (!s.rxPackets || s.delayHistogramCounts.empty() || s.histogramBinWidthSec<=0) return 0;
    const uint64_t total=std::accumulate(s.delayHistogramCounts.begin(),s.delayHistogramCounts.end(),uint64_t{0});
    if (!total)
    {
        return 0;
    }
    const double target = .95 * total;
    uint64_t cumulative = 0;
    for(size_t i=0;i<s.delayHistogramCounts.size();++i){cumulative+=s.delayHistogramCounts[i];if(double(cumulative)>=target)return (i+1)*s.histogramBinWidthSec*1000;}
    return s.delayHistogramCounts.size()*s.histogramBinWidthSec*1000;
}
} // namespace ns3::handover_congestion
