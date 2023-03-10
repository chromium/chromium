// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {FileSelection, FileSelectionHandler} from './file_selection.js';

/**
 * Mock FileSelectionHandler.
 * @extends {FileSelectionHandler}
 */
export class FakeFileSelectionHandler {
  constructor() {
    this.selection = /** @type {!FileSelection} */ ({});
    this.updateSelection([], []);
    this.eventTarget_ = new EventTarget();
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

  isAvailable() {
    return true;
  }
}
