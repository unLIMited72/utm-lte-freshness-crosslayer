# SPDX-License-Identifier: GPL-2.0-only
# Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
build_lib_example(
  NAME handover-congestion-baseline
  SOURCE_FILES handover-congestion-baseline.cc
  LIBRARIES_TO_LINK ${libhandover-congestion}
)
