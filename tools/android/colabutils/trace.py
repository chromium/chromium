# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pandas as pd
import io
import contextlib
import asyncio
import signal

from . import command_line


class TraceFile:
    """A class to represent and query a recorded trace file."""

    def __init__(self, trace_file):
        """Initializes the TraceFile object with the path to the trace file.

        Args:
            trace_file: The path to the recorded trace file.
        """
        self.trace_file = trace_file

    async def query(self, query):
        """Queries the trace file using the trace_processor tool.

        Args:
            query: The SQL query string to execute on the trace.

        Returns:
            The result of the query as a pandas dataframe.
        """
        result = await command_line.run(
            "third_party/perfetto/tools/trace_processor", self.trace_file,
            "-Q", query)
        # Use io.StringIO to treat the string as a file and read it with pandas
        return pd.read_csv(io.StringIO(result.stdout))

    @contextlib.asynccontextmanager
    async def record(self, config_file):
        """Records a trace from the configured device using the specified
        config.

        Args:
            config_file: The config file to use for recording.
        """
        recording_task = asyncio.create_task(
            command_line.run("third_party/perfetto/tools/record_android_trace",
                             "-o",
                             self.trace_file,
                             "-c",
                             config_file,
                             "-n",
                             interruption_signal=signal.SIGINT))
        if recording_task.done():
            Exception(f"Recording failed: {recording_task.result()}")

        try:
            yield
        finally:
            recording_task.cancel()
            try:
                await recording_task
            except asyncio.CancelledError:
                pass
