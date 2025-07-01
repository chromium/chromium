#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
from collections import defaultdict
from pathlib import Path

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
MOJO_EVENTS_QUERY = '''
INCLUDE PERFETTO MODULE slices.with_context;
SELECT
  (ts - (SELECT start_ts FROM trace_bounds)) / 1000000000.0 AS ts_delta,
  process_name,
  pid,
  thread_name,
  name,
  category AS event_category,
  GROUP_CONCAT(args.key || ": " ||
               COALESCE(args.int_value,
                        args.string_value,
                        args.real_value)) AS parameters
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
    if not Path(path).is_file():
        parser.error(f"Invalid path: {path}")
    return path


def process_mojo_msg_info(extra, spacing=2):
    if not extra or len(extra) != len(ADDITIONAL_DATA_FIELDS):
        return None

    event_name, event_category, parameters = extra
    spacer = ' ' * spacing
    output = ''

    if (event_category == 'mojom' and event_name.startswith("Send ")) or \
       (event_category == 'mojom' and event_name.startswith("Call ")):
        if parameters:
            if parameters.startswith('debug.'):
                parameters = parameters.replace('debug.', '', 1)
                parameters = parameters.split(',debug.')
            else:
                parameters = [parameters]
        else:
            parameters = []

    elif (event_category == 'toplevel' and event_name.startswith("Receive ")) or \
         (event_category == 'toplevel' and event_name == "Closed mojo endpoint"):
        if parameters:
            if parameters.startswith('chrome_mojo_event_info.'):
                parameters = parameters.replace('chrome_mojo_event_info.', '', 1)
                parameters = parameters.split(',chrome_mojo_event_info.')
                parameters = ['chrome_mojo_event_info.' + x for x in parameters]
            elif parameters.startswith('args.'):
                parameters = parameters.replace('args.', '', 1)
                parameters = parameters.split(',args.')
            else:
                parameters = [parameters]
        else:
            parameters = []

    results = defaultdict(list)
    for parameter in parameters:
        if '.' not in parameter:
            continue
        info_type, info = parameter.split('.', 1)
        results[info_type].append(info)

    for info_type, entries in results.items():
        output += f"{spacer}{info_type}:\n"
        for entry in entries:
            output += f"{spacer*2}{entry}\n"

    return output


def process_events(args, events):
    rows, extras = [], []
    fields = SUMMARY_FIELDS if args.summary else VERBOSE_FIELDS

    for row_data in events:
        row = [str(getattr(row_data, field)) for field in fields]
        rows.append(row)

        if not args.summary:
            extra_data = [getattr(row_data, f) for f in ADDITIONAL_DATA_FIELDS]
            extras.append(process_mojo_msg_info(extra_data))
        else:
            extras.append(None)

    # Add headers
    rows.insert(0, fields)
    extras.insert(0, None)

    return rows, extras


def print_rows(rows, extras, output_file=None):
    widths = [max(map(len, col)) for col in zip(*rows)]

    output_lines = []
    for i, row in enumerate(rows):
        line = "  ".join(value.ljust(width) for value, width in zip(row, widths)).rstrip()
        output_lines.append(line)
        if extras[i]:
            output_lines.append(extras[i])

    full_output = "\n".join(output_lines)
    if output_file:
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(full_output + '\n')
    else:
        print(full_output)


def main():
    try:
        from perfetto.trace_processor import TraceProcessor
    except ModuleNotFoundError:
        print(PERFETTO_NOT_FOUND_HELP_TEXT)
        sys.exit(1)

    import argparse
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=DESCRIPTION
    )
    parser.add_argument('tracefile', type=lambda p: is_valid_path(parser, p))
    parser.add_argument('--summary', action='store_true', help='Show summary output (default is verbose)')
    parser.add_argument('-o', '--output', help='Optional output file to save the results')

    args = parser.parse_args()

    tp = TraceProcessor(file_path=args.tracefile)
    events = tp.query(MOJO_EVENTS_QUERY)
    rows, extras = process_events(args, events)
    print_rows(rows, extras, output_file=args.output)


if __name__ == '__main__':
    sys.exit(main())
