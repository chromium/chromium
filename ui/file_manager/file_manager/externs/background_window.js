// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BackgroundBase} from './background/background_base.js';

/**
 * @extends {Window}
 */
export class BackgroundWindow {
  constructor() {
    /**
     * For File Manager it uses FileBrowserBackgroundFull.
     * For all other apps it uses BackgroundBase.
     *
     * TODO(crbug.com/1148545): Add `FileBrowserBackgroundFull` together with
     * `BackgroundBase` below.
     *
     * @type {!BackgroundBase}
     */
    this.background;
  }
}
