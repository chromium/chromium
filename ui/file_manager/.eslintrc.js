// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module.exports = {
  'rules' : {
    'no-console' : 'off',
  },

  'overrides' : [{
                'files' : ['**/*.ts'],
                'parser' : '../../third_party/node/node_modules/@typescript-eslint/parser/dist/index.js',
                'plugins' :
                          [
                            '@typescript-eslint',
                          ],
                'rules' : {
                  // rule override goes here.
                  'no-fallthrough' : 'error',
                },
              }],
};
