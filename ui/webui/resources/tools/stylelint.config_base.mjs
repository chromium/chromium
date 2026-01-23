// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export default {
  plugins: ['../../../../third_party/node/node_modules/@stylistic/stylelint-plugin/lib/index.js'],

  rules: {
    'block-no-empty': true,
    '@stylistic/no-missing-end-of-source-newline': true,

    // https://google.github.io/styleguide/htmlcssguide.html#CSS_Quotation_Marks
    '@stylistic/string-quotes': 'single',
  }
};
