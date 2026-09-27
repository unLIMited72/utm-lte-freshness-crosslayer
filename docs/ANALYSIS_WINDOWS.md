# Analysis window matrix

Times are seconds. "Whole trace" means no helper-level time selection, not an assertion that every event type exists at every time. The companion metric contract defines populations, aggregation and censoring.

| Metric | Input selection | Time basis | Start | End | Cleanup | Boundary |
|---|---|---|---|---|---|---|
| AoI p95/mean | STATUS first RX, accept newer generation; full history initializes state | continuous defined exposure | 15, or first useful RX if later | 120 | RX>120 excluded | RX=15 initializes; RX=120 zero subsequent exposure; no generation cut |
| delivery-age p95 | delivered fresh-at-first-RX STATUS | first app RX | 15 inclusive | 120 exclusive | excluded | pre-15 generation eligible when RX in window |
| useful-gap p95 | consecutive selected useful STATUS RXs | first app RX pair | both >=15 | both <120 | excluded | no leading cross-T0 gap or trailing-to-T1 gap |
| waiting p95 | STATUS with enqueue and first service | whole RLC lifecycle | no cut; apps start 1 | simulation stop 122 / dispositions | first service eligible | no T0/T1/enqueue-time filter; full service unnecessary |
| service duration p95 | STATUS with first and full service | whole RLC lifecycle | no cut | stop 122 | full service eligible | incomplete service excluded |
| canonical residence p95 | ENQUEUE-tracked STATUS, terminal FULLY_SERVED | whole RLC lifecycle | no cut | stop 122 | full service eligible | incomplete/overflow/censored excluded |
| post-RLC p95 | full STATUS lineage, TX_SUCCESS and delivered app join | full service to first app RX | no cut | sink stop 120 limits app RX | no matched cleanup-only sink RX | pooled packets, no explicit T0/T1 cut |
| qbar | all queue rows grouped by IMSI | clipped elapsed-time integration | 15 | 120 | post-120 state has no exposure | pre-window post-state carried forward; tie sort ignores event_order |
| backlog occupancy | same queue rows | time with queue_bytes>0 | 15 | 120 | as qbar | 105-s denominator; no synthetic disposal reset |
| BG PDR | UDP remote-host BG FlowMonitor flows | monitored first IP TX, tracked destination RX | monitor enable 15 | collection after stop 122; app TX stops 120 | IP RX eligible | scheduled enable/stop ordering; pre-enable TX not tracked |
| BG primary mean delay | received monitored BG flows | first IP TX→destination IP RX | monitor enable 15 | stop 122 | IP RX eligible | pooled delay sum/RX; no losses assigned latency |
| BG primary p95 | pooled received-flow histograms | same IP delay population | monitor enable 15 | stop 122 | IP RX eligible | first cumulative >=95%, 1 ms bin upper edge |
| production exploratory equal-UE p95/p99 | all delivered reconstructed BG | first app RX−generation | no cut; apps start 1 | sink stop 120 | no sink RX after closure | NumPy linear; no warmup exclusion |
| production worst-UE p95/p99 | same; max UE quantile | first app RX−generation | no cut | sink stop 120 | as above | max within run, not maximum packet |
| new Phase9 equal-UE p99 / worst-UE p95,p99 | delivered reconstructed BG with first RX filter | first app RX | 15 inclusive | 120 exclusive | excluded | Phase9 HEAVY context uses full-trace Phase7 values instead |

FlowMonitor's scheduled activation replaces its pending zero-time start; `ReportLastRx` rejects untracked pre-enable packets. Apps and sinks stop at 120 s; Simulator stops at 122 s. Do not describe all metrics as sharing [15,120).
