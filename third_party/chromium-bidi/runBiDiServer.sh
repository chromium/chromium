#!/bin/bash
# shellcheck disable=SC2155

set -euo pipefail

log() {
  echo "($0) $*"
}

usage() {
  log "Usage: [CHANNEL=<'' | chrome | chrome-beta | chrome-canary | chrome-dev>] [DEBUG=*] [HEADLESS=<true | false>] [PORT=8080] $0 [--headless=<true | false>]"
  log
  log "The --headless flag takes precedence over the HEADLESS environment variable."
}

if [[ $# -gt 0 && ("$1" == "-h" || "$1" == "--help") ]]; then
  usage
  exit 0
fi

# The chrome release channel to use: ``, `chrome`, `chrome-beta`, `chrome-canary`, `chrome-dev`.
readonly CHANNEL="${CHANNEL:-chrome-dev}"

# Options from npm 'debug' package.
readonly DEBUG="${DEBUG:-*}"
readonly DEBUG_COLORS="${DEBUG_COLORS:-yes}"

# Whether to start the server in headless or headful mode.
readonly HEADLESS="${HEADLESS:-true}"

# The directory wherein to store logs.
readonly LOG_DIR="${LOG_DIR:-logs}"

# The file to which to write BiDi logs.
readonly LOG_FILE="$LOG_DIR/$(date +%s).log"

# Node.JS options
readonly NODE_OPTIONS="${NODE_OPTIONS:---unhandled-rejections=strict}"

# The port on which to start the BiDi server.
readonly PORT="${PORT:-8080}"

log "Starting BiDi Server..."

# Go to the project root folder.
(cd "$(dirname "$0")/" && \
# Create a folder to store logs.
mkdir -p "$LOG_DIR" && \
env \
CHANNEL="$CHANNEL" \
DEBUG="$DEBUG" \
DEBUG_COLORS="$DEBUG_COLORS" \
NODE_OPTIONS="$NODE_OPTIONS" \
PORT="$PORT" \
npm run server-no-build -- --headless="$HEADLESS" "$@" 2>&1 | tee -a "$LOG_FILE")
