// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Declarations of prperaties added to global `window` for checking only (not in
 * production).
 */

// Global `window` context.
declare global {
  interface Window {
    initClient: Func;
    supersize: any;
  }
}

export {};
