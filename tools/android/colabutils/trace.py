# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import uuid
import os
import pandas as pd
import io

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


async def record(config, out_dir="/tmp"):
    """Records a trace from the configured device using the specified config.

    Args:
        config: The config file to use for recording.

    Returns:
        A TraceFile object if recording is successful, None otherwise.
    """
    trace_uuid = uuid.uuid4()
    trace_config_filename = os.path.join(out_dir, f"config_{trace_uuid}.pbtxt")
    with open(trace_config_filename, 'w') as file:
        file.write(config)

    trace_output_filename = os.path.join(out_dir, f"trace_{trace_uuid}.pb")

    await command_line.run("third_party/perfetto/tools/record_android_trace",
                           "-o", trace_output_filename, "-c",
                           trace_config_filename, "-n")
    return TraceFile(trace_output_filename)
