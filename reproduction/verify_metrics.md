# Verify metric definitions and values

METRIC_CONTRACT.md and ANALYSIS_WINDOWS.md specify populations, weighting, censoring, aggregation and paired-run inference. numerical_binding_public.csv supplies 79 result selectors, raw stored values, units and manuscript display rounding. Configuration/literal method numbers are documented separately in config/CONFIG_EVIDENCE_CLASSIFICATION.csv and Tables 1–3; they are not silently treated as statistical results.

Static verification compares retained cell text/numeric representations to the stored binding and frozen number macros, without recalculating a metric or a confidence interval. Raw-event validation would require packet/lineage/queue/FlowMonitor inputs absent here. The historical helper files are supplied for inspection with that limitation.
