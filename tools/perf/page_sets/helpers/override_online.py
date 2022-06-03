# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" A script that will override navigator.onLine to always return true. """
ALWAYS_ONLINE = '''
  Object.defineProperty(window.navigator, 'onLine', {
    get: function() {
      return true;
    },
  });'''

