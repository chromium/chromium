/**
 * Copyright 2022 Google LLC.
 * Copyright (c) Microsoft Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

const CI = process.env.CI || false;

module.exports = {
  root: true,
  env: {
    browser: true,
    es6: true,
    mocha: true,
    node: true,
  },
  globals: {
    globalThis: false,
  },
  parser: '@typescript-eslint/parser',

  settings: {
    'import/resolver': {
      typescript: true,
    },
  },
  plugins: ['@typescript-eslint', 'import', 'mocha', 'promise'],
  extends: [
    // keep-sorted start
    'eslint:recommended',
    'plugin:import/typescript',
    'plugin:mocha/recommended',
    'plugin:prettier/recommended',
    'plugin:promise/recommended',
    // keep-sorted end
  ],
  rules: {
    // https://denar90.github.io/eslint.github.io/docs/rules/
    // Some rules use 'warn' in order to ease local development iteration.
    // keep-sorted start block=yes sticky_comments=yes
    'func-names': 'error',
    'import/first': 'error',
    'import/newline-after-import': 'error',
    'import/no-cycle': ['error', {maxDepth: Infinity}],
    'import/no-duplicates': 'error',
    'import/no-unresolved': 'off',
    'import/order': [
      'error',
      {
        'newlines-between': 'always',
        alphabetize: {
          order: 'asc',
          caseInsensitive: true,
        },
      },
    ],
    'mocha/no-exclusive-tests': 'error',
    'mocha/no-mocha-arrows': 'off',
    'mocha/no-setup-in-describe': 'off',
    'no-console': CI ? 'error' : 'warn',
    'no-else-return': 'warn',
    'no-empty': ['warn', {allowEmptyCatch: true}],
    'no-implicit-coercion': 'error',
    'no-negated-condition': 'error',
    'no-undef': 'error',
    'no-underscore-dangle': 'error',
    'object-shorthand': 'error',
    'prefer-promise-reject-errors': 'error',
    'prefer-template': 'error',
    eqeqeq: 'error',
    // keep-sorted end
  },
  overrides: [
    {
      files: ['*.ts'],
      extends: [
        'plugin:@typescript-eslint/eslint-recommended',
        'plugin:@typescript-eslint/recommended',
        'plugin:@typescript-eslint/recommended-requiring-type-checking',
      ],
      parserOptions: {
        project: './tsconfig.base.json',
      },
      rules: {
        // keep-sorted start block=yes sticky_comments=yes
        '@typescript-eslint/array-type': 'warn',
        '@typescript-eslint/consistent-generic-constructors': 'warn',
        '@typescript-eslint/consistent-type-imports': [
          'warn',
          {fixStyle: 'inline-type-imports'},
        ],
        '@typescript-eslint/explicit-member-accessibility': [
          'warn',
          {accessibility: 'no-public'},
        ],
        '@typescript-eslint/no-empty-function': 'warn',
        '@typescript-eslint/no-explicit-any': 'off',
        '@typescript-eslint/no-extraneous-class': 'warn',
        '@typescript-eslint/no-import-type-side-effects': 'error',
        '@typescript-eslint/no-misused-promises': 'off',
        '@typescript-eslint/no-namespace': 'off',
        '@typescript-eslint/no-non-null-assertion': 'off',
        '@typescript-eslint/no-unsafe-argument': 'off',
        '@typescript-eslint/no-unsafe-assignment': 'off',
        '@typescript-eslint/no-unsafe-call': 'off',
        '@typescript-eslint/no-unsafe-enum-comparison': 'warn',
        '@typescript-eslint/no-unsafe-member-access': 'off',
        '@typescript-eslint/no-unsafe-return': 'off',
        '@typescript-eslint/no-unused-vars': [
          'error',
          {argsIgnorePattern: '^_'},
        ],
        '@typescript-eslint/no-useless-template-literals': 'error',
        '@typescript-eslint/prefer-return-this-type': 'warn',
        '@typescript-eslint/require-await': 'warn',
        '@typescript-eslint/restrict-template-expressions': 'off',
        // This is more performant; see https://v8.dev/blog/fast-async.
        '@typescript-eslint/return-await': ['error', 'always'],
        '@typescript-eslint/switch-exhaustiveness-check': 'error',
        // keep-sorted end
      },
    },
  ],
};
