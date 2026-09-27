// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#ifndef HANDOVER_CONGESTION_APPLICATION_TRACE_COLLECTOR_H
#define HANDOVER_CONGESTION_APPLICATION_TRACE_COLLECTOR_H

#include "experiment-config.h"
#include "traffic-installer.h"
#include "ns3/address.h"
#include "ns3/packet.h"

#include <ostream>

namespace ns3::handover_congestion
{

class ApplicationTraceCollector
{
  public:
    ApplicationTraceCollector(const ExperimentConfig& config, std::ostream& stream);
    void Connect(const TrafficArtifacts& traffic);

  private:
    static void TxAdapter(ApplicationTraceCollector*, const ApplicationBinding*, Ptr<const Packet>, const Address&, const Address&);
    static void RxAdapter(ApplicationTraceCollector*, const ApplicationBinding*, Ptr<const Packet>, const Address&, const Address&);
    void Write(const ApplicationBinding&, Ptr<const Packet>, const Address&, const Address&, const char* eventType);

    const ExperimentConfig& m_config;
    std::ostream& m_stream;
};

} // namespace ns3::handover_congestion
#endif
