# Lint as: python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Functions dealing with Chrome-specific naming conventions."""


def shorten_class(class_name: str) -> str:
    """Returns a shortened version of the fully qualilied class name."""
    return class_name.replace('org.chromium.',
                              '.').replace('chrome.browser.', 'c.b.')


def shorten_build_target(build_target: str) -> str:
    """Returns a shortened version of the build target."""
    if build_target == '//chrome/android:chrome_java':
        return 'chrome_java'

    return build_target.replace('//chrome/browser/', '//c/b/')
