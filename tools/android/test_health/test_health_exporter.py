# Lint as: python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import pathlib
import sys
from typing import Dict, List, Union

from java_test_utils import JavaTestHealth
from test_health_extractor import TestHealthInfo, GitRepoInfo

JsonSafeDict = Dict[str, Union[str, int]]


def to_json_file(test_health_list: List[TestHealthInfo],
                 output_path: pathlib.Path) -> None:
    """Exports test health information to a newline-delimited JSON file.

    Each line of the output file is an independent JSON object. This format is
    suitable for importing into BigQuery and is also known as JSON Lines
    (http://jsonlines.org/).

    Args:
        test_health_list:
            The list of `TestHealthInfo` objects to write as JSON.
        output_path:
            The path at which to create or overwrite the JSON output.
    """
    test_health_dicts = _to_test_health_dicts(test_health_list)

    with open(output_path, 'w') as json_file:
        for test_health in test_health_dicts:
            json.dump(test_health, json_file, allow_nan=False)
            json_file.write('\n')


def _to_test_health_dicts(test_health_list: List[TestHealthInfo]
                          ) -> List[JsonSafeDict]:
    """Transforms a list of `TestHealthInfo` into dicts of JSON-safe data."""
    java_test_health_list = []
    for test_health in test_health_list:
        if test_health.java_test_health:
            java_test_health_list.append(_to_test_health_dict(test_health))
        else:
            logging.warning(
                f'Skipped non-Java test "{test_health.test_name}"; currently'
                'only Java tests are supported.')

    return java_test_health_list


def _to_test_health_dict(test_health_info: TestHealthInfo) -> JsonSafeDict:
    """Transforms a `TestHealthInfo` into a dict of JSON-safe data."""
    test_health_dict: JsonSafeDict = dict(
        test_name=test_health_info.test_name,
        test_path=str(test_health_info.test_dir),
        test_filename=test_health_info.test_filename,
    )

    if test_health_info.java_test_health:
        test_health_dict.update(
            _to_java_test_health_dict(test_health_info.java_test_health))
    else:
        test_health_dict.update(test_type='UNKNOWN')

    test_health_dict.update(
        _to_git_repo_info_dict(test_health_info.git_repo_info))

    return test_health_dict


def _to_java_test_health_dict(java_test_health: JavaTestHealth
                              ) -> JsonSafeDict:
    """Transforms a `JavaTestHealth` into a dict of JSON-safe data."""
    test_dict: JsonSafeDict = dict(test_type='JAVA')

    if java_test_health.java_package:
        test_dict.update(dict(java_package=java_test_health.java_package))

    test_dict.update(
        dict(
            disabled_tests_count=java_test_health.disabled_tests_count,
            disable_if_tests_count=java_test_health.disable_if_tests_count,
            tests_count=java_test_health.tests_count,
            disabled_tests=java_test_health.disabled_tests,
            disable_if_tests=java_test_health.disable_if_tests,
        ))

    return test_dict


def _to_git_repo_info_dict(git_repo_info: GitRepoInfo) -> JsonSafeDict:
    """Transforms a `GitRepoInfo` into a dict of JSON-safe data."""
    return dict(git_head_hash=git_repo_info.git_head,
                git_head_timestamp=git_repo_info.git_head_time.isoformat(
                    timespec='microseconds'))
