// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** State of progress items. */
export enum ProgressItemState {
  SCANNING = 'scanning',
  PROGRESSING = 'progressing',
  COMPLETED = 'completed',
  ERROR = 'error',
  CANCELED = 'canceled',
  PAUSED = 'paused',
}

/**
 * Policy error type. Only applicable if DLP or Enterprise Connectors policies
 * apply.
 */
export enum PolicyErrorType {
  DLP = 'dlp',
  ENTERPRISE_CONNECTORS = 'enterprise_connectors',
  DLP_WARNING_TIMEOUT = 'dlp_warning_timeout',
}

/** Type of progress items. */
export enum ProgressItemType {
  // The item is file copy operation.
  COPY = 'copy',
  // The item is file delete operation.
  DELETE = 'delete',
  // The item is emptying the trash operation.
  EMPTY_TRASH = 'empty-trash',
  // The item is file extract operation.
  EXTRACT = 'extract',
  // The item is file move operation.
  MOVE = 'move',
  // The item is file zip operation.
  ZIP = 'zip',
  // The item is drive sync operation.
  SYNC = 'sync',
  // The item is restoring the trash.
  RESTORE = 'restore',
  RESTORE_TO_DESTINATION = 'restore_to_destination',
  // The item is general file transfer operation.
  // This is used for the mixed operation of summarized item.
  TRANSFER = 'transfer',
  // The item is being trashed.
  TRASH = 'trash',
  // The item is external drive format operation.
  FORMAT = 'format',
  // The item is archive operation.
  MOUNT_ARCHIVE = 'mount_archive',
  // The item is external drive partitioning operation.
  PARTITION = 'partition',
}

/**
 * Visual signals can have an additional button that, when clicked, performs
 * some arbitrary action. The `text` defines the button text to show and the
 * `callback` defines the arbitrary action.
 */
export interface ProgressItemExtraButton {
  text: string;
  callback: VoidCallback;
}

/** Item of the progress center. */
export class ProgressCenterItem {
  /** Item ID. */
  private id_ = '';

  /** State of the progress item. */
  state = ProgressItemState.PROGRESSING;

  /** Message of the progress item. */
  message = '';

  /** Source message for the progress item. */
  sourceMessage = '';

  /** Destination message for the progress item. */
  destinationMessage = '';

  /** Number of items being processed. */
  itemCount = 0;

  /** Max value of the progress. */
  progressMax = 0;

  /** Current value of the progress. */
  progressValue = 0;

  /*** Type of progress item. */
  type: ProgressItemType|null = null;

  /** Whether the item represents a single item or not. */
  single = true;

  /**
   * If the property is true, only the message of item shown in the progress
   * center and the notification of the item is created as priority = -1.
   */
  quiet = false;

  /** Callback function to cancel the item. */
  cancelCallback: VoidCallback|null = null;

  /** Optional callback to be invoked after dismissing the item. */
  dismissCallback: VoidCallback|null = null;

  /** The predicted remaining time to complete the progress item in seconds. */
  remainingTime = 0;

  /**
   * Contains the text and callback on an extra button when the progress
   * center item is either in COMPLETED, ERROR, or PAUSED state.
   */
  readonly extraButton = new Map<ProgressItemState, ProgressItemExtraButton>();

  /**
   * In the case of a copy/move operation, whether the destination folder is
   * a child of My Drive.
   */
  isDestinationDrive = false;

  /** The type of policy error that occurred, if any. */
  policyError: PolicyErrorType|null = null;

  /** The number of files with a policy restriction, if any. */
  policyFileCount: number|null = null;

  /** The name of the first file with a policy restriction, if any. */
  policyFileName: string|null = null;

  /**
   * List of files skipped during the operation because we couldn't decrypt
   * them.
   */
  skippedEncryptedFiles: string[] = [];

  /**
   * Sets the extra button text and callback. Use this to add an additional
   * button with configurable functionality.
   * @param text Text to use for the button.
   * @param state Which state to show the button for. Currently only
   *     `ProgressItemState.COMPLETED`, `ProgressItemState.ERROR`, and
   *     `ProgressItemState.PAUSED` are supported.
   * @param callback The callback to invoke when the button is pressed.
   */
  setExtraButton(
      state: ProgressItemState, text: string, callback: VoidCallback) {
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

  /** Sets the Item ID. */
  set id(value: string) {
    if (!this.id_) {
      this.id_ = value;
    } else {
      console.error('The ID is already set. (current ID: ' + this.id_ + ')');
    }
  }

  /** Gets the Item ID. */
  get id(): string {
    return this.id_;
  }

  /**
   * Gets progress rate in percent.
   *
   * If the current state is canceled or completed, it always returns 0 or 100
   * respectively.
   */
  get progressRateInPercent(): number {
    switch (this.state) {
      case ProgressItemState.CANCELED:
        return 0;
      case ProgressItemState.COMPLETED:
        return 100;
      default:
        return ~~(100 * this.progressValue / this.progressMax);
    }
  }

  /** Whether the item can be canceled or not. */
  get cancelable(): boolean {
    return !!(this.state === ProgressItemState.PROGRESSING &&
              this.cancelCallback && this.single) ||
        !!(this.state === ProgressItemState.PAUSED && this.cancelCallback);
  }

  /** Clones the item. */
  clone(): ProgressCenterItem {
    const clonedItem = Object.assign(new ProgressCenterItem(), this);
    return clonedItem;
  }
}
