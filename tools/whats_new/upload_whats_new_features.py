#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script to update the code base with new features for what's new.

Sample use:

python3 tools/whats_new/upload_whats_new_features.py
    --milestone-absolute-path="/Users/foo/Downloads/M123"

Run with --help to get a complete list of options this script runs with.
"""

import os
import subprocess
import pandas as pd
import sys
import whats_new_util
import argparse
import time


def _CreateBranch(milestone: str) -> None:
    try:
        start = time.time()
        command_git = ['git', 'checkout', '-b', f'whats_new_{milestone}']
        subprocess.run(command_git, check=True)
        end = time.time()
        execution_time = _GetExecutionTime(start, end)
        print(f'The time of execution to create a branch: {execution_time}')
    except subprocess.CalledProcessError as exc:
        raise Exception(f'Failed to create a branch: {exc}')


def _FormatChanges() -> None:
    try:
        start = time.time()
        command_git = ['git', 'cl', 'format']
        subprocess.run(command_git, check=True)
        end = time.time()
        execution_time = _GetExecutionTime(start, end)
        print(f'The time of execution to format the changes: {execution_time}')
    except subprocess.CalledProcessError as exc:
        raise Exception(f'Failed to format changes: {exc}')


def _AddChangesToBranch() -> None:
    command_git = ['git', 'add', '-A']
    try:
        start = time.time()
        subprocess.run(command_git, check=True)
        end = time.time()
        execution_time = _GetExecutionTime(start, end)
        print('The time of execution to add the changes to the branch: '
              f'{execution_time}')
    except subprocess.CalledProcessError as exc:
        raise Exception(f'Failed to add changes to a branch: {exc}')


def _CommitChangesToBranch() -> None:
    try:
        start = time.time()
        command_git = [
            'git', 'commit', '-m', 'Add new features to What\'s New'
        ]
        subprocess.run(command_git, check=True)
        end = time.time()
        execution_time = _GetExecutionTime(start, end)
        print('The time of execution to commit the changes to the branch: '
              f'{execution_time}')
    except subprocess.CalledProcessError as exc:
        raise Exception(f'Failed to commit the changes to a branch: {exc}')


def _UploadChangesToCL(message: str) -> None:
    try:
        start = time.time()
        command_git = [
            'git', 'cl', 'upload', '--bypass-hooks', '--bypass-watchlists',
            '--force', '--message', message, '--cq-dry-run', '--auto-submit',
            '-o', 'uploadvalidator~skip'
        ]
        subprocess.run(command_git, check=True)
        end = time.time()
        execution_time = _GetExecutionTime(start, end)
        print('The time of execution to upload the changes to a cl: '
              f'{execution_time}')
    except subprocess.CalledProcessError as exc:
        raise Exception(f'Failed to upload the changes to a branch: {exc}')


def _AddFeature(index: int, feature_dict: dict[str, str],
                path_to_milestone_folder: str) -> None:
    start = time.time()
    new_enum = whats_new_util.UpdateWhatsNewItemAndGetNewTypeValue(
        feature_dict)
    whats_new_util.UpdateWhatsNewPlist(feature_dict, new_enum)
    whats_new_util.UpdateWhatsNewUtils(feature_dict)
    whats_new_util.CopyAnimationFilesToResources(feature_dict,
                                                 path_to_milestone_folder)
    whats_new_util.UpdateResourcesBuildFile(feature_dict)
    whats_new_util.AddStrings(feature_dict, path_to_milestone_folder)
    whats_new_util.UploadScreenshots(feature_dict, path_to_milestone_folder)
    end = time.time()
    execution_time = _GetExecutionTime(start, end)
    print(f'The time of execution to add feature {index}: {execution_time}')


def _GetExecutionTime(start: float, end: float) -> str:
    return f'{(end-start)*10**3:.03f}ms'


def main():
    start = time.time()
    print('Start...')
    parser = parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        '--milestone-absolute-path',
        required=True,
        help='Specify the absolute path to the milestone directory.')
    args = parser.parse_args()
    if not args.milestone_absolute_path:
        raise ValueError('Missing input through --milestone-absolute-path.')

    milestone_folder_absolute_path = args.milestone_absolute_path
    xlsx_file_path = os.path.join(milestone_folder_absolute_path,
                                  'whats_new_features.xlsx')
    xlsx_content = pd.read_excel(xlsx_file_path)
    milestone = xlsx_content.iloc[0]['Milestone']

    #Create branch
    _CreateBranch(milestone.lower())

    # Delete existing features from the plist
    whats_new_util.CleanUpFeaturesPlist()
    features_name = []
    for index, feature_row in pd.DataFrame(xlsx_content).iterrows():
        feature_row.fillna('', inplace=True)
        _AddFeature(index, feature_row, milestone_folder_absolute_path)
        features_name.append(feature_row['Feature name'])

    # Update FET event
    whats_new_util.UpdateWhatsNewFETEvent(milestone.lower())

    # Prepare and upload changes
    _FormatChanges()
    _AddChangesToBranch()
    _CommitChangesToBranch()

    comment_builder = []
    comment_builder.append(
        f'[iOS][WhatsNew] Add new features for {milestone}\n')
    comment_builder.append(
        'This CL adds the following features to What\'s New:')
    comment_builder.append('\n'.join(features_name))
    _UploadChangesToCL('\n'.join(comment_builder))

    end = time.time()
    execution_time = _GetExecutionTime(start, end)
    print(f'The total time of execution: {execution_time}')


if __name__ == '__main__':
    sys.exit(main())
