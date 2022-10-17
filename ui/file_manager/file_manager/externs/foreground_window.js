// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview External objects and functions required for compiling tests.
 */

import {Crostini} from './background/crostini.js';
import {FileManagerBaseInterface} from './background/file_manager_base.js';

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
export class ForegroundWindow {
  constructor() {
    /** @type {FileManagerTestDeps} */
    this.fileManager;

    /**
     * @type {!FileManagerBaseInterface}
     */
    this.background;
  }

  /** @return {boolean} */
  isFocused() {}
}
