#!/usr/bin/env python3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
import json
import os
import re
import sys
import tempfile
"""
This script parses all the log files in a directory, looking for instances
of a particular warning. It collects all the ones it finds, and writes the
results to an output file, recording which files had warnings, and the
location(s) in each file. It also counts the total number of files/warnings.

It can be configured to either print a (somewhat) human-readable list of files
and locations, or a more structured json for automatic processing.

See README.md in this directory for more details.
"""


def parse_args(args):
    """
    Parse commandline flags. Possible options:

    Configuration options:
    log_dir :     The directory containing the log files to scrape
    output :      Where the collected warning information should go. Either the
                  string "stdout" (case-insensitive) or a path to a file.
    warning_text: The text in the log indicating a warning was raised
    summarize:    If True, we output a human-readable summary.
                  Otherwise, we output a json with more information
    """
    parser = argparse.ArgumentParser(description=__doc__,)
    parser.add_argument("-l",
                        "--log-dir",
                        required=True,
                        type=str,
                        help="Path to the directory containing the build logs.")
    parser.add_argument("-o",
                        "--output",
                        type=str,
                        help="Where the collected warning information should "
                        "go. This should be either the string 'stdout', a dash "
                        "(also meaning stdout), or a path to a file.\n"
                        "ex. -o out.txt, -o stdout, -o -")
    parser.add_argument("-w",
                        "--warning",
                        type=str,
                        required=True,
                        help="Text indicating the warning of interest. "
                        "Should appear at the end of a line containing the "
                        "filename and warning location.\n"
                        "ex. -w [-Wthread-safety-reference-return]")
    parser.add_argument(
        "-s",
        "--summarize",
        action="store_true",
        help="If present, output a (somewhat) human-readable text file "
        "cataloguing the warnings. Otherwise, output a json file "
        "with more detailed information about each instance.")

    parsed_args = vars(parser.parse_args(args))

    return parsed_args


_TARGET_RE = re.compile('([^:(]+)(?:[:(])([0-9]+)(?::|, ?)([0-9]+)\)?:')


def extract_warning_location(line):
    """
    Given a line of the build log indicating that a warning has occurred,
    extract the file name and position of the warning (line # + col #).
    """
    # Matches:
    # |/path/to/file(123, 45):...|, for Windows
    # |/path/to/file:123:45:...|, elsewhere
    # Captures path, line number, and column number.
    match = _TARGET_RE.match(line)
    if not match:
        return None
    path, line, col = match.groups()
    return path, int(line), int(col)


def collect_warning(summarize, log_name, log_file, collection, warning_info):
    """
    Add information about a warning into our collection, avoiding
    duplicates and merging as necessary.

    `collection` is expected to be a dictionary mapping log file names to the
    warning info generated in the file (the empty list, by default).
    If we're summarizing, we just collect the line and column number of each
    warning.

    If we're not summarizing, we also store the name of the log file (so we know
    which systems the warning occurs on), and the next line of the log file
    (which contains the text of the line, in case line numbers change later.)
    """
    path, line_num, col_num = warning_info

    # If we're collecting a summary, we just need the line and column numbers
    if summarize:
        logged_info = line_num, col_num
        if logged_info not in collection[path]:
            # Haven't seen this particular warning before
            collection[path].append(logged_info)
        return

    # If we're not summarizing, we store extra info:
    # 1. The next (nonempty) line, and
    # 2. the name of the log that the warning occurred in
    next_line = next(log_file)
    while not (next_line.strip()):
        next_line = next(log_file)

    log_name = os.path.basename(log_name)
    logged_info = (line_num, col_num, next_line.split("|")[1].strip(),
                   [log_name])

    # Should be either a singleton or empty
    existing_info = [
        x for x in collection[path]
        if x[0] == logged_info[0] and x[1] == logged_info[1]
    ]

    if len(existing_info) == 0:
        # Haven't seen this particular warning before
        collection[path].append(logged_info)
        return

    # If the info's already in the list, then just note the name of the log file
    # It's possible for the same warning to appear multiple times in a file
    if log_name not in existing_info[0][3]:
        existing_info[0][3].append(log_name)
    return


def read_file(filename, warning_text, summarize, collection, failures):
    """
    Go through a single build log, collecting all the warnings that occurred and
    storing them in `collection`. Also keep track of any lines we tried to get
    information from but failed (this shouldn't happen).
    """
    with open(filename) as file:
        for line in file:
            if not line.rstrip().endswith(warning_text):
                continue

            warning_info = extract_warning_location(line)
            if not warning_info:
                builder_name, _ = os.path.splitext(os.path.basename(filename))
                failures.append("{}: {}".format(builder_name, line))
                continue

            collect_warning(summarize, filename, file, collection, warning_info)


def log_output(summarize, collection, output):
    """
    Write the results of the collection to the output.
    If a summary was requested, output a text summary.
    Otherwise, dump to json.
    """

    output_to_stdout = (output == "-" or output.lower() == "stdout")

    if not output:
        extension = ".txt" if summarize else ".json"
        output_file = tempfile.NamedTemporaryFile(mode="w",
                                                  prefix="collected_warnings_",
                                                  suffix=extension,
                                                  delete=False)
        print("Writing output to " + output_file.name)
    elif output_to_stdout:
        output_file = sys.stdout
    else:
        output_file = open(output, "w")
        print("Writing output to " + os.path.abspath(output))

    if not summarize:
        json.dump(collection, output_file, indent=2, sort_keys=True)
        return

    keys = list(collection.keys())
    hits = 0
    for key in sorted(keys):
        values = collection[key]
        hits += len(values)
        output_file.write("{} ({} hits): {}\n".format(key, str(len(values)),
                                                      str(values)))

    output_file.write("\nTotal Files: {}, Total Hits: {}".format(
        len(keys), hits))

    if not output or not output_to_stdout:
        output_file.close()


def main(args):
    parsed_args = parse_args(args)
    log_files = [
        os.path.join(parsed_args["log_dir"], f)
        for f in os.listdir(parsed_args["log_dir"])
    ]

    collection = collections.defaultdict(list)
    failures = []
    for file in log_files:
        read_file(file, parsed_args["warning"], parsed_args["summarize"],
                  collection, failures)

    items = collection.copy().items()
    for path, locs in items:
        collection[path] = sorted(locs)

    log_output(parsed_args["summarize"], collection, parsed_args["output"])

    if failures:
        sys.stderr.write(
            "\nFound lines with an unexpected format but the right ending:")
        for line in failures:
            sys.stderr.write("\n" + line)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
