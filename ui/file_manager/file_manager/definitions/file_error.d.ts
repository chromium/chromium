// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Definitions for FileError.
 * This is for compatibility with Closure definition for File API [1].
 *
 * [1]
 * https://github.com/google/closure-compiler/blob/master/externs/browser/nonstandard_fileapi.js#L197
 */

interface FileError extends DOMException {
  NOT_FOUND_ERR: number;
  SECURITY_ERR: number;
  ABORT_ERR: number;
  NOT_READABLE_ERR: number;
  ENCODING_ERR: number;
  NO_MODIFICATION_ALLOWED_ERR: number;
  INVALID_STATE_ERR: number;
  SYNTAX_ERR: number;
  INVALID_MODIFICATION_ERR: number;
  QUOTA_EXCEEDED_ERR: number;
  TYPE_MISMATCH_ERR: number;
  PATH_EXISTS_ERR: number;
}
