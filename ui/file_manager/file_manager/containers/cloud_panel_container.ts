// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Maintains the state of the `<xf-cloud-panel>` and ensures the
 * information is passed to it appropriately.
 */

import {canBulkPinningCloudPanelShow} from '../common/js/util.js';
import type {State} from '../state/state.js';
import {getStore, type Store} from '../state/store.js';
import {type CloudPanelSettingsClickEvent, CloudPanelType, XfCloudPanel} from '../widgets/xf_cloud_panel.js';

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
   * The driveFsBulkPinningEnabled preference, used to identify if it has
   * changed or not.
   */
  private bulkPinningEnabled_: boolean = false;

  /**
   * If true, drive syncing is paused due to both being on a network reporting
   * as metered and also having the preference of syncing disabled on metered
   * networks.
   */
  private isOnMetered_: boolean = false;

  /**
   * Keeps track of the number of updates.
   * NOTE: Used for testing only.
   */
  private updates_ = 0;

  constructor(private panel_: XfCloudPanel, private test_: boolean = false) {
    this.store_ = getStore();
    this.store_.subscribe(this);

    this.panel_.addEventListener(
        XfCloudPanel.events.DRIVE_SETTINGS_CLICKED,
        this.onDriveSettingsClick_.bind(this));
  }

  get updates() {
    return this.updates_;
  }

  /**
   * When the "Google Drive settings" button is clicked, open OS Settings to the
   * Google Drive page.
   */
  private onDriveSettingsClick_(_: CloudPanelSettingsClickEvent) {
    chrome.fileManagerPrivate.openSettingsSubpage('googleDrive');
  }

  onStateChanged(state: State) {
    const progress = state.bulkPinning;
    const enabled = !!state.preferences?.driveFsBulkPinningEnabled;
    if (!progress) {
      this.bulkPinningEnabled_ = enabled;
      return;
    }

    const isOnMetered = state.drive?.connectionType ===
        chrome.fileManagerPrivate.DriveConnectionStateType.METERED;

    // Check if any of the required state has changed between store changes.
    if ((this.progress_ &&
         (this.progress_.stage === progress.stage &&
          this.progress_.filesToPin === progress.filesToPin &&
          this.progress_.pinnedBytes === progress.pinnedBytes &&
          this.progress_.bytesToPin === progress.bytesToPin &&
          this.progress_.remainingSeconds === progress.remainingSeconds)) &&
        this.bulkPinningEnabled_ === enabled &&
        this.isOnMetered_ === isOnMetered) {
      return;
    }

    this.progress_ = progress;
    this.bulkPinningEnabled_ = enabled;
    this.isOnMetered_ = isOnMetered;

    // If the bulk pinning cloud panel can't be shown, make sure to close any
    // open variants of it. This ensures if the panel is open when the
    // preference is disabled, it will not stay open with stale data.
    if (!canBulkPinningCloudPanelShow(progress.stage, enabled)) {
      this.panel_.close();
      return;
    }

    // When on a metered network, drive syncing is paused.
    if (this.isOnMetered_) {
      this.updatePanelType_(CloudPanelType.METERED_NETWORK);
      return;
    }

    // If the bulk pinning is paused, this indicates that it is currently
    // offline or battery saver mode is active.
    if (progress.stage === BulkPinStage.PAUSED_OFFLINE) {
      this.updatePanelType_(CloudPanelType.OFFLINE);
      return;
    }
    if (progress.stage === BulkPinStage.PAUSED_BATTERY_SAVER) {
      this.updatePanelType_(CloudPanelType.BATTERY_SAVER);
      return;
    }

    // Not enough space indicates the available local storage is insufficient to
    // store all the files required by the users My drive.
    if (progress.stage === BulkPinStage.NOT_ENOUGH_SPACE) {
      this.updatePanelType_(CloudPanelType.NOT_ENOUGH_SPACE);
      return;
    }
    this.panel_.removeAttribute('type');

    // Files to pin can't be negative the pinned bytes should never exceed
    // the bytes to pin (>100%).
    if (progress.filesToPin < 0 ||
        (progress.pinnedBytes > progress.bytesToPin)) {
      return;
    }

    this.clearAllAttributes_();
    this.panel_.setAttribute('items', String(progress.filesToPin));
    const percentage = (progress.bytesToPin === 0) ?
        '100' :
        (progress.pinnedBytes / progress.bytesToPin * 100).toFixed(0);
    if ((progress.filesToPin > 0 && progress.pinnedBytes > 0) ||
        (progress.pinnedBytes === 0 && progress.bytesToPin === 0) ||
        (progress.filesToPin === 0)) {
      this.panel_.setAttribute('percentage', String(percentage));
    }
    this.panel_.setAttribute('seconds', String(progress.remainingSeconds));
    this.increaseUpdates_();
  }

  /**
   * Updates the underlying panel to the `type` and removes the in progress
   * attributes.
   */
  private updatePanelType_(type: CloudPanelType) {
    this.panel_.setAttribute('type', type);
    this.clearAllAttributes_();
    this.increaseUpdates_();
  }

  /**
   * Clear all the attributes in anticipation of setting new ones.
   */
  private clearAllAttributes_() {
    this.panel_.removeAttribute('items');
    this.panel_.removeAttribute('percentage');
    this.panel_.removeAttribute('seconds');
  }

  /**
   * If in a test environment, keep track of the number of updates that have
   * been performed based on state changes.
   */
  private increaseUpdates_() {
    if (this.test_) {
      ++this.updates_;
    }
  }
}
