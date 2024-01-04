// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module.exports = {
  // Disable clang-format because it produces odd formatting for these rules.
  // clang-format off
  'rules' : {
    'no-fallthrough' : 'error',
    'eqeqeq' : ['error', 'always', {'null' : 'ignore'}],
    'no-console' : 'off',
    /**
     * https://google.github.io/styleguide/tsguide.html#type-inference
     */
    '@typescript-eslint/no-inferrable-types': [
      'error',
      {
        // Function parameters may have explicit types for clearer APIs.
        ignoreParameters: true,
        // Class properties may have explicit types for clearer APIs.
        ignoreProperties: true,
      },
    ],
    /**
     * https://google.github.io/styleguide/tsguide.html#function-expressions
     */
    'prefer-arrow-callback': 'error',
  },
  // clang-format on
};
