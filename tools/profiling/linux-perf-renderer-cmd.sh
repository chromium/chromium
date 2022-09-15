#!/bin/bash
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PERF_DATA_DIR="."
PERF_DATA_PREFIX="chrome_renderer"
RENDERER_ID="0"
PERF_RECORD_FREQ="max"

for i in "$@"; do
  case $i in
    --help)
cat <<EOM
Usage: path/to/chrome --renderer-cmd-prefix='${0} [OPTION]' [CHROME OPTIONS]

This script is used  to run linux-perf for each renderer process.
It generates perf.data files that can be read by pprof or linux-perf.

Output: \$OUT_DIR/\$PREFIX_\$PPID_\$RENDERER_ID.perf.data

Options:
  --perf-data-dir=OUT_DIR    Change the location where perf.data is written.
                             Default: '${PERF_DATA_DIR}'
  --perf-data-prefix=PREFIX  Set a custom prefix for all generated perf.data
                             files.
                             Default: '${PERF_DATA_PREFIX}'
  --perf-freq=FREQ           Sets the sampling frequency:
                               'perf record --freq=FREQ'
                             Default: '${PERF_RECORD_FREQ}'
EOM
      exit
      ;;
    --perf-data-dir=*)
      PERF_DATA_DIR="${i#*=}"
      shift
    ;;
    --perf-data-prefix=*)
      PERF_DATA_PREFIX="${i#*=}"
      shift
    ;;
    --perf-freq=*)
      PERF_RECORD_FREQ="${i#*=}"
      shift
    ;;
    --renderer-client-id=*)
      # Don't shift this option since it is passed in (and used by) chrome.
      RENDERER_ID="${i#*=}"
    ;;
    *)
      ;;
  esac
done

PERF_OUTPUT="$PERF_DATA_DIR/${PERF_DATA_PREFIX}_${PPID}_${RENDERER_ID}.perf.data"

perf record \
  --call-graph=fp --clockid=mono --freq="${PERF_RECORD_FREQ}" \
  --output="${PERF_OUTPUT}" \
  -- $@
