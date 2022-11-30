# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DESKTOP = 'desktop'
MOBILE = 'mobile'

ALL_PLATFORMS = frozenset({DESKTOP, MOBILE})

# Use the constants below to mark on which platforms the story has WPR
# recordings. To disable a story (e.g. because it crashes or takes too long),
# use StoryExpectations instead.
DESKTOP_ONLY = frozenset({DESKTOP})
MOBILE_ONLY = frozenset({MOBILE})
NO_PLATFORMS = frozenset()
