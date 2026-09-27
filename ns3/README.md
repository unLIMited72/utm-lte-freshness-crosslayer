# ns-3 source and modification provenance

Recorded ns-3.45 baseline: 4059af204636298c061df368e241941fa3ff97bf; production freeze: ec84414f367b0f525a700c30d13986667e17e1d8. Official upstream: https://www.nsnam.org/ ; source: https://gitlab.com/nsnam/ns-3-dev . The full upstream distribution and executables are not bundled.

custom_source/ contains the 31 project-integrated module/example/test/build files from the production freeze, with GPL-2.0-only license comments added for this release. modified_source/ contains annotated full representations of all seven targets in status_priority_production.patch. Original upstream headers are retained. Per-file modification notices list every recorded baseline-to-production change commit/date. See [public source provenance](../docs/PUBLIC_SOURCE_PROVENANCE.csv).

For later source assembly, obtain the exact baseline externally, apply status_priority_production.patch, then release_notice.patch, and place custom_source/contrib/handover-congestion under contrib/. Alternatively replace the seven patched target files with the supplied modified_source/ versions. Do not apply both alternatives twice. The historical patch stays byte-identical; the notice patch changes comments only. Static patch application was checked against recorded source bytes. No build or simulation was run, and no bit-identical historical replay is guaranteed.

UPSTREAM_LICENSE and [license scope](../docs/LICENSE_SCOPE.md) preserve GPL-2.0-only. The full dependency lock, historical executable checksum and selected executed runtime attributes remain unavailable. Raw-event replay needs external raw inputs. Source inclusion is complete for the declared project-modification scope, not a complete upstream/environment archive.
