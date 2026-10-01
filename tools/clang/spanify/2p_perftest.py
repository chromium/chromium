#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to run Pinpoint performance tests for 2P library CLs using
DEPS-based workflow.

This script modifies the Chromium DEPS file to inject hooks that fetch and apply
the 2P library patch, uploads this temporary DEPS change to Gerrit, and then
triggers Pinpoint using the generated Chromium CL.

Assumptions:
  Both Chromium and its submodules must not have any uncommitted, tracked
  changes. If you have local changes in Chromium (e.g. adaptation fixes for
  the 2P API change),
  you must commit them locally on your branch before running this script.

Examples:
  1. Compare a 2P CL against Chromium main (HEAD):
     ./tools/clang/spanify/2p_perftest.py \
       --exp-cl https://skia-review.googlesource.com/c/skia/+/1256436 \
       --benchmark speedometer3 \
       --bot mac-m1_mini_2020-perf \
       --story Speedometer3 \
       --attempts 150

  2. Compare two different 2P CLs against each other:
     ./tools/clang/spanify/2p_perftest.py \
       --exp-cl https://skia-review.googlesource.com/c/skia/+/1256436 \
       --base-cl https://skia-review.googlesource.com/c/skia/+/1256430 \
       --benchmark speedometer3 \
       --bot mac-m1_mini_2020-perf \
       --story Speedometer3 \
       --attempts 150
"""

import argparse
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import time
import urllib.parse
import urllib.request

CL_TAG = "pinpoint-tmp"
TEMP_BRANCH_PREFIX = "pinpoint-deps-exp-"
COMMIT_MSG_TEMPLATE = "Temp DEPS roll for Pinpoint: {project} CL {change_num}"


class GitHelper:
    def __init__(self, cwd, dry_run=False):
        self.cwd = cwd
        self.dry_run = dry_run

    def run_cmd(self, cmd):
        if self.dry_run:
            print(f"[DRY RUN] Would run: {' '.join(cmd)} (cwd: {self.cwd})")
            return 0, "", ""

        try:
            result = subprocess.run(
                cmd, cwd=self.cwd, capture_output=True, text=True, check=True
            )
            return result.returncode, result.stdout, result.stderr
        except subprocess.CalledProcessError as e:
            print(
                f"Command failed: {' '.join(cmd)}\n"
                f"Stdout: {e.stdout}\n"
                f"Stderr: {e.stderr}",
                file=sys.stderr,
            )
            raise

    def is_clean(self):
        if self.dry_run:
            return True
        _, stdout, _ = self.run_cmd(["git", "status", "--porcelain", "-uno"])
        return len(stdout.strip()) == 0

    def get_current_branch(self):
        if self.dry_run:
            return "main"
        _, stdout, _ = self.run_cmd(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"]
        )
        return stdout.strip()

    def create_and_checkout_branch(self, branch_name):
        self.run_cmd(["git", "checkout", "-b", branch_name, "origin/main"])

    def cherry_pick_local_commits(self, original_branch):
        if self.dry_run:
            print(
                f"[DRY RUN] Would cherry-pick local commits from "
                f"{original_branch} and clean up tools/clang/spanify/"
            )
            return

        # Find commits on original_branch that are not in origin/main
        _, stdout, _ = self.run_cmd(
            ["git", "log", f"origin/main..{original_branch}", "--format=%H"]
        )
        commits = stdout.strip().splitlines()
        if not commits:
            return

        print(f"Found local commits to cherry-pick: {len(commits)}")
        # Cherry-pick them in chronological order (reverse of git log output)
        for commit in reversed(commits):
            try:
                print(f"Cherry-picking local commit: {commit}")
                self.run_cmd(["git", "cherry-pick", commit])
            except Exception as e:
                print(
                    f"Error cherry-picking commit {commit}. You may need to "
                    "rebase your branch.",
                    file=sys.stderr,
                )
                raise

        # Revert any changes to tools/clang/spanify/ to keep the CL clean of
        # script/scratch changes
        print(
            "Cleaning up tools/clang/spanify/ changes from temporary branch..."
        )
        self.run_cmd(
            ["git", "checkout", "origin/main", "--", "tools/clang/spanify/"]
        )

        # Commit the revert if there are changes
        _, stdout, _ = self.run_cmd(
            ["git", "status", "--porcelain", "tools/clang/spanify/"]
        )
        if stdout.strip():
            self.run_cmd(
                [
                    "git",
                    "commit",
                    "-m",
                    "Revert script/scratch changes",
                    "--no-verify",
                ]
            )

    def checkout_branch(self, branch_name):
        self.run_cmd(["git", "checkout", branch_name])

    def delete_branch(self, branch_name):
        self.run_cmd(["git", "branch", "-D", branch_name])

    def commit_all_changes(self, message):
        self.run_cmd(["git", "add", "-u"])
        self.run_cmd(["git", "commit", "-m", message])

    def upload_cl(self, message):
        if self.dry_run:
            print("[DRY RUN] Would upload CL to Gerrit")
            # Return a mock URL that looks like a Chromium CL
            return "https://chromium-review.googlesource.com/c/chromium/src/+/999999"

        description = message + f"\n\nTAG={CL_TAG}"

        cmd = [
            "git",
            "cl",
            "upload",
            "-m",
            description,
            "--bypass-hooks",
            "-f",
        ]
        _, stdout, stderr = self.run_cmd(cmd)

        output = stdout + "\n" + stderr
        match = re.search(
            r"https://chromium-review\.googlesource\.com/c/chromium/src/\+/(\d+)",
            output,
        )
        if match:
            return match.group(0)

        raise RuntimeError(
            f"Could not find uploaded CL URL in git cl output:\n{output}"
        )


def normalize_gerrit_host(host):
    if host.endswith('.git.corp.google.com'):
        return host.replace('.git.corp.google.com', '.googlesource.com')
    return host


def get_latest_patchset(gerrit_host, project, change_num):
    gerrit_host = normalize_gerrit_host(gerrit_host)
    encoded_project = urllib.parse.quote_plus(project)
    url = f"https://{gerrit_host}/changes/{encoded_project}~{change_num}?o=CURRENT_REVISION"
    try:
        # We use a short timeout to avoid hanging
        with urllib.request.urlopen(url, timeout=10) as response:
            content = response.read().decode("utf-8")
            if content.startswith(")]}'"):
                content = content[4:]
            data = json.loads(content)
            current_revision = data.get("current_revision")
            revisions = data.get("revisions", {})
            if current_revision in revisions:
                return revisions[current_revision].get("_number")
    except Exception as e:
        print(
            f"WARNING: Failed to get latest patchset from Gerrit: {e}",
            file=sys.stderr,
        )
    return None


def get_related_changes(gerrit_host, project, change_num):
    gerrit_host = normalize_gerrit_host(gerrit_host)
    encoded_project = urllib.parse.quote_plus(project)
    url = f"https://{gerrit_host}/changes/{encoded_project}~{change_num}/revisions/current/related"
    try:
        with urllib.request.urlopen(url, timeout=10) as response:
            content = response.read().decode("utf-8")
            if content.startswith(")]}'"):
                content = content[4:]
            data = json.loads(content)
            return data.get("changes", [])
    except Exception as e:
        print(
            f"WARNING: Failed to get related changes from Gerrit: {e}",
            file=sys.stderr,
        )
    return []


def fetch_open_parents(gerrit_host, project, change_num):
    """Fetches related changes and returns only the open parents of the
    target CL.
    """
    print("Querying Gerrit for related changes (chains)...")
    related_changes = get_related_changes(gerrit_host, project, change_num)
    for i, change in enumerate(related_changes):
        if change.get("_change_number") == change_num:
            # Parents are after the target CL in the list
            return [
                c for c in related_changes[i + 1 :] if c.get("status") == "NEW"
            ]
    return []


def parse_cl_url(url):
    parsed = urllib.parse.urlparse(url)
    gerrit_host = parsed.netloc
    path = parsed.path

    # Gerrit URL format: /c/project/+/change_num[/patchset]
    match = re.match(r"^/c/(.+)/\+/(\d+)(?:/(\d+))?/?$", path)
    if not match:
        raise ValueError(f"Invalid Gerrit CL URL: {url}")

    project = match.group(1)
    change_num = int(match.group(2))
    patchset = match.group(3)

    if patchset:
        patchset = int(patchset)
    else:
        patchset = get_latest_patchset(gerrit_host, project, change_num)
        if not patchset:
            print(
                "WARNING: Could not resolve latest patchset, defaulting to 1",
                file=sys.stderr,
            )
            patchset = 1

    # Gerrit shards change references by the last two digits of the change
    # number
    # to avoid directory scaling issues on the git server.
    # i.e.
    # Change 1257717 ends in 17, so its ref is under refs/changes/17/1257717/...
    # Change 1257705 ends in 05, so its ref is under refs/changes/05/1257705/...

    git_ref = f"refs/changes/{change_num % 100:02d}/{change_num}/{patchset}"

    return gerrit_host, project, change_num, patchset, git_ref


def normalize_project_name(url):
    parsed = urllib.parse.urlparse(url)
    path = parsed.path
    if path.startswith("/c/"):
        path = path[3:]
    elif path.startswith("/"):
        path = path[1:]
    if "/+/" in path:
        path = path.split("/+/")[0]
    if path.endswith(".git"):
        path = path[:-4]
    return path


def find_dep_path_for_cl(deps_content, cl_url):
    """Finds the local checkout path for a given CL URL in the DEPS file.

    Examples of matches this pattern will find:

    1. Skia
       CL URL: https://skia-review.googlesource.com/c/skia/+/1257718
       target_project: "skia"
       Matches in DEPS:
         'src/third_party/skia':
           Var('skia_git') + '/skia.git' + '@' +  Var('skia_revision'),
       Returns: "src/third_party/skia"

    2. Dawn
       CL URL: https://dawn-review.googlesource.com/c/dawn/+/314495
       target_project: "dawn"
       Matches in DEPS:
         'src/third_party/dawn':
           Var('chromium_git') + '/dawn.git' + '@' + 'refs/heads/main',
       Returns: "src/third_party/dawn"

    3. Angle
       CL URL: https://chromium-review.googlesource.com/c/angle/angle/+/12345
       target_project: "angle/angle"
       Matches in DEPS:
         'src/third_party/angle':
           Var('chromium_git') + '/angle/angle.git' + '@' + 'refs/heads/main',
       Returns: "src/third_party/angle"
    """
    target_project = normalize_project_name(cl_url)
    pattern = (
        rf"['\"]"  # Matches opening quote (' or ")
        rf"(src/[^'\"]+)"  # Captures the dependency path starting with src/
        rf"['\"]"  # Matches closing quote (' or ")
        rf"\s*:\s*"  # Matches colon separator with optional spacing
        rf"[^,]+?"  # Non-greedily matches value up to the project URL
        rf"{re.escape(target_project)}\.git"  # Matches the target project's git repository
    )
    match = re.search(pattern, deps_content)
    if match:
        return match.group(1)
    return None


def modify_deps(deps_path, deps_content, repo_path, repo_url, git_refs):
    lines = deps_content.splitlines()
    hooks_start_idx = None
    for i, line in enumerate(lines):
        # Find the line starting the 'hooks' list: e.g., "hooks = ["
        if re.match(r"^hooks\s*=\s*\[", line.strip()):
            hooks_start_idx = i
            break

    if hooks_start_idx is None:
        raise RuntimeError("Could not find 'hooks' definition in DEPS")

    # Count brackets line-by-line to find the matching closing bracket ']'
    bracket_count = 0
    hooks_end_idx = None
    for i in range(hooks_start_idx, len(lines)):
        # Strip comments to avoid counting brackets in comments
        clean_line = lines[i].split("#")[0]
        bracket_count += clean_line.count("[")
        bracket_count -= clean_line.count("]")
        if bracket_count == 0:
            hooks_end_idx = i
            break

    if hooks_end_idx is None:
        raise RuntimeError("Could not find the end of 'hooks' list in DEPS")

    new_hooks_list = []
    for idx, git_ref in enumerate(git_refs):
        new_hooks_list.append(f"""
  {{
    'name': '2p_patch_fetch_{idx}',
    'pattern': '.',
    'cwd': '{repo_path}',
    'action': [
        'git',
        'fetch',
        '{repo_url}',
        '{git_ref}',
    ],
  }},
  {{
    'name': '2p_patch_apply_{idx}',
    'pattern': '.',
    'cwd': '{repo_path}',
    'action': [
        'git',
        'cherry-pick',
        'FETCH_HEAD',
    ],
  }},
""")
    new_hooks = "\n".join(new_hooks_list)

    # Ensure the last element in the hooks list ends with a comma
    prev_idx = hooks_end_idx - 1
    while prev_idx >= 0 and not lines[prev_idx].strip():
        prev_idx -= 1

    if prev_idx >= 0 and not lines[prev_idx].strip().endswith(","):
        if not lines[prev_idx].strip().startswith("#"):
            lines[prev_idx] += ","

    # Insert the new hooks right before the closing bracket line
    lines.insert(hooks_end_idx, new_hooks)

    with open(deps_path, "w") as f:
        f.write("\n".join(lines) + "\n")


def generate_deps_roll_cl(git_helper, cl_url, deps_path):
    gerrit_host, project, change_num, patchset, git_ref = parse_cl_url(cl_url)
    print(
        f"\n--- Processing 2P CL: {cl_url} ---\n"
        f"Parsed CL: host={gerrit_host}, project={project}, "
        f"change={change_num}, patchset={patchset}\n"
        f"Constructed git ref: {git_ref}"
    )

    # Reconstruct the Git repository URL from Gerrit host and project name
    git_host = gerrit_host.replace(
        "-review.googlesource.com", ".googlesource.com"
    )
    dep_url = f"https://{git_host}/{project}"

    git_refs = []
    open_parents = fetch_open_parents(gerrit_host, project, change_num)
    for change in reversed(open_parents):
        c_num = change.get("_change_number")
        p_set = change.get("_revision_number")
        ref = f"refs/changes/{c_num % 100:02d}/{c_num}/{p_set}"
        git_refs.append(ref)
        print(f"  - Add parent ref to chain: {ref} (CL {c_num})")

    # Always apply the target CL last
    git_refs.append(git_ref)
    print(f"  - Add target ref to chain: {git_ref} (CL {change_num})")

    with open(deps_path, "r") as f:
        deps_content = f.read()
    dep_path = find_dep_path_for_cl(deps_content, cl_url)

    if not dep_path:
        raise RuntimeError(
            f"Could not find dependency in DEPS for project: {project}"
        )

    print(f"Found matching dependency: path={dep_path}, url={dep_url}")

    original_branch = git_helper.get_current_branch()
    temp_branch = f"{TEMP_BRANCH_PREFIX}{change_num}-{int(time.time())}"

    print(f"Creating temporary branch: {temp_branch}")
    git_helper.create_and_checkout_branch(temp_branch)

    # Cherry-pick local commits (e.g. adaptation fixes) from original branch,
    # then clean up tools/clang/spanify/ to keep the CL focused.
    git_helper.cherry_pick_local_commits(original_branch)

    success = False
    try:
        print("Modifying DEPS to inject hooks...")
        if not git_helper.dry_run:
            modify_deps(deps_path, deps_content, dep_path, dep_url, git_refs)
        else:
            print(f"[DRY RUN] Would modify {deps_path} with refs {git_refs}")

        print("Committing DEPS changes...")
        git_helper.commit_all_changes(
            COMMIT_MSG_TEMPLATE.format(project=project, change_num=change_num)
        )

        print("Uploading CL to Gerrit...")
        uploaded_cl_url = git_helper.upload_cl(
            COMMIT_MSG_TEMPLATE.format(project=project, change_num=change_num)
        )
        print(f"Uploaded Chromium CL: {uploaded_cl_url}")
        success = True
        return uploaded_cl_url
    finally:
        if not success:
            print("Restoring DEPS file...")
            git_helper.run_cmd(["git", "checkout", "--", str(deps_path)])
        print(f"Switching back to original branch: {original_branch}")
        git_helper.checkout_branch(original_branch)
        print(f"Deleting temporary branch: {temp_branch}")
        git_helper.delete_branch(temp_branch)


def find_pinpoint_cli():
    """Locates the pinpoint CLI executable in PATH."""
    return shutil.which("pinpoint")


def build_pinpoint_command(pinpoint_path, exp_cl, base_cl, args):
    """Builds the pinpoint command line list."""
    cmd = [
        pinpoint_path,
        "experiment-telemetry-start",
        "-base-commit",
        args.base_commit,
        "-exp-commit",
        args.base_commit,
        "-exp-patch-url",
        exp_cl,
        "-benchmark",
        args.benchmark,
        "-cfg",
        args.bot,
    ]

    if base_cl:
        cmd.extend(["-base-patch-url", base_cl])
    if args.story:
        cmd.extend(["-story", args.story])
    if args.story_tags:
        cmd.extend(["-story-tags", args.story_tags])
    if args.attempts is not None:
        cmd.extend(["-attempts", str(args.attempts)])

    return cmd


def parse_arguments(args):
    parser = argparse.ArgumentParser(
        description=(
            "Run Pinpoint performance tests for 2P library CLs using DEPS"
            " workflow."
        )
    )
    parser.add_argument(
        "--exp-cl",
        required=True,
        help=(
            "Gerrit CL URL of the experiment 2P library change (e.g.,"
            " https://chromium-review.googlesource.com/c/angle/angle/+/123456)"
        ),
    )
    parser.add_argument(
        "--base-cl",
        help=(
            "Gerrit CL URL of the base change (optional, for comparing two CLs)"
        ),
    )
    parser.add_argument(
        "--base-commit",
        default="HEAD",
        help="Base commit hash for Pinpoint (default: %(default)s)",
    )
    parser.add_argument(
        "--benchmark",
        required=True,
        help="Telemetry benchmark to run (e.g., speedometer3)",
    )
    parser.add_argument(
        "--bot",
        default="mac-m1_mini_2020-perf",
        help="Pinpoint bot configuration (default: %(default)s)",
    )
    parser.add_argument(
        "--story",
        help="Specific story to run (optional)",
    )
    parser.add_argument(
        "--story-tags",
        help="Comma-separated list of story tags (optional)",
    )
    parser.add_argument(
        "--attempts",
        type=int,
        default=150,
        help="Number of attempts to run (default: %(default)s)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the commands instead of running them (for debugging)",
    )

    return parser.parse_args(args)


def run_performance_test(git_helper, pinpoint_path, deps_path, args):
    """Orchestrates the generation of temporary CLs and triggers Pinpoint."""
    # Generate Experiment Chromium CL
    exp_chromium_cl = generate_deps_roll_cl(git_helper, args.exp_cl, deps_path)

    # Generate Base Chromium CL if requested
    base_chromium_cl = None
    if args.base_cl:
        base_chromium_cl = generate_deps_roll_cl(
            git_helper, args.base_cl, deps_path
        )

    # Build and run Pinpoint command
    pinpoint_cmd = build_pinpoint_command(
        pinpoint_path, exp_chromium_cl, base_chromium_cl, args
    )

    if args.dry_run:
        print(f"\n[DRY RUN] Pinpoint command to run:\n{' '.join(pinpoint_cmd)}")
        return

    print(f"\nRunning Pinpoint: {' '.join(pinpoint_cmd)}")
    result = subprocess.run(
        pinpoint_cmd, capture_output=True, text=True, check=True
    )
    print(result.stdout)
    if result.stderr:
        print(result.stderr, file=sys.stderr)

    # Try to extract Job ID from Pinpoint output.
    # Format can be UUID or hex string.
    # e.g., "Finished actions for batch: <job_id>" or
    # "Created job batch: <job_id>"
    match = re.search(
        r"https://pinpoint-dot-chromeperf\.appspot\.com/job/([a-f0-9]+)",
        result.stdout,
    )
    if not match:
        print(
            "\nCould not parse Job ID from Pinpoint output.\n"
            "To monitor later, find the Job ID in the output above and run:\n"
            "  pinpoint wait-job -name <job_id>"
        )
        return

    job_id = match.group(1)
    # TODO: Simplify instructions and remove warning once pinpoint CLI
    # -download-results works reliably.
    print(
        f"\nPinpoint job started successfully.\n"
        f"To monitor the job, run:\n"
        f"  pinpoint wait-job -name {job_id}\n\n"
        "WARNING: The pinpoint CLI '-download-results' flag may fail"
        " silently.\n"
        f"If the CLI fails to download the CSV, download it manually from:\n"
        f"  https://pinpoint-dot-chromeperf.appspot.com/job/{job_id}\n"
        f"click 'export' and 'RAW CSV' to download it manually\n"
        f"and save it to '~/Downloads/{job_id}.csv'."
    )
    if args.benchmark == "speedometer3":
        print(
            "\nOnce downloaded, you can calculate confidence intervals by"
            " running:\n"
            f"  out/Default/pinpoint_ci ~/Downloads/{job_id}.csv"
        )


def main(args):
    parsed_args = parse_arguments(args)

    chromium_src = pathlib.Path(__file__).resolve().parents[3]
    deps_path = chromium_src / "DEPS"

    git_helper = GitHelper(chromium_src, parsed_args.dry_run)

    if not git_helper.is_clean():
        print(
            "Error: Workspace has uncommitted changes. Please commit or stash"
            " them.",
            file=sys.stderr,
        )
        return 1

    pinpoint_path = find_pinpoint_cli()
    if not pinpoint_path:
        print(
            "Error: Could not find pinpoint CLI in PATH.",
            file=sys.stderr,
        )
        return 1

    try:
        run_performance_test(git_helper, pinpoint_path, deps_path, parsed_args)
    except Exception as e:
        print(f"\nError: {e}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
