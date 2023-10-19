// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {Store} from '../../externs/ts/store.js';
import {updateDirectoryContent, updateSelection} from '../../state/ducks/current_directory.js';

// @ts-ignore: error TS6133: 'FileSelectionHandler' is declared but its value is
// never read.
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
  // @ts-ignore: error TS2322: Type 'null' is not assignable to type 'Store |
  // undefined'.
  updateSelection(entries, mimeTypes, store = null) {
    this.selection = /** @type {!FileSelection} */ ({
      entries: entries,
      mimeTypes: mimeTypes,
      // @ts-ignore: error TS6133: 'metadataModel' is declared but its value is
      // never read.
      computeAdditional: (metadataModel) => {
        this.computeAdditionalCallback();
        return new Promise((resolve) => {
          // @ts-ignore: error TS2810: Expected 1 argument, but got 0. 'new
          // Promise()' needs a JSDoc hint to produce a 'resolve' that can be
          // called without arguments.
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

  // @ts-ignore: error TS7019: Rest parameter 'args' implicitly has an 'any[]'
  // type.
  addEventListener(...args) {
    // @ts-ignore: error TS2556: A spread argument must either have a tuple type
    // or be passed to a rest parameter.
    return this.eventTarget_.addEventListener(...args);
  }

  isAvailable() {
    return true;
  }
}
