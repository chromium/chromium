#!/bin/bash
# shellcheck disable=SC2155

set -uo pipefail

trap ctrl_c INT
ctrl_c() {
    exit 1
}

export LOG_FILE="logs/$(basename "$0").log"
export UPDATE_EXPECTATIONS="${UPDATE_EXPECTATIONS:-true}"

if [[ -n "$*" ]]; then
    for test in "$@"; do
        CHROMEDRIVER=true HEADLESS=false npm run wpt -- "$test"
        CHROMEDRIVER=true HEADLESS=true npm run wpt -- "$test"
        HEADLESS=false npm run wpt -- "$test"
        HEADLESS=true npm run wpt -- "$test"
    done
else
    CHROMEDRIVER=true HEADLESS=false npm run wpt
    CHROMEDRIVER=true HEADLESS=true npm run wpt
    HEADLESS=false npm run wpt
    HEADLESS=true npm run wpt
fi
