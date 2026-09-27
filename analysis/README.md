# Analysis helper provenance and use

historical/ retains the original relative ns-3 helper layout for imports. It includes packet reconstruction, continuous-time AoI, queue-state processing, RLC/background aggregation and supporting-load analysis. statistics/ contains the successful untracked production summary/statistics helpers reconstructed from historical patches. They were not Git-tracked; session logs themselves are excluded. figures/ contains the manuscript presentation generator adapted only to repository-relative inputs and work/presentation outputs.

SOURCE_PROVENANCE.csv in docs records origin, matching commits where known, original SHA256 and public SHA256. Copies retain documented path routing and add GPL-2.0-only license comments for this release; scientific logic is unchanged. Names containing phase7/phase9 remain implementation provenance identifiers, not extra study populations.

Raw-dependent helpers require reconstructed packet/lineage/FlowMonitor inputs absent from this minimum package; external_inputs/ paths are explicit input placeholders. The summary helper's historical module dependency is supplied under historical/contrib/handover-congestion/analysis/. Historical scripts can have top-level actions: DO NOT import them merely to inspect dependencies. Static AST parsing, not import/execution, was used here.

No new statistics or plots were generated. Running statistical code later is a separate analysis operation, not part of the safe file-verification workflow. All accepted results remain in data/canonical_results.
