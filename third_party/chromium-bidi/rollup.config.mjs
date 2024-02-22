/**
 * Copyright 2021 Google LLC.
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
import path from 'path';

import commonjs from '@rollup/plugin-commonjs';
import {nodeResolve} from '@rollup/plugin-node-resolve';
import terser from '@rollup/plugin-terser';
import license from 'rollup-plugin-license';

export default {
  input: 'lib/cjs/bidiTab/bidiTab.js',
  output: {
    name: 'mapperTab',
    file: 'lib/iife/mapperTab.js',
    sourcemap: true,
    format: 'iife',
  },
  plugins: [
    license({
      thirdParty: {
        output: {
          file: path.join('lib', 'THIRD_PARTY_NOTICES'),
        },
      },
    }),
    nodeResolve(),
    commonjs({
      // `crypto` is only imported in the uuid polyfill for Node versions
      // without webcrypto exposes globally.
      ignore: ['crypto'],
    }),
    terser({
      format: {
        ascii_only: true,
      },
    }),
  ],
};
