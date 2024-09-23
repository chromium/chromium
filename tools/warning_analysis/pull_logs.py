#!/usr/bin/env python3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import glob
import json
import os
import subprocess
import sys
import tempfile
"""
This script connects to Buildbucket to pull the logs from all tryjobs for a
gerrit cl, and writes them to local files.
Since logs tend to be very large, it can also filter them, only writing lines
of interest.

See README.md in this directory for more details.
"""

bb = "bb.bat" if os.name == 'nt' else "bb"

# Types of builder which don't compile, and which we therefore ignore
ignored_recipes = [
    "chromium/orchestrator",  # Calls another builder to do the compilation
    "presubmit",  # No compilation at all
]

verbose = False


def log(msg):
    """
    Print a string for monitoring or debugging purposes, only if
    we're in verbose mode.
    """
    if verbose:
        print(msg)


def parse_args(args):
    """
    Parse the user's command-line options. Possible flags:

    log-dir: Where to store the downloaded log files.
    cl: The number of the cl to look up.
    patchset: The number of the patchset to download logs for.
    step-names: A list of possible build step names to download logs for.
                If multiple, logs will be pulled for the first one that exists.
    filter: A predicate on lines in the log. Lines that return false are removed
            before saving the log.
    """
    # Note: For local usage, it's often more convenient to edit these defaults
    # than to use the cli arguments, especially if you want a custom filter.
    default_config = {
        "log_dir": None,
        "cl": 0,
        "patchset": 0,
        "step_names": [
            "compile (with patch)", "compile", "compile (without patch)"
        ],
        "filter": lambda s: not s.startswith("["),
    }

    parser = argparse.ArgumentParser(description=__doc__,)
    parser.add_argument("-c",
                        "--cl",
                        type=int,
                        default=default_config["cl"],
                        help="CL number whose logs should be pulled.")
    parser.add_argument("-p",
                        "--patchset",
                        type=int,
                        default=default_config["patchset"],
                        help="Patchset number whose logs should be pulled.")
    parser.add_argument(
        "-l",
        "-o",
        "--log-dir",
        "--out-dir",
        type=str,
        default=default_config["log_dir"],
        help="Absolute path to a directory to store the downloaded logs. "
        "Will be created if it doesn't exist. "
        "Include a trailing slash.")
    parser.add_argument("-s",
                        "--step",
                        type=str,
                        action="append",
                        default=default_config["step_names"],
                        help="Name of the build step to pull logs for. "
                        "May be specified multiple times; logs are pulled "
                        "for each step in order until one succeeds.")
    parser.add_argument(
        "-f",
        "--filter",
        action="store_true",
        help="If true, strip unintesting build lines (those which begin "
        "with '[').")
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="If passed, print additional logging information for moitoring "
        "or debugging purposes.")

    handle_existing = parser.add_mutually_exclusive_group()
    handle_existing.add_argument(
        "-d",
        "--delete-logs",
        action="store_true",
        help="If passed, delete existing txt files from the log directory. "
        "Mutually exclusive with --resume.")
    handle_existing.add_argument(
        "-r",
        "--resume",
        action="store_true",
        help="If passed, don't download logs that are already present in the"
        "output directory. Useful if the previous download got interrupted. "
        "Mutually exclusive with --delete-logs.")

    parsed_args = vars(parser.parse_args(args))

    # Validate and/or the parsed args before returning.
    if (parsed_args["cl"] <= 0):
        raise ValueError("You must enter a real CL number")
    if parsed_args["patchset"] <= 0:
        raise ValueError("You must enter a real patchset number")

    if parsed_args["filter"]:
        parsed_args["filter"] = default_config["filter"]
    else:
        parsed_args["filter"] = lambda _: True

    if not parsed_args["log_dir"]:
        parsed_args["log_dir"] = tempfile.mkdtemp(prefix="pulled_logs_")

    global verbose
    verbose = parsed_args["verbose"]

    return parsed_args


def identify_builds(cl_id, patchset):
    """
    Use the bb tool to retrieve list of builds associated with this cl and
    patchset. Only return builds associated with the most recent run.
    """
    cl_str = ("https://chromium-review.googlesource.com/"
              "c/chromium/src/+/{}/{}".format(cl_id, patchset))

    # Make sure we're only getting the most recent set of builds by grabbing the
    # cq_attempt_key tag from the first build returned.
    # This strategy relies on the fact that that builds are returned in reverse
    # chronological order.
    most_recent_build = subprocess.run([bb, "ls", "-cl", cl_str, "-1", "-json"],
                                       check=True,
                                       stdout=subprocess.PIPE,
                                       text=True)

    if (len(most_recent_build.stdout) == 0):
        raise RuntimeError("Couldn't find any builds. Did you use a valid "
                           "cl_id AND patchset number?")

    output = json.loads(most_recent_build.stdout)
    for tag in output["tags"]:
        if tag["key"] == "cq_attempt_key":
            cq_attempt_key = tag["value"]
            break

    # Grab the info for all builds in the most recent set
    build_list = subprocess.run([
        bb, "ls", "-cl", cl_str, "-json", "-fields", "input", "-t",
        "cq_attempt_key:" + cq_attempt_key
    ],
                                check=True,
                                stdout=subprocess.PIPE,
                                text=True)
    if (len(build_list.stdout) == 0):
        raise RuntimeError("Somehow couldn't find any builds the second time.")

    # Retrieve the name and id of each build
    parsed_builds = [
        json.loads(build) for build in build_list.stdout.splitlines()
    ]

    target_builds = [
        (build["builder"]["builder"], build["id"])
        for build in parsed_builds
        if build["input"]["properties"]["recipe"] not in ignored_recipes
    ]

    log("Found {} target builds".format(len(target_builds)))
    return target_builds


def try_pull_step(build_id, step_names):
    """
    Try to pull each possible step name until one works or we've tried them all.
    If one is successfully pulled, return the incoming data as a stream.
    """
    for step_name in step_names:
        output = subprocess.Popen([bb, "log", build_id, step_name],
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT,
                                  text=True)
        first_line = output.stdout.readline()
        if first_line.startswith("step \"{}\" not found".format(step_name)):
            continue

        return output, first_line

    return None


def write_line(filter_fun, file, line):
    """
    Write a line to a file if it passes the filter.
    """
    if filter_fun(line):
        file.write(line + "\n")


# Pull the compilation logs, and filter them, only writing lines of interest.
def pull_and_filter_logs(parsed_args, target_builds):
    """
    Pull the compilation logs for each identifier builder. Strip uninteresting
    lines before saving to disk.

    Note that this will create the output directory if it doesn't exist.
    """
    # Keep track of any builders which we unexpectedly failed to pull logs for.
    failures = []  # Completely failed (e.g. step didn't exist)
    partial_logs = []  # Partial failure (e.g. builder died mid-compilation)

    log_dir = parsed_args["log_dir"]
    try:
        os.mkdir(log_dir)
    except FileExistsError:
        pass

    print("Storing logs in " + os.path.abspath(log_dir))

    if parsed_args["delete_logs"]:
        for f in glob.glob(os.path.join(log_dir, "*.txt")):
            os.remove(f)

    for name, build_id in target_builds:
        output_file = os.path.join(log_dir, name + ".txt")

        if parsed_args["resume"] and os.path.isfile(output_file):
            log("Log for {} already exists, skipping".format(name))
            continue

        log("Pulling logs for " + name)

        pulled_result = try_pull_step(build_id, parsed_args["step"])
        if not pulled_result:
            log("  Failed to pull logs for " + name)
            failures.append(name + " ({})".format(build_id) + "\n")
            continue

        output, first_line = pulled_result

        with open(output_file, "w") as file:
            write_line(parsed_args["filter"], file, first_line)
            for line in output.stdout:
                # If the builder died mid-compilation, bb may stop returning
                # data partway through, and just start printing an error message
                # every 5 seconds instead.
                if "No logs returned" in line:
                    log("  Only pulled partial log for " + name)
                    partial_logs.append(name + " ({})".format(build_id) + "\n")
                    output.kill()
                    write_line(
                        lambda _: True, file,
                        "Failed to pull entire log for {} ({})".format(
                            name, build_id))
                    break
                write_line(parsed_args["filter"], file, line)
    return failures, partial_logs


def main(args):
    parsed_args = parse_args(args)
    builds = identify_builds(parsed_args["cl"], parsed_args["patchset"])
    failures, partial_logs = pull_and_filter_logs(parsed_args, builds)

    if len(failures) > 0:
        sys.stderr.write(
            "Unexpectedly failed to pull logs for the following builders:\n")
        for failure in sorted(failures):
            sys.stderr.write(failure)

    if len(partial_logs) > 0:
        sys.stderr.write(
            "Only pulled partial logs for the following builders. You might "
            "want to download them manually and/or re-run the builders:\n")
        for failure in sorted(partial_logs):
            sys.stderr.write(failure)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
