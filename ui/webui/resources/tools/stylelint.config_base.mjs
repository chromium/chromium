// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export default {
  plugins: ['../../../../third_party/node/node_modules/@stylistic/stylelint-plugin/lib/index.js'],

  rules: {
    /* Correctness checks */

    'at-rule-no-unknown': true,
    'color-no-invalid-hex': true,
    'custom-property-no-missing-var-function': true,
    'declaration-block-no-duplicate-custom-properties': true,
    'declaration-block-no-duplicate-properties': true,
    'declaration-block-no-shorthand-property-overrides': true,
    'declaration-property-value-no-unknown': true,
    'function-calc-no-unspaced-operator': true,
    'function-no-unknown': true,
    'media-feature-name-no-unknown': true,
    'media-feature-name-value-no-unknown': true,
    'no-duplicate-selectors': true,
    'no-irregular-whitespace': true,
    'property-no-unknown': true,
    'selector-pseudo-class-no-unknown': true,
    'selector-pseudo-element-no-unknown': true,
    'unit-no-unknown': true,

    /*  Stylistic chceks. */

    'block-no-empty': true,

    // https://google.github.io/styleguide/htmlcssguide.html#Hexadecimal_Notation
    'color-hex-length': 'short',

    // https://google.github.io/styleguide/htmlcssguide.html#0_and_Units
    'length-zero-no-unit': [true, { "ignore": ["custom-properties"] }],

    // https://google.github.io/styleguide/htmlcssguide.html#Rule_Separation
    'rule-empty-line-before': [
      'always', {
        'ignore': ['after-comment', 'first-nested', 'inside-block'],
      },
    ],

    '@stylistic/no-missing-end-of-source-newline': true,

    // https://google.github.io/styleguide/htmlcssguide.html#CSS_Quotation_Marks
    '@stylistic/string-quotes': 'single',

    // https://google.github.io/styleguide/htmlcssguide.html#Selector_and_Declaration_Separation
    '@stylistic/declaration-block-semicolon-newline-after': 'always',
    '@stylistic/declaration-block-semicolon-newline-before': 'never-multi-line',
    '@stylistic/declaration-block-semicolon-space-before': 'never',
    '@stylistic/declaration-block-trailing-semicolon': 'always',
    '@stylistic/no-extra-semicolons': true,
    '@stylistic/selector-list-comma-newline-after': 'always',

    // https://google.github.io/styleguide/htmlcssguide.html#Property_Name_Stops
    '@stylistic/media-feature-colon-space-after': 'always',
    '@stylistic/media-feature-colon-space-before': 'never',
    '@stylistic/media-feature-range-operator-space-before': 'always',
    '@stylistic/media-feature-range-operator-space-after': 'always',
    '@stylistic/media-feature-parentheses-space-inside': 'never',
    '@stylistic/media-feature-name-case': 'lower',
  }
};
