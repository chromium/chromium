# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This file provides some additional globals/filters for processing the
file_types_data.js templates.
"""


def _get_extension_to_type_map(file_types):
    """
    Gets the extestension to file type mapping from the |file_types| data.

    Args:
        file_types: file types data loaded from the JSON file.
    Returns:
        A extension -> file type mapping.
    """
    extension_to_type = dict()
    for file_type in file_types:
        for file_ext in file_type['extensions']:
            if file_ext not in extension_to_type:
                extension_to_type[file_ext] = file_type
    return extension_to_type


def _get_mime_to_type_map(file_types):
    """
    Gets the mime type to file type mapping from the |file_types| data.

    Args:
        file_types: file types data loaded from the JSON file.
    Returns:
        A mime -> file type mapping.
    """
    mime_to_type = dict()
    for file_type in file_types:
        if 'mime' in file_type and file_type['mime'] not in mime_to_type:
            mime_to_type[file_type['mime']] = file_type
    return mime_to_type


def get_custom_globals(model):
    return {
        'extension_to_type_map':
        _get_extension_to_type_map(model['file_types']),
        'mime_to_type_map': _get_mime_to_type_map(model['file_types'])
    }


def get_custom_filters(model):
    return {}
