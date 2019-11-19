// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Mock FileSelectionHandler.
 * @extends {FileSelectionHandler}
 */
class FakeFileSelectionHandler {
  constructor() {
    this.selection = /** @type {!FileSelection} */ ({});
    this.updateSelection([], []);
    this.eventTarget_ = new cr.EventTarget();
  }

  computeAdditionalCallback() {}

  updateSelection(entries, mimeTypes) {
    this.selection = /** @type {!FileSelection} */ ({
      entries: entries,
      mimeTypes: mimeTypes,
      computeAdditional: (metadataModel) => {
        this.computeAdditionalCallback();
        return new Promise((resolve) => {
          resolve();
        });
      },
    });
  }

  addEventListener(...args) {
    return this.eventTarget_.addEventListener(...args);
  }
}
