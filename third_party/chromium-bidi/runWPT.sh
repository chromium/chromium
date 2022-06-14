#!/bin/sh

# Go to the project root folder.
cd "$(dirname $0)/"

npm run build && \
./wpt/wpt run \
  --webdriver-binary ./runBiDiServer.sh \
  --binary "$WPT_BROWSER_PATH" \
  --manifest ./wpt/MANIFEST.json \
  --metadata ./wpt-metadata \
  --log-wptreport wptreport.json \
  chromium \
  $1
#  --timeout-multiplier 4 \
