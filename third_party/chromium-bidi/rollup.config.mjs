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

import {nodeResolve} from '@rollup/plugin-node-resolve';
import cleanup from 'rollup-plugin-cleanup';
import license from 'rollup-plugin-license';

// Generate Mapper Tab from ESM as we can run that in the browser
const mapperTabConfig = {
  input: 'lib/esm/bidiTab/bidiTab.js',
  output: {
    name: 'mapperTab',
    file: 'lib/iife/mapperTab.js',
    sourcemap: true,
    format: 'iife',
  },
  plugins: [
    cleanup({
      // Keep license comments. Other comments are removed due to
      // http://b/390559299 and
      // https://github.com/microsoft/TypeScript/issues/60811.
      comments: [/Copyright/i],
    }),
    license({
      thirdParty: {
        allow: {
          test: (dependency) => {
            let allowed_licenses = ['MIT', 'Apache 2.0'];
            return allowed_licenses.includes(dependency.license);
          },
          failOnUnlicensed: true,
          failOnViolation: true,
        },
        output: {
          file: path.join('lib', 'THIRD_PARTY_NOTICES'),
          template(dependencies) {
            const stringified_dependencies = dependencies.map((dependency) => {
              let arr = [];
              arr.push(`Name: ${dependency.name ?? 'N/A'}`);
              let url = dependency.homepage ?? dependency.repository;
              if (url !== null && typeof url !== 'string') {
                url = url.url;
              }
              arr.push(`URL: ${url ?? 'N/A'}`);
              arr.push(`Version: ${dependency.version ?? 'N/A'}`);
              arr.push(`License: ${dependency.license ?? 'N/A'}`);
              if (dependency.licenseText !== null) {
                arr.push('');
                arr.push(dependency.licenseText.replaceAll('\r', ''));
              }
              return arr.join('\n');
            });
            const divider =
              '\n\n-------------------- DEPENDENCY DIVIDER --------------------\n\n';
            return stringified_dependencies.join(divider);
          },
        },
      },
    }),
    nodeResolve(),
  ],
};

export default [mapperTabConfig];
