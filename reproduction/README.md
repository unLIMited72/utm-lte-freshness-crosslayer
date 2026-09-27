# Reproduction workflow and execution boundary

Safe, read-only check: `python3 reproduction/verify_package.py`. This checks file hashes, JSON/CSV selectors, included assets and syntax without executing analytical helpers. It does not simulate, estimate metrics, recompute statistics or regenerate plots.

Read reproduce_tables.md, reproduce_figures.md, verify_metrics.md and expected_outputs.md before using any original helper. The package supports inspection and processed-value traceability now; full raw-to-result replay is PARTIAL because raw traces and parts of the historical environment are absent. Do not describe the static check as independent scientific replication.
