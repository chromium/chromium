// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Declarations for extern libraries for checking only (not production).
 */

// Google APIs definitions.
declare namespace gapi {
  function load(string, Func): any;
  function client(): any;

  declare namespace auth2 {
    function getAuthInstance(): any;
  }

  declare namespace client {
    function init(Object): any;
  }
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
