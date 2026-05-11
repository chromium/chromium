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
import argparse
import getpass
import os
import random
import re
import subprocess
import sys
from spanify_utils import scratch_dir

from google.auth.transport.requests import Request
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from googleapiclient.discovery import build
from googleapiclient.errors import HttpError
from googleapiclient.http import MediaFileUpload

# common gn args for spanify project scripts.
from gnconfigs import GnConfigs, GenerateGnTarget

PROJECTS = {
    # http://go/autospan-tracker
    'chrome': {
        'spreadsheet_id': '14YCQY2eBlLDr2wd8XaCfbLacz0t94YRuyt5CjkohBK4',
        'build_targets': ['all'],
    },
    # http://go/autospan-partition-alloc-tracker
    'partition_alloc': {
        'spreadsheet_id':
        '15AuAyRmxG95L5G8ejTlJ7C2p6zgvKTLt8lV9lg6xmDk',
        'build_targets': [
            'base/allocator/partition_allocator/src/partition_alloc:partition_alloc'
        ],
    },
    # http://go/autospan-dawn-tracker
    'dawn': {
        'spreadsheet_id': '11I41N369S7tcbMrWhsn6gStLKbOxseGSOMSTE3dsQok',
        'build_targets': ['third_party/dawn/src/dawn:dawn'],
    },
    # http://go/autospan-skia-tracker
    'skia': {
        'spreadsheet_id': '1dJ5PIQMsQ4IBYTcZUrshOJUIOmoa0MOwedK-jGzHYxI',
        'build_targets': ['skia:skia'],
    },
    # http://go/autospan-angle-tracker
    'angle': {
        'spreadsheet_id': '10g9-rrhGRQM1bGfHyZyK4Yris6X1ZfPmq-iRxw-9n38',
        'build_targets': ['third_party/angle:angle'],
    },
    # http://go/autospan-webrtc-tracker
    'webrtc': {
        'spreadsheet_id': '1gDu0ZCAONoIm242lRscCoYKmrfWLbwflxtwgWi-NUVk',
        'build_targets': ['third_party/webrtc_overrides:webrtc_component'],
    },
}


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
    print('-' * 80)
    if sys.stdout.isatty():
        print(f"\033[7m{command}\033[0m")
    else:
        print(command)
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


def get_google_creds():
    """
    Returns creds to both google spreadsheet and drive, potentially asking for
    it to be allowed.
    """
    creds = None
    SCOPES = [
        'https://www.googleapis.com/auth/spreadsheets',
        'https://www.googleapis.com/auth/drive'
    ]

    token_path = 'token.json'
    if not os.path.exists(token_path):
        token_path = os.path.expanduser('~/token.json')

    if os.path.exists(token_path):
        creds = Credentials.from_authorized_user_file(token_path, SCOPES)
        # If there are no (valid) credentials available, let the user log in.
        if not creds or not creds.valid:
            if creds and creds.expired and creds.refresh_token:
                creds.refresh(Request())
    else:
        creds_path = 'credentials.json'
        if not os.path.exists(creds_path):
            creds_path = os.path.expanduser('~/credentials.json')

        flow = InstalledAppFlow.from_client_secrets_file(creds_path, SCOPES)
        creds = flow.run_local_server(port=1234)

    # Save the token to the path we found it or the local directory.
    with open(token_path, 'w') as token:
        token.write(creds.to_json())

    return creds


def get_spreadsheet(creds):
    """
    Returns a Google spreadsheet object to interact with the Autospan tracker.
    """

    spreadsheet = (build('sheets', 'v4',
                         credentials=creds).spreadsheets().values())

    return spreadsheet


def append_row(spreadsheet, spreadsheet_id, values):
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
            spreadsheetId=spreadsheet_id,
            range='Data!A1:D',
            body={
                "majorDimension": "ROWS",
                "values": [values]
            },
            valueInputOption="USER_ENTERED",
        ).execute(num_retries=5)

    except HttpError as err:
        print(f"append_row failed: {err}", file=sys.stderr)


def upload_zip_to_drive_folder(creds, zip_file):
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
        print(f"uploaded {zip_file} as {name} "
              f"to file: https://drive.google.com/file/d/{file.get('id')}")
    except HttpError as error:
        print(f"Uploading to drive errored: {error}", file=sys.stderr)


def upload_scratch(creds, file_name):
    """
    Uploads a new file to the provided google drive folder.
    """
    try:
        # Since we've reduced the size of stdout we can include everything in
        # the scratch directory.
        output_zip = scratch_dir() / file_name
        run(f'zip -q "{output_zip}" "{scratch_dir()}"/*')
        upload_zip_to_drive_folder(creds, output_zip)
    except Exception as e:
        print(f"Failed to upload scratch: {e}", file=sys.stderr)


def report_case_result(result, spreadsheet, spreadsheet_id, today, index,
                       patches, user, error_msg, diff, final_file):
    with (scratch_dir() / "evaluation.csv").open("a") as f:
        f.write(f"{index}, {result}, {error_msg}\n")
    try:
        append_row(spreadsheet, spreadsheet_id, [
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
            append_row(spreadsheet, spreadsheet_id, [
                today,
                index,
                len(patches),
                result,
                f"\"Failed to upload to spreadsheet: {str(e)}\"",
                f"diff_len: {len(diff)} error_msg_len: {len(error_msg)}",
                user,
            ])
        except Exception as err:
            print(
                f"Failed to append_row for simplified data spreadsheet: {err}",
                file=sys.stderr)

        print(f"Failed to append_row but uploaded error to spreadsheet: {e}",
              file=sys.stderr)

    with (scratch_dir() / f"patch_{index}.{result}").open("w+") as f:
        f.write(final_file)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Evaluate patches produced by the spanify tool.")
    parser.add_argument("--patch-limit",
                        type=int,
                        default=100,
                        help="Limit of patches to evaluate.")
    parser.add_argument("platform",
                        nargs="?",
                        default="linux",
                        help="Platform to evaluate the patches on.")
    parser.add_argument("--project",
                        choices=PROJECTS.keys(),
                        default="chrome",
                        help="Project to evaluate.")
    args = parser.parse_args()

    platform = args.platform
    patch_limit = args.patch_limit
    project = args.project
    spreadsheet_id = PROJECTS[project]['spreadsheet_id']
    rewrite_project = project
    build_targets = " ".join(PROJECTS[project]['build_targets'])

    today = datetime.now().strftime("%Y/%m/%d")
    today_underscore = today.replace("/", "_")
    creds = get_google_creds()
    spreadsheet = get_spreadsheet(creds)
    user = getpass.getuser()

    def report_success(index, patches, error_msg, diff, final_file):
        report_case_result("pass", spreadsheet, spreadsheet_id, today, index,
                           patches, user, error_msg, diff, final_file)

    def report_failure(index, patches, error_msg, diff, final_file):
        report_case_result("fail", spreadsheet, spreadsheet_id, today, index,
                           patches, user, error_msg, diff, final_file)

    print(f"Running evaluate_patches.py for project {project}...")

    # Fetch the latest changes from the main branch.
    run("git fetch origin")
    run("git checkout main", exit_on_error=False)  # Might be already on main.
    run("git reset --hard origin/main")

    # Setup a build directory to evaluate the patches. This is common to all the
    # patches to avoid recompiling the entire project for each patch.
    run("gclient sync -fD", exit_on_error=False)

    try:
        run("gcertstatus --check_remaining=3h --nocheck_ssh")
        print("Remote exec available. Enabling.")
        GenerateGnTarget(platform, GnConfigs(True).min_all_platforms[platform])
    except:
        print("Remote exec not available. Disabling.")
        GenerateGnTarget(platform,
                         GnConfigs(False).min_all_platforms[platform])

    # We've updated the args and need to generate new build files.
    run(f"gn gen out/{platform}", f"Failed to generate out/{platform}.")

    # Produce a full rewrite, and store individual patches below <scratch>/patch_*
    rewrite_script = "./tools/clang/spanify/rewrite_multiple_platforms.py"
    run(f"{rewrite_script} --platform={platform} --project={rewrite_project}")

    run("git reset --hard origin/main")  # Restore source code.
    run("gclient sync -fD", exit_on_error=False)  # Restore compiler.
    run(f'git rev-parse HEAD > "{(scratch_dir() / "git_revision.txt")}"')

    patches = [p.name for p in scratch_dir().glob("patch_*.txt")]

    # Sort numerically the patches to evaluate.
    patches.sort(key=lambda x: int(x.split("_")[1].split(".")[0]))
    patches = patches[:patch_limit]

    print(f"Found {len(patches)} patches to evaluate.")

    # This file will store a summary of each patch evaluation.
    # ```
    # patch, status, error_msg
    # 0, pass, ""
    # 1, fail, "subcommand failed"
    # 2, pass, ""
    # 3, pass, ""
    # 4, fail, "subcommand failed"
    # ```
    with (scratch_dir() / "evaluation.csv").open("w+") as f:
        f.write("patch, status, error_msg\n")

    # Perform a clean build to ensure a valid state for the incremental builds.
    run(f"autoninja -C out/{platform} {build_targets}",
        "Failed to build the project.",
        exit_on_error=False)

    # Create and evaluate patches
    try:
        for patch in patches:
            # Extract the patch index from the patch name string: patch_{index}.txt
            index = int(patch.split("_")[1].split(".")[0])

            print(f"Producing patch {index}/{len(patches)}: {patch}")

            # Apply edits
            run(f"git branch -D spanification_rewrite_evaluate_{index}",
                exit_on_error=False)
            run(f"git new-branch spanification_rewrite_evaluate_{index}")
            try:
                result = subprocess.run(
                    f'cat "{(scratch_dir() / patch)}" ' +
                    " | tools/clang/scripts/apply_edits.py" +
                    f" -p ./out/{platform}/",
                    shell=True,
                    check=True,
                    capture_output=True,
                    text=True)
            except subprocess.CalledProcessError as e:
                error_msg = ("\"" + str(e) + " !!! exception(stderr): " +
                             str(e.stderr) + "\"")

                run(f'git diff  > "{(scratch_dir() / f"patch_{index}.diff")}"')
                diff = (scratch_dir() / f"patch_{index}.diff").read_text()

                final_file = str(e.stderr) + "\n" + str(e.stdout)

                report_failure(index, patches, error_msg, diff, final_file)

                run("git restore .", "Failed to restore after failed patch.")
                continue

            run("git cl format")

            # Commit changes
            run("git add -u", "Failed to add changes.")

            with open("commit_message.txt", "w+") as f:
                f.write(
                    f"""spanification patch {index} applied.\n\nPatch: {index}"""
                )
            # Sometimes we generate patches that apply_edits will skip (for example
            # third_party) thus don't treat failure to commit as an error.
            if not run("git commit -F commit_message.txt",
                       exit_on_error=False):
                # We fail when there is no diff get the replacements instead.
                diff = (scratch_dir() / f"patch_{index}.txt").read_text()

                report_failure(index, patches, "Failed to commit diff", diff,
                               "")
                continue

            # Serialize changes
            run(f'git diff HEAD~...HEAD > "{(scratch_dir() / f"patch_{index}.diff")}"'
                )
            diff = (scratch_dir() / f"patch_{index}.diff").read_text()

            # Build and evaluate
            print(f"Evaluating patch {index}/{len(patches)}")
            print("Building...")

            result = subprocess.run(
                f"time autoninja -C out/{platform} {build_targets}",
                shell=True,
                capture_output=True,
                text=True)

            print(result.stdout)
            print(result.stderr)

            final_file = result.stderr + "\n" + result.stdout
            with (scratch_dir() / f"patch_{index}.out").open("w+") as f:
                f.write(result.stderr)
                f.write(result.stdout)

            if result.returncode != 0:
                error_msg = ""
                # Errors format:
                # <file>:<line>:<column>: [fatal] error: <error_msg>
                error_regex = re.compile(
                    r"([^:]+:\d+(?::\d+)?: (?:fatal )?error: .*)")
                for line in result.stdout.split("\n") + result.stderr.split(
                        "\n"):
                    match = error_regex.search(line)
                    if match:
                        error_msg = match.group(1)
                        break

                if not error_msg:
                    # Fallback to Siso/Ninja FAILED line
                    failed_regex = re.compile(r"FAILED: .*")
                    for line in result.stdout.split(
                            "\n") + result.stderr.split("\n"):
                        match = failed_regex.search(line)
                        if match:
                            error_msg = match.group(0)
                            break

                report_failure(index, patches, error_msg, diff, final_file)
            elif not run(f'gn check out/{platform}', exit_on_error=False):
                error_msg = "failed gn check"
                report_failure(index, patches, error_msg, diff, final_file)
                continue
            else:
                report_success(index, patches, "", diff, final_file)
    finally:
        # Regardless of success or failure we want to upload the scratch directory
        # to the shared google drive for easy debugging of either compile errors or
        # the evaluate_patches tool itself.
        unique_id = random.randint(1, 10000)
        file_name = f"{today_underscore}_evaluate_patches_{user}_{unique_id}.zip"
        upload_scratch(creds, file_name)
