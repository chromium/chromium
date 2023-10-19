// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * State of progress items.
 * @const @enum {string}
 */
export const ProgressItemState = {
  SCANNING: 'scanning',
  PROGRESSING: 'progressing',
  COMPLETED: 'completed',
  ERROR: 'error',
  CANCELED: 'canceled',
  PAUSED: 'paused',
};
Object.freeze(ProgressItemState);

/**
 * Policy error type. Only applicable if DLP or Enterprise Connectors policies
 * apply.
 * @const @enum {string}
 */
export const PolicyErrorType = {
  DLP: 'dlp',
  ENTERPRISE_CONNECTORS: 'enterprise_connectors',
  DLP_WARNING_TIMEOUT: 'dlp_warning_timeout',
};
Object.freeze(PolicyErrorType);

/**
 * Type of progress items.
 * @const @enum {string}
 */
export const ProgressItemType = {
  // The item is file copy operation.
  COPY: 'copy',
  // The item is file delete operation.
  DELETE: 'delete',
  // The item is emptying the trash operation.
  EMPTY_TRASH: 'empty-trash',
  // The item is file extract operation.
  EXTRACT: 'extract',
  // The item is file move operation.
  MOVE: 'move',
  // The item is file zip operation.
  ZIP: 'zip',
  // The item is drive sync operation.
  SYNC: 'sync',
  // The item is restoring the trash.
  RESTORE: 'restore',
  RESTORE_TO_DESTINATION: 'restore_to_destination',
  // The item is general file transfer operation.
  // This is used for the mixed operation of summarized item.
  TRANSFER: 'transfer',
  // The item is being trashed.
  TRASH: 'trash',
  // The item is external drive format operation.
  FORMAT: 'format',
  // The item is archive operation.
  MOUNT_ARCHIVE: 'mount_archive',
  // The item is external drive partitioning operation.
  PARTITION: 'partition',
};
Object.freeze(ProgressItemType);

/**
 * Visual signals can have an additional button that, when clicked, performs
 * some arbitrary action. The `text` defines the button text to show and the
 * `callback` defines the arbitrary action.
 * @typedef {{
 *   text: string,
 *   callback: !function():void,
 * }}
 */
// @ts-ignore: error TS7005: Variable 'ProgressItemExtraButton' implicitly has
// an 'any' type.
export let ProgressItemExtraButton;

/**
 * Item of the progress center.
 */
export class ProgressCenterItem {
  constructor() {
    /**
     * Item ID.
     * @private @type {string}
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
     * @type {?function():void}
     */
    this.cancelCallback = null;

    /**
     * Optional callback to be invoked after dismissing the item.
     */
    // @ts-ignore: error TS7008: Member 'dismissCallback' implicitly has an
    // 'any' type.
    this.dismissCallback = null;

    /**
     * The predicted remaining time to complete the progress item in seconds.
     * @type {number}
     */
    this.remainingTime;

    /**
     * Contains the text and callback on an extra button when the progress
     * center item is either in COMPLETED, ERROR, or PAUSED state.
     * @type {!Map<!ProgressItemState, !ProgressItemExtraButton>}
     */
    this.extraButton = new Map();

    /**
     * In the case of a copy/move operation, whether the destination folder is
     * a child of My Drive.
     * @type {boolean}
     */
    this.isDestinationDrive = false;

    /**
     * The type of policy error that occurred, if any.
     * @type {?PolicyErrorType}
     */
    this.policyError = null;

    /**
     * The number of files with a policy restriction, if any.
     * @type {?number}
     */
    this.policyFileCount = null;

    /**
     * The name of the first file with a policy restriction, if any.
     * @type {?string}
     */
    this.policyFileName = null;
  }

  /**
   * Sets the extra button text and callback. Use this to add an additional
   * button with configurable functionality.
   * @param {string} text Text to use for the button.
   * @param {!ProgressItemState} state Which state to show the button for.
   *     Currently only `ProgressItemState.COMPLETED`,
   * `ProgressItemState.ERROR`, and `ProgressItemState.PAUSED` are supported.
   * @param {!function():void} callback The callback to invoke when the button
   *     is pressed.
   */
  setExtraButton(state, text, callback) {
    if (!text || !callback) {
      console.warn('Text and callback must be supplied');
      return;
    }
    if (this.extraButton.has(state)) {
      console.warn('Extra button already defined for state:', state);
      return;
    }
    const extraButton = {text, callback};
    this.extraButton.set(state, extraButton);
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
    return !!(this.state == ProgressItemState.PROGRESSING &&
              this.cancelCallback && this.single) ||
        !!(this.state == ProgressItemState.PAUSED && this.cancelCallback);
  }

  /**
   * Clones the item.
   * @return {!ProgressCenterItem} New item having the same properties as this.
   */
  clone() {
    const clonedItem = Object.assign(new ProgressCenterItem(), this);
    return /** @type {!ProgressCenterItem} */ (clonedItem);
  }
}
