#!/bin/sh

# Go to the project root folder.
cd "$(dirname $0)/"

# Crate a folder to store logs.
mkdir -p logs

NODE_OPTIONS="--unhandled-rejections=strict" \
DEBUG=* \
CHANNEL=chrome-dev \
npm run server-no-build -- "$@" 2>&1 | tee -a logs/$$.log
