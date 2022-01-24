// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is used for js_checker_eslint_test.py

module.exports = {
  'overrides': [{
    'files': ['**/*.ts'],
    'plugins': ['@typescript-eslint'],
    'rules': {
      '@typescript-eslint/no-explicit-any': 'warn',
      '@typescript-eslint/no-inferrable-types': 'error',
    },
  }]
}
