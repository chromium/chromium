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
# ```
#
# Usage for automatic spanification
# ---------------------------------
# By running this script before and after a spanification, we can determine
# which files have been fixed. (not 100% exhaustive).
#
# Example:
#
# 1. Checkout "main"
# 2. Generate a spanification patch: "rewrite".
# 3. Generate the pragma after spanification: "pragma-after".
# 5. Checkout "main".
# 4. Generate the pragma before spanification: "pragma-before".
# 6. Checkout "rewrite".
# 7. Generate the list of files that have been fixed: This is the pragma removed
#    by "rewrite-after" but kept by "rewrite-before".
#    ```
#    comm -13 \
#        <(git show --pretty="" --name-only pragma-before | sort -u) \
#        <(git show --pretty="" --name-only pragma-after  | sort -u) \
#        > fixed
#    ```
# 8. Apply the changes to the files.
#    ```
#    for file in $(cat fixed); do
#      git diff pragma-after -- $file | git apply
#    done
# 9. Commit "fixed".
#
# Illustration:
#
# main -> rewrite --------------------------------> fixed
#   |        |                                  ^
#   |         `-> pragma-after  --.             |
#   |                               |-> comm-13-/
#   `-----------> pragma-before --'
#
import os
import subprocess

# Make sure dependencies are up to date.
os.system("gclient sync -f -D")

# Building is going to fail on multiple files. They will be fixed automatically
# inserting `opt_out_lines` in the file after the copyright notice.
opt_out_lines = [
    "",
    "#ifdef UNSAFE_BUFFERS_BUILD",
    "// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.",
    "#pragma allow_unsafe_buffers",
    "#endif",
]

# Before adding the opt_out lines, we need to clear them in every files. Note
# that the opt_out_lines are not very stable, the bug and comments might vary.
# We should delete the whole block.
#
# It is important to modify only the files in the git repository. We can use
# `git grep 'allow_unsafe_buffers'` to get the list of files.
print("Removing opt_out from files...")
files_with_pragma = """
    git grep 'allow_unsafe_buffers' \
        | cut -d':' -f1 \
        | sort -u
"""
for file in subprocess.check_output(files_with_pragma, shell=True)\
        .decode("utf-8")\
        .split("\n"):
    if not file:
        continue

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
        print("Failed to remove opt_out from %s: %s" % (file, str(e)))

# Try building chromium on multiple target_os.
gn_args = {}
gn_args["linux"] = [
    'target_os="linux"',
    'use_remoteexec=true',
    'dcheck_always_on=true',
    'is_asan=true',
    'optimize_for_fuzzing=true',
    'use_libfuzzer=true',
]
gn_args["mac"] = [
    'target_os="mac"',
    'use_remoteexec=true',
    'dcheck_always_on=true',
    'is_asan=true',
    'optimize_for_fuzzing=true',
    'use_libfuzzer=true',
]
gn_args["win"] = [
    'target_os="win"',
    'use_remoteexec=true',
    'dcheck_always_on=true',
    'is_asan=true',
    'optimize_for_fuzzing=true',
    'use_libfuzzer=true',
]
gn_args["android"] = [
    'target_os="android"',
    'use_remoteexec=true',
    'dcheck_always_on=true',
    'is_asan=true',
    'optimize_for_fuzzing=true',
    'use_libfuzzer=true',
]
gn_args["fuchsia"] = [
    'target_os="fuchsia"',
    'use_remoteexec=true',
    'dcheck_always_on=true',
    'is_asan=true',
    'optimize_for_fuzzing=true',
    'use_libfuzzer=true',
]
gn_args["chromeos"] = [
    'target_os="chromeos"',
    'use_remoteexec=true',
    'dcheck_always_on=true',
    'is_asan=true',
    'optimize_for_fuzzing=true',
    'use_libfuzzer=true',
]

for target, args in gn_args.items():
    print("Building for %s:" % target)
    # Configure the target.
    os.system("gn gen out/%s --args='%s'" % (target, "\n".join(args)))

    while True:
        # Try building all the targets, exit if the build succeeds. Do not print the output.
        if os.system("autoninja -C out/%s" % target) == 0:
            print("Build succeeded for %s." % target)
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

        rewrittens = []
        print("Unsafe buffer operations found in:")
        for file in unsafe_buffers_files:
            print(file)

        # Fix the files by inserting the opt_out_lines before the first line, not
        # starting with //.
        for file in unsafe_buffers_files:
            try:
                print("Opting out %s" % file)
                if (not os.path.exists(file)):
                    print("File %s does not exist." % file)
                    continue

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
                print("Failed to opt out %s: %s" % (file, str(e)))

        if not rewrittens:
            print("No files were fixed.")
            break

# Once it compiles on every targets, format the code, and create a commit.
os.system("git cl format")
os.system("git add -u")

git_commit_description =\
    """spanification: Add `#pragma allow_unsafe_buffers` to xxx

This is a preparation to fix each files.
This CL has no behavior changes.

This patch was fully automated using script:
https://paste.googleplex.com/5614491201175552

See internal doc about it:
https://docs.google.com/document/d/1erdcokeh6rfBqs_h0drHqSLtbDbB61j7j3O2Pz8NH78/edit?resourcekey=0-hNe6w1hYAYyVXGEpWI7HVA&tab=t.0

Bug: 40285824"""

with open("commit_description.txt", "w") as f:
    f.write(git_commit_description)

os.system("git commit -F commit_description.txt --no-edit")
