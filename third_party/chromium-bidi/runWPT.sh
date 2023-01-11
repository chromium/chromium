#!/bin/sh

readonly LOG_DIR="logs"
readonly LOG_FILE="$LOG_DIR/$(date +%s).log"

# Go to the project root folder.
(cd "$(dirname $0)/" && \
# Create a folder to store logs.
mkdir -p "$LOG_DIR" && \
npm run build && \
./wpt/wpt run \
  --binary "$WPT_BROWSER_PATH" \
  --webdriver-binary runBiDiServer.sh \
  --webdriver-arg="--verbose" \
  --webdriver-arg="--log-path=$LOG_FILE" \
  --log-wptreport wptreport.json \
  --manifest wpt/MANIFEST.json \
  --metadata wpt-metadata \
  --timeout-multiplier 8 \
  chromium \
  "$@")

# Alternate usage:
#
# Run with locally built Chromium and Chromedriver, after `autoninja -C out/Default chrome chromedriver`:
#
#   --binary /path/to/chromium/src/out/Default/Chromium.app/Contents/MacOS/Chromium \
#   --webdriver-binary /path/to/chromium/src/out/Default/chromedriver \
#   --webdriver-arg="--bidi-mapper-path=$PWD/lib/iife/mapperTab.js" \
