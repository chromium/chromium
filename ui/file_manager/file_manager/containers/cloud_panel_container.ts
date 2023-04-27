// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Maintains the state of the `<xf-cloud-panel>` and ensures the
 * information is passed to it appropriately.
 * @suppress {checkTypes} TS already checks this file.
 */

import {util} from '../common/js/util.js';
import {State} from '../externs/ts/state.js';
import {getStore, Store} from '../state/store.js';
import {XfCloudPanel} from '../widgets/xf_cloud_panel.js';

export type BulkPinProgress = chrome.fileManagerPrivate.BulkPinProgress;
export const BulkPinStage = chrome.fileManagerPrivate.BulkPinStage;

/**
 * The `CloudPanelContainer` acts as a translation layer between the data in the
 * store relating to bulk pinning and the `<xf-cloud-panel>` element that
 * presents the data.
 */
export class CloudPanelContainer {
  /**
   * The store element.
   */
  private store_: Store;

  /**
   * The previously stored progress, used to identify if any changes have
   * occurred. Store updates could happen to any key within the store, so make
   * sure it's to one that this container uses.
   */
  private progress_: BulkPinProgress|null = null;

  /**
   * Keeps track of the number of updates.
   * NOTE: Used for testing only.
   */
  private updates_ = 0;

  constructor(private panel_: XfCloudPanel, private test_: boolean = false) {
    this.store_ = getStore();
    this.store_.subscribe(this);
  }

  get updates() {
    return this.updates_;
  }

  onStateChanged(state: State) {
    const bulkPinProgress = state.bulkPinning;
    if (!bulkPinProgress) {
      return;
    }

    // Check if any of the keys have changed.
    if (this.progress_ && this.progress_ === bulkPinProgress) {
      return;
    }
    this.progress_ = bulkPinProgress;

    // If the `stage` represents an in progress stage, then update the
    // attributes on the `<xf-cloud-panel>` element to reflect the current
    // progress.
    if (!util.isBulkPinningInProgress(bulkPinProgress.stage)) {
      return;
    }

    if (bulkPinProgress.filesToPin < 0 ||
        (bulkPinProgress.pinnedBytes > bulkPinProgress.bytesToPin)) {
      return;
    }

    this.panel_.setAttribute('items', String(bulkPinProgress.filesToPin));
    const percentage =
        (bulkPinProgress.pinnedBytes / bulkPinProgress.bytesToPin * 100)
            .toFixed(0);
    this.panel_.setAttribute('percentage', String(percentage));

    // If in a test environment, keep track of the number of updates that have
    // been performed based on state changes.
    if (this.test_) {
      ++this.updates_;
    }
  }
}
