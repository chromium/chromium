#!/bin/bash
# shellcheck disable=SC2155

set -euo pipefail

log() {
  echo "($0) $*"
}

usage() {
  log "Usage: [HEADLESS=<true | false>] [UPDATE_EXPECTATIONS=<true | false>] $0 [webdriver/tests/bidi/[...]]"
}

if [[ $# -gt 0 && ("$1" == "-h" || "$1" == "--help") ]]; then
  usage
  exit 0
fi

# Whether to start the server in headless or headful mode.
readonly HEADLESS="${HEADLESS:-true}"

# The path to the WPT manifest file.
readonly MANIFEST="${MANIFEST:-MANIFEST.json}"

# The browser product to test.
readonly PRODUCT="${PRODUCT:-chromium}"

# Whether to update the WPT expectations after running the tests.
readonly UPDATE_EXPECTATIONS="${UPDATE_EXPECTATIONS:-true}"

# The path to the browser binary.
readonly WPT_BROWSER_PATH="${WPT_BROWSER_PATH:-/Applications/Google Chrome Dev.app/Contents/MacOS/Google Chrome Dev}"

# The path to the WPT report file.
readonly WPT_REPORT="${WPT_REPORT:-wptreport.json}"

# The path to the WPT metadata directory.
if [[ "$HEADLESS" == "true" ]]; then
  readonly WPT_METADATA="wpt-metadata/mapper/headless"
else
  readonly WPT_METADATA="wpt-metadata/mapper/headful"
fi

log "Running WPT in headless=$HEADLESS mode..."

# Go to the project root folder.
(cd "$(dirname "$0")/" && \
./wpt/wpt run \
  --binary "$WPT_BROWSER_PATH" \
  --webdriver-binary runBiDiServer.sh \
  --webdriver-arg="--headless=$HEADLESS" \
  --log-wptreport "$WPT_REPORT" \
  --manifest "$MANIFEST" \
  --metadata "$WPT_METADATA" \
  "$PRODUCT" \
  "$@")

if [[ "$UPDATE_EXPECTATIONS" == "true" ]]; then
  log "Updating WPT expectations..."

  ./wpt/wpt update-expectations \
    --product "$PRODUCT" \
    --manifest "$MANIFEST" \
    --metadata "$WPT_METADATA" \
    "$WPT_REPORT"
fi

# Alternate usage:
#
# Run with locally built Chromium and Chromedriver, after
# `autoninja -C out/Default chrome chromedriver`:
#
#   --binary "$WPT_BROWSER_PATH" \
#   --webdriver-binary /path/to/chromium/src/out/Default/chromedriver \
#   --webdriver-arg="--bidi-mapper-path=$PWD/lib/iife/mapperTab.js" \
