# Colab Utilities for Android

This directory contains a collection of Python utilities designed to facilitate
Android performance analysis, particularly within a Colab or Jupyter notebook
environment. These tools provide a programmatic interface for controlling Chrome
on a connected Android device, recording Perfetto traces, and analyzing the
collected data.

**Googlers-only:** go/chrome-colabutils-example contains a Colab notebook with
examples of how to use colabutils.

## Making colabutils available to the python runtime
The `colabutils` module assumes that the notebook's current working directory is
the Chromium source directory. With that in mind, the `colabutils` module
requires the following code snippet to be run to be make available to the python
runtime for importing:

```python
import sys
import os

colabutils_path = os.path.abspath(os.path.join(os.getcwd(), 'tools', 'android'))
if colabutils_path not in sys.path:
    sys.path.append(colabutils_path)
```

## Example: Record a perfetto trace of cold launch of Chrome
Create a custom configuration using [Perfetto
UI](https://ui.perfetto.dev/#!/record/cmdline) and then save it as a file that
can be accessed by the python notebook.

Then, use asyncio to concurrently launch Chrome and record a trace.
```python
import asyncio
from colabutils import chrome, trace

# Kill the current chrome instance
app = chrome.App(chrome.Channel.STABLE)
await app.stop()

# Use asyncio.gather with asyncio.create_task to run the two async functions
# concurrently
recorded_trace = trace.TraceFile("/tmp/recorded_trace.pftrace")
async with recorded_trace.record():
  await asyncio.sleep(2) # wait for trace recording to start
  await app.start(url="https://chromium.org")
```

Once you have a trace file, you can use the `perfetto_ui.open_trace` function to
immediately open and visualize it in the Perfetto UI.
```python
# Opens the trace on https://ui.perfetto.dev
from colabutils import perfetto_ui
perfetto_ui.open_trace(recorded_trace)
```

Furthermore, you can also programmatically query the trace data using
PerfettoSQL.
```python
# PerfettoSQL query to search for slices with the name
# RenderFrameHostImpl::DidCommitNavigation
perfetto_query = r"""
    INCLUDE PERFETTO MODULE slices.with_context;

    SELECT
      thread_or_process_slice.id AS id,
      thread_or_process_slice.ts AS ts,
      thread_or_process_slice.dur AS dur,
      thread_or_process_slice.category AS category,
      thread_or_process_slice.name AS name,
      thread_or_process_slice.utid AS utid,
      thread_1.tid AS thread__utid____tid,
      thread_1.name AS thread__utid____name,
      thread_or_process_slice.upid AS upid,
      process_2.pid AS process__upid____pid,
      process_2.name AS process__upid____name,
      thread_or_process_slice.parent_id AS parent_id
    FROM _viz_slices_for_ui_table AS thread_or_process_slice
    LEFT JOIN thread AS thread_1 ON thread_1.id = thread_or_process_slice.utid
    LEFT JOIN process AS process_2 ON process_2.id = thread_or_process_slice.upid
    WHERE
      thread_or_process_slice.name = 'RenderFrameHostImpl::DidCommitNavigation'
"""

import pandas as pd

# The query result is returned as a pandas dataframe. So the the total duration
# can be calculated from the 'dur' column.
df = await recorded_trace.query(perfetto_query)
total_duration_ms = df['dur'].sum() / 1_000_000
print(f"Total time spent in RenderFrameHostImpl::DidCommitNavigation = {total_duration_ms} milliseconds")
```

## Example: Using Web Page Replay
Web Page Replay (WPR) is a tool for capturing and replaying network traffic.
This is useful for eliminating network variability. WPR can be integrated with
other automation as shown below:
```python
# Create the archive file
wpr_archive = wpr.WebPageReplayArchive(f"/tmp/wpr_archive_{session_id}.wprgo")

# Record into the archive file
async with wpr_archive.record():
  # Run an automation that simulates the network traffic expected during replay
  await app.stop()
  await app.start(url="https://chromium.org")
  await asyncio.sleep(10)
  await app.stop()

# Replay the archive file
async with wpr_archive.replay():
  # Run the test automation while network traffic is replayed. This example
  # extends the previous example which records a cold launch of Chrome.
  recorded_trace = trace.TraceFile("/tmp/recorded_trace.pftrace")
  async with recorded_trace.record():
    await asyncio.sleep(2) # wait for trace recording to start
    await app.start(url="https://chromium.org")
```

## Example: Collect and visualize a heap snapshot
This example is under development. It will describe how to collect a Perfetto
trace with a heap dump, symbolize it, and visualize in a Colab notebook.

To set up the build environment follow [instructions for heaptrack](https://chromium.googlesource.com/chromium/src/+/main/docs/memory/heap_profiling_external.md#heaptrack).
In particular, it is essential to replace PartitionAlloc with the system
allocator via these GN flags:
```
forward_through_malloc = true
is_component_build = true
use_partition_alloc_as_malloc = false
enable_backup_ref_ptr_support = false
```

After starting the browser on the device, use the `heap_profile` tool from the
Perfetto repository to obtain a `symbolized-trace`.
```bash
DURATION_MS=30000
CONTINUOUS_DUMP_INTERVAL_MS=500
PERFETTO_REPO_PATH=/path/to/perfetto
$PERFETTO_REPO_PATH/tools/heap_profile -d "$DURATION_MS" --name org.chromium.chrome --continuous-dump "$CONTINUOUS_DUMP_INTERVAL_MS"
RAW_TRACE="$(mktemp)"
PERFETTO_BINARY_PATH=out/$OUTDIR/lib.unstripped $PERFETTO_REPO_PATH/tools/traceconv symbolize "$RAW_TRACE" >logs/symbols
cat "$RAW_TRACE" logs/symbols >symbolized-trace
rm "$RAW_TRACE"
```

The `MemoryUsageView` can load this trace like so:
```python
from colabutils.memory_usage import MemoryUsageView

view = MemoryUsageView.from_heap_dump('symbolized-trace')
view.toplevel_names()
view.display()
```

View a diff between two memory usage views:
```python
from colabutils.memory_usage import MemoryUsageView

view1 = MemoryUsageView.from_heap_dump('symbolized-trace1')
view2 = MemoryUsageView.from_heap_dump('symbolized-trace2')
diff_view = MemoryUsageView.from_comparison(view, view2)
diff_view.display()
```
