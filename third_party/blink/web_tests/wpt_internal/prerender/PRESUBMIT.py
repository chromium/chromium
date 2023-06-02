# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Chromium presubmit script for prerender in Web Platform Tests.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

import os


def _CheckFileTimeoutMetaTags(f):
    """Checks if the given file has timeout meta tags."""
    new_contents = f.NewContents()

    for line in new_contents:
        if 'name="timeout" content="long"' in line:
            return True
    return False


def _CheckTimeoutMetaTags(input_api, output_api):
    """ This function ensures that all WPTs for prerendering have meta tags
        to lengthen test timeout as some tests can possibly run out of time
        on windows platform.
    """
    results = []

    def file_filter(f):
        return (f.LocalPath().endswith(('html'))
                and (os.path.join('resources', '') not in f.LocalPath()))

    for f in input_api.AffectedFiles(include_deletes=False,
                                     file_filter=file_filter):
        if not _CheckFileTimeoutMetaTags(f):
            results.append(
                output_api.PresubmitError(
                    ('Missing long timeout. '
                     'Add `<meta name="timeout" content="long">` to %s.') %
                    f.LocalPath()))
    return results


def CheckChangeOnUpload(input_api, output_api):
    results = []
    results.extend(_CheckTimeoutMetaTags(input_api, output_api))
    return results


def CheckChangeOnCommit(input_api, output_api):
    results = []
    results.extend(_CheckTimeoutMetaTags(input_api, output_api))
    return results
