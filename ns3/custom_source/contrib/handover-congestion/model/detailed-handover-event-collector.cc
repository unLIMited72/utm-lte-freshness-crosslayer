// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#include "detailed-handover-event-collector.h"

#include "ns3/lte-ue-net-device.h"
#include "ns3/lte-ue-rrc.h"
#include "ns3/simulator.h"

namespace ns3::handover_congestion
{

DetailedHandoverEventCollector::DetailedHandoverEventCollector(const ExperimentConfig& config,
                                                               std::ostream& stream)
    : m_config(config),
      m_stream(stream)
{
}

void
DetailedHandoverEventCollector::Connect(const NetworkArtifacts& network)
{
    for (uint32_t ueId = 0; ueId < network.ueDevices.GetN(); ++ueId)
    {
        auto device = network.ueDevices.Get(ueId)->GetObject<LteUeNetDevice>();
        const uint64_t imsi = device->GetImsi();
        auto rrc = device->GetRrc();
        rrc->TraceConnectWithoutContext(
            "HandoverStart",
            MakeBoundCallback(&DetailedHandoverEventCollector::StartAdapter, this, ueId));
        rrc->TraceConnectWithoutContext(
            "HandoverEndOk",
            MakeBoundCallback(&DetailedHandoverEventCollector::OkAdapter, this));
        rrc->TraceConnectWithoutContext(
            "HandoverEndError",
            MakeBoundCallback(&DetailedHandoverEventCollector::ErrorAdapter, this));
        m_nextEventId.emplace(imsi, 0);
    }
}

void
DetailedHandoverEventCollector::StartAdapter(DetailedHandoverEventCollector* self,
                                             uint32_t ueId,
                                             uint64_t imsi,
                                             uint16_t sourceCell,
                                             uint16_t rnti,
                                             uint16_t targetCell)
{
    self->ObserveStart(ueId, imsi, sourceCell, rnti, targetCell,
                       Simulator::Now().GetNanoSeconds());
}

void
DetailedHandoverEventCollector::OkAdapter(DetailedHandoverEventCollector* self,
                                          uint64_t imsi,
                                          uint16_t cell,
                                          uint16_t rnti)
{
    self->ObserveEnd(imsi, cell, rnti, Simulator::Now().GetNanoSeconds(), true);
}

void
DetailedHandoverEventCollector::ErrorAdapter(DetailedHandoverEventCollector* self,
                                             uint64_t imsi,
                                             uint16_t cell,
                                             uint16_t rnti)
{
    self->ObserveEnd(imsi, cell, rnti, Simulator::Now().GetNanoSeconds(), false);
}

void
DetailedHandoverEventCollector::ObserveStart(uint32_t ueId,
                                             uint64_t imsi,
                                             uint16_t sourceCell,
                                             uint16_t rnti,
                                             uint16_t targetCell,
                                             int64_t timeNs)
{
    auto active = m_active.find(imsi);
    if (active != m_active.end())
    {
        WriteClosed(active->second, sourceCell, rnti, timeNs, "SUPERSEDED");
        m_active.erase(active);
    }
    ActiveEvent event;
    event.eventId = ++m_nextEventId[imsi];
    event.ueId = ueId;
    event.imsi = imsi;
    event.sourceCell = sourceCell;
    event.targetCell = targetCell;
    event.startRnti = rnti;
    event.startTimeNs = timeNs;
    m_active.emplace(imsi, event);
}

void
DetailedHandoverEventCollector::ObserveEnd(uint64_t imsi,
                                           uint16_t endCell,
                                           uint16_t rnti,
                                           int64_t timeNs,
                                           bool success)
{
    auto active = m_active.find(imsi);
    if (active == m_active.end())
    {
        return;
    }
    WriteClosed(active->second, endCell, rnti, timeNs, success ? "OK" : "ERROR");
    m_active.erase(active);
}

void
DetailedHandoverEventCollector::WriteClosed(const ActiveEvent& event,
                                            uint16_t endCell,
                                            uint16_t endRnti,
                                            int64_t endTimeNs,
                                            const char* result)
{
    m_stream << "handover-events/1.0," << m_config.experimentId << ',' << m_config.runId << ','
             << event.eventId << ',' << event.ueId << ',' << event.imsi << ',' << event.sourceCell
             << ',' << event.targetCell << ',' << endCell << ',' << event.startTimeNs << ','
             << endTimeNs << ',' << result << ',' << (endTimeNs - event.startTimeNs) << ','
             << event.startRnti << ',' << endRnti << '\n';
}

void
DetailedHandoverEventCollector::WriteIncomplete(const ActiveEvent& event)
{
    m_stream << "handover-events/1.0," << m_config.experimentId << ',' << m_config.runId << ','
             << event.eventId << ',' << event.ueId << ',' << event.imsi << ',' << event.sourceCell
             << ',' << event.targetCell << ",," << event.startTimeNs
             << ",,INCOMPLETE,," << event.startRnti << ",\n";
}

void
DetailedHandoverEventCollector::Finalize()
{
    for (const auto& [imsi, event] : m_active)
    {
        WriteIncomplete(event);
    }
    m_active.clear();
}

} // namespace ns3::handover_congestion
