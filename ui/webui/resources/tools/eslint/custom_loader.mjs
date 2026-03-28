// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {join} from 'node:path';

const ROOT = new URL(join('..', '..', '..', '..', '..'), import.meta.url);

const ALLOWED_URLS = new Set([
  '/third_party/node/node_modules/eslint-plugin-lit/lib/index.js',
  '/third_party/node/node_modules/esquery/dist/esquery.esm.min.js',
  '/third_party/node/node_modules/@typescript-eslint/eslint-plugin/dist/index.js',
  '/third_party/node/node_modules/@typescript-eslint/parser/dist/index.js',
  '/third_party/node/node_modules/@typescript-eslint/types/dist/index.js',
  '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js',
  '/third_party/node/node_modules/typescript/lib/typescript.js',
]);

export async function resolve(specifier, context, nextResolve) {
  if (ALLOWED_URLS.has(specifier)) {
    // Redirect allow-listed URLs to //third_party/node/node_modules/...
    const redirected = new URL(specifier.slice(1), ROOT).href;
    return nextResolve(redirected, context);
  }
  return nextResolve(specifier, context);
}
