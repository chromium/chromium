# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for //ui/file_manager/base/gn.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

import sys

PRESUBMIT_VERSION = '2.0.0'


def _load_json_data(input_api, file_path):
    """ Loads json data from the file |file_path| via json5 module.
    Args:
        input_api: InputApi instance from depot_tools's presumbit_support.py
        file_path: the full file path string.
    Returns:
        The loaded json data.
    """
    try:
        json5_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                            '..', '..', '..', '..',
                                            'third_party', 'pyjson5', 'src')
        sys.path.append(json5_path)
        import json5
        return json5.load(open(file_path, encoding='utf-8'))
    finally:
        # Restore sys.path to what it was before.
        sys.path.remove(json5_path)


def _validate_json_schema(json_data, file_path, output_api):
    """ Validates the json schema for the json data |json_data|.
    Args:
        json_data: The json data to be validated.
        file_path: the full file path string.
        output_api: OutputApi instance from depot_tools's presumbit_support.py
    Returns:
        The validation result array which contains various presubmit error
        messages. Empty array will return if the json data passes the
        validation.
    """
    validation_results = []
    if not isinstance(json_data, list):
        validation_results.append(
            output_api.PresubmitError(f'{file_path}: must be a json array.'))
    else:
        required_str_fields = ['translationKey', 'type', 'subtype']
        for item in json_data:
            for field in required_str_fields:
                if not isinstance(item.get(field), str):
                    validation_results.append(
                        output_api.PresubmitError(
                            f'{file_path}: field "{field}" must be a string for'
                            ' each file type.'))
            # Field "icon" is optional.
            if 'icon' in item and not isinstance(item['icon'], str):
                validation_results.append(
                    output_api.PresubmitError(
                        f'{file_path}: field "icon" must be a string for each'
                        ' file type.'))
            # Field "mime" is optional.
            if 'mime' in item and not isinstance(item['mime'], str):
                validation_results.append(
                    output_api.PresubmitError(
                        f'{file_path}: field "mime" must be a string for each'
                        ' file type.'))
            if isinstance(item.get('extensions'), list):
                if not item['extensions']:
                    validation_results.append(
                        output_api.PresubmitError(
                            f'{file_path}: "extensions" array needs to include'
                            ' at least 1 file extension.'))
                else:
                    missing_dots = [
                        ext for ext in item['extensions']
                        if not (ext and ext.startswith('.'))
                    ]
                    if missing_dots:
                        validation_results.append(
                            output_api.PresubmitError(
                                f'{file_path}: the following extension(s)'
                                ' should start with dot'
                                ' "{", ".join(missing_dots)}"'))
                    unique_ext_keys = len(set(item['extensions']))
                    if unique_ext_keys != len(item['extensions']):
                        validation_results.append(
                            output_api.PresubmitError(
                                f'{file_path}: "extensions" array should not'
                                ' include duplicate extensions.'))
            else:
                validation_results.append(
                    output_api.PresubmitError(
                        f'{file_path}: field "extensions" must be an array for'
                        ' each file type.'))

    return validation_results


def CheckFileTypesJSONSchema(input_api, output_api):
    """ Main check function during PreSubmit.
    Args:
        input_api: InputApi instance from depot_tools's presumbit_support.py
        output_api: OutputApi instance from depot_tools's presumbit_support.py
    Returns:
        The result array which contains various presubmit error messages.
    """
    file_name = 'file_types.json5'
    file_path = input_api.os_path.relpath(
        input_api.os_path.join(input_api.PresubmitLocalPath(), file_name),
        input_api.change.RepositoryRoot())
    file_types_json = input_api.AffectedSourceFiles(lambda x: x.LocalPath() ==
                                                    file_path)
    if not file_types_json:
        return []

    results = []
    try:
        data = _load_json_data(input_api, file_name)
        results.extend(_validate_json_schema(data, file_path, output_api))
    except ValueError as err:
        results.append(
            output_api.PresubmitError(f'{file_path}: must be a valid json.'))
    return results
