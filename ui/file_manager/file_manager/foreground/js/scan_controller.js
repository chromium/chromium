// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Handler for scan related events of DirectoryModel.
 */
class ScanController {
  /**
   * @param {!DirectoryModel} directoryModel
   * @param {!ListContainer} listContainer
   * @param {!SpinnerController} spinnerController
   * @param {!CommandHandler} commandHandler
   * @param {!FileSelectionHandler} selectionHandler
   */
  constructor(
      directoryModel, listContainer, spinnerController, commandHandler,
      selectionHandler) {
    /** @private @const {!DirectoryModel} */
    this.directoryModel_ = directoryModel;

    /** @private @const {!ListContainer} */
    this.listContainer_ = listContainer;

    /** @private @const {!SpinnerController} */
    this.spinnerController_ = spinnerController;

    /** @private @const {!CommandHandler} */
    this.commandHandler_ = commandHandler;

    /** @private @const {!FileSelectionHandler} */
    this.selectionHandler_ = selectionHandler;

    /**
     * Whether a scan is in progress.
     * @private {boolean}
     */
    this.scanInProgress_ = false;

    /**
     * Timer ID to delay UI refresh after a scan is updated.
     * @private {number}
     */
    this.scanUpdatedTimer_ = 0;

    /**
     * @private {?function()}
     */
    this.spinnerHideCallback_ = null;

    this.directoryModel_.addEventListener(
        'scan-started', this.onScanStarted_.bind(this));
    this.directoryModel_.addEventListener(
        'scan-completed', this.onScanCompleted_.bind(this));
    this.directoryModel_.addEventListener(
        'scan-failed', this.onScanCancelled_.bind(this));
    this.directoryModel_.addEventListener(
        'scan-cancelled', this.onScanCancelled_.bind(this));
    this.directoryModel_.addEventListener(
        'scan-updated', this.onScanUpdated_.bind(this));
    this.directoryModel_.addEventListener(
        'rescan-completed', this.onRescanCompleted_.bind(this));
  }

  /**
   * @private
   */
  onScanStarted_() {
    if (this.scanInProgress_) {
      this.listContainer_.endBatchUpdates();
    }

    if (window.IN_TEST) {
      this.listContainer_.element.removeAttribute('scan-completed');
      this.listContainer_.element.setAttribute(
          'scan-started', this.directoryModel_.getCurrentDirName());
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

  /**
   * @private
   */
  onScanCompleted_() {
    if (!this.scanInProgress_) {
      console.error('Scan-completed event received. But scan is not started.');
      return;
    }

    if (window.IN_TEST) {
      this.listContainer_.element.removeAttribute('scan-started');
      this.listContainer_.element.setAttribute(
          'scan-completed', this.directoryModel_.getCurrentDirName());
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
   * @private
   */
  onScanUpdated_() {
    if (!this.scanInProgress_) {
      console.error('Scan-updated event received. But scan is not started.');
      return;
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
   * @private
   */
  onScanCancelled_() {
    if (!this.scanInProgress_) {
      console.error('Scan-cancelled event received. But scan is not started.');
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
   * Handle the 'rescan-completed' from the DirectoryModel.
   * @private
   */
  onRescanCompleted_() {
    this.selectionHandler_.onFileSelectionChanged();
  }

  /**
   * When a spinner is shown, updates the UI to remove items in the previous
   * directory.
   * @private
   */
  onSpinnerShown_() {
    if (this.scanInProgress_) {
      this.listContainer_.endBatchUpdates();
      this.listContainer_.startBatchUpdates();
    }
  }

  /**
   * Hides the spinner if it's shown or scheduled to be shown.
   * @private
   */
  hideSpinner_() {
    if (this.spinnerHideCallback_) {
      this.spinnerHideCallback_();
      this.spinnerHideCallback_ = null;
    }
  }
}
