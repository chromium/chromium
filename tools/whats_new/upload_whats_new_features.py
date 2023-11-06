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


def _AddFeature(feature_dict: dict[str, str],
                path_to_milestone_folder: str) -> None:
    new_enum = whats_new_util.UpdateWhatsNewItemAndGetNewTypeValue(
        feature_dict)
    whats_new_util.UpdateWhatsNewPlist(feature_dict, new_enum)
    whats_new_util.UpdateWhatsNewUtils(feature_dict)
    whats_new_util.CopyAnimationFilesToResources(feature_dict,
                                                 path_to_milestone_folder)
    whats_new_util.UpdateResourcesBuildFile(feature_dict)
    whats_new_util.AddStrings(feature_dict, path_to_milestone_folder)
    whats_new_util.UploadScreenshots(feature_dict, path_to_milestone_folder)


def main():
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
    # Delete existing features from the plist
    whats_new_util.CleanUpFeaturesPlist()
    for index, feature_row in pd.DataFrame(xlsx_content).iterrows():
        _AddFeature(feature_row, milestone_folder_absolute_path)
    command_git_format = ['git', 'cl', 'format']
    error = subprocess.Popen(command_git_format,
                             universal_newlines=True,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE).communicate()
    if error:
        print(error)


if __name__ == '__main__':
    sys.exit(main())
