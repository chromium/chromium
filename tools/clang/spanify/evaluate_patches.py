#!/usr/bin/env python3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# ----------------------------------------------------------------------------
# A temporary script to evaluate how many patches produced by the tool compile.
# The output are stored on the Google internal spreadsheet:
# http://go/autospan-tracker
# ----------------------------------------------------------------------------

import os
import random
import shutil
import subprocess
from datetime import datetime

# To install the required dependencies to interact with the Google Sheets API:
# ```
# python3 -m venv env
# source env/bin/activate
# pip install --upgrade \
#   google-api-python-client \
#   google-auth-httplib2 \
#   google-auth-oauthlib
# ```
from google.auth.transport.requests import Request
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from googleapiclient.discovery import build
from googleapiclient.errors import HttpError


def run(command, error_message=None, exit_on_error=True):
    """
    Helper function to run a shell command.
    """
    try:
        output = subprocess.run(command,
                                shell=True,
                                capture_output=True,
                                check=True,
                                text=True)
        print(output.stdout)
        print(output.stderr)

    except subprocess.CalledProcessError as e:
        print(error_message if error_message else "Failed to run command.")
        print(e.stdout)
        print(e.stderr)
        if exit_on_error:
            raise e
        return False

    return True

def getSpreadsheet():
    """
    Returns a Google spreadsheet object to interact with the Autospan tracker.
    """
    creds = None
    SCOPES = ['https://www.googleapis.com/auth/spreadsheets']
    if os.path.exists('token.json'):
        creds = Credentials.from_authorized_user_file('token.json', SCOPES)
        # If there are no (valid) credentials available, let the user log in.
        if not creds or not creds.valid:
            if creds and creds.expired and creds.refresh_token:
                creds.refresh(Request())
    else:
        flow = InstalledAppFlow.from_client_secrets_file(
            'credentials.json', SCOPES)
        creds = flow.run_local_server(port=1234)
    with open('token.json', 'w') as token:
        token.write(creds.to_json())

    spreadsheet = (build('sheets', 'v4',
                         credentials=creds).spreadsheets().values())

    return spreadsheet


def appendRow(spreadsheet, values):
    """
    Appends a row to the Google spreadsheet.
    """
    try:
        spreadsheet.append(
            spreadsheetId='14YCQY2eBlLDr2wd8XaCfbLacz0t94YRuyt5CjkohBK4',
            range='Data!A1:D',
            body={
                "majorDimension": "ROWS",
                "values": [values]
            },
            valueInputOption="USER_ENTERED",
        ).execute()

    except HttpError as err:
        print(err)


today = datetime.now().strftime("%Y/%m/%d")
scratch_dir = os.path.expanduser("~/scratch")
spreadsheet = getSpreadsheet()

print("Running evaluate_patches.py...")

# Fetch the latest changes from the main branch.
run("git fetch origin")
run("git checkout main", exit_on_error=False)  # Might be already on main.
run("git reset --hard origin/main")

# Setup a build directory to evaluate the patches. This is common to all the
# patches to avoid recompiling the entire project for each patch.
run("gclient sync -fD", exit_on_error=False)
run("gn gen out/linux", "Failed to generate out/linux.")
with open("out/linux/args.gn", "w") as f:
    f.write("use_remoteexec = true\n")

# Produce a full rewrite, and store individual patches below ~/scratch/patch_*
run("./tools/clang/spanify/rewrite-multiple-platforms.sh", exit_on_error=False)

run("git reset --hard origin/main")  # Restore source code.
run("gclient sync -fD", exit_on_error=False)  # Restore compiler.

patches = [
    name for name in os.listdir(scratch_dir)
    if name.startswith("patch_") and name.endswith(".txt")
]

print(f"Found {len(patches)} patches to evaluate.")

# Shuffle patches to avoid any bias in the evaluation.
random.shuffle(patches)

# This file will store a summary of each patch evaluation.
# ```
# patch, status, error_msg
# 0, pass, ""
# 1, fail, "subcommand failed"
# 2, pass, ""
# 3, pass, ""
# 4, fail, "subcommand failed"
# ```
with open(scratch_dir + "/evaluation.csv", "w+") as f:
    f.write("patch, status, error_msg\n")

# Perform a clean build to ensure a valid state for the incremental builds.
run("autoninja -C out/linux", "Failed to build the project.")

# Create and evaluate patches
patch_limit = 100
for patch in patches:
    # Limit the number of patches to evaluate. We don't want to spent too many
    # resources on this.
    if patch_limit == 0:
        break
    patch_limit -= 1

    # At some point, we won't have enough time to evaluate all patches. Check
    # that we have enough time to evaluate this patch, or quit.
    run("gcertstatus --check_remaining=1h --check_loas2=false",
        "You need to run gcert to extend your remoteexec access.")

    # Extract the patch index from the patch name string: patch_{index}.txt
    index = int(patch.split("_")[1].split(".")[0])

    print(f"Producing patch {index}/{len(patches)}: {patch}")

    # Apply edits
    run(f"git branch -D spanification_rewrite_evaluate_{index}",
        exit_on_error=False)
    run(f"git new-branch spanification_rewrite_evaluate_{index}")
    run(f"cat ~/scratch/{patch} " +
        " | tools/clang/scripts/apply_edits.py -p ./out/linux/")

    # Commit changes
    run("git add -u", "Failed to add changes.")

    with open("commit_message.txt", "w+") as f:
        f.write(f"""spanification patch {index} applied.\n\nPatch: {index}""")
    run("git commit -F commit_message.txt")

    # Serialize changes
    run(f"git diff HEAD~...HEAD > ~/scratch/patch_{index}.diff")
    diff = open(scratch_dir + f"/patch_{index}.diff").read()

    # Build and evaluate
    print(f"Evaluating patch {index}/{len(patches)}")
    print("Building...")

    result = subprocess.run("time autoninja -C out/linux",
                            shell=True,
                            capture_output=True,
                            text=True)
    print(result.stdout)
    print(result.stderr)

    with open(scratch_dir + f"/patch_{index}.out", "w+") as f:
        f.write(result.stderr)
        f.write(result.stdout)

    if "subcommand failed" in result.stdout.lower():
        error_msg = ""
        for line in result.stdout.split("\n") + result.stderr.split("\n"):
            if "error:" in line:
                error_msg = line[line.index("error:") + len("error:"):].strip()
                break

        with open(scratch_dir + "/evaluation.csv", "a") as f:
            f.write(f"{index}, fail, {error_msg}\n")

        appendRow(spreadsheet, [today, index, "fail", error_msg, diff])

        shutil.copy(scratch_dir + f"/patch_{index}.out",
                    scratch_dir + f"/patch_{index}.fail")
    else:
        with open(scratch_dir + "/evaluation.csv", "a") as f:
            f.write(f"{index}, pass, \"\"\n")
        appendRow(spreadsheet, [today, index, "pass", "", diff])
        shutil.copy(scratch_dir + f"/patch_{index}.out",
                    scratch_dir + f"/patch_{index}.pass")
