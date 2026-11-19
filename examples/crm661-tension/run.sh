#!/bin/sh
# CRM 661 (Nimonic 75) pulled: NPL's TENSTAND file 06, measured, against
# GridForge and ccx's prediction from TenSense's analysis of the same file
# (README.md). The data is fetched here and never committed: NPL states no
# licence for it. Needs TenSense's ts-analyse (TS_ANALYSE) and ccx
# (GF_CCX, or on PATH).
set -eu
here=$(cd "$(dirname "$0")" && pwd)
gf=${GF_STUDY:-$here/../../build-ninja/src/gf-study}
# TenSense: TS_ANALYSE, else on PATH, else its build beside GridForge.
ts=${TS_ANALYSE:-$(command -v ts-analyse || echo "$here/../../../TenSense/build-ninja/src/ts-analyse")}
[ -x "$ts" ] || { echo "run.sh: TenSense's ts-analyse not found; set TS_ANALYSE to it" >&2; exit 1; }
# ccx: GF_CCX, else on PATH, else GridForge's own build (tools/build-ccx.sh).
if [ -z "${GF_CCX:-}" ] && ! command -v ccx >/dev/null 2>&1 && [ -x "$here/../../build-ccx/install/bin/ccx" ]; then
    export GF_CCX="$here/../../build-ccx/install/bin/ccx"
fi
url='https://npl.co.uk/getattachment/bf46dd80-b0ca-43a5-b836-ad4dab8c0064/tensile-ascii-set.zip?lang=en-US'
sha=e8671a82c2887b03be4d872206007d841bcc293db9dd53e6615eee019f305ed7
cd "$here"
mkdir -p data inputs
if [ ! -f data/06.txt ]; then
    curl -sSL -o data/tenstand.zip "$url"
    echo "$sha  data/tenstand.zip" | sha256sum -c -
    unzip -qjo data/tenstand.zip '*06.txt' '*01.txt' -d data
fi
"$ts" --profile iso-6892-1-2019 --vtp inputs/06.vtp --freecad inputs/CRM661 data/06.txt
"$gf" solve pull.json bar.step --size 2 --order 2 -o job
"$gf" compare job/results/results.vtkhdf inputs/06.vtp --force-on "Pulled end" \
    --gauge "5,-1,1;55,-1,1" --times 4 -o compare.csv
# A second test, predicted from the first's analysis: TENSTAND file 01
# ("Nimonic", the same test piece within 0.1 % of its area).
"$ts" --profile iso-6892-1-2019 --vtp inputs/01.vtp data/01.txt
"$gf" compare job/results/results.vtkhdf inputs/01.vtp --force-on "Pulled end" \
    --gauge "5,-1,1;55,-1,1" --times 4 -o compare-01.csv
