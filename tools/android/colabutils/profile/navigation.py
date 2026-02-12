# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import asyncio
import os
import tempfile
from contextlib import ExitStack

from .. import trace

__FCP_HISTOGRAM_NAME = "PageLoad.PaintTiming.NavigationToFirstContentfulPaint"


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
    with ExitStack() as stack:
        if trace_config is None:
            trace_config = stack.enter_context(
                trace.histograms_trace_config(__FCP_HISTOGRAM_NAME))

        if not os.path.exists(trace_config):
            raise FileNotFoundError(
                f"Trace config not found at {trace_config}")

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

    df = await trace_file.query(
        trace.histogram_values_query(__FCP_HISTOGRAM_NAME))
    try:
        return int(df.iloc[0, 0])
    except IndexError:
        raise LookupError
