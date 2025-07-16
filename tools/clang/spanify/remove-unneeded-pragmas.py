#!/usr/bin/env python3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is used to list files that require `#pragma allow_unsafe_buffers`
# in the Chromium codebase.
#
# Prerequisites:
# --------------
# Ensure your .gclient contains:
# ```
# target_os = ["win", "android", "linux", "chromeos", "mac", "fuchsia"]
# solutions = [
#   {
#     ...
#     "custom_vars": {
#       "checkout_src_internal": True,
#       "download_remoteexec_cfg": True,
#       "checkout_pgo_profiles": True,
#       "checkout_mobile_internal": True,
#       "checkout_google_internal": True,
#     },
#  },
#]
# ```
# You'll also need to run some scripts like:
# ```
# build/linux/sysroot_scripts/install-sysroot.py --arch=arm
# build/linux/sysroot_scripts/install-sysroot.py --arch=arm64
# gclient sync -f -D
# ```
#
# Usage for automatic spanification
# ---------------------------------
# By running this script we can determine remove files that have been fixed
# (not 100% exhaustive).
#
# Example:
#
# 1. Checkout "main"
# 2. Generate a spanification patch: "rewrite" (or run at HEAD)
# 3. Run this script to remove unneeded pragmas: "pragma-after".
# 4. Commit "pragma-after".

import json
import os
import subprocess
import sys

# common gn args for spanify project scripts.
from gnconfigs import GnConfigs, GenerateGnTarget

# Building is going to fail on multiple files. They will be fixed automatically
# inserting `opt_out_lines` in the file after the copyright notice.
opt_out_lines = [
    "",
    "#ifdef UNSAFE_BUFFERS_BUILD",
    "// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.",
    "#pragma allow_unsafe_buffers",
    "#endif",
]


# Looks through all potential code files for any mention of
# 'allow_unsafe_buffers', note that this will be a super set of ones that
# actually have the pragma because it could just be mentioned in comments.
def FindCodeFilesWithPragma() -> set[str]:
    files_with_pragma = """
        git grep -l '#pragma allow_unsafe_buffers' --\
            '*.h' '*.cc' '*.c' '*.cpp' '*.mm' '*.m'
    """
    files_that_had_pragma = set(f.strip() for f in subprocess.check_output(
        files_with_pragma, shell=True).decode("utf-8").split("\n"))
    files_that_had_pragma.discard("")
    return files_that_had_pragma


# For a given target (just a label for the out directory) and the associated
# gn args generate the json representation of the targets and find all files we
# will compile when compiling this target.
def FindReachableFilesForConfigsInSet(target, args,
                                      files_to_check) -> set[str]:
    current_target = set()
    # Generate the project.json file, you could also specify the name with
    # --json-file-name but no real need.
    os.system("gn gen out/%s --ide=json --args='%s'" %
              (target, "\n".join(args)))
    with open('out/%s/project.json' % target) as f:
        data = json.load(f)
        for tar, values in data['targets'].items():
            if 'sources' not in values:
                continue
            for source in values['sources']:
                file = source.removeprefix("//")
                if file in files_to_check:
                    current_target.add(file)
    return current_target


# Opens every file finds the ifdef for UNSAFE_BUFFERS_BUILD and removes all
# lines from then on, until we reach the #endif. The rest of the file is
# unchanged.
#
# It is important to modify only the files in the git repository. We can use
# `git grep 'allow_unsafe_buffers'` to get the list of files.
def RemovePragmasFromFiles(files):
    for file in files:
        print("Removing opt out for: %s" % file, flush=True)
        try:
            with open(file, 'r') as f:
                lines = f.readlines()

            with open(file, 'w') as f:
                in_opt_out = False
                for line in lines:
                    if in_opt_out:
                        if "#endif" in line:
                            in_opt_out = False
                    else:
                        if "#ifdef UNSAFE_BUFFERS_BUILD" in line:
                            in_opt_out = True
                        else:
                            f.write(line)
        except Exception as e:
            print("Failed to remove opt_out from %s: %s" % (file, str(e)),
                  flush=True)


def AddPragmasToFiles(unsafe_buffers_files, files_that_had_pragma) -> [str]:
    rewrittens = []
    print("Unsafe buffer operations found in:", flush=True)
    for file in unsafe_buffers_files:
        print(file, flush=True)

    # Fix the files by inserting the opt_out_lines before the first line,
    # not starting with //.
    for file in unsafe_buffers_files:
        try:
            print("Opting out %s" % file, flush=True)
            if (not os.path.exists(file)):
                print("File %s does not exist." % file, flush=True)
                continue
            # If the file already had the pragma, restore the old state.
            # This prevents touching or changing too many files.
            if file in files_that_had_pragma:
                os.system("git checkout main %s" % file)
            else:
                with open(file, 'r') as f:
                    lines = f.readlines()

                with open(file, 'w') as f:
                    inserted = False
                    for line in lines:
                        if not inserted and not line.startswith("//"):
                            for opt_out_line in opt_out_lines:
                                f.write("%s\n" % opt_out_line)
                            inserted = True
                        f.write(line)
            rewrittens.append(file)
        except Exception as e:
            print("Failed to opt out %s: %s" % (file, str(e)), flush=True)
    return rewrittens


def AddPragmasUntilTargetCompiles(target, args, files_that_had_pragma) -> bool:
    # Configure the target.

    assert GenerateGnTarget(target, args), "Failed to configure target"
    no_files_rewritten = False
    while True:
        # Try building all the targets, exit if the build succeeds.
        # Do not print the output.
        if os.system("autoninja -C out/%s" % target) == 0:
            print("Build succeeded for %s." % target, flush=True)
            # Some compiles are really big clean up after ourselves.
            os.system("gn clean out/%s" % target)
            break

        # Clang is reporting errors likes:
        # <file>:<line>:<column>: error: unsafe pointer arithmetic [-Werror,-Wunsafe-buffer-usage]
        #
        # On Windows, this will be:
        # <file>(line,column): error: unsafe pointer arithmetic [-Werror,-Wunsafe-buffer-usage]
        # <file>:<line>:<column>: error: unsafe pointer arithmetic [-Werror,-Wunsafe-buffer-usage]
        # This is because the file is using unsafe buffer operations.
        # We will fix this by inserting the opt_out_lines in the file.

        # Get the list of files with unsafe buffer operations.
        unsafe_buffers_files = subprocess.check_output(
            """
            autoninja -k 0 -C out/%s |\
                grep -E 'Wunsafe-buffer-usage' |\
                cut -d':' -f1 |\
                cut -d'(' -f1 |\
                sort -u
            """ % target,
            shell=True).decode("utf-8").split("\n")

        # Strip the ../../ from the file paths.
        unsafe_buffers_files = [
            file.replace("../../", "") for file in unsafe_buffers_files
        ]

        # Clean empty strings.
        unsafe_buffers_files = [file for file in unsafe_buffers_files if file]

        rewrittens = AddPragmasToFiles(unsafe_buffers_files,
                                       files_that_had_pragma)

        if not rewrittens:
            print("No files were fixed.", flush=True)
            if no_files_rewritten:
                # Don't stop the whole script but report an error so someone
                # can check if there is a fix needed  to get this compiling.
                # Perhaps this was a bad git commit.
                print("Two unsuccessful builds in a row without opt-outs: %s" %
                      target)
                break
            else:
                no_files_rewritten = True
                continue
        no_files_rewritten = False


def main():
    # Collect all files that have the pragma we are interested in.
    print("Collecting files with opt_out...", flush=True)
    files_that_had_pragma = FindCodeFilesWithPragma()
    print("found %d files with pragmas." % len(files_that_had_pragma),
          flush=True)

    # Find all the reachable files for each gn target (this limits the removals
    # to ones we'll actually build and thus can be sure if we build properly).
    reachable_files_with_pragmas = set()
    for target, args in GnConfigs(True).all_platforms_and_configs.items():
        print("Determining reachable files for %s" % target, flush=True)
        current_target = FindReachableFilesForConfigsInSet(
            target, args, files_that_had_pragma)
        print('target: %s has %d' % (target, len(current_target)), flush=True)
        reachable_files_with_pragmas |= current_target

    print("Found %d reachable files that had pragmas." %
          len(reachable_files_with_pragmas),
          flush=True)

    # Before adding the opt_out lines, we need to clear them in every files.
    # Note that the opt_out_lines are not very stable, the bug and comments
    # might vary. We should delete the whole block.
    RemovePragmasFromFiles(reachable_files_with_pragmas)

    for target, args in GnConfigs(True).all_platforms_and_configs.items():
        print("Building for %s:" % target, flush=True)
        AddPragmasUntilTargetCompiles(target, args, files_that_had_pragma)

    # Once it compiles on every targets, format the code.
    os.system("git cl format")

    # Regenerate a couple autogen files that run into issues consistently.
    os.system("vpython3 gpu/command_buffer/build_gles2_cmd_buffer.py")
    os.system("vpython3 gpu/command_buffer/build_raster_cmd_buffer.py")

    # Some files are not properly covered by this script. Revert the known
    # failing files.
    exclusions = [
        # Reproduce on android-cronet-riscv64-dbg.
        "base/profiler/register_context_registers.h",

        # Reproduce on linux-cast-arm-rel
        "media/parsers/h264_bit_reader.h",
    ]
    for exclusion in exclusions:
        print("Reverting %s" % exclusion, flush=True)
        os.system("git checkout HEAD -- %s" % exclusion)

    # Add changed code and create the commit.
    os.system("git add -u")

    git_commit_description =\
        """spanification: remove `#pragma allow_unsafe_buffers` to xxx

        This is a clean up of any files that now compile without the pragma.
        This CL has no behavior changes.

        This patch was fully automated using script:
        /tools/clang/spanify/remove-unneeded-pragmas.py

        See internal doc about it:
        https://docs.google.com/document/d/1erdcokeh6rfBqs_h0drHqSLtbDbB61j7j3O2Pz8NH78/edit?resourcekey=0-hNe6w1hYAYyVXGEpWI7HVA&tab=t.0

        Bug: 40285824"""

    with open("commit_description.txt", "w") as f:
        f.write(git_commit_description)

    os.system("git commit -F commit_description.txt --no-edit")


if __name__ == "__main__":
    sys.exit(main())
