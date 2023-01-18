// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module.exports = {
  'rules' : {
    'no-console' : 'off',
  },

  'overrides': [{
    'files': ['**/*.ts'],
    'parser': '../../third_party/node/node_modules/@typescript-eslint/parser',
    'plugins': [
      '@typescript-eslint',
    ],
    'rules': {
      // TODO(b/265863256): Re-enable when TypeScript annotations complication
      // has been fixed.
      '@typescript-eslint/no-unused-vars': 'off',
    },
  }],
};
