// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {recordDirectoryListLoadWithTolerance, startInterval} from '../../common/js/metrics.js';
import {RootType, VolumeType} from '../../common/js/volume_manager_types.js';
import {updateDirectoryContent} from '../../state/ducks/current_directory.js';
import {PropStatus} from '../../state/state.js';
import {getStore, type Store} from '../../state/store.js';

import type {CurDirScanUpdatedEvent, DirectoryModel} from './directory_model.js';
import type {FileSelectionHandler} from './file_selection.js';
import type {SpinnerController} from './spinner_controller.js';
import type {ListContainer} from './ui/list_container.js';

/**
 * Handler for scan related events of DirectoryModel.
 */
export class ScanController {
  private readonly store_: Store;
  /**
   * Whether a scan is in progress.
   */
  private scanInProgress_: boolean = false;
  /**
   * Timer ID to delay UI refresh after a scan is updated.
   */
  private scanUpdatedTimer_: number = 0;

  private spinnerHideCallback_: VoidCallback|null = null;

  constructor(
      private readonly directoryModel_: DirectoryModel,
      private readonly listContainer_: ListContainer,
      private readonly spinnerController_: SpinnerController,
      private readonly selectionHandler_: FileSelectionHandler) {
    this.store_ = getStore();
    this.directoryModel_.addEventListener(
        'cur-dir-scan-started', this.onScanStarted_.bind(this));
    this.directoryModel_.addEventListener(
        'cur-dir-scan-completed', this.onScanCompleted_.bind(this));
    this.directoryModel_.addEventListener(
        'cur-dir-scan-failed', this.onScanCancelled_.bind(this));
    this.directoryModel_.addEventListener(
        'cur-dir-scan-canceled', this.onScanCancelled_.bind(this));
    this.directoryModel_.addEventListener(
        'cur-dir-scan-updated', this.onScanUpdated_.bind(this));
    this.directoryModel_.addEventListener(
        'cur-dir-rescan-completed', this.onRescanCompleted_.bind(this));
  }

  private onScanStarted_() {
    if (this.scanInProgress_) {
      this.listContainer_.endBatchUpdates();
    }

    if (window.IN_TEST) {
      this.listContainer_.element.removeAttribute('cur-dir-scan-completed');
      this.listContainer_.element.setAttribute(
          'scan-started', this.directoryModel_.getCurrentDirName());
    }

    const volumeInfo = this.directoryModel_.getCurrentVolumeInfo();
    if (volumeInfo &&
        (volumeInfo.volumeType === VolumeType.DOWNLOADS ||
         volumeInfo.volumeType === VolumeType.MY_FILES)) {
      startInterval(`DirectoryListLoad.${RootType.MY_FILES}`);
    }

    this.listContainer_.startBatchUpdates();
    this.scanInProgress_ = true;

    if (this.scanUpdatedTimer_) {
      clearTimeout(this.scanUpdatedTimer_);
      this.scanUpdatedTimer_ = 0;
    }

    this.hideSpinner_();
    this.spinnerHideCallback_ = this.spinnerController_.showWithDelay(
        500, this.onSpinnerShown_.bind(this));
  }

  private onScanCompleted_() {
    if (!this.scanInProgress_) {
      console.warn('Scan-completed event received. But scan is not started.');
      return;
    }

    if (window.IN_TEST) {
      this.listContainer_.element.removeAttribute('scan-started');
      this.listContainer_.element.setAttribute(
          'scan-completed', this.directoryModel_.getCurrentDirName());
    }

    // Update the store with the new entries before hiding the spinner.
    this.updateStore_();

    this.hideSpinner_();

    if (this.scanUpdatedTimer_) {
      clearTimeout(this.scanUpdatedTimer_);
      this.scanUpdatedTimer_ = 0;
    }

    this.scanInProgress_ = false;
    this.listContainer_.endBatchUpdates();

    // TODO(crbug.com/1290197): Currently we only care about the load time for
    // local files, filter out all the other root types.
    if (this.directoryModel_.getCurrentDirEntry()) {
      const volumeInfo = this.directoryModel_.getCurrentVolumeInfo();
      if (volumeInfo &&
          (volumeInfo.volumeType === VolumeType.DOWNLOADS ||
           volumeInfo.volumeType === VolumeType.MY_FILES)) {
        const metricName = `DirectoryListLoad.${RootType.MY_FILES}`;
        recordDirectoryListLoadWithTolerance(
            metricName, this.directoryModel_.getFileList().length,
            [10, 100, 1000], /*tolerance=*/ 0.2);
      }
    }
  }

  /**
   * Sends the scanned directory content to the Store.
   */
  private updateStore_() {
    const entries = this.directoryModel_.getFileList().slice();
    this.store_.dispatch(
        updateDirectoryContent({entries, status: PropStatus.SUCCESS}));
  }

  /**
   */
  private onScanUpdated_(e: CurDirScanUpdatedEvent) {
    if (!this.scanInProgress_) {
      console.warn('Scan-updated event received. But scan is not started.');
      return;
    }

    // Call this immediately (instead of debouncing it with `scanUpdatedTimer_`)
    // so the current directory entries don't get accidentally removed from the
    // store by `clearCachedEntries()`, when the scan is store-based the entries
    // are already in the store.
    if (!e.detail.isStoreBased) {
      this.updateStore_();
    }

    if (this.scanUpdatedTimer_) {
      return;
    }

    // Show contents incrementally by finishing batch updated, but only after
    // 200ms elapsed, to avoid flickering when it is not necessary.
    this.scanUpdatedTimer_ = setTimeout(() => {
      this.hideSpinner_();

      // Update the UI.
      if (this.scanInProgress_) {
        this.listContainer_.endBatchUpdates();
        this.listContainer_.startBatchUpdates();
      }
      this.scanUpdatedTimer_ = 0;
    }, 200);
  }

  /**
   */
  private onScanCancelled_() {
    if (!this.scanInProgress_) {
      console.warn('Scan-cancelled event received. But scan is not started.');
      return;
    }

    this.hideSpinner_();

    if (this.scanUpdatedTimer_) {
      clearTimeout(this.scanUpdatedTimer_);
      this.scanUpdatedTimer_ = 0;
    }

    this.scanInProgress_ = false;
    this.listContainer_.endBatchUpdates();
  }

  /**
   * Handle the 'cur-dir-rescan-completed' from the DirectoryModel.
   */
  private onRescanCompleted_() {
    this.updateStore_();
    this.selectionHandler_.onFileSelectionChanged();
  }

  /**
   * When a spinner is shown, updates the UI to remove items in the previous
   * directory.
   */
  private onSpinnerShown_() {
    if (this.scanInProgress_) {
      this.listContainer_.endBatchUpdates();
      this.listContainer_.startBatchUpdates();
    }
  }

  /**
   * Hides the spinner if it's shown or scheduled to be shown.
   */
  private hideSpinner_() {
    if (this.spinnerHideCallback_) {
      this.spinnerHideCallback_();
      this.spinnerHideCallback_ = null;
    }
  }
}
