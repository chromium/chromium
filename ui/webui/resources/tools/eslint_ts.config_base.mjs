// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import typescriptEslint from '../../../../third_party/node/node_modules/@typescript-eslint/eslint-plugin/dist/index.js';
import tsParser from '../../../../third_party/node/node_modules/@typescript-eslint/parser/dist/index.js';
import webUiEslint from './webui_eslint_plugin.js';

export default {
  languageOptions: {
    ecmaVersion: 2020,
    sourceType: 'module',
    parser: tsParser,
    parserOptions: {
      disallowAutomaticSingleRunInference: true,
    },

    // The following field should be specified by client code. as follows:
    //
    // parserOptions: {
    //   project: [path.join(import.meta.dirname, './tsconfig_build_ts.json')],
    // },
  },

  plugins: {
    '@typescript-eslint': typescriptEslint,
    '@webui-eslint': webUiEslint,
  },

  files: ['**/*.ts'],

  rules: {
    'require-await': 'off',
    '@typescript-eslint/require-await' : 'error',
    '@typescript-eslint/no-unnecessary-type-assertion': 'error',

    '@webui-eslint/lit-property-accessor': 'error',
    '@webui-eslint/polymer-property-declare': 'error',
    '@webui-eslint/polymer-property-class-member': 'error',
  },
};
