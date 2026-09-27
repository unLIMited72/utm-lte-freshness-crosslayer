# Public file manifests

FILE_MANIFEST.csv lists payload files except itself, root RELEASE_MANIFEST.csv and SHA256SUMS.txt. RELEASE_MANIFEST.csv includes FILE_MANIFEST.csv and all other payload files except itself and SHA256SUMS.txt. SHA256SUMS.txt covers every file except itself. This order avoids circular hashes. Source indices preserve original and current public hashes; PUBLIC_SOURCE_PROVENANCE.csv records release software provenance without private records. Internal review/approval records are not payload.
