#!/bin/bash
# shellcheck disable=SC2155

set -euo pipefail

log() {
  echo "($0) $*"
}

usage() {
  log "Usage: [CHANNEL=<stable | beta | canary | dev>] [DEBUG=*] [HEADLESS=<true | false>] [PORT=8080] $0 [--headless=<true | false>]"
  log
  log "The --headless flag takes precedence over the HEADLESS environment variable."
}

if [[ $# -gt 0 && ("$1" == "-h" || "$1" == "--help") ]]; then
  usage
  exit 0
fi

# The chrome release channel to use: `stable`, `beta`, `canary`, `dev`.
readonly CHANNEL="${CHANNEL:-dev}"

# Options from npm 'debug' package.
readonly DEBUG="${DEBUG:-*}"
readonly DEBUG_COLORS="${DEBUG_COLORS:-yes}"

# Whether to start the server in headless or headful mode.
readonly HEADLESS="${HEADLESS:-true}"

# The directory wherein to store logs.
readonly LOG_DIR="${LOG_DIR:-logs}"

# The file to which to write BiDi logs.
readonly LOG_FILE="$LOG_DIR/$(date '+%Y-%m-%d-%H-%M-%S').log"

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
DEBUG="$DEBUG" \
DEBUG_COLORS="$DEBUG_COLORS" \
NODE_OPTIONS="$NODE_OPTIONS" \
PORT="$PORT" \
node lib/cjs/bidiServer/index.js --channel="$CHANNEL" --headless="$HEADLESS" "$@" 2>&1 | tee -a "$LOG_FILE")
