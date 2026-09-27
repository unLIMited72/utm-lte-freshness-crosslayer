# Software versions

| Context | Evidence | Status |
|---|---|---|
| Historical simulation source | ns-3.45; baseline 4059af204636298c061df368e241941fa3ff97bf | SOURCE_CONSTRUCTION; not original binary identity |
| Production source freeze | ec84414f367b0f525a700c30d13986667e17e1d8; followup-status-priority-production-freeze-v2 | Git provenance |
| Later supporting analysis source | d932033efe968819cf5b692edd85309e29549d59 | Git provenance; not substituted for production version |
| Historical analysis NumPy | 1.21.5 | recovered execution evidence |
| Historical analysis mpmath | 1.3.0 | successful production statistics helper |
| Historical compiler, Python exact build, OS dependency lock | UNKNOWN / incomplete | not inferred from present tools |
| Current recovery environment | Ubuntu 22.04.5, Python 3.10.12 | 2026-09-27 recovery record, not historical lock |
| Presentation dependencies | Python, Matplotlib | exact historical Matplotlib pin not established here |

An unsuccessful SciPy-based helper variant is excluded; it is not the final executed production statistics implementation. Python stdlib handles static package verification. NumPy, mpmath and Matplotlib are needed for corresponding analytical/presentation helpers, but those helpers were not run in this preparation.
