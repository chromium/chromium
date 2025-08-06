#!/usr/bin/env python3
#
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script is used to apply patches that successfully compile from the
# 'rewrite-multiple-platforms.sh` script in the Chromium codebase, and then
# test on various gn configurations until a merged patch that is likely to
# compile successfully on the bots is generated.
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
# By running this script we can determine which patches work and compile
# on most (not 100% exhaustive) platforms.
#
# example
# 1. Checkout "main"
# 2. download a spanification patch: "rewrite" (or run at head) into your
#    ~/scratch directory
# 3. run this script to apply patches and figure out the set that compile on
#    all relevant platforms.
# 4. upload the patch and fix any tests or things this script missed.
import glob
import os
import re
import sys
import subprocess
import getpass

# common gn args for spanify project scripts.
from gnconfigs import GnConfigs, GenerateGnTarget
from enum import Enum

BRANCHES = []
NO_PATCHES = tuple([])


class CacheResult(Enum):
    NOT_CACHED = 0
    NOT_APPLIED = 1
    FAILED_COMPILE = 2
    COMPILED = 3


# Quick and dirty memoize based on target + patches, we save and load from a
# file so that interrupted (due to gcert or machine restarts) can quickly
# "know" the result without having to apply or compile (which are the
# bottlenecks in terms of script performance), the rest of the script will play
# out exactly as if there was no restart.
class MemoizeCache:

    def __init__(self, cache_file):
        self.cache_file = cache_file
        self.patches_already_done = dict()
        if cache_file is not None:
            self.LoadCacheFromFile()

    # Loads from `cache_file` and populates `patches_already_done` must be kept
    # in sync with WriteResultToCache().
    def LoadCacheFromFile(self):
        if self.cache_file is None or not os.path.exists(self.cache_file):
            return
        print(f'loading cache from {sys.argv[1]}', flush=True)
        with open(self.cache_file, 'r') as f:
            for line in f:
                split = line.split(':::')
                assert len(split) >= 3
                target = split[0]
                result = None
                if split[1] == "True":
                    result = True
                elif split[1] == "False":
                    result = False
                patches = []
                # If we had some patches fill the array with the rest.
                if split[2].strip() != "None":
                    for i in range(2, len(split)):
                        assert split[i].strip().isdigit(
                        ), f'"{split[i]}" is not a digit in {line}'
                        patches.append(int(split[i].strip()))
                key = (target, tuple(patches))
                print(f'loading {key} as {result}')
                if result is None:
                    self.patches_already_done[key] = CacheResult.NOT_APPLIED
                elif not result:
                    self.patches_already_done[key] = CacheResult.FAILED_COMPILE
                else:
                    assert result, 'Should have compiled but got value'
                    self.patches_already_done[key] = CacheResult.COMPILED

    # Saves the result into `cache_file` and `patches_already_done`
    # The value can be
    #   1) "None" if we failed to apply the patches
    #   2) "True" if we applied patches and compiled successfully.
    #   3) "False" if we applied patches and failed to compile.
    # Must be kept in sync with LoadCacheFromFile().
    def WriteResultToCache(self, target, patches, applied, compiled):
        # Update the in-memory cache
        key = (target, tuple(patches))
        if not applied:
            self.patches_already_done[key] = CacheResult.NOT_APPLIED
        elif not compiled:
            self.patches_already_done[key] = CacheResult.FAILED_COMPILE
        else:
            assert applied and compiled, "huh"
            self.patches_already_done[key] = CacheResult.COMPILED
        # if the self.cache_file is set we save any new results into it.
        if self.cache_file is None:
            return
        print('Saving to cache file', flush=True)
        result_str = "None"
        if applied:
            result_str = "True" if compiled else "False"
        patch_str = "" if len(patches) > 0 else "None"
        for patch in patches:
            if patch_str != "":
                patch_str += ":::"
            patch_str += str(patch)
        with open(self.cache_file, 'a') as f:
            result_str = f'{target}:::{result_str}:::{patch_str}'
            print(f'saving(to {self.cache_file})={result_str}', flush=True)
            f.write(f'{result_str}\n')

    def Result(self, target, patches):
        key = (target, patches)
        if key in self.patches_already_done:
            return self.patches_already_done[key]
        return CacheResult.NOT_CACHED


CACHE = MemoizeCache(None)


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


def FindSuccessfulPatchNumbers(scratch_dir: str) -> tuple:
    files = glob.glob(scratch_dir + '/patch_*.pass')
    result = []
    regex = re.compile(r'.*/patch_([0-9]+)\.pass$')
    for f in files:
        result.append(int(regex.match(f).group(1)))
    return tuple(sorted(result))


def CreateNewBranch(branch_name: str) -> ():
    global BRANCHES
    print(f'switching to {branch_name}')
    run(f'git branch -D {branch_name} 1>/dev/null 2>/dev/null',
        exit_on_error=False)
    run(f'git new-branch --upstream {BRANCHES[-1]} {branch_name} 2>&1')
    BRANCHES.append(branch_name)


def PopBranch() -> ():
    global BRANCHES
    assert len(BRANCHES) > 0, 'tried to pop past historical'
    old_branch = BRANCHES.pop()
    print(f'poping out of {old_branch}')
    run(f'git checkout {BRANCHES[-1]} 1>/dev/null 2>/dev/null')


def CollectEditsInFile(patches: tuple, scratch_dir: str, label: str) -> str:
    assert len(patches) != 0, 'should always have at least one patch'
    assert all(isinstance(p, int) for p in patches)
    file_offset_and_replacement = []
    for patch in patches:
        with open(scratch_dir + f'/patch_{str(patch)}.txt', 'r') as edits:
            for line in edits.readlines():
                if line[-1] != '\n':
                    line = line + '\n'
                splits = line.split(':::')
                assert len(splits) >= 4, f'Not enough splits: {len(splits)}'
                assert splits[0] in [
                    'r', 'include-system-header', 'include-user-header'
                ], f'Incorrect: {line}'
                if splits[0] == 'r':
                    assert splits[2].isdigit(
                    ), f'not a digit {splits[2]} and {line}'
                    file_offset_and_replacement.append((int(splits[2]), line))
                else:
                    # Headers are at -1 but isdigit() doesn't work on '-1'
                    assert splits[
                        2] == '-1', f'not a digit {splits[2]} and {line}'
                    file_offset_and_replacement.append((-1, line))
    file_offset_and_replacement = sorted(file_offset_and_replacement,
                                         key=lambda x: x[0],
                                         reverse=True)
    output = scratch_dir + f'/combined_edits_{label}.txt'
    with open(output, 'w') as out:
        for offset, replacement in file_offset_and_replacement:
            out.write(replacement)
    return output


def ApplyEdits(patches: tuple, scratch_dir, label) -> bool:
    if len(patches) == 0:
        return True
    assert all(isinstance(p, int) for p in patches)

    edits = CollectEditsInFile(patches, scratch_dir, label)
    print(f'applying {patches} in {label} in {edits}', flush=True)
    try:
        result = subprocess.run(f"cat {edits}" +
                                " | tools/clang/scripts/apply_edits.py" +
                                " -p ./out/linux/",
                                shell=True,
                                check=True,
                                capture_output=True,
                                text=True)
    except subprocess.CalledProcessError as e:
        error_msg = ("\"" + str(e) + " !!! exception(stderr): " +
                     str(e.stderr) + "\"")
        print(f'applying {label} failed because {error_msg}', flush=True)
        run(f"git diff  > {scratch_dir}/patch_{label}.diff")
        run("git restore .", "Failed to restore after failed patch.")
        return False
    run("git cl format")

    # Commit changes
    run("git add -u", "Failed to add changes.")

    with open("commit_message.txt", "w+") as f:
        f.write(
            f"""spanification patches {label} applied.\n\nPatches: {label}""")
    # Sometimes we generate patches that apply_edits will skip (for example
    # third_party) thus don't treat failure to commit as an error.
    if not run("git commit -F commit_message.txt", exit_on_error=False):
        # We fail when there is no diff get the replacements instead.
        diff = open(scratch_dir + f"/combined_edits_{label}.txt").read()
        print('had empty diff: ' + diff)
    # Serialize changes
    run(f"git diff HEAD~...HEAD > {scratch_dir}/patch_{label}.diff")
    diff = open(scratch_dir + f"/patch_{label}.diff").read()
    print('applied diff')
    return True


def TriggerGCert():
    glogin_args = [
        '/usr/bin/gcert', '-glogin_connect_timeout=60s',
        '-glogin_request_timeout=60s'
    ]
    try:
        password = bytes(
            getpass.getpass(
                f'Please enter password for {getpass.getuser()} (not stored): '
            ),
            encoding='utf-8',
        )
        process = subprocess.Popen(glogin_args, stdin=subprocess.PIPE)
        process.communicate(password)
        process.wait()
    except KeyboardInterrupt:
        print('Aborted')
        sys.exit(1)


def CompileCurrentBranch(out_dir):
    result = subprocess.run(f'time autoninja -C {out_dir}',
                            shell=True,
                            capture_output=True,
                            text=True)
    print(result.stdout)
    print(result.stderr)
    if "build failed" in result.stdout.lower():
        return False
    if 'need to run `siso login`' in result.stderr.lower():
        print("gcert has expired prompting user to gcert",
              flush=True,
              file=sys.stderr)
        TriggerGCert()
        return CompileCurrentBranch(out_dir)
    elif not run(f'gn check {out_dir}', exit_on_error=False):
        return False
    return True


# Takes a list of `patches` applies it in branch (with `label` suffix), and
# then generates a GN directory using `target` and `args`. It also uses the
# global `patches_already_done` in memory cache to avoid redoing this work if
# the result is already know.
def CheckPatchesForTarget(target, args, patches, scratch_dir, label) -> bool:
    global CACHE
    working = lambda x: x == CacheResult.COMPILED
    # If we've already compiled this set of patches for this target we can skip
    # we know the result.
    result = CACHE.Result(target, patches)
    if result != CacheResult.NOT_CACHED:
        print('returning cached result: ' + str(result))
        return working(result)
    CreateNewBranch(f'spanification_apply_patches_{label}')
    applied = ApplyEdits(patches, scratch_dir, label)
    compiled = False
    if applied:
        compiled = CompileCurrentBranch(f'out/{target}')
    PopBranch()
    # Cache the result.
    CACHE.WriteResultToCache(target, patches, applied, compiled)
    return working(CACHE.Result(target, patches))


def HandleLen2BaseCase(target, args, base, to_try, scratch_dir,
                       label) -> tuple:
    assert len(to_try) == 2, "Invalid length passed"
    err_msg = "base has to be a tuple of all ints."
    assert isinstance(base, tuple), err_msg
    assert all(isinstance(b, int) for b in base), err_msg

    left_patch = to_try[0]
    left_patches = base + (left_patch, )
    left = CheckPatchesForTarget(target, args, left_patches, scratch_dir,
                                 f'{label}_left')

    right_patch = to_try[1]
    right_patches = base + (right_patch, )
    right = CheckPatchesForTarget(target, args, right_patches, scratch_dir,
                                  f'{label}_right')

    if left and right:
        # Both compile but not when included together.
        print(f'Patch {left_patch} and patch {right_patch}' +
              f'do not work together on {target}, droping {right_patch}')
        return left_patches
    elif left:
        print(
            f'Patch {right_patch} was dropped, it does not merge on {target}')
        return left_patches
    elif right:
        print(f'Patch {left_patch} was dropped, it does not merge on {target}')
        return right_patches
    else:
        # Both doesn't compile/apply when added to `base` drop both.
        print(f'Patch {left_patch} and patch {right_patch}' +
              f'do not work together on {target} while merging. droping both')
        return base


def FindCompatiblePatchesByMerging(base, to_try, target, args, scratch_dir,
                                   label) -> tuple:
    if CheckPatchesForTarget(target, args, base + to_try, scratch_dir,
                             f'{label}_initial_check'):
        return base + to_try
    # We failed to compile and there is only 1.
    if len(to_try) == 1:
        print(f'Patch {to_try[0]} failed when added for {label}, on {target}')
        return base
    if len(to_try) == 2:
        return HandleLen2BaseCase(target, args, base, to_try, scratch_dir,
                                  label)
    midpoint = len(to_try) // 2
    left = FindCompatiblePatchesByMerging(base, to_try[:midpoint], target,
                                          args, scratch_dir, f'{label}_left')
    return FindCompatiblePatchesByMerging(left, to_try[midpoint:], target,
                                          args, scratch_dir, f'{label}_right')


def FindCompilingAndCompatiblePatchesImpl(target, args, patches, scratch_dir,
                                          label) -> tuple:
    assert len(patches) > 0, 'No patches provided'
    # optimistically try them all
    if CheckPatchesForTarget(target, args, patches, scratch_dir, f'{label}'):
        return patches
    if len(patches) == 1:
        return tuple()
    elif len(patches) == 2:
        return HandleLen2BaseCase(target, args, tuple(), patches, scratch_dir,
                                  label)
    # Recursive call
    midpoint = len(patches) // 2
    left = FindCompilingAndCompatiblePatchesImpl(target, args,
                                                 patches[:midpoint],
                                                 scratch_dir, f'{label}_left')
    right = FindCompilingAndCompatiblePatchesImpl(target, args,
                                                  patches[midpoint:],
                                                  scratch_dir,
                                                  f'{label}_right')

    # Some early out opportunities to reduce headspace. If we compile on only
    # one side (or neither side) then we can just early out (assuming the
    # invariant that this recursive call correctly found all compatible
    # patches).
    if len(left + right) == 0:
        return tuple()
    elif len(left) == 0:
        return right
    elif len(right) == 0:
        return left

    # Optimistically try them both together.
    if CheckPatchesForTarget(target, args, left + right, scratch_dir,
                             f'{label}_left_with_right'):
        # After removing non-compiling/non-compatible patches this combination
        # works.
        return left + right

    # We need now to find the exact set of patches we can add in from the
    # smaller. We do this base taking the larger amount of patches as our base
    # and then splitting the smaller into parts recursively until we find the
    # exact 2 patches that doesn't work together when applied at the same time.
    larger = left if len(left) >= len(right) else right
    smaller = left if len(left) < len(right) else right
    result = FindCompatiblePatchesByMerging(larger, smaller, target, args,
                                            scratch_dir, f'{label}_merging')
    return result


def FindCompilingAndCompatiblePatches(target, args, patches,
                                      scratch_dir) -> tuple:
    assert len(patches) > 0, 'No patches provided'
    assert all(isinstance(p, int) for p in patches)
    # optimistically try them all
    if CheckPatchesForTarget(target, args, patches, scratch_dir,
                             f'all_{target}_patches'):
        return patches
    return FindCompilingAndCompatiblePatchesImpl(target, args, patches,
                                                 scratch_dir,
                                                 f'{target}_patches_start')


def main():
    # Cache variables.
    global CACHE
    global BRANCHES
    # This will serve ensure we can run this script consistently by going to
    # main, and then creating a base branch. Creates a new branch that tracks
    # whatever the current state is, all future branches will be based on it.
    assert len(BRANCHES) == 0
    BRANCHES.append("spanification-base-for-rewrite")
    run(f'git checkout {BRANCHES[0]} 1>/dev/null 2>/dev/null')

    CreateNewBranch(f'spanification_apply_patches_base')

    # Look in the scratch directory and find all our patches.
    scratch_dir = os.path.expanduser('~/scratch')
    patches = FindSuccessfulPatchNumbers(scratch_dir)

    if len(sys.argv) > 1:
        CACHE = MemoizeCache(os.path.expanduser(sys.argv[1]))

    curr_result = patches
    for target, args in GnConfigs(True).all_platforms_and_configs.items():
        assert GenerateGnTarget(target, args), "Failed to configure target"
        # If a clean no patches build fails something is incorrect with the gn
        # args or the build setup. Thus if this returns false we skip the target
        # to avoid spending time spinning to determine all patches are not
        # compiling.
        cache_result = CACHE.Result(target, NO_PATCHES)
        if cache_result == CacheResult.NOT_CACHED:
            if not CompileCurrentBranch(f'out/{target}'):
                # Sometimes (often with chromeos or windows) if a sync hasn't
                # recently been run after having trying to compile a platform it
                # will fail. And just syncing fixes it.
                run('gclient sync -fD')
            # If we compiled successfully above this should quickly finish and
            # cache that result, and if it didn't we'll give it a chance after
            # a gclient sync to succeed.
            CheckPatchesForTarget(
                target, args, NO_PATCHES, scratch_dir,
                f'spanification_clean_compile_check_{target}')
        # This was either already in the map or was updated above.
        if CACHE.Result(target, NO_PATCHES) != CacheResult.COMPILED:
            print(f'Failed to compile cleanly {target}... skipping {target}',
                  flush=True)
            continue
        curr_result = FindCompilingAndCompatiblePatches(
            target, args, curr_result, scratch_dir)
        assert CheckPatchesForTarget(target, args, curr_result, scratch_dir,
                                     f'{target}_final_patch')
        print(f'working patches for {target}:', flush=True)
        print(curr_result, flush=True)
        print('finished', flush=True)
        run(f'gn clean out/{target}')

    print(f'working patches for all targets:', flush=True)
    print(curr_result, flush=True)
    # Now we create the final branch to store the applied edits.
    branch_name = f'spanification_apply_all_targets_final_patches'
    CreateNewBranch(branch_name)
    applied = ApplyEdits(curr_result, scratch_dir, branch_name)
    assert applied, "reached end up couldn't apply edits"
    compiled = CompileCurrentBranch(f'out/linux-rel')
    assert compiled, "reached end but couldn't compile linux-rel"
    print('finished, final working patch in "{branch_name}"', flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
