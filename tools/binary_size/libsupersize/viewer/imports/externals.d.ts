// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Declarations for extern libraries for checking only (not production).
 */

// Google APIs definitions.
declare namespace google {
  declare namespace accounts {
    declare namespace oauth2 {
      function revoke(string, Func): void;
      function initTokenClient({}): TokenClient;
    }
  }
}

interface TokenClient {
  requestAccessToken: Func;
  callback: Func;
}

// Emscripten definitions.
declare namespace Module {
  function _malloc(number): any;
  function cwrap(...args): any;
  function UTF8ToString(string, number?): string;
  function _free(number): any;

  let HEAPU8: Uint8Array;
}

// Diff2Html definitions.
declare namespace Diff2Html {
  function html(string, Object): string;
}
