#!/bin/sh

readonly LOG_DIR="logs"

# Go to the project root folder.
(cd "$(dirname $0)/" && \
# Create a folder to store logs.
mkdir -p "$LOG_DIR" && \
NODE_OPTIONS="--unhandled-rejections=strict" \
DEBUG=* \
CHANNEL=chrome-dev \
npm run server-no-build -- "$@" 2>&1 | tee -a "$LOG_DIR/$(date +%s).log")
