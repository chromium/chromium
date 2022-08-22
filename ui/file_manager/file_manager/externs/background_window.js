// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FileManagerBaseInterface} from './background/file_manager_base.js';

/**
 * @extends {Window}
 */
export class BackgroundWindow {
  constructor() {
    /**
     * @type {!FileManagerBaseInterface}
     */
    this.background;
  }
}
