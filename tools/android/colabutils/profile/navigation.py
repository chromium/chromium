# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import asyncio
import os
import tempfile
from contextlib import ExitStack

from .. import trace


async def measure_fcp(app, url, trace_file=None, trace_config=None):
    """Measures FCP to navigate to `url` by checking the value recorded into
    `PageLoad.PaintTiming.NavigationToFirstContentfulPaint`

    See `tools/metrics/histograms/metadata/page/histograms.xml` for more
    details about when the metric is recorded.

    This function performs a navigation to `url` while recording a trace and
    then queries the trace for the histogram value. The navigation starts from
    the currently open page.

    Args:
        app: The chrome.App object with which to start and stop chrome
        url: The URL to navigate to.
        trace_file: An optional `trace.TraceFile` object to store the trace.
                    If not provided, a temporary file will be created.
        trace_config: An optional path to a Perfetto trace config file.
                      If not provided, a default config is used.

    Returns:
        The FCP time in milliseconds.
    """
    if trace_config is None:
        # This assumes the current working directory is the chromium src root.
        trace_config = os.path.join(os.getcwd(), "tools", "android",
                                    "colabutils", "res", "fcp_trace_cfg.pbtxt")

    if not os.path.exists(trace_config):
        raise FileNotFoundError(f"Trace config not found at {trace_config}")

    with ExitStack() as stack:
        # If no trace file is provided, create a temporary one that will be
        # cleaned up upon exiting the context.
        if trace_file is None:
            temporary_recorded_trace = stack.enter_context(
                tempfile.NamedTemporaryFile(mode='w'))
            trace_file = trace.TraceFile(temporary_recorded_trace.name)

        return await _measure_fcp(app, url, trace_file, trace_config)


async def _measure_fcp(app, url, trace_file, trace_config):
    async with trace_file.record(trace_config):
        # Allow Perfetto to start recording the trace
        await asyncio.sleep(2)

        await app.start(url=url)
        await asyncio.sleep(10)

    df = await trace_file.query(_FCP_TIME_QUERY)
    try:
        return int(df.iloc[0, 0])
    except IndexError:
        raise LookupError


_FCP_TIME_QUERY = r"""
INCLUDE PERFETTO MODULE slices.with_context;

SELECT
-- Select the display_value from the second join to the args table.
-- This will be the value for 'chrome_histogram_sample.sample'.
args_sample.display_value AS sample_value
FROM
thread_or_process_slice AS slice
-- First join to args table to FIND the event by its name.
LEFT JOIN
args AS args_name ON slice.arg_set_id = args_name.arg_set_id
-- Second join to the same args table to GET the sample value from that event.
LEFT JOIN
args AS args_sample ON slice.arg_set_id = args_sample.arg_set_id
WHERE
-- Use the first join to filter for the specific event name.
args_name.display_value = 'PageLoad.PaintTiming.NavigationToFirstContentfulPaint'
AND args_name.key = 'chrome_histogram_sample.name'
-- Use the second join to specify which key's value you want to select.
AND args_sample.key = 'chrome_histogram_sample.sample'
"""
