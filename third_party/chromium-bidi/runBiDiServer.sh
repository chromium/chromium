#!/bin/bash
# shellcheck disable=SC2155

set -euo pipefail

log() {
  echo "($0) $*"
}

usage() {
  log "Usage: [CHANNEL=<stable | beta | canary | dev>] [DEBUG=*] [DEBUG_COLORS=<yes | no>] [HEADLESS=<true | false>] [LOG_DIR=logs] [NODE_OPTIONS=--unhandled-rejections=strict] [PORT=8080] $0 [--headless=<true | false>]"
  log
  log "The --headless flag takes precedence over the HEADLESS environment variable."
}

if [[ $# -gt 0 && ("$1" == "-h" || "$1" == "--help") ]]; then
  usage
  exit 0
fi

# The chrome release channel to use: `stable`, `beta`, `canary`, `dev`.
# `local` means to use the browser version specified in `.browser`.
CHANNEL="${CHANNEL:-local}"

if [ "$CHANNEL" == "local" ]; then
  export BROWSER_BIN="$(node install-browser.mjs --shell)"
  # Need to pass valid CHANNEL to the command below
  CHANNEL='canary'
fi

# Options from npm 'debug' package. DEBUG= (empty) is allowed, hence no colon below.
readonly DEBUG="${DEBUG-*}"
readonly DEBUG_COLORS="${DEBUG_COLORS:-yes}"

# Whether to start the server in headless or headful mode.
readonly HEADLESS="${HEADLESS:-true}"

# The directory and file wherein to store BiDi logs.
readonly LOG_DIR="${LOG_DIR:-logs}"
readonly LOG_FILE="${LOG_FILE:-$LOG_DIR/$(date '+%Y-%m-%d-%H-%M-%S').log}"

# Node.JS options
readonly NODE_OPTIONS="${NODE_OPTIONS:---unhandled-rejections=strict}"

# The port on which to start the BiDi server.
readonly PORT="${PORT:-8080}"

log "Starting BiDi Server with DEBUG='$DEBUG'..."

(cd "$(dirname "$0")/" && \
mkdir -p "$LOG_DIR" && \
env \
DEBUG="$DEBUG" \
DEBUG_COLORS="$DEBUG_COLORS" \
NODE_OPTIONS="$NODE_OPTIONS" \
PORT="$PORT" \
node lib/cjs/bidiServer/index.js --channel "$CHANNEL" --headless "$HEADLESS" "$@" 2>&1 | tee -a "$LOG_FILE")
