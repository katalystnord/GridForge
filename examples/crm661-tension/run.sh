#!/bin/sh
# CRM 661 (Nimonic 75) pulled: NPL's TENSTAND file 06, measured, against
# GridForge and ccx's prediction from TenSense's analysis of the same file
# (README.md). The data is fetched here and never committed: NPL states no
# licence for it. Needs TenSense's ts-analyse (TS_ANALYSE) and ccx
# (GF_CCX, or on PATH).
set -eu
here=$(cd "$(dirname "$0")" && pwd)
gf=${GF_STUDY:-$here/../../build-ninja/src/gf-study}
ts=${TS_ANALYSE:-ts-analyse}
url='https://npl.co.uk/getattachment/bf46dd80-b0ca-43a5-b836-ad4dab8c0064/tensile-ascii-set.zip?lang=en-US'
sha=e8671a82c2887b03be4d872206007d841bcc293db9dd53e6615eee019f305ed7
cd "$here"
mkdir -p data inputs
if [ ! -f data/06.txt ]; then
    curl -sSL -o data/tenstand.zip "$url"
    echo "$sha  data/tenstand.zip" | sha256sum -c -
    unzip -qjo data/tenstand.zip '*06.txt' -d data
fi
"$ts" --profile iso-6892-1-2019 --vtp inputs/06.vtp --freecad inputs/CRM661 data/06.txt
"$gf" solve pull.json bar.step --size 2 --order 2 -o job
"$gf" compare job/results/results.vtkhdf inputs/06.vtp --force-on "Pulled end" \
    --gauge "5,-1,1;55,-1,1" --times 4 -o compare.csv
