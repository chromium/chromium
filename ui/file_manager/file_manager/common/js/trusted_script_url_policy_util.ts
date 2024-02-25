// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';


// Trusted script URLs used by the Files app.
const ALLOWED_SCRIPT_URLS = new Set([
  'foreground/js/main.js',
  'background/js/runtime_loaded_test_util.js',
  'foreground/js/deferred_elements.js',
  'foreground/js/metadata/metadata_dispatcher.js',
]);

/**
 * Create a TrustedTypes script URL policy from a list of allowed sources, if
 * one does not already exist.
 *
 * We are storing this policy inside `window` as a global variable, because of
 * GN's `optimize_webui` concatenating this file multiple times and causing
 * duplicate policy definitions when we use a local variable.
 */

declare global {
  interface Window {
    trustedScriptUrlPolicy_: TrustedTypePolicy|undefined;
  }
}

if (!window.hasOwnProperty('trustedScriptUrlPolicy_')) {
  assert(window.trustedTypes);
  window.trustedScriptUrlPolicy_ =
      window.trustedTypes.createPolicy('file-manager-trusted-script', {
        createScriptURL: (url: string) => {
          if (!ALLOWED_SCRIPT_URLS.has(url)) {
            throw new Error('Script URL not allowed: ' + url);
          }
          return url;
        },
        createHTML: () => assertNotReached(),
        createScript: () => assertNotReached(),
      });
}

/**
 * Create a TrustedTypes script URL policy from a list of allowed sources, and
 * return a sanitized script URL using this policy.
 *
 * @param url Script URL to be sanitized.
 */
export function getSanitizedScriptUrl(url: string): TrustedScriptURL {
  assert(window.trustedScriptUrlPolicy_);
  return window.trustedScriptUrlPolicy_.createScriptURL(url);
}
