# An ns-3–Based Study of Mobile Network Characteristics in Low-Altitude UTM Service Environments: Receiver Freshness and Background Delay

**Repository status: PUBLIC. Archival package: PUBLISHED on Zenodo. Version: v1.0-submission.**

Repository: https://github.com/unLIMited72/utm-lte-freshness-crosslayer

Version-frozen archival record: https://doi.org/10.5281/zenodo.22994406

All Zenodo versions: https://doi.org/10.5281/zenodo.22994405

Zenodo publication date: 2026-09-27.

The immutable `v1.0-submission` tag remains fixed to commit `33685ca67cd383e7069d7e47d1438219a83bdf91`. Later main-branch changes synchronize publication metadata only; they do not replace the archived scientific snapshot. The published archive and bundled manifests/checksums remain unchanged. On main, README.md and CITATION.cff intentionally differ from those snapshot checksums. Use the fixed tag for strict whole-snapshot checksum validation; source, data and numerical bindings are unchanged.

## Overview

Periodic UAV state updates share LTE/EPC uplinks with competing traffic. This study examines RLC pre-service waiting, receiver information freshness (Age of Information, AoI), a simple STATUS-priority service order, and its background-traffic delay cost. STATUS is a study-specific periodic message, not a standardized UTM message.

## Main contribution

The contribution is cross-layer diagnosis, explicit metric definitions and paired benefit/cost evaluation, rather than invention of priority scheduling. In the recorded HEAVY experiment, AoI p95 changed from 1315.59 to 316.59 ms and waiting p95 from 840.79 to 33.46 ms. Background primary p95 increased from 727.07 to 764.01 ms. These are stored manuscript values, not newly computed estimates. Waiting reduction is consistent with reduced pre-service blocking; causal mediation is not established.

## Repository contents

- [Metric contract](docs/METRIC_CONTRACT.md), [windows](docs/ANALYSIS_WINDOWS.md), [study design](docs/STUDY_DESIGN.md) and [scope](docs/STUDY_SCOPE.md).
- [Simulation modifications](ns3/README.md) and [historical analysis helpers](analysis/README.md).
- `data/canonical_results/`: stored endpoint, run, pair and supporting summaries.
- `config/`: production runtime/CRN records and production/supporting manifests.
- `manuscript_support/`: original figure PDFs, table source and public numerical bindings.
- [File manifest](docs/FILE_MANIFEST.md) and [source provenance](docs/SOURCE_PROVENANCE.csv).

## Reproduction scope

This is an analysis and processed-result reproducibility package with documented limitations on exact historical simulation-environment reconstruction. The primary purpose is inspection and traceability of published values, definitions and code. Static validation has been performed; no simulation, statistical reanalysis or figure regeneration was performed during packaging. See [reproduction instructions](reproduction/README.md).

## Known limitations

The contemporaneous original executable checksum, executed antenna runtime attributes, executed S1-U/S5 runtime attributes and a complete historical dependency lock are unavailable. A current binary hash cannot establish historical binary identity. Source defaults are not executed settings. Production exploratory application tails use the full trace; new intermediate-load exploratory tails use first reception in [15,120). Their p99/worst-UE trends are not uniform-window comparisons.

There is no operational UTM validation, safety claim or 5G/6G inference. No optimality, starvation guarantee or standards compliance is established. Raw traces are not included; raw-event recomputation requires unavailable external raw inputs. No representative raw sample is included. See [reproducibility boundary](docs/REPRODUCIBILITY_BOUNDARY.md).

## Requirements

Python 3 supports the checksum/input inspection workflow. Historical analysis uses NumPy and mpmath; presentation helpers also require Matplotlib. This is not a complete environment lock. See [versions](environment/software_versions.md).

## Analysis workflow

1. Read the metric and window contracts before interpreting any CSV.
2. From the fixed `v1.0-submission` tag, run `python3 reproduction/verify_package.py` to check files, snapshot hashes and stored-value bindings only. On main, its two README/CFF checksum differences reflect the publication-metadata update described above.
3. Inspect the analysis code; do not treat raw-dependent helpers as a one-command replay.
4. Consult [expected outputs](reproduction/expected_outputs.md) and input requirements before any later, separately authorized execution.

## Figure/table reproduction

[Figures](reproduction/reproduce_figures.md) and [tables](reproduction/reproduce_tables.md) have explicit mappings. Eight figure PDFs and six table sources are included unchanged. The presentation helper reads stored summaries; it does not estimate new statistics. Its execution was not tested in this packaging task.

## Data structure

`LOW` in original production files means background-OFF, not an intermediate low load. `HEAVY` means a 20-ms background interval; I80/I40/I30 mean 80/40/30 ms (25/50/66.7%). Original identifiers, numerical cells and NaN values are retained. Local storage paths were replaced by relative input placeholders; these are not new executed paths. See [data dictionary](data/README.md).

## Citation

Associated manuscript (unpublished): **An ns-3–Based Study of Mobile Network Characteristics in Low-Altitude UTM Service Environments: Receiver Freshness and Background Delay**. Manuscript authors, in order: Won-hyuk Choi (ORCID https://orcid.org/0009-0003-0754-2494); Seoungjun Lim (ORCID https://orcid.org/0009-0004-4911-8285). Corresponding author: Won-hyuk Choi (choiwh@hanseo.ac.kr).

The top-level software/resource authors in CITATION.cff remain the original artifact creators; preferred-citation identifies the current manuscript authors. Historical resource titles, creator names and the fixed archive retain their original attribution. Manuscript metadata corrections do not change software/data ownership or the archived scientific files.

Use [CITATION.cff](CITATION.cff). The manuscript remains unpublished; the reproducibility package was published on Zenodo on 2026-09-27. Cite version DOI https://doi.org/10.5281/zenodo.22994406 for the fixed v1.0-submission artifact. Concept DOI https://doi.org/10.5281/zenodo.22994405 identifies all Zenodo versions and does not replace the version-specific DOI.

## License

Software is licensed under GPL-2.0-only; project-controlled data and original documentation under CC BY 4.0. Existing upstream notices are preserved, and third-party works are not relicensed. See [LICENSE](LICENSE), [scope map](docs/LICENSE_SCOPE.md) and [NOTICE](NOTICE).

## Third-party materials

[Third-party references](docs/THIRD_PARTY_REFERENCES.md) provides links and notices. Journal PDFs, proprietary manuals, private session logs and the full upstream ns-3 distribution are excluded. Existing copyright author attribution in source is intentionally retained.

## Contact

Manuscript corresponding author: Won-hyuk Choi, choiwh@hanseo.ac.kr; ORCID 0009-0003-0754-2494. Manuscript coauthor and resource contact: Seoungjun Lim, limcraft7260@gmail.com; ORCID 0009-0004-4911-8285. Manuscript affiliation: Department of Avionics, Hanseo University, Republic of Korea.
