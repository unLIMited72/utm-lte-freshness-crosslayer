// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#include "ns3/baseline-trace-collector.h"
#include "ns3/application-trace-collector.h"
#include "ns3/experiment-config.h"
#include "ns3/detailed-handover-event-collector.h"
#include "ns3/flow-metrics.h"
#include "ns3/packet-lineage-trace-collector.h"
#include "ns3/lte-network-builder.h"
#include "ns3/result-writer.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/rlc-queue-trace-collector.h"
#include "ns3/rlc-lineage-trace-collector.h"
#include "ns3/rlc-policy-decision-trace-collector.h"
#include "ns3/scenario-builder.h"
#include "ns3/simulator.h"
#include "ns3/traffic-installer.h"

#include <memory>

using namespace ns3;
using namespace ns3::handover_congestion;

int main(int argc,char* argv[])
{
    ExperimentConfig config=ExperimentConfig::Parse(argc,argv);
    RngSeedManager::SetSeed(config.rngSeed);
    RngSeedManager::SetRun(config.rngRun);

    ResultWriter writer(config);writer.Prepare();
    LteNetworkBuilder networkBuilder(config);networkBuilder.ConfigurePreDeviceBaseline();
    ScenarioBuilder scenarioBuilder(config);ScenarioNodes nodes=scenarioBuilder.Build();
    NetworkArtifacts network=networkBuilder.InstallDevicesAndNetwork(nodes);
    writer.WriteCrnStreamManifest(nodes,network);

    BaselineTraceCollector traces(config);
    traces.SetSampleStreams(writer.AssociationStream(),writer.UeCellStream(),writer.MeasurementStream());
    traces.Connect(network);
    std::unique_ptr<RlcQueueTraceCollector> rlcQueueEvents;
    if(config.enableRlcQueueEvents)
    {
        rlcQueueEvents=std::make_unique<RlcQueueTraceCollector>(config,*writer.RlcQueueEventStream());
        rlcQueueEvents->Connect(network);
    }
    std::unique_ptr<RlcLineageTraceCollector> rlcLineageEvents;
    if(config.enableRlcLineageEvents)
    {
        rlcLineageEvents=std::make_unique<RlcLineageTraceCollector>(config,*writer.RlcLineageEventStream());
        rlcLineageEvents->Connect(network);
    }
    std::unique_ptr<RlcPolicyDecisionTraceCollector> policyDecisions;
    if(config.enablePolicyDecisions)
    {
        policyDecisions=std::make_unique<RlcPolicyDecisionTraceCollector>(config,*writer.PolicyDecisionStream());
        policyDecisions->Connect(network);
    }
    std::unique_ptr<DetailedHandoverEventCollector> detailedHandoverEvents;
    if(config.enableDetailedHandoverEvents)
    {
        detailedHandoverEvents=std::make_unique<DetailedHandoverEventCollector>(config,*writer.HandoverEventStream());
        detailedHandoverEvents->Connect(network);
    }
    networkBuilder.AddX2AndAttach(nodes,network);

    TrafficInstaller trafficInstaller(config);TrafficArtifacts traffic=trafficInstaller.Install(nodes,network);
    std::unique_ptr<PacketLineageTraceCollector> packetLineageEvents;
    if(config.enablePacketLineage)
    {
        packetLineageEvents=std::make_unique<PacketLineageTraceCollector>(config,*writer.PacketLineageEventStream());
        packetLineageEvents->Connect(traffic);
    }
    std::unique_ptr<ApplicationTraceCollector> applicationTraces;
    if(config.enablePacketEvents)
    {
        applicationTraces=std::make_unique<ApplicationTraceCollector>(config,*writer.PacketEventStream());
        applicationTraces->Connect(traffic);
    }
    FlowMetrics flowMetrics(config);FlowMonitorArtifacts flow=flowMetrics.InstallMonitor();
    traces.ScheduleAssociationSampling();
    Simulator::Stop(Seconds(config.simTimeSec+config.cleanupTimeSec));
    Simulator::Run();
    if(detailedHandoverEvents)
    {
        detailedHandoverEvents->Finalize();
    }
    RunMetrics metrics=flowMetrics.Collect(flow,network,traffic,traces.GetRunState());
    writer.Write(metrics,traces.GetRunState(),network,flow);
    Simulator::Destroy();
    return 0;
}
