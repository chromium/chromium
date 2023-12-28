// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ESLint configuration file to be inherited by folder-specific configurations
// as part of gradually rolling out https://crbug.com/1494527.
// TODO(crbug.com/1494527): Delete this file once this check has been added to
// the default .eslintrc.js file.
module.exports = {
  'overrides': [{
    'files': ['**/*.ts'],
    'parser': '../../../../third_party/node/node_modules/@typescript-eslint/parser/dist/index.js',
    'plugins': [
      '@typescript-eslint',
    ],
    'rules': {
      '@typescript-eslint/consistent-type-imports': 'error',
    },
  }],
};
