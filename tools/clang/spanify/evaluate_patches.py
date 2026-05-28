#!/usr/bin/env python3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# ----------------------------------------------------------------------------
# A temporary script to evaluate how many patches produced by the tool compile.
# The output are stored on the Google internal spreadsheet:
#
# ----------------------------------------------------------------------------
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

from datetime import datetime
import argparse
import getpass
import os
import pathlib
import random
import re

import subprocess
import sys
from spanify_utils import scratch_dir


def strip_ansi(text):
    ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
    return ansi_escape.sub('', text)

from google.auth.transport.requests import Request
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from googleapiclient.discovery import build
from googleapiclient.errors import HttpError
from googleapiclient.http import MediaFileUpload

# common gn args for spanify project scripts.
from gnconfigs import GnConfigs, GenerateGnTarget
from project import PROJECTS

GOOGLE_DRIVE_FOLDER_ID = '18lLW_YPCXRGUYiPghDXTA6h2TC4WpGeJ'


def run(command, error_message=None, exit_on_error=True, cwd=None):
    """
    Helper function to run a shell command.
    """
    print('-' * 80)
    if sys.stdout.isatty():
        print(f"\033[7m{command}\033[0m")
    else:
        print(command)
    try:
        output = subprocess.run(command,
                                shell=True,
                                check=True,
                                text=True,
                                cwd=cwd)

    except subprocess.CalledProcessError as e:

        print(error_message if error_message else "Failed to run command: `" +
              command + "`",
              file=sys.stderr)
        if exit_on_error:
            raise e
        return False

    return True


def get_current_branch(cwd=None):
    """
    Returns the current branch name or revision.
    """
    # --abbrev-ref HEAD returns the branch name or "HEAD"
    result = subprocess.run("git rev-parse --abbrev-ref HEAD",
                            shell=True,
                            check=True,
                            text=True,
                            capture_output=True,
                            cwd=cwd)
    branch = result.stdout.strip()
    if branch != "HEAD":
        return branch

    result = subprocess.run("git rev-parse HEAD",
                            shell=True,
                            check=True,
                            text=True,
                            capture_output=True,
                            cwd=cwd)
    return result.stdout.strip()


def restore(cwd):
    """
    Restores the staged and unstaged working tree changes of the repository.
    """
    run("git restore --staged .",
        "Failed to restore staged repository",
        cwd=cwd)
    run("git restore .", "Failed to restore repository", cwd=cwd)


def analyze_error(stdout, stderr):
    """
    Analyzes compiler output to extract a clean, readable error message.
    """
    # Errors format: <file>:<line>:<column>: [fatal] error: <error_msg>
    error_regex = re.compile(r"([^:]+:\d+(?::\d+)?: (?:fatal )?error: .*)")
    for line in stdout.split("\n") + stderr.split("\n"):
        match = error_regex.search(line)
        if match:
            return match.group(1)

    # Fallback to Siso/Ninja FAILED line
    failed_regex = re.compile(r"FAILED: .*")
    for line in stdout.split("\n") + stderr.split("\n"):
        match = failed_regex.search(line)
        if match:
            return match.group(0)

    # Generic fallback: grab the first non-empty, non-utility line of output
    for line in (stderr + "\n" + stdout).split("\n"):
        if (line.strip() and not line.startswith("ninja: Entering directory")
                and not line.startswith("shutdown cloud logging")):
            return line.strip()

    return "Unknown compilation error"


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
        file_metadata = {"name": name, "parents": [GOOGLE_DRIVE_FOLDER_ID]}
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
                       total_patches, user, error_msg, diff, final_file):
    with (scratch_dir() / "evaluation.csv").open("a") as f:
        f.write(f"{index}, {result}, {error_msg}\n")
    try:
        append_row(spreadsheet, spreadsheet_id, [
            today,
            index,
            total_patches,
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
                total_patches,
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

    root_dir = pathlib.Path(__file__).resolve().parents[3]
    os.environ['CHROMIUM_BUILDTOOLS_PATH'] = str(root_dir / 'buildtools')

    platform = args.platform
    patch_limit = args.patch_limit
    project = args.project
    spreadsheet_id = PROJECTS[project]['spreadsheet_id']
    rewrite_project = project
    submodule = PROJECTS[project].get('submodule', '.')
    standalone = submodule != '.'
    run_gn_check = PROJECTS[project].get('run_gn_check', True)

    print(f"Running evaluate_patches.py for project {project}...")

    # Produce a full rewrite, and store individual patches below <scratch>/patch_*
    run(f"./tools/clang/spanify/rewrite_multiple_platforms.py \
        --platform={platform} \
        --project={rewrite_project} \
        ")

    # Record the starting branch/revision to restore it at the end.
    original_top_branch = get_current_branch()
    original_submodule_branch = get_current_branch(submodule)

    today = datetime.now().strftime("%Y/%m/%d")
    today_underscore = today.replace("/", "_")
    creds = get_google_creds()
    spreadsheet = get_spreadsheet(creds)
    user = getpass.getuser()

    def report_success(index, total_patches, error_msg, diff, final_file):
        report_case_result("pass", spreadsheet, spreadsheet_id, today, index,
                           total_patches, user, error_msg, diff, final_file)

    def report_failure(index, total_patches, error_msg, diff, final_file):
        report_case_result("fail", spreadsheet, spreadsheet_id, today, index,
                           total_patches, user, error_msg, diff, final_file)

    cwd = submodule if standalone else None

    # Generate build files
    configs = GnConfigs(False)
    gn_args = configs.get_config(platform, project)

    if gn_args is None:
        print(
            f"Error: Unknown platform/project combination: {platform}/{project}"
        )
        sys.exit(1)

    try:
        run("gcertstatus --check_remaining=3h --nocheck_ssh")
        if project in ['chrome', 'partition_alloc']:
            print("Remote exec available. Enabling.")
            gn_args = GnConfigs(True).get_config(platform, project) or gn_args
        else:
            print(
                "Submodule project. Keeping remote exec disabled to avoid ACL issues."
            )

        gn_path = root_dir / 'buildtools/linux64/gn'

        # Ensure the build directory exists and write GN args directly to args.gn
        # to avoid shell escaping and quoting issues.
        out_dir = (pathlib.Path(cwd)
                   if cwd else pathlib.Path(".")).resolve() / f"out/{platform}"
        out_dir.mkdir(parents=True, exist_ok=True)
        (out_dir / "args.gn").write_text('\n'.join(gn_args))

        # We manually run gn gen to ensure it's in the right directory.
        run(f"{gn_path} gen out/{platform}",
            f"Failed to generate out/{platform}.",
            cwd=cwd)
    except Exception as e:
        print(f"Remote exec not available or failed: {e}. Disabling.")
        gn_args = GnConfigs(False).get_config(platform, project)
        if gn_args is None:
            print(
                f"Error: Unknown platform/project combination: {platform}/{project}"
            )
            sys.exit(1)

        gn_path = root_dir / 'buildtools/linux64/gn'

        # Ensure the build directory exists and write GN args directly to args.gn
        # to avoid shell escaping and quoting issues.
        out_dir = (pathlib.Path(cwd)
                   if cwd else pathlib.Path(".")).resolve() / f"out/{platform}"
        out_dir.mkdir(parents=True, exist_ok=True)
        (out_dir / "args.gn").write_text('\n'.join(gn_args))

        run(f"{gn_path} gen out/{platform}",
            f"Failed to generate out/{platform}.",
            cwd=cwd)

    run(f'git rev-parse HEAD > "{(scratch_dir() / "git_revision.txt")}"',
        cwd=submodule)

    # Restore the workspace to a clean state, excluding the spanify tools directory.
    restore(cwd)

    patches = [p.name for p in scratch_dir().glob("patch_*.txt")]

    # Sort numerically the patches to evaluate.
    patches.sort(key=lambda x: int(x.split("_")[1].split(".")[0]))
    total_patches_count = len(patches)
    patches = patches[:patch_limit]

    print(
        f"Found {total_patches_count} patches to evaluate (limiting to {len(patches)})."
    )

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
    run(f"autoninja -C out/{platform}",
        "Failed to build the project. Ensure the output directory and targets are correct.",
        exit_on_error=True,
        cwd=cwd)


    # Create and evaluate patches
    try:
        for patch in patches:
            # Extract the patch index from the patch name string: patch_{index}.txt
            index = int(patch.split("_")[1].split(".")[0])

            print(f"Producing patch {index}/{len(patches)}: {patch}")

            diff = ""
            final_file = ""
            try:
                # Apply edits
                apply_edits_path = os.path.abspath(
                    "tools/clang/scripts/apply_edits.py")
                cmd = [
                    sys.executable,
                    apply_edits_path,
                    "-p",
                    f"out/{platform}",
                ]
                try:
                    with open(scratch_dir() / patch, "rb") as patch_f:
                        result = subprocess.run(
                            cmd,
                            stdin=patch_f,
                            capture_output=True,
                            text=True,
                            cwd=cwd,
                            check=True,
                        )
                except subprocess.CalledProcessError as e:
                    stdout_clean = strip_ansi(e.stdout)
                    stderr_clean = strip_ansi(e.stderr)
                    error_msg = ("\"" + str(e) + " !!! exception(stderr): " +
                                 stderr_clean + "\"")

                    run(f'git diff > "{(scratch_dir() / f"patch_{index}.diff")}"',
                        cwd=cwd)
                    diff = (scratch_dir() / f"patch_{index}.diff").read_text()
                    final_file = stderr_clean + "\n" + stdout_clean

                    report_failure(index, total_patches_count, error_msg, diff,
                                   final_file)
                    continue

                run("git cl format", cwd=cwd, exit_on_error=False)

                # Serialize changes (Generate diff directly from working tree)
                run(f'git diff > "{(scratch_dir() / f"patch_{index}.diff")}"',
                    cwd=cwd)
                diff = (scratch_dir() / f"patch_{index}.diff").read_text()

                if not diff.strip():
                    # We fail when there is no diff, get the replacements instead
                    diff = (scratch_dir() / f"patch_{index}.txt").read_text()
                    final_file = strip_ansi(result.stderr) + "\n" + strip_ansi(
                        result.stdout)
                    report_failure(index, total_patches_count,
                                   "No files modified by patch", diff,
                                   final_file)
                    continue

                # Build and evaluate
                print(f"Evaluating patch {index}/{len(patches)}")
                print("Building...")

                result = subprocess.run(f"time autoninja -C out/{platform}",
                                        shell=True,
                                        capture_output=True,
                                        text=True,
                                        cwd=cwd)

                stdout_clean = strip_ansi(result.stdout)
                stderr_clean = strip_ansi(result.stderr)
                final_file = stderr_clean + "\n" + stdout_clean
                with (scratch_dir() / f"patch_{index}.out").open("w+") as f:
                    f.write(stderr_clean)
                    f.write(stdout_clean)

                if result.returncode != 0:
                    error_msg = analyze_error(stdout_clean, stderr_clean)
                    report_failure(index, total_patches_count, error_msg, diff,
                                   final_file)
                elif run_gn_check and not run(f'gn check out/{platform}',
                                              exit_on_error=False,
                                              cwd=cwd):
                    error_msg = "failed gn check"
                    report_failure(index, total_patches_count, error_msg, diff,
                                   final_file)
                else:
                    report_success(index, total_patches_count, "", diff,
                                   final_file)
            finally:
                # Unconditionally restore the workspace to clean for the next patch
                restore(cwd)
    finally:
        # Restore the starting branch/revision.
        run(f"git checkout {original_submodule_branch}",
            exit_on_error=False,
            cwd=submodule)
        run(f"git checkout {original_top_branch}", exit_on_error=False)

        # Regardless of success or failure we want to upload the scratch directory
        # to the shared google drive for easy debugging of either compile errors or
        # the evaluate_patches tool itself.
        unique_id = random.randint(1, 10000)
        file_name = f"{today_underscore}_evaluate_patches_{user}_{unique_id}.zip"
        upload_scratch(creds, file_name)
