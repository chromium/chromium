// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {FileSelectionHandler, FileSelection} from './file_selection.m.js';
// #import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';
// clang-format on

/**
 * Mock FileSelectionHandler.
 * @extends {FileSelectionHandler}
 */
/* #export */ class FakeFileSelectionHandler {
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

  isAvailable() {
    return true;
  }
}
