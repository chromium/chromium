// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/******** VisOptions ********/
/**
 * Defines globally shared, mutable state for rendering options.
 */
class VisOptions {
  constructor() {}
}

/******** MainModel ********/
/**
 * Global source of truth representing the device state. Owns data fetching from
 * the ADB server.
 */
class MainModel {
  constructor() {
    this.visOpts = new VisOptions();
    this.screenshotBlob = null;
  }

  /** Fetches the latest device data from the ADB server. */
  async fetchData() {
    const screenshotResponse = await fetch('/api/screenshot.png');

    if (!screenshotResponse.ok) throw new Error('Screenshot fetch failed');

    this.screenshotBlob = await screenshotResponse.blob();
  }
}
