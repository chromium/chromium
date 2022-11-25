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

import typescript from '@rollup/plugin-typescript';
import json from '@rollup/plugin-json';
import {string} from 'rollup-plugin-string';
import {terser} from 'rollup-plugin-terser';
import {nodeResolve} from '@rollup/plugin-node-resolve';
import commonjs from '@rollup/plugin-commonjs';

export default {
  input: 'src/bidiMapper/mapper.ts',
  output: {
    file: 'src/.build/bidiMapper/mapper.js',
    sourcemap: true,
    format: 'iife',
  },
  plugins: [
    json(),
    string({
      include: 'src/bidiMapper/scripts/*.es',
    }),
    typescript({
      tsconfig: 'src/bidiMapper/tsconfig.json',
    }),
    nodeResolve(),
    commonjs(),
    terser(),
  ],
};
