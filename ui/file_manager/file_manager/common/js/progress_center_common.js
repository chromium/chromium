// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Event of the ProgressCenter class.
 * @const @enum {string}
 */
const ProgressCenterEvent = {
  /**
   * Background page notifies item update to application windows.
   */
  ITEM_UPDATED: 'itemUpdated',

  /**
   * Background page notifies all the items are cleared.
   */
  RESET: 'reset'
};
Object.freeze(ProgressCenterEvent);

/**
 * State of progress items.
 * @const @enum {string}
 */
const ProgressItemState = {
  PROGRESSING: 'progressing',
  COMPLETED: 'completed',
  ERROR: 'error',
  CANCELED: 'canceled'
};
Object.freeze(ProgressItemState);

/**
 * Type of progress items.
 * @const @enum {string}
 */
const ProgressItemType = {
  // The item is file copy operation.
  COPY: 'copy',
  // The item is file move operation.
  MOVE: 'move',
  // The item is file delete operation.
  DELETE: 'delete',
  // The item is file zip operation.
  ZIP: 'zip',
  // The item is drive sync operation.
  SYNC: 'sync',
  // The item is general file transfer operation.
  // This is used for the mixed operation of summarized item.
  TRANSFER: 'transfer'
};
Object.freeze(ProgressItemType);

/**
 * Item of the progress center.
 */
class ProgressCenterItem {
  constructor() {
    /**
     * Item ID.
     * @private {string}
     */
    this.id_ = '';

    /**
     * State of the progress item.
     * @type {ProgressItemState}
     */
    this.state = ProgressItemState.PROGRESSING;

    /**
     * Message of the progress item.
     * @type {string}
     */
    this.message = '';

    /**
     * Source message for the progress item.
     * @type {string}
     */
    this.sourceMessage = '';

    /**
     * Destination message for the progress item.
     * @type {string}
     */
    this.destinationMessage = '';

    /**
     * Number of items being processed.
     * @type {number}
     */
    this.itemCount = 0;

    /**
     * Max value of the progress.
     * @type {number}
     */
    this.progressMax = 0;

    /**
     * Current value of the progress.
     * @type {number}
     */
    this.progressValue = 0;

    /**
     * Type of progress item.
     * @type {?ProgressItemType}
     */
    this.type = null;

    /**
     * Whether the item represents a single item or not.
     * @type {boolean}
     */
    this.single = true;

    /**
     * If the property is true, only the message of item shown in the progress
     * center and the notification of the item is created as priority = -1.
     * @type {boolean}
     */
    this.quiet = false;

    /**
     * Callback function to cancel the item.
     * @type {?function()}
     */
    this.cancelCallback = null;
  }

  /**
   * Setter of Item ID.
   * @param {string} value New value of ID.
   */
  set id(value) {
    if (!this.id_) {
      this.id_ = value;
    } else {
      console.error('The ID is already set. (current ID: ' + this.id_ + ')');
    }
  }

  /**
   * Getter of Item ID.
   * @return {string} Item ID.
   */
  get id() {
    return this.id_;
  }

  /**
   * Gets progress rate in percent.
   *
   * If the current state is canceled or completed, it always returns 0 or 100
   * respectively.
   *
   * @return {number} Progress rate in percent.
   */
  get progressRateInPercent() {
    switch (this.state) {
      case ProgressItemState.CANCELED:
        return 0;
      case ProgressItemState.COMPLETED:
        return 100;
      default:
        return ~~(100 * this.progressValue / this.progressMax);
    }
  }

  /**
   * Whether the item can be canceled or not.
   * @return {boolean} True if the item can be canceled.
   */
  get cancelable() {
    return !!(
        this.state == ProgressItemState.PROGRESSING && this.cancelCallback &&
        this.single);
  }

  /**
   * Clones the item.
   * TODO(adanilo) This is used by ProgressCenterItemGroup only, remove when
   * VS feedback panels are turned on permanently.
   * @return {!ProgressCenterItem} New item having the same properties as this.
   */
  clone() {
    const clonedItem = Object.assign(new ProgressCenterItem(), this);
    return /** @type {!ProgressCenterItem} */ (clonedItem);
  }
}
