#!/usr/bin/env python3

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Python port of upload-rewrites.sh.

Refines automatic C++ spanification, runs memory safety / modernization
gemini-cli configurations, commits compiler fixes, calculates git diff stats,
and uploads to Gerrit.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from project import PROJECTS
from spanify_utils import scratch_dir

SCRIPT_DIR = None
TEMPLATE_PATH = None
PATCHES_CSV = None
PROMPT_DIR = None
UNSAFE_BUFFERS_DOCS = None


def initialize_globals(base_dir=None):
    """Initializes global paths and variables."""
    global SCRIPT_DIR, TEMPLATE_PATH, PATCHES_CSV, PROMPT_DIR
    global UNSAFE_BUFFERS_DOCS
    if base_dir is None:
        base_dir = os.path.dirname(os.path.abspath(__file__))
    SCRIPT_DIR = base_dir
    TEMPLATE_PATH = os.path.join(SCRIPT_DIR, "upload-prompt-template.txt")
    PATCHES_CSV = os.path.expanduser(f"{scratch_dir()}/patches.csv")
    PROMPT_DIR = f"{scratch_dir()}/prompts"

    # Fetch the content of docs/unsafe_buffers.md directly from the repository
    unsafe_buffers_path = os.path.abspath(
        os.path.join(SCRIPT_DIR, "../../../docs/unsafe_buffers.md"))
    if os.path.exists(unsafe_buffers_path):
        with open(unsafe_buffers_path, "r", encoding="utf-8") as f:
            UNSAFE_BUFFERS_DOCS = f.read()
    else:
        UNSAFE_BUFFERS_DOCS = (
            "Reference docs can be found under: docs/unsafe_buffers.md\n"
            "URL: "
            "https://chromium.googlesource.com/chromium/src/+/HEAD/docs/"
            "unsafe_buffers.md")


def sh(cmd, check=False, cwd=None):
    """Helper to run shell commands safely."""
    res = subprocess.run(cmd, shell=True, capture_output=True, cwd=cwd)
    stdout_str = res.stdout.decode("utf-8", errors="replace")
    stderr_str = res.stderr.decode("utf-8", errors="replace")

    class CommandResult:

        def __init__(self, returncode, stdout, stderr):
            self.returncode = returncode
            self.stdout = stdout
            self.stderr = stderr

    result = CommandResult(res.returncode, stdout_str, stderr_str)

    if check and result.returncode != 0:
        raise Exception(f"Command failed: {cmd}\n"
                        f"exit code: {result.returncode}\n"
                        f"stdout: {result.stdout}\n"
                        f"stderr: {result.stderr}")
    return result


def cleanup_prompt_files():
    """Deletes any leftover prompt files in the temp directory
    to start fresh."""
    try:
        if os.path.exists(PROMPT_DIR):
            for filename in os.listdir(PROMPT_DIR):
                if filename.startswith("tmp_prompt_") and filename.endswith(
                        ".txt"):
                    path = os.path.join(PROMPT_DIR, filename)
                    try:
                        os.remove(path)
                    except Exception as e:
                        print(f"""Warning: Failed to delete leftover prompt
                            {path}: {e}""")
    except Exception as e:
        print(f"Warning: Failed to scan prompt directory for cleanup: {e}")


def find_build_root(start_dir, out_name="out/linux"):
    """Traverses upwards to find a parent directory containing out_name."""
    curr = os.path.abspath(start_dir)
    while True:
        potential = os.path.join(curr, out_name)
        if os.path.isdir(potential):
            return curr
        parent = os.path.dirname(curr)
        if parent == curr:
            break
        curr = parent
    return None


def get_target(project):
    """Returns the ninja target to compile for the given project."""
    if project == "dawn":
        return "all"
    return project


def fetch_and_filter_patches(start_branch_num):
    """Fetches and filters patches based on the start branch number."""
    print("Fetching and filtering patches...")
    patches = [p.name for p in scratch_dir().glob("patch_*.txt")]
    # Sort patches by index to ensure they are processed in order.
    patches.sort(key=lambda x: int(x.split("_")[1].split(".")[0]))
    if start_branch_num > 0:
        filtered_patches = []
        for patch in patches:
            match = re.search(r"patch_(\d+)\.txt", patch)
            if match:
                num = int(match.group(1))
                if num >= start_branch_num:
                    filtered_patches.append(patch)
        patches = filtered_patches
    return patches


def get_patch_index(patch):
    return int(patch.split("_")[1].split(".")[0])


def apply_patch(patch, platform, submodule, sub_main):
    """Applies the patch to the current branch."""
    print(f"Applying patch {patch}...")
    apply_edits_path = os.path.abspath("tools/clang/scripts/apply_edits.py")
    cmd = [
        sys.executable,
        apply_edits_path,
        "-p",
        f"out/{platform}",
    ]
    patch_file_path = scratch_dir() / patch
    try:
        with open(patch_file_path, "rb") as patch_f:
            subprocess.run(
                cmd,
                stdin=patch_f,
                capture_output=True,
                text=True,
                check=True,
                cwd=submodule,
            )
    except subprocess.CalledProcessError as e:
        print(f"ERROR: Failed to apply patch {patch}: {e}")
        print(e.stderr)
        sh(f"git checkout {sub_main}", cwd=submodule)
        return False
    return True


def format_and_check_changes(patch, submodule, sub_main):
    """Formats the applied changes and checks if any changes were introduced."""
    print("Formatting changes...")
    sh("git cl format", cwd=submodule)

    # Check if changes were introduced
    diff_quiet = sh("git diff --quiet", cwd=submodule)
    if diff_quiet.returncode == 0:
        print(f"WARNING: Patch {patch} did not produce any changes. Skipping.")
        sh(f"git checkout {sub_main}", cwd=submodule)
        return False
    return True


def use_submodule(project):
    """Returns the submodule path if it is used, otherwise empty string."""
    submodule = PROJECTS.get(project, {}).get("submodule")
    if submodule:
        print(f"Using submodule: {submodule}")
        # Ensure we checkout main branch in the submodule
        res_chk = sh("git checkout main", cwd=submodule)
        if res_chk.returncode != 0:
            sh("git checkout master", check=True, cwd=submodule)  # nocheck
        sh("git pull --rebase", check=True, cwd=submodule)
    return submodule


def commit_applied_edits(submodule):
    """Commits the applied edits to the current branch."""
    # Commit the applied edits
    print("Committing changes...")
    sh("git add -u -- ':!third_party'", cwd=submodule)
    sh('git commit -m "Apply automated spanification patch"',
       check=True,
       cwd=submodule)


def get_modified_files(submodule, sub_main, branch):
    """Returns the list of modified files in the current branch."""
    # Find modified files list compared to the latest upstream main base
    files_res = sh(f"git diff --name-only origin/{sub_main}...{branch}",
                   cwd=submodule)
    return " ".join(files_res.stdout.splitlines()).strip()


def create_prompt_file(files, submodule, branch, sub_main):
    """Creates the prompt file with target files and git diff."""
    # Capture full git diff compared to upstream main base
    diff_res = sh(f"git diff origin/{sub_main}...{branch}", cwd=submodule)
    git_diff = diff_res.stdout

    # Load prompt template
    if not os.path.exists(TEMPLATE_PATH):
        print(f"ERROR: Prompt template file not found at {TEMPLATE_PATH}")
        sh(f"git checkout {sub_main}", cwd=submodule)
        return [False, False]

    with open(TEMPLATE_PATH, "r") as template_file:
        prompt = template_file.read()

    # Safe substitutions of dynamic context data
    working_dir = os.path.join(os.getcwd(),
                               submodule) if submodule else os.getcwd()
    prompt = prompt.replace("{{TARGET_FILES}}", files)
    prompt = prompt.replace("{{GIT_DIFF}}", git_diff)
    prompt = prompt.replace("{{UNSAFE_BUFFERS_DOCS}}", UNSAFE_BUFFERS_DOCS)

    # Write prompt contents to a temporary file inside the jetski temp
    # directory to ensure it is never committed
    os.makedirs(PROMPT_DIR, exist_ok=True)
    with tempfile.NamedTemporaryFile(mode="w+",
                                     prefix="tmp_prompt_",
                                     suffix=".txt",
                                     delete=False,
                                     encoding="utf-8",
                                     dir=PROMPT_DIR) as f_prompt:
        f_prompt.write(prompt)
        prompt_file_path = f_prompt.name
    return [prompt_file_path, working_dir]


def call_jetski_cli(prompt_file_path, working_dir, model, abs_working_dir):
    """Calls jetski-cli with the prompt file and returns the result."""
    # Try to find 'jetski' in PATH, otherwise use default Cloudtop path
    jetski_path = "jetski"
    if not shutil.which("jetski"):
        cloudtop_path = "/google/bin/releases/jetski-devs/tools/cli"
        if os.path.exists(cloudtop_path):
            jetski_path = cloudtop_path

    # 1. Create a safe, isolated temporary directory for
    # this run's configuration
    with tempfile.TemporaryDirectory(
            prefix="jetski_config_") as temp_gemini_dir:

        # 2. Replicate the expected internal layout:
        # <gemini_dir>/<app_data_dir>/cli/
        # --gemini_dir defaults to '.gemini' and
        # --app_data_dir defaults to 'jetski'
        settings_dir = os.path.join(temp_gemini_dir, "jetski", "cli")
        os.makedirs(settings_dir, exist_ok=True)
        settings_file_path = os.path.join(settings_dir, "settings.json")

        # 3. Define the localized fine-grained permissions
        settings_data = {
            "permissions": {
                "allow": [
                    "command(git status)",
                    "command(git branch)",
                    "command(git diff)",
                    "command(autoninja)",
                    "command(gn)",
                    "command(pwd)",
                    "command(ls)",
                    "command(tools/autotest.py)",
                    "command(echo)",
                    "command(cat)",
                    "command(grep)",
                    "command(diff)",
                    "command(clang-format)",
                    f"read_file({abs_working_dir})",
                    f"write_file({abs_working_dir})",
                ]
            }
        }

        try:
            # 4. Write the settings file directly into our sandboxed directory
            with open(settings_file_path, "w", encoding="utf-8") as f:
                json.dump(settings_data, f, indent=2)
        except Exception as e:
            print(f"ERROR: Failed to write settings.json: {e}")
        # 5. Execute the binary using the native --gemini_dir flag override
        jetski_cmd = [
            jetski_path,
            "--gemini_dir",
            os.path.abspath(temp_gemini_dir),
            "--model",
            model,
            "-p",
            f"@{prompt_file_path}",
        ]

        try:
            print("Running jetski-cli with native sandbox "
                  f"path: {temp_gemini_dir}")
            subprocess.run(jetski_cmd, text=True, cwd=working_dir, check=True)
        finally:
            if os.path.exists(prompt_file_path):
                os.remove(prompt_file_path)


def compile_branch(platform, target, submodule):
    """Compiles the branch and returns the result."""
    print(f"Compiling the branch to verify Jetski fixes... in {submodule}")
    out_dir = f"out/{platform}"

    compile_res = subprocess.run(f"autoninja -C {out_dir} {target}",
                                 shell=True,
                                 cwd=submodule)
    compile_success = (compile_res.returncode == 0)
    compile_result = "SUCCESS" if compile_success else "ERROR"
    print(f"Compilation status: {compile_result}")
    return compile_result


def commit_if_changes(submodule):
    """Commits changes if any were introduced."""
    diff_quiet = sh("git diff --quiet -- ':!third_party'", cwd=submodule)
    if diff_quiet.returncode != 0:
        print("Formatting Jetski fixes...")
        sh("git cl format", cwd=submodule)
        print("Jetski made fixes. Committing changes...")
        sh("git add -u -- ':!third_party'", cwd=submodule)
        sh('git commit -m "Jetski auto-fixes for compilation"',
           check=True,
           cwd=submodule)


def compute_diff_stats(submodule, sub_main, branch):
    """Calculate final modifications compared to upstream main
    after fixes were applied."""
    final_files_res = sh(f"git diff --name-only origin/{sub_main}...{branch}",
                         cwd=submodule)
    first_file = ""
    files_list = [
        line.strip() for line in final_files_res.stdout.splitlines()
        if line.strip()
    ]
    num_files = len(files_list)
    if files_list:
        first_file = files_list[0]

    # Capture git shortstat to calculate progress
    shortstat_res = sh(f"git diff origin/{sub_main}...{branch} --shortstat",
                       cwd=submodule)
    shortstat = shortstat_res.stdout.strip()
    plus_delta = 0
    minus_delta = 0

    if shortstat:
        plus_match = re.search(r"(\d+)\s+insertion", shortstat)
        if plus_match:
            plus_delta = int(plus_match.group(1))
        minus_match = re.search(r"(\d+)\s+deletion", shortstat)
        if minus_match:
            minus_delta = int(minus_match.group(1))

    total_delta = plus_delta + minus_delta
    return [plus_delta, minus_delta, total_delta, num_files, first_file]


def append_row(branch, gerrit_url, compile_result, plus_delta, minus_delta,
               total_delta, num_files):
    """Appends a row to the specified Google Sheet."""
    # patch source identifier
    with open(PATCHES_CSV, "a") as f_csv:
        f_csv.write(f'"{branch}","{gerrit_url}",{compile_result},'
                    f'{plus_delta},{minus_delta},{total_delta},'
                    f'{num_files}\n')


def upload_to_gerrit(submodule, branch, bug_number, compile_result, plus_delta,
                     minus_delta, total_delta, num_files, first_file,
                     fixed_text, patch):
    """Uploads the branch to Gerrit with the specified bug number."""
    # Use the current patch file name as original patch info
    original_patch = f"Original patch: {patch}"

    # Compose complete commit description
    commit_msg = (f"Spanification of {first_file}, etc.\n\n"
                  f"{fixed_text}\n\n"
                  f"{original_patch}\n\n"
                  f"Bug: {bug_number}")

    # Handle Gerrit upload securely using temporary message file
    with tempfile.NamedTemporaryFile(mode="w+", suffix=".txt",
                                     delete=False) as f_msg:
        f_msg.write(commit_msg)
        f_msg_path = f_msg.name

    try:
        print(f"Uploading {branch} to Gerrit...")
        upload_cmd = [
            "git",
            "cl",
            "upload",
            "--force",
            "--squash",
            "--no-autocc",
            "--bypass-watchlists",
            "--dry-run",
            "--message-file",
            f_msg_path,
        ]
        process = subprocess.Popen(upload_cmd,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT,
                                   text=True,
                                   cwd=submodule)
        upload_output_list = []
        while True:
            line = process.stdout.readline()
            if not line and process.poll() is not None:
                break
            if line:
                sys.stdout.write(line)
                sys.stdout.flush()
                upload_output_list.append(line)
        returncode = process.wait()
        upload_output = "".join(upload_output_list)

        if returncode == 0:
            print(f"SUCCESS: {branch} uploaded")
            gerrit_url_match = re.search(
                r"https://[-a-zA-Z0-9.]+-review\.googlesource\.com/c/[^\s]+",
                upload_output,
            )
            if gerrit_url_match:
                gerrit_url = gerrit_url_match.group(0).strip()
                cl_number = os.path.basename(gerrit_url)
                print(f"Gerrit CL URL: {gerrit_url}")
                print(f"Gerrit CL Number: {cl_number}")
                append_row(branch, gerrit_url, compile_result, plus_delta,
                           minus_delta, total_delta, num_files)
            else:
                print("WARNING: Could not parse Gerrit URL " +
                      "from upload output.")
        else:
            print(f"ERROR: git cl upload failed for branch {branch}")
            if "User has no Gerrit Account" in upload_output:
                print("\n" + "=" * 80)
                print("[ERROR] Gerrit Authentication Failure: "
                      "User has no Gerrit Account.")
                print("This usually means you need to register your "
                      "account on the Gerrit host.")
                print("Please visit the Gerrit UI in your browser and sign in "
                      "once to activate your account.")
                print("For Dawn, visit: https://dawn-review.googlesource.com/")
                print("For Chromium, visit: "
                      "https://chromium-review.googlesource.com/")
                print("\nOther troubleshooting steps:")
                print("1. Ensure you have a valid SSO ticket by running: "
                      "sso-client")
                print("2. Run 'git cl creds-check' in the repository to "
                      "verify your credentials.")
                print("=" * 80 + "\n")
    finally:
        if os.path.exists(f_msg_path):
            os.remove(f_msg_path)


def get_arguments():
    """Returns the arguments passed to the script."""
    parser = argparse.ArgumentParser(
        description=(
            "Refines automatic C++ spanification branches using jetski-cli,\n"
            "compiles changes locally to verify fixes, and uploads patchsets "
            "to Gerrit."),
        epilog=("How to run the command:\n"
                "  1. Run with standard/default bug option:\n"
                "     python3 upload-rewrites.py\n\n"
                "  2. Run with custom Buganizer/Gerrit bug number:\n"
                "     python3 upload-rewrites.py --bug 123456789\n"
                "     python3 upload-rewrites.py -b 123456789\n\n"
                "  3. Run starting from branch review/compilation index"
                "     (e.g., branch 40):\n"
                "     python3 upload-rewrites.py --start 40\n\n"
                "  4. Run selecting another platform:\n"
                "     python3 upload-rewrites.py --platform android"),
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "--bug_number",
        "-b",
        type=str,
        default="439964610",
        help="Gerrit/Buganizer bug number to include in the commit description"
        "(default: 439964610)")
    parser.add_argument(
        "--project",
        type=str,
        default="auto",
        help=
        "Project to select for spanification (e.g., chrome, partition_alloc, "
        "dawn, skia, angle; default: auto)")
    parser.add_argument(
        "--start_branch",
        "-s",
        type=int,
        default=0,
        help="The branch number to start iterating from "
        "(e.g., 40 to start from spanification_rewrite_evaluate_40)")
    parser.add_argument(
        "--platform",
        "-p",
        type=str,
        default="linux",
        help=
        "Platform to select for building/verifying changes (default: linux)")
    parser.add_argument(
        "--model",
        "-m",
        type=str,
        default="gemini-3-flash-preview",
        help="Model to use for jetski-cli (default: gemini-3-flash-preview)")
    return parser.parse_args()


def main():
    initialize_globals()
    args = get_arguments()
    bug_number = args.bug_number
    project = args.project
    start_branch = args.start_branch
    platform = args.platform
    model = args.model

    target = get_target(project)

    # Clean up any leftover prompt files in the temp directory
    cleanup_prompt_files()

    # Ensure patches.csv is prepared
    append_row("original_patch", "gerrit_url", "compile_result", "plus_delta",
               "minus_delta", "total_delta", "num_files")

    # # Switch to main branch first
    # sh("git checkout main", check=True)
    # sh("git pull --rebase && gclient sync -D", check=True)

    submodule = use_submodule(project)

    patches = fetch_and_filter_patches(start_branch)

    fixed_text = f"""This is the result of running the automatic spanification
on {platform} and updating code to use and pass spans where size is known.
The original patch was fully automated using script:
//tools/clang/spanify/rewrite-multiple-platforms.sh -platforms={platform}
Then refined with jetski-cli and at last manually refined"""

    for patch in patches:
        # Extract the patch index from the patch name string: patch_{index}.txt
        index = get_patch_index(patch)
        branch = f"spanification_rewrite_evaluate_{index}"

        print(f"\n--- Processing patch {patch} (branch: {branch}) ---")
        cleanup_prompt_files()

        # Ensure any stale rebase from a previous run is aborted before checkout
        sh("git rebase --abort", cwd=submodule)

        # Determine main branch of submodule or top-level repository
        sub_main = "main"
        if submodule:
            res_chk = sh("git rev-parse --verify main", cwd=submodule)
            if res_chk.returncode != 0:
                sub_main = "master"  # nocheck

        # Create/reset the branch off main
        res_checkout = sh(f"git checkout -B {branch} {sub_main}",
                          cwd=submodule)
        if res_checkout.returncode != 0:
            print(f"ERROR: Failed to checkout/create branch {branch}")
            continue

        # Apply edits
        patch_applied = apply_patch(patch, platform, submodule, sub_main)
        if not patch_applied:
            continue

        has_changes = format_and_check_changes(patch, submodule, sub_main)
        if not has_changes:
            continue

        commit_applied_edits(submodule)

        print("Invoking jetski-cli to fix potential compilation errors...")

        files = get_modified_files(submodule, sub_main, branch)

        # Create prompt file with target files and git diff
        [prompt_file_path,
         working_dir] = create_prompt_file(files, submodule, branch, sub_main)

        if not prompt_file_path:
            break

        abs_working_dir = os.path.abspath(working_dir)
        call_jetski_cli(prompt_file_path, working_dir, model, abs_working_dir)

        commit_if_changes(submodule)

        compile_result = compile_branch(platform, target, submodule)

        [plus_delta, minus_delta, total_delta, num_files,
         first_file] = compute_diff_stats(submodule, sub_main, branch)

        if num_files == 0:
            print(f"WARNING: No diff found between {branch} and main."
                  "Skipping upload.")
            sh(f"git checkout {sub_main}", cwd=submodule)
            continue

        upload_to_gerrit(submodule, branch, bug_number, compile_result,
                         plus_delta, minus_delta, total_delta, num_files,
                         first_file, fixed_text, patch)

        cleanup_prompt_files()

    cleanup_prompt_files()
    print("\n--- Finished processing branches ---")


if __name__ == "__main__":
    main()
