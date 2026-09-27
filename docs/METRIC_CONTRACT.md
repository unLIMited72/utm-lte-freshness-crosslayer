# Metric contract

**AoI p95.** For each reconstructed STATUS UE stream, continuous receiver age is time since the latest strictly newer received generation. Its exact time-weighted p95 over defined exposure within 15–120 s is computed and averaged equally across estimable UEs into one run scalar. Undefined prefixes/no-delivery streams are not assigned artificial ages; all saved production UE exposures are in fact 105 s. Compare equal-weight CRN run pairs, not packets/time samples.

**AoI mean.** Integrate each UE's continuous age over its defined 15–120 s exposure, divide by that UE's exposure, and equally average finite UE means. Missing-prefix time is excluded in the general code; saved production prefixes are zero. Inference is on paired run scalars.

**Delivery-age p95.** Among freshness-effective first application STATUS receptions in [15,120), use RX−generation delay; compute the explicit linear sample p95 per UE and then its equal-UE mean. Generation is not window-filtered; duplicates, obsolete and undelivered updates do not contribute extra observations, and nonestimable UEs are omitted. Infer on CRN run pairs.

**Useful-gap p95.** For each UE, use complete intervals between consecutive freshness-effective STATUS first receptions selected in [15,120). Exclude the leading crossing-T0 gap and trailing interval to T1; UEs with fewer than two selected receptions are omitted. Average linear per-UE p95s equally, then compare run pairs.

**Waiting p95.** On the entire STATUS RLC trace, measure first SERVICE_FRAGMENT−enqueue for SDUs with both observations, including partially served SDUs even if later censored. Omit never-served/missing-enqueue records. Take linear per-UE p95 then equal-UE mean; compare run pairs as mechanism evidence, with no 15-s warmup filter.

**Service-duration p95.** On the full STATUS RLC lifecycle, measure FULLY_SERVED−first SERVICE_FRAGMENT for completed SDUs, excluding incomplete/censored service. Average linear UE p95s equally into each run; inference uses run pairs. Cleanup service is eligible and application delivery is not required.

**Residence p95.** On all ENQUEUE-tracked STATUS lineages terminating FULLY_SERVED, use full−enqueue, with no time-window filter. Average per-IMSI linear p95s equally, excluding uncompleted/censored SDUs. Compare run pairs; R=W+S holds per completed packet but not for marginal percentiles.

**Post-RLC p95.** Join fully served STATUS lineage IDs through TX_SUCCESS to UE/sequence and first reconstructed application RX, using the whole trace. Pool all matched RX−full delays within run and take a linear p95; this is packet-weighted, not equal-UE. Exclude missing full service/mapping/RX, without imputing censored delays. Run pairs remain the inference unit.

**Queue bytes.** Carry each IMSI's observed QueueState post-state to its next row and integrate queue bytes over [15,120), dividing by 105 s; then equally average included IMSIs. No-event UEs are absent and lifecycle censoring does not inject zeros. Same-time sorting and no multi-bearer summation follow the original queue helper. Compare run pairs.

**Backlog occupancy.** Use the same QueueState stream, exposure and equal-IMSI weighting, integrating the indicator queue_bytes>0 rather than bytes. No-event streams are omitted and last state persists to T1. It describes observed post-state occupancy, with run-pair inference.

**BG PDR.** Select FlowMonitor UDP flows to the remote BG ports; pool tracked destination IP RX and first IP TX counts after monitor enablement at 15 s, with app TX stopping at 120 s and IP RX observable through cleanup to simulation stop at 122 s. Divide summed RX by summed TX (zero for no TX). Lost/in-flight TXs remain denominator-only; compare run-pair ratios using the frozen HEAVY harm rule.

**BG mean delay.** Over received tracked BG IP packets in that FlowMonitor population, divide pooled delay sum by pooled RX count, then retain a single run mean. Lost packets have no imputed delay; packet weighting applies within run and equal run-pair weighting across runs.

**BG primary p95.** Sum 1 ms FlowMonitor delay-histogram counts across eligible BG flows, returning the upper edge of the first bin attaining 95% of received-packet counts. No bin interpolation or equal-UE p95 averaging is performed; nonreceived packets are excluded from latency and cleanup IP RX is eligible. Compare per-run percentiles using run pairs.

**Exploratory equal-UE p95/p99.** In production Phase7, take all delivered reconstructed BG generation-to-first-application-RX delays over the whole application trace, with no warmup filter. Compute each UE's linear p95/p99, then average equally across UEs with deliveries; undelivered packets/nonestimable UEs are excluded. Infer only on run/pair scalars. New Phase9 intermediate loads restrict first RX to [15,120), unlike imported production context.

**Worst-UE p95/p99.** For the same exploratory packet population, take the maximum among UE p95/p99 values within each run, excluding no-delivery UEs. It is the worst UE percentile, not worst single-packet delay; compare run-pair maxima. The Phase7/Phase9 window difference also applies here.

Production inference uses 88 paired runs per load; supporting inference remains separate (20 pairs per intermediate load). No packet, UE or time sample is an independent replicate.
