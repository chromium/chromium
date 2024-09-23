// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OneDrivePlaceholder} from '../../common/js/files_app_entry_types.js';
import {isSkyvaultV2Enabled} from '../../common/js/flags.js';
import {addUiEntry, removeUiEntry} from '../../state/ducks/ui_entries.js';
import {oneDriveFakeRootKey} from '../../state/ducks/volumes.js';
import type {State} from '../../state/state.js';
import {getStore} from '../../state/store.js';
import type {Store} from '../../state/store.js';

/**
 * OneDriveController handles the foreground UI relating to ODFS placeholder.
 */
export class OneDriveController {
  private localUserFilesAllowed_: boolean|undefined = undefined;
  private defaultLocation_: chrome.fileManagerPrivate.DefaultLocation|
      undefined = undefined;

  private store_: Store;

  constructor() {
    this.store_ = getStore();
    this.store_.subscribe(this);
  }

  async onStateChanged(state: State) {
    if (!isSkyvaultV2Enabled()) {
      return;
    }
    const localUserFilesAllowed = state.preferences?.localUserFilesAllowed;
    const defaultLocation = state.preferences?.defaultLocation;

    if (this.localUserFilesAllowed_ !== localUserFilesAllowed ||
        this.defaultLocation_ !== defaultLocation) {
      this.localUserFilesAllowed_ = localUserFilesAllowed;
      this.defaultLocation_ = defaultLocation;
      return this.refresh();
    }
  }

  /**
   * Adds or removes the OneDrive placeholder based on the SkyVault policies.
   */
  async refresh() {
    if (!isSkyvaultV2Enabled()) {
      return;
    }
    if (!this.localUserFilesAllowed_ &&
        this.defaultLocation_ ===
            chrome.fileManagerPrivate.DefaultLocation.ONEDRIVE) {
      // TODO(b/334511998): Use proper strings.
      const oneDriveFakeRoot = new OneDrivePlaceholder('Microsoft OneDrive');
      this.store_.dispatch(addUiEntry(oneDriveFakeRoot));
    } else {
      this.store_.dispatch(removeUiEntry(oneDriveFakeRootKey));
    }
  }
}
