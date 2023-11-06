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

  // @ts-ignore: error TS2355: A function whose declared type is neither 'void'
  // nor 'any' must return a value.
  /** @return {boolean} */
  isFocused() {}
}
