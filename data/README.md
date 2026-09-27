# Processed data

All selected original canonical numeric fields are preserved. CSV/JSON identifiers and NaN entries retain historical semantics. Only private/local storage strings were sanitized. No statistics, confidence intervals or p-values were recalculated.

| File family | Role | Inclusion tier |
|---|---|---|
| production_results_statistics.json and table_* | canonical processed results / stored statistics | MINIMUM_PUBLIC |
| production_run_summary.csv | run-level scalars | MINIMUM_PUBLIC |
| production_pair_differences.csv | pair-level differences | MINIMUM_PUBLIC |
| phase7_endpoint_distribution_summary.csv | mechanism and exploratory summaries | MINIMUM_PUBLIC |
| phase9_load_level_summary.csv, phase9_endpoint_summary.csv | supporting-load summaries, with production context identified | MINIMUM_PUBLIC |
| phase9_run_summary.csv, phase9_paired_differences.csv | supporting run/pair records | MINIMUM_PUBLIC |

Historical LOW = OFF (background disabled); not a 25% load. I80/I40/I30 identify 25/50/66.7%. Primary BG p95 is pooled FlowMonitor histogram p95; equal-UE and worst-UE exploratory tails are different estimands. Production exploratory full-trace tails and intermediate RX-window tails must not be treated as equivalent. See ../docs/ANALYSIS_WINDOWS.md.

Raw full-campaign event files are NOT_PUBLIC_DUE_TO_SIZE in this candidate. Selected representative raw runs are EXTENDED_OPTIONAL, not included or implicitly authorized. They could help independent raw-to-metric checking but cannot substitute for a full paired campaign. Stored processed outputs suffice for figure/table numeric traceability, not independent recomputation of every endpoint from events.
