// SPDX-License-Identifier: GPL-2.0-only
// Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
#include "result-writer.h"

#include "ns3/lte-ue-net-device.h"

#include <filesystem>
#include <iomanip>

namespace ns3::handover_congestion
{
namespace
{
void Parent(const std::string& path){auto p=std::filesystem::path(path);if(p.has_parent_path())std::filesystem::create_directories(p.parent_path());}
void WriteAggregate(std::ostream& o,const FlowAggregate& s)
{
    o<<s.txPackets<<','<<s.rxPackets<<','<<s.lostPackets<<','<<FlowMetrics::Pdr(s)<<','<<FlowMetrics::Plr(s)<<','
     <<FlowMetrics::AvgDelayMs(s)<<','<<FlowMetrics::AvgJitterMs(s)<<','<<FlowMetrics::P95DelayMs(s)<<','
     <<FlowMetrics::MaxDelayMs(s)<<','<<FlowMetrics::MinDelayMs(s);
}
void WritePerUeAggregate(std::ostream& o,const FlowAggregate& s)
{
    o<<s.txPackets<<','<<s.rxPackets<<','<<s.lostPackets<<','<<FlowMetrics::Pdr(s)<<','<<FlowMetrics::Plr(s)<<','
     <<FlowMetrics::AvgDelayMs(s)<<','<<FlowMetrics::AvgJitterMs(s)<<','<<FlowMetrics::P95DelayMs(s)<<','<<FlowMetrics::MaxDelayMs(s);
}
} // namespace
ResultWriter::ResultWriter(const ExperimentConfig& config):m_config(config){}
void ResultWriter::Prepare()
{
    Parent(m_config.FilePrefix()+"_per_ue.csv");
    Parent(m_config.outputPrefix+"_summary.csv");
    WriteEffectiveConfig(); WriteRunMetadata();
    if(m_config.enablePacketEvents)
    {
        m_packetEvents.open(m_config.FilePrefix()+"_packet_events.csv");
        m_packetEvents<<"schema_version,experiment_id,run_id,time_ns,ue_id,imsi,traffic_class,sequence,event_type,generation_time_ns,packet_size_bytes,local_port,remote_port\n";
    }
    if(m_config.enableDetailedHandoverEvents)
    {
        m_handoverEvents.open(m_config.FilePrefix()+"_handover_events.csv");
        m_handoverEvents<<"schema_version,experiment_id,run_id,ho_event_id,ue_id,imsi,source_cell,target_cell,end_cell,start_time_ns,end_time_ns,result,duration_ns,start_rnti,end_rnti\n";
    }
    if(m_config.enableRlcQueueEvents)
    {
        m_rlcQueueEvents.open(m_config.FilePrefix()+"_rlc_queue_events.csv");
        m_rlcQueueEvents<<"schema_version,experiment_id,run_id,time_ns,event_order,ue_id,imsi,direction,rnti,cell_id,lcid,event_type,queue_bytes,queue_container_count,hol_delay_ns,queue_empty,affected_bytes\n";
    }
    if(m_config.enablePacketLineage)
    {
        m_packetLineageEvents.open(m_config.FilePrefix()+"_packet_lineage_events.csv");
        m_packetLineageEvents<<"schema_version,experiment_id,run_id,time_ns,event_order,lineage_id,ue_id,imsi,traffic_class,application_sequence,generation_time_ns,application_tx_time_ns,event_type,packet_size_bytes\n";
    }
    if(m_config.enableRlcLineageEvents)
    {
        m_rlcLineageEvents.open(m_config.FilePrefix()+"_rlc_status_lineage.csv");
        m_rlcLineageEvents<<"schema_version,experiment_id,run_id,time_ns,event_order,lineage_id,ue_id,imsi,direction,rnti,cell_id,lcid,traffic_class,event_type,rlc_pdu_id,original_sdu_bytes,segment_bytes,served_total_bytes,remaining_bytes,enqueue_time_ns,generation_time_ns,application_attempt_time_ns,disposition\n";
    }
    if(m_config.enablePolicyDecisions)
    {
        m_policyDecisions.open(m_config.FilePrefix()+"_policy_decisions.csv");
        m_policyDecisions<<"schema_version,experiment_id,run_id,time_ns,event_order,ue_id,imsi,policy,action,reason,selected_lineage_id,selected_traffic_class,selected_generation_time_ns,selected_enqueue_time_ns,selected_waiting_age_ns,selected_remaining_bytes,queue_bytes_total,queued_status_count,queued_status_bytes,queued_other_count,queued_other_bytes,rnti,cell_id,lcid,ho_active\n";
    }
    if(m_config.saveAssociationCsv)
    {
        m_association.open(m_config.FilePrefix()+"_assoc.csv");m_association<<"timeSec,cellId,enbIndex,attachedUeCount\n";
        m_ueCell.open(m_config.FilePrefix()+"_ue_cell.csv");m_ueCell<<"timeSec,imsi,nodeId,cellId\n";
    }
    if(m_config.saveMeasurementCsv)
    {m_measurement.open(m_config.FilePrefix()+"_meas.csv");m_measurement<<"timeSec,imsi,nodeId,cellId,rsrpDbm,rsrqDb,isServing,sinrDb\n";}
}
std::ostream* ResultWriter::AssociationStream(){return m_association.is_open()?&m_association:nullptr;}
std::ostream* ResultWriter::UeCellStream(){return m_ueCell.is_open()?&m_ueCell:nullptr;}
std::ostream* ResultWriter::MeasurementStream(){return m_measurement.is_open()?&m_measurement:nullptr;}
std::ostream* ResultWriter::PacketEventStream(){return m_packetEvents.is_open()?&m_packetEvents:nullptr;}
std::ostream* ResultWriter::HandoverEventStream(){return m_handoverEvents.is_open()?&m_handoverEvents:nullptr;}
std::ostream* ResultWriter::RlcQueueEventStream(){return m_rlcQueueEvents.is_open()?&m_rlcQueueEvents:nullptr;}
std::ostream* ResultWriter::PacketLineageEventStream(){return m_packetLineageEvents.is_open()?&m_packetLineageEvents:nullptr;}
std::ostream* ResultWriter::RlcLineageEventStream(){return m_rlcLineageEvents.is_open()?&m_rlcLineageEvents:nullptr;}
std::ostream* ResultWriter::PolicyDecisionStream(){return m_policyDecisions.is_open()?&m_policyDecisions:nullptr;}
void ResultWriter::WriteCrnStreamManifest(const ScenarioNodes& nodes,const NetworkArtifacts& network) const
{
    const std::string f=m_config.outputPrefix+"_crn-stream-manifest.json";Parent(f);std::ofstream o(f);
    o<<std::setprecision(17);
    o<<"{\n  \"schemaVersion\": \"crn-stream-manifest/1.0\",\n"
     <<"  \"rngSeed\": "<<m_config.rngSeed<<",\n  \"rngRun\": "<<m_config.rngRun<<",\n"
     <<"  \"crnStreamBase\": "<<m_config.crnStreamBase<<",\n"
     <<"  \"mobilityMode\": \""<<m_config.mobilityMode<<"\",\n"
     <<"  \"mobilityStreamBase\": "<<nodes.mobilityStreamBase<<",\n"
     <<"  \"mobilityStreamsAssigned\": "<<nodes.mobilityStreamsAssigned<<",\n"
     <<"  \"lteStreamBase\": "<<network.lteStreamBase<<",\n"
     <<"  \"lteStreamsAssigned\": "<<network.lteStreamsAssigned<<",\n"
     <<"  \"remoteInternetStreamBase\": "<<network.remoteInternetStreamBase<<",\n"
     <<"  \"remoteInternetStreamsAssigned\": "<<network.remoteInternetStreamsAssigned<<",\n"
     <<"  \"ueInternetStreamBase\": "<<network.ueInternetStreamBase<<",\n"
     <<"  \"ueInternetStreamsAssigned\": "<<network.ueInternetStreamsAssigned<<",\n"
     <<"  \"trafficSchedule\": \"deterministic-udp-client\",\n"
     <<"  \"nodeCreationOrder\": \"enb-then-ue-then-epc-remote\",\n"
     <<"  \"policyUsesRng\": false,\n  \"linearMobilityIdentity\": [\n";
    for(std::size_t i=0;i<nodes.mobilityIdentity.size();++i)
    {
        const auto& m=nodes.mobilityIdentity[i];
        o<<"    {\"ueId\": "<<m.ueId<<", \"initialX\": "<<m.initialX
         <<", \"y\": "<<m.y<<", \"z\": "<<m.z<<", \"speed\": "<<m.speed<<"}"
         <<(i+1<nodes.mobilityIdentity.size()?",":"")<<"\n";
    }
    o<<"  ]\n}\n";
}
void ResultWriter::WriteEffectiveConfig() const
{
    const std::string f=m_config.outputPrefix+"_effective-config.json";Parent(f);std::ofstream o(f);
    o<<std::boolalpha<<"{\n  \"schemaVersion\": 1,\n  \"scenarioTag\": \""<<m_config.scenarioTag<<"\",\n"
     <<"  \"outputPrefix\": \""<<m_config.outputPrefix<<"\",\n  \"rngSeed\": "<<m_config.rngSeed<<",\n  \"rngRun\": "<<m_config.rngRun<<",\n"
     <<"  \"crnStreamBase\": "<<m_config.crnStreamBase<<",\n"
     <<"  \"experimentId\": \""<<m_config.experimentId<<"\",\n  \"runId\": \""<<m_config.runId<<"\",\n"
     <<"  \"timeResolution\": \"ns\",\n"
     <<"  \"enablePacketEvents\": "<<m_config.enablePacketEvents<<",\n"
     <<"  \"enableDetailedHandoverEvents\": "<<m_config.enableDetailedHandoverEvents<<",\n"
     <<"  \"enableRlcQueueEvents\": "<<m_config.enableRlcQueueEvents<<",\n"
     <<"  \"enablePacketLineage\": "<<m_config.enablePacketLineage<<",\n"
     <<"  \"enableRlcLineageEvents\": "<<m_config.enableRlcLineageEvents<<",\n"
     <<"  \"queuePolicy\": \""<<m_config.queuePolicy<<"\",\n"
     <<"  \"enablePolicyDecisions\": "<<m_config.enablePolicyDecisions<<",\n"
     <<"  \"simTimeSec\": "<<m_config.simTimeSec<<",\n  \"cleanupTimeSec\": "<<m_config.cleanupTimeSec<<",\n  \"appStartSec\": "<<m_config.appStartSec<<",\n"
     <<"  \"warmupSec\": "<<m_config.warmupSec<<",\n  \"numEnbs\": "<<m_config.numEnbs<<",\n  \"enbSpacingM\": "<<m_config.enbSpacingM<<",\n"
     <<"  \"enbHeightM\": "<<m_config.enbHeightM<<",\n  \"nUes\": "<<m_config.nUes<<",\n  \"hoHysteresisDb\": "<<m_config.hoHysteresisDb<<",\n  \"hoTttMs\": "<<m_config.hoTttMs<<",\n"
     <<"  \"schedulerType\": \""<<m_config.schedulerType<<"\",\n  \"rlcMapping\": \"RLC_UM_ALWAYS\",\n"
     <<"  \"useIdealRrc\": "<<m_config.useIdealRrc<<",\n  \"dlBandwidthRb\": "<<m_config.dlBandwidthRb<<",\n"
     <<"  \"ulBandwidthRb\": "<<m_config.ulBandwidthRb<<",\n  \"dlEarfcn\": "<<m_config.dlEarfcn<<",\n"
     <<"  \"ulEarfcn\": "<<m_config.ulEarfcn<<",\n  \"enbTxPowerDbm\": "<<m_config.enbTxPowerDbm<<",\n"
     <<"  \"ueTxPowerDbm\": "<<m_config.ueTxPowerDbm<<",\n  \"channelMode\": \""<<m_config.channelMode<<"\",\n  \"enableFading\": "<<m_config.enableFading<<",\n"
     <<"  \"fadingTrace\": \""<<m_config.fadingTrace<<"\",\n  \"fadingTraceSha256\": \""<<m_config.fadingTraceSha256<<"\",\n"
     <<"  \"pathlossExponent\": "<<m_config.pathlossExponent<<",\n  \"refLossDb\": "<<m_config.refLossDb<<",\n  \"refDistanceM\": "<<m_config.refDistanceM<<",\n"
     <<"  \"shadowSigmaOutdoorDb\": "<<m_config.shadowSigmaOutdoorDb<<",\n  \"mobilityMode\": \""<<m_config.mobilityMode<<"\",\n  \"linearMarginM\": "<<m_config.linearMarginM<<",\n"
     <<"  \"xMinM\": "<<m_config.xMinM<<",\n  \"xMaxM\": "<<m_config.xMaxM<<",\n  \"yMinM\": "<<m_config.yMinM<<",\n  \"yMaxM\": "<<m_config.yMaxM<<",\n  \"zMinM\": "<<m_config.zMinM<<",\n  \"zMaxM\": "<<m_config.zMaxM<<",\n"
     <<"  \"gmTimeStepSec\": "<<m_config.gmTimeStepSec<<",\n  \"gmAlpha\": "<<m_config.gmAlpha<<",\n  \"speedMinMps\": "<<m_config.speedMinMps<<",\n  \"speedMaxMps\": "<<m_config.speedMaxMps<<",\n"
     <<"  \"dirMinRad\": "<<m_config.dirMinRad<<",\n  \"dirMaxRad\": "<<m_config.dirMaxRad<<",\n  \"pitchMinRad\": "<<m_config.pitchMinRad<<",\n  \"pitchMaxRad\": "<<m_config.pitchMaxRad<<",\n"
     <<"  \"normalVelocityVariance\": "<<m_config.normalVelocityVariance<<",\n  \"normalDirectionVariance\": "<<m_config.normalDirectionVariance<<",\n  \"normalPitchVariance\": "<<m_config.normalPitchVariance<<",\n"
     <<"  \"statusIntervalMs\": "<<m_config.statusIntervalMs<<",\n  \"statusPacketSize\": "<<m_config.statusPacketSize<<",\n  \"enableBackground\": "<<m_config.enableBackground<<",\n"
     <<"  \"backgroundStartSec\": "<<m_config.backgroundStartSec<<",\n  \"backgroundStopSec\": "<<m_config.backgroundStopSec<<",\n  \"backgroundIntervalMs\": "<<m_config.backgroundIntervalMs<<",\n  \"backgroundPacketSize\": "<<m_config.backgroundPacketSize<<",\n"
     <<"  \"enableBurst\": "<<m_config.enableBurst<<",\n  \"burstStartSec\": "<<m_config.burstStartSec<<",\n  \"burstDurationSec\": "<<m_config.burstDurationSec<<",\n  \"burstIntervalMs\": "<<m_config.burstIntervalMs<<",\n  \"burstPacketSize\": "<<m_config.burstPacketSize<<",\n"
     <<"  \"saveXml\": "<<m_config.saveXml<<",\n  \"saveCsv\": "<<m_config.saveCsv<<",\n  \"xmlHistograms\": "<<m_config.xmlHistograms<<",\n  \"xmlProbes\": "<<m_config.xmlProbes<<",\n"
     <<"  \"saveAssociationCsv\": "<<m_config.saveAssociationCsv<<",\n  \"saveMeasurementCsv\": "<<m_config.saveMeasurementCsv<<",\n  \"enableAnimation\": "<<m_config.enableAnimation<<",\n"
     <<"  \"associationSamplePeriodSec\": "<<m_config.associationSamplePeriodSec<<",\n  \"delayBinWidthSec\": "<<m_config.delayBinWidthSec<<",\n  \"jitterBinWidthSec\": "<<m_config.jitterBinWidthSec<<",\n  \"packetSizeBinWidth\": "<<m_config.packetSizeBinWidth<<"\n}\n";
}
void ResultWriter::WriteRunMetadata() const
{
    const std::string f=m_config.outputPrefix+"_run-metadata.json";Parent(f);std::ofstream o(f);
    o<<"{\n  \"schemaVersion\": 1,\n  \"implementation\": \"handover-congestion modular baseline\",\n"
     <<"  \"experimentId\": \""<<m_config.experimentId<<"\",\n  \"runId\": \""<<m_config.runId<<"\",\n"
     <<"  \"timeResolution\": \"ns\",\n"
     <<"  \"legacySourceSha256\": \"bab8db02795948bb9d5692d528208439478f3f9d6c864064a5098a5cc3d5fd2a\",\n"
     <<"  \"ns3BaselineCommit\": \"4059af204636298c061df368e241941fa3ff97bf\"\n}\n";
}
void ResultWriter::WriteSummary(const RunMetrics& m) const
{
    const std::string f=m_config.outputPrefix+"_summary.csv";const bool exists=std::filesystem::exists(f);std::ofstream o(f,std::ios::app);
    if(!exists)o<<"scenarioTag,rngSeed,rngRun,numEnbs,nUes,simTime,channelMode,enableFading,pathlossExponent,mobilityMode,hoHysteresisDb,hoTtTMs,statusIntervalMs,statusPacketSize,enableBackground,bgIntervalMs,bgPacketSize,enableBurst,burstStart,burstDuration,burstIntervalMs,burstPacketSize,gmTimeStep,gmAlpha,speedMin,speedMax,pitchMin,pitchMax,xMin,xMax,yMin,yMax,zMin,zMax,totalHoCount,avgHoCountPerUe,avgHoInterruptionMs,maxHoInterruptionMs,avgAssocChangePerUe,avgServingRsrpDbm,avgServingRsrqDb,avgServingSinrDb,statusTx,statusRx,statusLost,statusPdr,statusPlr,statusAvgDelayMs,statusAvgJitterMs,statusP95DelayMs,statusMaxDelayMs,statusMinDelayMs,bgTx,bgRx,bgLost,bgPdr,bgPlr,bgAvgDelayMs,bgAvgJitterMs,bgP95DelayMs,bgMaxDelayMs,bgMinDelayMs,burstTx,burstRx,burstLost,burstPdr,burstPlr,burstAvgDelayMs,burstAvgJitterMs,burstP95DelayMs,burstMaxDelayMs,burstMinDelayMs\n";
    o<<m_config.scenarioTag<<','<<m_config.rngSeed<<','<<m_config.rngRun<<','<<m_config.numEnbs<<','<<m_config.nUes<<','<<m_config.simTimeSec<<','
     <<m_config.channelMode<<','<<(m_config.enableFading?1:0)<<','<<m_config.pathlossExponent<<','<<m_config.mobilityMode<<','<<m_config.hoHysteresisDb<<','<<m_config.hoTttMs<<','
     <<m_config.statusIntervalMs<<','<<m_config.statusPacketSize<<','<<(m_config.enableBackground?1:0)<<','<<m_config.backgroundIntervalMs<<','<<m_config.backgroundPacketSize<<','
     <<(m_config.enableBurst?1:0)<<','<<m_config.burstStartSec<<','<<m_config.burstDurationSec<<','<<m_config.burstIntervalMs<<','<<m_config.burstPacketSize<<','
     <<m_config.gmTimeStepSec<<','<<m_config.gmAlpha<<','<<m_config.speedMinMps<<','<<m_config.speedMaxMps<<','<<m_config.pitchMinRad<<','<<m_config.pitchMaxRad<<','
     <<m_config.xMinM<<','<<m_config.xMaxM<<','<<m_config.yMinM<<','<<m_config.yMaxM<<','<<m_config.zMinM<<','<<m_config.zMaxM<<','
     <<m.radio.totalHoCount<<','<<m.radio.avgHoCountPerUe<<','<<m.radio.avgHoInterruptionMs<<','<<m.radio.maxHoInterruptionMs<<','<<m.radio.avgAssociationChangePerUe<<','
     <<m.radio.avgServingRsrpDbm<<','<<m.radio.avgServingRsrqDb<<','<<m.radio.avgServingSinrDb<<',';
    WriteAggregate(o,m.status);o<<',';WriteAggregate(o,m.background);o<<',';WriteAggregate(o,m.burst);o<<'\n';
}
void ResultWriter::WritePerUe(const RunMetrics& m,const RunState& state,const NetworkArtifacts& network) const
{
    std::ofstream o(m_config.FilePrefix()+"_per_ue.csv");
    o<<"imsi,nodeId,currentCell,lastServingRsrpDbm,lastServingRsrqDb,lastServingSinrDb,hoCount,assocChangeCount,hoInterruptionSumMs,hoInterruptionMaxMs,statusTx,statusRx,statusLost,statusPdr,statusPlr,statusAvgDelayMs,statusAvgJitterMs,statusP95DelayMs,statusMaxDelayMs,bgTx,bgRx,bgLost,bgPdr,bgPlr,bgAvgDelayMs,bgAvgJitterMs,bgP95DelayMs,bgMaxDelayMs,burstTx,burstRx,burstLost,burstPdr,burstPlr,burstAvgDelayMs,burstAvgJitterMs,burstP95DelayMs,burstMaxDelayMs\n";
    for(uint32_t u=0;u<m_config.nUes;++u)
    {
        const uint64_t imsi=network.ueDevices.Get(u)->GetObject<LteUeNetDevice>()->GetImsi();const auto& s=state.at(imsi);
        o<<imsi<<','<<s.nodeId<<','<<s.currentCell<<','<<s.lastServingRsrpDbm<<','<<s.lastServingRsrqDb<<','<<s.lastServingSinrDb<<','<<s.hoCount<<','<<s.associationChangeCount<<','<<s.hoInterruptionSum.GetSeconds()*1000<<','<<s.hoInterruptionMax.GetSeconds()*1000<<',';
        WritePerUeAggregate(o,m.statusPerUe[u]);o<<',';WritePerUeAggregate(o,m.backgroundPerUe[u]);o<<',';WritePerUeAggregate(o,m.burstPerUe[u]);o<<'\n';
    }
}
void ResultWriter::Write(const RunMetrics& m,const RunState& state,const NetworkArtifacts& network,FlowMonitorArtifacts& flow)
{
    if(m_config.saveXml)flow.monitor->SerializeToXmlFile(m_config.FilePrefix()+"_flow.xml",m_config.xmlHistograms,m_config.xmlProbes);
    if(m_config.saveCsv)
    {
        WriteSummary(m);
    }
    WritePerUe(m,state,network);
}
} // namespace ns3::handover_congestion
