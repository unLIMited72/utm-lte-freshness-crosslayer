// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#ifndef HANDOVER_CONGESTION_DETAILED_HANDOVER_EVENT_COLLECTOR_H
#define HANDOVER_CONGESTION_DETAILED_HANDOVER_EVENT_COLLECTOR_H

#include "experiment-config.h"
#include "lte-network-builder.h"

#include <cstdint>
#include <map>
#include <ostream>

namespace ns3::handover_congestion
{

class DetailedHandoverEventCollector
{
  public:
    DetailedHandoverEventCollector(const ExperimentConfig& config, std::ostream& stream);
    void Connect(const NetworkArtifacts& network);
    void ObserveStart(uint32_t ueId, uint64_t imsi, uint16_t sourceCell, uint16_t rnti,
                      uint16_t targetCell, int64_t timeNs);
    void ObserveEnd(uint64_t imsi, uint16_t endCell, uint16_t rnti, int64_t timeNs, bool success);
    void Finalize();

  private:
    struct ActiveEvent
    {
        uint32_t eventId{0};
        uint32_t ueId{0};
        uint64_t imsi{0};
        uint16_t sourceCell{0};
        uint16_t targetCell{0};
        uint16_t startRnti{0};
        int64_t startTimeNs{0};
    };

    static void StartAdapter(DetailedHandoverEventCollector*, uint32_t, uint64_t, uint16_t,
                             uint16_t, uint16_t);
    static void OkAdapter(DetailedHandoverEventCollector*, uint64_t, uint16_t, uint16_t);
    static void ErrorAdapter(DetailedHandoverEventCollector*, uint64_t, uint16_t, uint16_t);
    void WriteClosed(const ActiveEvent&, uint16_t endCell, uint16_t endRnti, int64_t endTimeNs,
                     const char* result);
    void WriteIncomplete(const ActiveEvent&);

    const ExperimentConfig& m_config;
    std::ostream& m_stream;
    std::map<uint64_t, ActiveEvent> m_active;
    std::map<uint64_t, uint32_t> m_nextEventId;
};

} // namespace ns3::handover_congestion
#endif
