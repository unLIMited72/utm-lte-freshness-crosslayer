// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#include "baseline-trace-collector.h"

#include "ns3/lte-ue-net-device.h"
#include "ns3/lte-ue-phy.h"
#include "ns3/lte-ue-rrc.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <cmath>

namespace ns3::handover_congestion
{
BaselineTraceCollector::BaselineTraceCollector(const ExperimentConfig& config)
    : m_config(config)
{
}

void
BaselineTraceCollector::SetSampleStreams(std::ostream* association, std::ostream* ueCell, std::ostream* measurement)
{
    m_association = association;
    m_ueCell = ueCell;
    m_measurement = measurement;
}

void
BaselineTraceCollector::Connect(const NetworkArtifacts& network)
{
    m_ueDevices = network.ueDevices;
    m_cellMap = network.cellIdToEnbIndex;
    for (uint32_t u = 0; u < m_ueDevices.GetN(); ++u)
    {
        auto device = m_ueDevices.Get(u)->GetObject<LteUeNetDevice>();
        const uint64_t imsi = device->GetImsi();
        const uint32_t nodeId = device->GetNode()->GetId();
        m_state[imsi].imsi = imsi;
        m_state[imsi].nodeId = nodeId;
        auto rrc = device->GetRrc();
        auto phy = device->GetPhy();
        rrc->TraceConnectWithoutContext("ConnectionEstablished", MakeBoundCallback(&ConnectionAdapter, this, imsi, nodeId));
        rrc->TraceConnectWithoutContext("HandoverStart", MakeBoundCallback(&HandoverStartAdapter, this, imsi, nodeId));
        rrc->TraceConnectWithoutContext("HandoverEndOk", MakeBoundCallback(&HandoverEndAdapter, this, imsi, nodeId));
        phy->TraceConnectWithoutContext("ReportUeMeasurements", MakeBoundCallback(&MeasurementAdapter, this, imsi, nodeId));
        phy->TraceConnectWithoutContext("ReportCurrentCellRsrpSinr", MakeBoundCallback(&RsrpSinrAdapter, this, imsi, nodeId));
    }
}

void BaselineTraceCollector::ConnectionAdapter(BaselineTraceCollector* self, uint64_t boundImsi, uint32_t nodeId, uint64_t, uint16_t cell, uint16_t rnti)
{
    auto& s = self->m_state[boundImsi];
    s.imsi = boundImsi; s.nodeId = nodeId; s.currentCell = cell; s.lastPolledCell = cell; s.rnti = rnti;
}
void BaselineTraceCollector::HandoverStartAdapter(BaselineTraceCollector* self, uint64_t boundImsi, uint32_t nodeId, uint64_t, uint16_t cell, uint16_t rnti, uint16_t target)
{
    auto& s = self->m_state[boundImsi];
    s.imsi = boundImsi; s.nodeId = nodeId; s.rnti = rnti; s.currentCell = cell; s.targetCell = target;
    s.hoActive = true; s.hoStart = Simulator::Now(); ++s.hoCount;
}
void BaselineTraceCollector::HandoverEndAdapter(BaselineTraceCollector* self, uint64_t boundImsi, uint32_t nodeId, uint64_t, uint16_t cell, uint16_t rnti)
{
    auto& s = self->m_state[boundImsi];
    s.imsi = boundImsi; s.nodeId = nodeId; s.rnti = rnti; s.currentCell = cell;
    if (s.hoActive)
    {
        const Time duration = Simulator::Now() - s.hoStart;
        s.hoInterruptionSum += duration;
        s.hoInterruptionMax = std::max(s.hoInterruptionMax, duration);
        s.hoActive = false;
    }
}
void BaselineTraceCollector::MeasurementAdapter(BaselineTraceCollector* self, uint64_t imsi, uint32_t nodeId, uint16_t, uint16_t cell, double rsrp, double rsrq, bool serving, uint8_t)
{
    auto& s = self->m_state[imsi]; s.imsi = imsi; s.nodeId = nodeId;
    if (serving)
    {
        s.currentCell = cell; s.lastServingRsrpDbm = rsrp; s.lastServingRsrqDb = rsrq;
        s.rsrpSum += rsrp; s.rsrqSum += rsrq; ++s.rsrpCount; ++s.rsrqCount;
    }
    if (self->m_config.saveMeasurementCsv && self->m_measurement)
    {
        *self->m_measurement << Simulator::Now().GetSeconds() << ',' << imsi << ',' << nodeId << ',' << cell << ','
                             << rsrp << ',' << rsrq << ',' << (serving ? 1 : 0) << ','
                             << (std::isnan(s.lastServingSinrDb) ? 0.0 : s.lastServingSinrDb) << '\n';
    }
}
void BaselineTraceCollector::RsrpSinrAdapter(BaselineTraceCollector* self, uint64_t imsi, uint32_t nodeId, uint16_t cell, uint16_t, double rsrp, double sinr, uint8_t)
{
    auto& s = self->m_state[imsi]; s.imsi = imsi; s.nodeId = nodeId; s.currentCell = cell;
    s.lastServingRsrpDbm = rsrp > 0 ? 10.0 * std::log10(rsrp) + 30.0 : s.lastServingRsrpDbm;
    s.lastServingSinrDb = sinr > 0 ? 10.0 * std::log10(sinr) : s.lastServingSinrDb;
    s.sinrSum += s.lastServingSinrDb; ++s.sinrCount;
}

void BaselineTraceCollector::ScheduleAssociationSampling()
{
    Simulator::Schedule(Seconds(0.5), &BaselineTraceCollector::SampleAssociations, this);
}
void BaselineTraceCollector::SampleAssociations()
{
    std::map<uint16_t, uint32_t> counts;
    for (uint32_t i = 0; i < m_ueDevices.GetN(); ++i)
    {
        auto device = m_ueDevices.Get(i)->GetObject<LteUeNetDevice>();
        const uint64_t imsi = device->GetImsi();
        const uint16_t cell = device->GetRrc()->GetCellId();
        auto& s = m_state[imsi]; s.imsi = imsi; s.nodeId = device->GetNode()->GetId(); s.currentCell = cell;
        if (s.lastPolledCell == 0) s.lastPolledCell = cell;
        else if (cell != s.lastPolledCell) { ++s.associationChangeCount; s.lastPolledCell = cell; }
        ++counts[cell];
        if (m_config.saveAssociationCsv && m_ueCell)
            *m_ueCell << Simulator::Now().GetSeconds() << ',' << imsi << ',' << s.nodeId << ',' << cell << '\n';
    }
    if (m_config.saveAssociationCsv && m_association)
        for (const auto& [cell, index] : m_cellMap)
            *m_association << Simulator::Now().GetSeconds() << ',' << cell << ',' << index << ',' << counts[cell] << '\n';
    if (Simulator::Now() + Seconds(m_config.associationSamplePeriodSec) < Seconds(m_config.simTimeSec))
        Simulator::Schedule(Seconds(m_config.associationSamplePeriodSec), &BaselineTraceCollector::SampleAssociations, this);
}

const RunState& BaselineTraceCollector::GetRunState() const { return m_state; }

} // namespace ns3::handover_congestion
