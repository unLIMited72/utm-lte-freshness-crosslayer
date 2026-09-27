# Study design

Production compares FIFO and STATUS_PRIORITY_NON_DROPPING separately at OFF (historical LOW) and HEAVY, with 88 common-random-number pairs per condition. Pair identity, RNG seed/run and stream identities are recorded in config/production. The policy continues a partially served SDU, otherwise prefers a waiting STATUS SDU and falls back to queue order. It neither replaces updates nor drops based on age.

Supporting intermediate loads have 20 pairs each: I80 = 80 ms = 25%, I40 = 40 ms = 50%, I30 = 30 ms = 66.7% of HEAVY nominal background load. These are contextual evidence and are not pooled with production as a confirmatory factorial sample. Production context copied into supporting tables is not extra independent replication.

The inferential unit is the paired run. Original paired-t confidence intervals and the fixed endpoint sequence (AoI p95, AoI mean, delivery-age p95, useful-gap p95) are retained, not recalculated. The plan was prespecified, not externally preregistered. The sample plan used precision as well as power, selecting 88 rather than the 60-pair power requirement. The 200-ms materiality reference is one reporting period, not an operational UTM threshold. The HEAVY PDR one-sided margin is -1 percentage point, a study choice; primary BG p95 is a mandatory descriptor rather than an acceptance gate.

CRN matches exogenous random streams, not every endogenous packet path. Waiting/residence and freshness use different populations/windows. Queue bytes do not materially decrease in the reported contrast. No marginal delay p95 values may be added to form an end-to-end p95.
