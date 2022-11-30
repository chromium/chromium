# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re

_SRC_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))
COLOR_PALETTE_RELATIVE_PATH = 'ui/android/java/res/values/color_palette.xml'
COLOR_PALETTE_PATH = os.path.join(_SRC_ROOT, COLOR_PALETTE_RELATIVE_PATH)
ONE_OFF_COLORS_RELATIVE_PATH = 'ui/android/java/res/values/one_off_colors.xml'
ONE_OFF_COLORS_PATH = os.path.join(_SRC_ROOT, ONE_OFF_COLORS_RELATIVE_PATH)
BUTTOM_COMPAT_WIDGET_RELATIVE_PATH = (
    'ui/android/java/src/org/chromium/ui/widget/ButtonCompat.java')

COLOR_PATTERN = re.compile(r'(>|")(#[0-9A-Fa-f]+)(<|")')
VALID_COLOR_PATTERN = re.compile(
    r'^#([0-9A-F][0-9A-E]|[0-9A-E][0-9A-F])?[0-9A-F]{6}$')
XML_APP_NAMESPACE_PATTERN = re.compile(
    r'xmlns:(\w+)="http://schemas.android.com/apk/res-auto"')
TEXT_APPEARANCE_STYLE_PATTERN = re.compile(r'^TextAppearance\.')
INCLUDED_PATHS = [
    r'^(chrome|ui|components|content)[\\/](.*[\\/])?java[\\/]res.+\.xml$'
]
DYNAMIC_COLOR_INCLUDED_PATHS = [
    r'^(chrome|components)[\\/](.*[\\/])?java[\\/]res.+\.xml$'
]
INCLUDED_GRD_PATHS = [
    r'^(chrome|ui|components|content)[\\/](.*[\\/])?android[\\/](.*)\.grd$'
]
# TODO(lazzzis): Check color references in java source files.
COLOR_REFERENCE_PATTERN = re.compile(
    '''
    @color/   # starts with '@color'
    ([\w|_]+)   # color name is only composed of numbers, letters and underscore
''', re.VERBOSE)

DYNAMIC_COLOR_SUPPORTING_DIRS = {'components', 'chrome'}
COLOR_STATE_LIST_DIRS = {
    # Generated with the command below. When color state lists in new folders
    # are added, re-run this command and update.
    # find chrome/ components/ -name *\.xml | grep "/res/color" | xargs grep "<selector" | cut -d: -f1 | xargs dirname | sort | uniq | sed "s/^/'/" | sed "s/$/\/',/"
    'chrome/browser/feed/android/java/res/color/',
    'components/browser_ui/styles/android/java/res/color/',
    'components/browser_ui/styles/android/java/res/color-night/',
    'components/browser_ui/widget/android/java/res/color/',
    'components/permissions/android/res/color/',
}
