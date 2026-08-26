/**
 * Copyright 2026 Google LLC.
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

/**
 * This file configures rules and version restrictions for `npm-check-updates` (ncu)
 * when checking and upgrading dependencies in `package.json`.
 */
export default {
  reject: [
    // `devtools-protocol` definitions must stay in sync with the Chrome/CDP
    // version used in Chromium and are updated separately.
    'devtools-protocol',
  ],
  target: (packageName) => {
    // `typescript-eslint` does not yet support TypeScript >= 7.0.0, so keep
    // TypeScript on the 6.x release line.
    if (packageName === 'typescript') {
      return '@6';
    }
    // The published package supports Node 20 LTS as its minimum runtime for
    // external clients (per `engines.node` in package.json), so keep
    // `@types/node` on the 20.x line (< 21.0.0) to avoid using APIs unavailable
    // in Node 20.
    if (packageName === '@types/node') {
      return '@20';
    }
    // Zod 4.x introduces breaking API changes (e.g. error format changes) that
    // are incompatible with the current protocol parser implementation.
    if (packageName === 'zod') {
      return '@3';
    }
    return 'latest';
  },
  filterResults: (packageName, {upgradedVersionSemver}) => {
    const upgradedMajor = parseInt(upgradedVersionSemver?.major, 10);
    // Ignore TypeScript >= 7.0.0
    if (packageName === 'typescript' && upgradedMajor >= 7) {
      return false;
    }
    // Ignore @types/node >= 21.0.0
    if (packageName === '@types/node' && upgradedMajor >= 21) {
      return false;
    }
    // Ignore zod >= 4.0.0
    if (packageName === 'zod' && upgradedMajor >= 4) {
      return false;
    }
    return true;
  },
};
