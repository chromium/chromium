// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(b/199452030): Fix duplication with volume_manager_types.js
/**
 * Paths that can be handled by the dialog opener in native code.
 * @enum {string}
 * @const
 */
export const AllowedPaths = {
  NATIVE_PATH: 'nativePath',
  ANY_PATH: 'anyPath',
  ANY_PATH_OR_URL: 'anyPathOrUrl',
};
