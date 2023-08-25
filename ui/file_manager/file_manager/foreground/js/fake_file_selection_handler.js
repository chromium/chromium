// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {Store} from '../../externs/ts/store.js';
import {updateDirectoryContent, updateSelection} from '../../state/ducks/current_directory.js';

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

  /**
   * @param entries {!Array<Entry|FilesAppEntry>}
   * @param mimeTypes {!Array<string>}
   * @param store {Store=}
   */
  updateSelection(entries, mimeTypes, store = null) {
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

    if (store) {
      // Make sure that the entry is in the directory content.
      store.dispatch(updateDirectoryContent({entries}));
      // Mark the entry as selected.
      store.dispatch(updateSelection({
        selectedKeys: entries.map(e => e.toURL()),
        entries,
      }));
    }
  }

  addEventListener(...args) {
    return this.eventTarget_.addEventListener(...args);
  }

  isAvailable() {
    return true;
  }
}
