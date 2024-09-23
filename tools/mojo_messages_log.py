#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
from collections import defaultdict

DESCRIPTION = \
'''This script takes in a Chromium trace file and extracts info about Mojo
messages that were sent/received.

Trace files can be created using chrome://tracing or from passing
'--enable-tracing' to a Chrome or browser test executable. In the
chrome://tracing UI, ensure that the 'mojom' and 'toplevel' categories are
selected when setting up a new trace. Also, the trace events available will
have much more information (including message contents and return values) if
the executable generating the trace file is built with the
`extended_tracing_enabled = true` gn arg.
'''

PERFETTO_NOT_FOUND_HELP_TEXT = \
'''Error: perfetto module not found.

This script requires the perfetto Python module. To install it, use something
like `pip install perfetto`, or for Googlers on gLinux use the following (in a
Chromium checkout):
```
sudo apt-get install python3-venv
python3 -m venv venv
./venv/bin/python3 -mpip install perfetto
./venv/bin/python3 tools/mojo_messages_log.py <script args>
```
'''

# Note: Ignore 'mojo::Message::Message' (from the disabled by default 'mojom'
# category) because there is usually higher-level information that's more
# helpful, even in release builds.

# TODO(awillia): The 'Send mojo message' and 'Receive mojo sync reply' trace
# events (both from the toplevel.flow category) should have a message ID
# associated with them but I'm not sure how to access it. With the former we
# could figure out the sender of a message, but without the message ID the
# events aren't very helpful.
MOJO_EVENTS_QUERY = \
'''INCLUDE PERFETTO MODULE slices.with_context;
SELECT
  (ts - (SELECT start_ts FROM trace_bounds)) / 1000000000.0 AS ts_delta,
  process_name,
  pid, -- Useful for distinguishing renderer processes
  thread_name,
  name,
  category AS event_category,
  GROUP_CONCAT(args.key || ": " ||
               COALESCE(args.int_value,
                        args.string_value,
                        args.real_value)) AS parameters
  -- Note that we could get argument type info as well if that's worthwhile
  FROM thread_slice
  LEFT JOIN args on args.arg_set_id = thread_slice.arg_set_id
  WHERE (category IS 'mojom' AND name GLOB 'Send *') OR
        (category IS 'mojom' AND name GLOB 'Call *') OR
        (category IS 'toplevel' AND name GLOB 'Receive *') OR
        (category IS 'toplevel' AND name IS 'Closed mojo endpoint')
  GROUP BY thread_slice.id, args.arg_set_id
  ORDER BY ts;
'''

SUMMARY_FIELDS = ['ts_delta', 'process_name', 'name']

VERBOSE_FIELDS = ['ts_delta', 'process_name', 'pid', 'thread_name', 'name']
ADDITIONAL_DATA_FIELDS = ['name', 'event_category', 'parameters']


def is_valid_path(parser, path):
  if not os.path.exists(path):
    parser.error("Invalid path: %s" % (path))
  else:
    return path


def process_mojo_msg_info(extra, spacing=2):
  if not extra or len(extra) != len(ADDITIONAL_DATA_FIELDS):
    return
  output = ''
  spacer = ' ' * spacing
  event_name, event_category, parameters = extra

  # The parameters exist as a single comma separated line, so break it into
  # separate lines. Each if statement block here corresponds to a WHERE
  # condition in the SQL query.
  if (event_category == 'mojom' and event_name.startswith("Send ")) or \
     (event_category == 'mojom' and event_name.startswith("Call ")):
    if parameters is None:
      # The call has no parameters
      parameters = []
    else:
      assert (parameters.startswith('debug.'))
      parameters = parameters.replace('debug.', '', 1)
      parameters = parameters.split(',debug.')

  elif (event_category == 'toplevel' and event_name.startswith("Receive ")) or \
       (event_category == 'toplevel' and event_name == "Closed mojo endpoint"):
    if parameters is None:
      parameters = []
    elif parameters.startswith('chrome_mojo_event_info.'):
      parameters = parameters.replace('chrome_mojo_event_info.', '', 1)
      parameters = parameters.split(',chrome_mojo_event_info.')
      parameters = ['chrome_mojo_event_info.' + x for x in parameters]
    else:
      assert (parameters.startswith('args.'))
      parameters = parameters.replace('args.', '', 1)
      parameters = parameters.split(',args.')

  results = defaultdict(lambda: [])
  for parameter in parameters:
    info_type, info = parameter.split('.', 1)
    results[info_type].append(info)

  for info_type in results:
    output += spacer + info_type + ':\n'
    for entry in results[info_type]:
      output += spacer * 2 + entry + '\n'
  return output


# Formats the event data into the structured data that can be shown in the
# displayed table and additional unstructured data that should be shown
# underneath each event.
def process_events(args, events):
  rows = []
  extras = []
  for row_data in events:
    row = []
    extra = []
    if args.summary:
      for field in SUMMARY_FIELDS:
        row.append(str(getattr(row_data, field)))
    else:
      for field in VERBOSE_FIELDS:
        row.append(str(getattr(row_data, field)))

      for field in ADDITIONAL_DATA_FIELDS:
        extra.append(getattr(row_data, field))
      extra = process_mojo_msg_info(extra)
    rows.append(row)
    extras.append(extra)
  return rows, extras


try:
  from perfetto.trace_processor import TraceProcessor
except ModuleNotFoundError:
  print(PERFETTO_NOT_FOUND_HELP_TEXT)
  sys.exit(1)


def main():
  import argparse
  parser = argparse.ArgumentParser(
      formatter_class=argparse.RawDescriptionHelpFormatter,
      description=DESCRIPTION)
  parser.add_argument('tracefile',
                      type=lambda path: is_valid_path(parser, path))
  parser.add_argument('--summary', action="store_true")
  args = parser.parse_args()

  tp = TraceProcessor(file_path=args.tracefile)

  results = tp.query(MOJO_EVENTS_QUERY)

  rows, extras = process_events(args, results)

  # Add headers for the table.
  if args.summary:
    rows.insert(0, SUMMARY_FIELDS)
  else:
    rows.insert(0, VERBOSE_FIELDS)
  # Keep `extras` the same length as `rows`.
  extras.insert(0, None)

  # Calculate the appropriate widths of each column.
  widths = [max(map(len, column)) for column in zip(*rows)]

  for i in range(len(rows)):
    row = rows[i]
    extra = extras[i]
    # Format the structured data so the fields align with the table headers.
    out = (value.ljust(width) for value, width in zip(row, widths))
    out = "  ".join(out).rstrip()
    print(out)
    if extra:
      print(extra)


if __name__ == '__main__':
  sys.exit(main())
