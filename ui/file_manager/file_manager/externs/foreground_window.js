// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview External objects and functions required for compiling tests.
 */

/** @interface */
class FileManagerTestDeps {
  constructor() {
    /** @type {Crostini} */
    this.crostini;
  }
}

/**
 * @extends {Window}
 */
class ForegroundWindow {
  constructor() {
    /** @type {FileManagerTestDeps} */
    this.fileManager;
  }
}
