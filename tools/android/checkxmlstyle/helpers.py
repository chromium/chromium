# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re


def normpath(path):
  '''Version of os.path.normpath that also changes backward slashes to
  forward slashes when not running on Windows.
  '''
  # This is safe to always do because the Windows version of os.path.normpath
  # will replace forward slashes with backward slashes.
  path = path.replace(os.sep, '/')
  return os.path.normpath(path)


_SRC_ROOT = normpath(
    os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..')))
COLOR_PALETTE_RELATIVE_PATH = normpath(
    'ui/android/java/res/values/color_palette.xml')
COLOR_PALETTE_PATH = normpath(
    os.path.join(_SRC_ROOT, COLOR_PALETTE_RELATIVE_PATH))
ONE_OFF_COLORS_RELATIVE_PATH = normpath(
    'ui/android/java/res/values/one_off_colors.xml')
ONE_OFF_COLORS_PATH = normpath(
    os.path.join(_SRC_ROOT, ONE_OFF_COLORS_RELATIVE_PATH))
BUTTOM_COMPAT_WIDGET_RELATIVE_PATH = (
    normpath('ui/android/java/src/org/chromium/ui/widget/ButtonCompat.java'))

COLOR_PATTERN = re.compile(r'(>|")(#[0-9A-Fa-f]+)(<|")')
VALID_COLOR_PATTERN = re.compile(
    r'^#([0-9A-F][0-9A-E]|[0-9A-E][0-9A-F])?[0-9A-F]{6}$')
XML_APP_NAMESPACE_PATTERN = re.compile(
    r'xmlns:(\w+)="http://schemas.android.com/apk/res-auto"')
TEXT_APPEARANCE_STYLE_PATTERN = re.compile(r'^TextAppearance\.?')
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
    normpath('chrome/browser/feed/android/java/res/color/'),
    normpath('components/browser_ui/styles/android/java/res/color/'),
    normpath('components/browser_ui/styles/android/java/res/color-night/'),
    normpath('components/browser_ui/widget/android/java/res/color/'),
    normpath('components/permissions/android/res/color/'),
}

KNOWN_STYLE_ATTRIBUTE = re.compile(
    r' (android:theme|android:textAppearance|style)=\"(.*)\"')
STYLE_REF_PREFIX = re.compile('^(@style|\?attr|\?android:attr)/')
