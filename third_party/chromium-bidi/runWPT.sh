#!/bin/bash
# shellcheck disable=SC2155

set -uo pipefail

log() {
  echo "($0) $*"
}

usage() {
  log "Usage: [CHROMEDRIVER=<true | false>] [HEADLESS=<true | false>] [UPDATE_EXPECTATIONS=<true | false>] $0 [webdriver/tests/bidi/[...]]"
}

if [[ $# -gt 0 && ("$1" == "-h" || "$1" == "--help") ]]; then
  usage
  exit 0
fi

# The path to the browser binary.
readonly BROWSER_BIN="${BROWSER_BIN:-/Applications/Google Chrome Dev.app/Contents/MacOS/Google Chrome Dev}"

# Whether to use Chromedriver with mapper.
readonly CHROMEDRIVER="${CHROMEDRIVER:-false}"

# Whether to start the server in headless or headful mode.
readonly HEADLESS="${HEADLESS:-true}"

# The path to the WPT manifest file.
readonly MANIFEST="${MANIFEST:-MANIFEST.json}"

# The browser product to test.
readonly PRODUCT="${PRODUCT:-chrome}"

# Multiplier relative to standard test timeout to use.
readonly TIMEOUT_MULTIPLIER="${TIMEOUT_MULTIPLIER:-8}"

# Whether to update the WPT expectations after running the tests.
readonly UPDATE_EXPECTATIONS="${UPDATE_EXPECTATIONS:-false}"

# The path to the WPT report file.
readonly WPT_REPORT="${WPT_REPORT:-wptreport.json}"

# Only set WPT_METADATA if it's not already set.
if [ -z ${WPT_METADATA+x} ]; then
  # The path to the WPT metadata directory.
  if [[ "$CHROMEDRIVER" == "true" ]]; then
    readonly WPT_METADATA="wpt-metadata/chromedriver/headless"
  else
    if [[ "$HEADLESS" == "true" ]]; then
      readonly WPT_METADATA="wpt-metadata/mapper/headless"
    else
      readonly WPT_METADATA="wpt-metadata/mapper/headful"
    fi
  fi

fi

if [[ "$HEADLESS" == "true" ]]; then
  log "Running WPT in headless mode..."
else
  log "Running WPT in headful mode..."
fi

declare -a WPT_RUN_ARGS=(
  --binary "$BROWSER_BIN"
  --webdriver-binary runBiDiServer.sh
  --webdriver-arg="--headless=$HEADLESS"
  --log-wptreport "$WPT_REPORT"
  --manifest "$MANIFEST"
  --metadata "$WPT_METADATA"
  --timeout-multiplier "$TIMEOUT_MULTIPLIER"
)

if [[ "$CHROMEDRIVER" == "true" ]]; then
  log "Using Chromedriver with mapper..."
  WPT_RUN_ARGS+=(
    --binary-arg="--headless=new"
    --install-webdriver
    --webdriver-arg="--bidi-mapper-path=lib/iife/mapperTab.js"
    --webdriver-arg="--log-path=out/chromedriver.log"
    --webdriver-arg="--verbose"
    --channel=dev
    --yes
  )
else
  log "Using pure mapper..."
fi

WPT_RUN_ARGS+=(
  "$PRODUCT"
  "$@"
)

(cd "$(dirname "$0")/" && ./wpt/wpt run --skip-implementation-status=backlog "${WPT_RUN_ARGS[@]}")
status="$?"

if [[ "$UPDATE_EXPECTATIONS" == "true" ]]; then
  log "Updating WPT expectations..."

  ./wpt/wpt update-expectations \
    --product "$PRODUCT" \
    --manifest "$MANIFEST" \
    --metadata "$WPT_METADATA" \
    "$WPT_REPORT"
fi

# Script status should match "wpt run" for CI.
exit "$status"

# Alternate usage:
#
# Run with locally built Chromium and Chromedriver, after
# `autoninja -C out/Default chrome chromedriver`:
#
#   --binary "$BROWSER_BIN" \
#   --webdriver-binary /path/to/chromium/src/out/Default/chromedriver \
#   --webdriver-arg="--bidi-mapper-path=lib/iife/mapperTab.js" \
