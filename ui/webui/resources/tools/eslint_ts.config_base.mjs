// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import eslintPluginLit from '../../../../third_party/node/node_modules/eslint-plugin-lit/lib/index.js';
import typescriptEslint from '../../../../third_party/node/node_modules/@typescript-eslint/eslint-plugin/dist/index.js';
import tsParser from '../../../../third_party/node/node_modules/@typescript-eslint/parser/dist/index.js';
import webUiEslint from './webui_eslint_plugin.js';

export const defaultConfig = [
  {
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
  },
  {
    files: ['**/*.html.ts'],
    plugins: {
      'eslint-plugin-lit': eslintPluginLit,
    },
    rules: {
      'eslint-plugin-lit/binding-positions': 'error',
      'eslint-plugin-lit/no-duplicate-template-bindings': 'error',
      'eslint-plugin-lit/no-invalid-escape-sequences': 'error',
      'eslint-plugin-lit/no-invalid-html': 'error',
      'eslint-plugin-lit/no-private-properties': ['error', {'private': '_$'}],
    },
  },
];

// A dedicated config that is only added when
// enable_web_component_missing_deps=true is passed to eslint_ts().
// TODO(crbug.com/457866803): Move this to the defaultConfig above once all
// violations are fixed.
export const webComponentMissingDepsConfig = {
  files: ['**/*.html.ts'],
  plugins: {
    'eslint-plugin-lit': eslintPluginLit,
  },
  rules: {
    '@webui-eslint/web-component-missing-deps': 'error',
  },
};
