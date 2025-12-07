#!/usr/bin/env python3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# ----------------------------------------------------------------------------
# A temporary script to evaluate how many patches produced by the tool compile.
# The output are stored on the Google internal spreadsheet:
# http://go/autospan-tracker
# ----------------------------------------------------------------------------

from datetime import datetime
import getpass
import os
import random
import re
import subprocess
import sys

from google.auth.transport.requests import Request
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from googleapiclient.discovery import build
from googleapiclient.errors import HttpError
from googleapiclient.http import MediaFileUpload

# common gn args for spanify project scripts.
from gnconfigs import GnConfigs, GenerateGnTarget


# To install the required dependencies to interact with the Google Sheets API:
# ```
# python3 -m venv env
# source env/bin/activate
# pip install --upgrade \
#   google-api-python-client \
#   google-auth-httplib2 \
#   google-auth-oauthlib
#
# In addition when invoking evaluate_patches.py you can pass an optional integer
# argument which will set the limit of patches to evaluate. Default is 100.
# ```


def run(command, error_message=None, exit_on_error=True):
    """
    Helper function to run a shell command.
    """
    try:
        output = subprocess.run(command, shell=True, check=True, text=True)

    except subprocess.CalledProcessError as e:

        print(error_message if error_message else "Failed to run command: `" +
              command + "`",
              file=sys.stderr)
        if exit_on_error:
            raise e
        return False

    return True


def getGoogleCreds():
    """
    Returns creds to both google spreadsheet and drive, potentially asking for
    it to be allowed.
    """
    creds = None
    SCOPES = [
        'https://www.googleapis.com/auth/spreadsheets',
        'https://www.googleapis.com/auth/drive'
    ]
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

    return creds


def getSpreadsheet(creds):
    """
    Returns a Google spreadsheet object to interact with the Autospan tracker.
    """

    spreadsheet = (build('sheets', 'v4',
                         credentials=creds).spreadsheets().values())

    return spreadsheet


def appendRow(spreadsheet, values):
    """
    Appends a row to the Google spreadsheet.

    It performs some basic truncation and other logic to prevent easy to make
    errors (like to much text in a single cell).
    """
    # Google spreadsheet enforces a maximum 50000 chars so we truncate the data
    # down to fit to prevent the error.
    values = list(
        map(lambda val: val[0:50000]
            if len(str(val)) > 50000 else val, values))
    try:
        spreadsheet.append(
            spreadsheetId='14YCQY2eBlLDr2wd8XaCfbLacz0t94YRuyt5CjkohBK4',
            range='Data!A1:D',
            body={
                "majorDimension": "ROWS",
                "values": [values]
            },
            valueInputOption="USER_ENTERED",
        ).execute(num_retries=5)

    except HttpError as err:
        print(f"appendRow failed: {err}", file=sys.stderr)


def uploadZIPToDriveFolder(creds, zip_file):
    """
    uploads the provided zip_file to the autospan folder for easy storage and
    inspection of patches as needed.

    This folder can be found at:

    https://drive.google.com/drive/folders/18llw_ypcxrguyipghdxta6h2tc4wpgej
    """
    try:
        service = build("drive", "v3", credentials=creds)
        name = os.path.basename(zip_file)
        file_metadata = {
            "name": name,
            "parents": ["18lLW_YPCXRGUYiPghDXTA6h2TC4WpGeJ"]
        }
        media = MediaFileUpload(zip_file, mimetype="application/zip")
        #pylint: disable=maybe-no-member
        file = (service.files().create(body=file_metadata,
                                       media_body=media,
                                       fields="id").execute())
        print("uploaded " + zip_file + " as " + name +
              " to file: https://drive.google.com/file/d/" + file.get("id"))
    except HttpError as error:
        print(f"Uploading to drive errored: {error}", file=sys.stderr)


def uploadScratch(creds, file_name, scratch_dir):
    """
    Uploads a new file to the provided google drive folder.
    """
    try:
        # Since we've reduced the size of stdout we can include everything in
        # the scratch directory.
        output_zip = f"{scratch_dir}/{file_name}"
        run(f"zip -q {output_zip} {scratch_dir}/*")
        uploadZIPToDriveFolder(creds, f"{scratch_dir}/{file_name}")
    except Exception as e:
        print(f"Failed to upload scratch: {e}", file=sys.stderr)


def ReportCaseResult(scratch_dir, result, spreadsheet, today, index, patches,
                     user, error_msg, diff, final_file):
    with open(scratch_dir + "/evaluation.csv", "a") as f:
        f.write(f"{index}, {result}, {error_msg}\n")
    try:
        appendRow(spreadsheet, [
            today,
            index,
            len(patches),
            result,
            error_msg,
            diff,
            user,
        ])
    except Exception as e:
        try:
            appendRow(spreadsheet, [
                today,
                index,
                len(patches),
                result,
                f"\"Failed to upload to spreadsheet: {str(e)}\"",
                f"diff_len: {len(diff)} error_msg_len: {len(error_msg)}",
                user,
            ])
        except Exception as err:
            print(f"Failed to appendRow for simplified data spreadsheet: {err}",
                  file=sys.stderr)

        print(f"Failed to appendRow but uploaded error to spreadsheet: {e}",
              file=sys.stderr)

    with open(scratch_dir + f"/patch_{index}.{result}", "w+") as f:
        f.write(final_file)


today = datetime.now().strftime("%Y/%m/%d")
today_underscore = today.replace("/", "_")
scratch_dir = os.path.expanduser("~/scratch")
creds = getGoogleCreds()
spreadsheet = getSpreadsheet(creds)
user = getpass.getuser()
platform = "linux"

# Curry ReportCaseResult to use the variables above to simplify the code below.
# Preventing code duplication and mistakes.
report_success = lambda error_msg, diff, final_file: ReportCaseResult(
        scratch_dir, "pass", spreadsheet, today, index, patches, user,
        error_msg, diff, final_file)
report_failure = lambda error_msg, diff, final_file: ReportCaseResult(
        scratch_dir, "fail", spreadsheet, today, index, patches, user,
        error_msg, diff, final_file)

print("Running evaluate_patches.py...")

# Fetch the latest changes from the main branch.
run("git fetch origin")
run("git checkout main", exit_on_error=False)  # Might be already on main.
run("git reset --hard origin/main")

# Setup a build directory to evaluate the patches. This is common to all the
# patches to avoid recompiling the entire project for each patch.
run("gclient sync -fD", exit_on_error=False)

if len(sys.argv) > 2:
    # If you pass both a patch limit override and a second argument, use the
    # second argument as the platform to rewrite-multiple-platforms. This
    # allows doing something like:
    # "evaluate_patches.py 3000 android"
    # To override the default Linux platform and do android instead.
    platform = sys.argv[2]

try:
    run("gcertstatus --check_remaining=3h --nocheck_ssh")
    print("Remote exec available. Enabling.")
    GenerateGnTarget(platform, GnConfigs(True).min_all_platforms[platform])
except:
    print("Remote exec not available. Disabling.")
    GenerateGnTarget(platform, GnConfigs(False).min_all_platforms[platform])

# We've updated the args and need to generate new build files.
run(f"gn gen out/{platform}", f"Failed to generate out/{platform}.")

# Produce a full rewrite, and store individual patches below ~/scratch/patch_*
rewrite_script = "./tools/clang/spanify/rewrite-multiple-platforms.sh"
print(f"{rewrite_script} --platform={platform}")
run(f"{rewrite_script} --platform={platform}")

run("git reset --hard origin/main")  # Restore source code.
run("gclient sync -fD", exit_on_error=False)  # Restore compiler.
run("git rev-parse origin/main > ~/scratch/git_revision.txt")  # Save commit.

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
run(f"autoninja -C out/{platform}", "Failed to build the project.")

# Create and evaluate patches
try:
    patch_limit = 100
    if len(sys.argv) > 1:
        patch_limit = int(sys.argv[1])
    for patch in patches:
        # Limit the number of patches to evaluate. We don't want to spent too
        # many resources on this.
        if patch_limit == 0:
            break
        patch_limit -= 1

        # Extract the patch index from the patch name string: patch_{index}.txt
        index = int(patch.split("_")[1].split(".")[0])

        print(f"Producing patch {index}/{len(patches)}: {patch}")

        # Apply edits
        run(f"git branch -D spanification_rewrite_evaluate_{index}",
            exit_on_error=False)
        run(f"git new-branch spanification_rewrite_evaluate_{index}")
        try:
            result = subprocess.run(f"cat ~/scratch/{patch} " +
                                    " | tools/clang/scripts/apply_edits.py" +
                                    f" -p ./out/{platform}/",
                                    shell=True,
                                    check=True,
                                    capture_output=True,
                                    text=True)
        except subprocess.CalledProcessError as e:
            error_msg = ("\"" + str(e) + " !!! exception(stderr): " +
                         str(e.stderr) + "\"")

            run(f"git diff  > ~/scratch/patch_{index}.diff")
            diff = open(scratch_dir + f"/patch_{index}.diff").read()

            final_file = str(e.stderr) + "\n" + str(e.stdout)

            report_failure(error_msg, diff, final_file)

            run("git restore .", "Failed to restore after failed patch.")
            continue

        run("git cl format")

        # Commit changes
        run("git add -u", "Failed to add changes.")

        with open("commit_message.txt", "w+") as f:
            f.write(
                f"""spanification patch {index} applied.\n\nPatch: {index}""")
        # Sometimes we generate patches that apply_edits will skip (for example
        # third_party) thus don't treat failure to commit as an error.
        if not run("git commit -F commit_message.txt", exit_on_error=False):
            # We fail when there is no diff get the replacements instead.
            diff = open(scratch_dir + f"/patch_{index}.txt").read()

            report_failure("Failed to commit diff", diff, "")
            continue

        # Serialize changes
        run(f"git diff HEAD~...HEAD > ~/scratch/patch_{index}.diff")
        diff = open(scratch_dir + f"/patch_{index}.diff").read()

        # Build and evaluate
        print(f"Evaluating patch {index}/{len(patches)}")
        print("Building...")

        result = subprocess.run(f"time autoninja -C out/{platform}",
                                shell=True,
                                capture_output=True,
                                text=True)
        print(result.stdout)
        print(result.stderr)

        final_file = result.stderr + "\n" + result.stdout
        with open(scratch_dir + f"/patch_{index}.out", "w+") as f:
            f.write(result.stderr)
            f.write(result.stdout)

        if "build failed" in result.stdout.lower():
            error_msg = ""
            # Errors format:
            # <file>:<line>:<column>: error: <error_msg>
            error_regex = re.compile(r"^(.*):(\d+):(\d+): error: (.*)$")
            for line in result.stdout.split("\n") + result.stderr.split("\n"):
                match = error_regex.match(line)
                if match:
                    error_msg = match.group(4)
                    break

            report_failure(error_msg, diff, final_file)
        elif not run(f'gn check out/{platform}', exit_on_error=False):
            error_msg = "failed gn check"
            report_failure(error_msg, diff, final_file)
            continue
        else:
            report_success("", diff, final_file)
finally:
    # Regardless of success or failure we want to upload the scratch directory
    # to the shared google drive for easy debugging of either compile errors or
    # the evaluate_patches tool itself.
    unique_id = random.randint(1, 10000)
    file_name = f"{today_underscore}_evaluate_patches_{user}_{unique_id}.zip"
    uploadScratch(creds, file_name, scratch_dir)
