// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/ash/common/assert.js';

import {PolicyErrorType, ProgressCenterItem, ProgressItemState, ProgressItemType} from '../../../common/js/progress_center_common.js';
import {str, strf, util} from '../../../common/js/util.js';
import {ProgressCenterPanelInterface} from '../../../externs/progress_center_panel.js';
import {DisplayPanel} from '../../elements/xf_display_panel.js';

/**
 * Progress center panel.
 * @implements {ProgressCenterPanelInterface}
 */
export class ProgressCenterPanel {
  constructor() {
    /**
     * Reference to the feedback panel host.
     * @private {!DisplayPanel}
     */
    this.feedbackHost_ = /** @type {!DisplayPanel} */ (
        document.querySelector('#progress-panel'));

    /**
     * Items that are progressing, or completed.
     * Key is item ID.
     * @private {!Object<ProgressCenterItem>}
     */
    this.items_ = {};

    /**
     * Callback to be called with the ID of the progress item when the cancel
     * button is clicked.
     * @type {?function(string)}
     */
    this.cancelCallback = null;

    /**
     * Callback to be called with the ID of the error item when user pressed
     * dismiss button of it.
     * @type {?function(string)}
     */
    this.dismissErrorItemCallback = null;

    /**
     * Defer showing in progress operation to avoid displaying quick
     * operations, e.g. the notification panel only shows if the task is
     * processing longer than this time.
     * @private {number}
     */
    this.PENDING_TIME_MS_ = 2000;

    /**
     * Timeout for removing the notification panel, e.g. the notification
     * panel will be removed after this time.
     * @private {number}
     */
    this.TIMEOUT_TO_REMOVE_MS_ = 4000;

    if (window.IN_TEST) {
      this.PENDING_TIME_MS_ = 0;
    }
  }

  /**
   * Generate source string for display on the feedback panel.
   * @param {!ProgressCenterItem} item Item we're generating a message for.
   * @param {?Object} info Cached information to use for formatting.
   * @return {string} String formatted based on the item state.
   */
  generateSourceString_(item, info) {
    info = info || {};
    const {source, destination, count} = info;
    switch (item.state) {
      case ProgressItemState.PROGRESSING:
        // Single items:
        if (item.itemCount === 1) {
          if (item.type === ProgressItemType.COPY) {
            return strf('COPY_FILE_NAME', source);
          }
          if (item.type === ProgressItemType.EXTRACT) {
            return strf('EXTRACT_FILE_NAME', source);
          }
          if (item.type === ProgressItemType.MOVE) {
            return strf('MOVE_FILE_NAME', source);
          }
          if (item.type === ProgressItemType.DELETE) {
            return strf('DELETE_FILE_NAME', source);
          }
          if (item.type === ProgressItemType.TRASH) {
            return strf('MOVE_TO_TRASH_FILE_NAME', source);
          }
          if (item.type === ProgressItemType.RESTORE_TO_DESTINATION ||
              item.type === ProgressItemType.RESTORE) {
            return strf('RESTORING_FROM_TRASH_FILE_NAME', source);
          }
          return item.message;
        }

        // Multiple items:
        if (item.type === ProgressItemType.COPY) {
          return strf('COPY_ITEMS_REMAINING', count);
        }
        if (item.type === ProgressItemType.EXTRACT) {
          return strf('EXTRACT_ITEMS_REMAINING', count);
        }
        if (item.type === ProgressItemType.MOVE) {
          return strf('MOVE_ITEMS_REMAINING', count);
        }
        if (item.type === ProgressItemType.DELETE) {
          return strf('DELETE_ITEMS_REMAINING', count);
        }
        if (item.type === ProgressItemType.TRASH) {
          return strf('MOVE_TO_TRASH_ITEMS_REMAINING', count);
        }
        if (item.type === ProgressItemType.RESTORE_TO_DESTINATION ||
            item.type === ProgressItemType.RESTORE) {
          return strf('RESTORING_FROM_TRASH_ITEMS_REMAINING', count);
        }
        return item.message;
      case ProgressItemState.COMPLETED:
        if (count > 1) {
          return strf('FILE_ITEMS', count);
        }
        return source || item.message;
      case ProgressItemState.ERROR:
        return item.message;
      case ProgressItemState.CANCELED:
        return '';
      default:
        assertNotReached();
        break;
    }
    return '';
  }

  /**
   * Test if we have an empty or all whitespace string.
   * @param {string} candidate String we're checking.
   * @return {boolean} true if there's content in the candidate.
   */
  isNonEmptyString_(candidate) {
    if (!candidate || candidate.trim().length === 0) {
      return false;
    }
    return true;
  }

  /**
   * Generate primary text string for display on the feedback panel.
   * It is used for TransferDetails mode.
   * @param {!ProgressCenterItem} item Item we're generating a message for.
   * @param {Object} info Cached information to use for formatting.
   * @return {string} String formatted based on the item state.
   */
  generatePrimaryString_(item, info) {
    info = info || {};
    const {source, destination, count} = info;
    const hasDestination = this.isNonEmptyString_(destination);
    switch (item.state) {
      case ProgressItemState.SCANNING:
      case ProgressItemState.PROGRESSING:
        // Source and primary string are the same for missing destination.
        if (!hasDestination) {
          return this.generateSourceString_(item, info);
        }
        // fall through
      case ProgressItemState.COMPLETED:
        // Single items:
        if (item.itemCount === 1) {
          if (item.type === ProgressItemType.COPY) {
            return hasDestination ?
                getStrForCopyWithDestination(item, source, destination) :
                strf('FILE_COPIED', source);
          }
          if (item.type === ProgressItemType.EXTRACT) {
            return hasDestination ?
                strf('EXTRACT_FILE_NAME_LONG', source, destination) :
                strf('FILE_EXTRACTED', source);
          }
          if (item.type === ProgressItemType.MOVE) {
            return hasDestination ?
                getStrForMoveWithDestination(item, source, destination) :
                strf('FILE_MOVED', source);
          }
          if (item.type === ProgressItemType.ZIP) {
            return strf('ZIP_FILE_NAME', source);
          }
          if (item.type === ProgressItemType.DELETE) {
            return strf('DELETE_FILE_NAME', source);
          }
          if (item.type === ProgressItemType.TRASH) {
            return item.state == ProgressItemState.PROGRESSING ?
                strf('MOVE_TO_TRASH_FILE_NAME', source) :
                strf('UNDO_DELETE_ONE', source);
          }
          if (item.type === ProgressItemType.RESTORE_TO_DESTINATION ||
              item.type === ProgressItemType.RESTORE) {
            return item.state == ProgressItemState.PROGRESSING ?
                strf('RESTORING_FROM_TRASH_FILE_NAME', source) :
                strf('RESTORE_TRASH_FILE_NAME', source);
          }
          return item.message;
        }

        // Multiple items:
        if (item.type === ProgressItemType.COPY) {
          return hasDestination ?
              item.isDestinationDrive ?
              strf('PREPARING_ITEMS_MY_DRIVE', count, destination) :
              strf('COPY_ITEMS_REMAINING_LONG', count, destination) :
              strf('FILE_ITEMS_COPIED', source);
        }
        if (item.type === ProgressItemType.EXTRACT) {
          return item.state == ProgressItemState.PROGRESSING ?
              strf('EXTRACT_ITEMS_REMAINING', count) :
              strf('FILE_ITEMS_EXTRACTED', count);
        }
        if (item.type === ProgressItemType.MOVE) {
          return hasDestination ?
              item.isDestinationDrive ?
              strf('PREPARING_ITEMS_MY_DRIVE', count, destination) :
              strf('MOVE_ITEMS_REMAINING_LONG', count, destination) :
              strf('FILE_ITEMS_MOVED', count);
        }
        if (item.type === ProgressItemType.ZIP) {
          return strf('ZIP_ITEMS_REMAINING', count);
        }
        if (item.type === ProgressItemType.DELETE) {
          return strf('DELETE_ITEMS_REMAINING', count);
        }
        if (item.type === ProgressItemType.TRASH) {
          return item.state == ProgressItemState.PROGRESSING ?
              strf('MOVE_TO_TRASH_ITEMS_REMAINING', count) :
              strf('UNDO_DELETE_SOME', count);
        }
        if (item.type === ProgressItemType.RESTORE_TO_DESTINATION ||
            item.type === ProgressItemType.RESTORE) {
          return item.state == ProgressItemState.PROGRESSING ?
              strf('RESTORING_FROM_TRASH_ITEMS_REMAINING', count) :
              strf('RESTORE_TRASH_MANY_ITEMS', count);
        }
        return item.message;
      case ProgressItemState.PAUSED:
        switch (item.type) {
          case ProgressItemType.COPY:
            return str('DLP_FILES_COPY_REVIEW_TITLE');
          case ProgressItemType.MOVE:
            return str('DLP_FILES_MOVE_REVIEW_TITLE');
          default:
            console.error('Unexpected operation type: ' + item.type);
            return '';
        }
      case ProgressItemState.ERROR:
        if (item.policyError) {
          return getStrForPolicyError(item);
        }
        // General error
        return item.message;
      case ProgressItemState.CANCELED:
        return '';
      default:
        assertNotReached();
        break;
    }
    return '';

    function getStrForMoveWithDestination(item, source, destination) {
      return item.isDestinationDrive ?
          strf('PREPARING_FILE_NAME_MY_DRIVE', source, destination) :
          strf('MOVE_FILE_NAME_LONG', source, destination);
    }

    function getStrForCopyWithDestination(item, source, destination) {
      return item.isDestinationDrive ?
          strf('PREPARING_FILE_NAME_MY_DRIVE', source, destination) :
          strf('COPY_FILE_NAME_LONG', source, destination);
    }

    function getStrForPolicyError(item) {
      if (!item.policyError) {
        console.warn('Policy error must be supplied');
        return '';
      }
      switch (item.policyError) {
        case PolicyErrorType.DLP:
        case PolicyErrorType.ENTERPRISE_CONNECTORS:
          if (!item.policyFileCount) {
            console.warn('Policy file count missing');
            return '';
          }
          switch (item.type) {
            case ProgressItemType.COPY:
              return item.policyFileCount === 1 ?
                  str('DLP_FILES_COPY_BLOCKED_TITLE_SINGLE') :
                  strf(
                      'DLP_FILES_COPY_BLOCKED_TITLE_MULTIPLE',
                      item.policyFileCount);
            case ProgressItemType.MOVE:
              return item.policyFileCount === 1 ?
                  str('DLP_FILES_MOVE_BLOCKED_TITLE_SINGLE') :
                  strf(
                      'DLP_FILES_MOVE_BLOCKED_TITLE_MULTIPLE',
                      item.policyFileCount);
            default:
              console.warn(`Unexpected task type: ${item.type}`);
              return '';
          }
        case PolicyErrorType.DLP_WARNING_TIMEOUT:
          switch (item.type) {
            case ProgressItemType.COPY:
              return str('DLP_FILES_COPY_TIMEOUT_TITLE');
            case ProgressItemType.MOVE:
              return str('DLP_FILES_MOVE_TIMEOUT_TITLE');
            default:
              console.warn(`Unexpected task type: ${item.type}`);
              return '';
          }
        default:
          console.warn(`Unexpected security error type: ${item.policyError}`);
          return '';
      }
    }
  }

  /**
   * Generates the secondary string to display on the feedback panel.
   *
   * The string can be empty in case of errors or tasks with no
   * remaining time set. In case of data protection policy related
   * notifications, the message provides more info about the state of the task.
   * Otherwise the message shows formatted remaining task time.
   *
   * The time format in hour and minute and the durations more
   * than 24 hours also formatted in hour.
   *
   * As ICU syntax is not implemented in web ui yet (crbug/481718), the i18n
   * of time part is handled using Intl methods.
   *
   * @param {!ProgressCenterItem} item Item we're generating a message for.
   * @return {!string} Secondary string message.
   * @private
   */
  generateSecondaryString_(item) {
    if (item.state === ProgressItemState.PAUSED) {
      if (!item.policyFileCount) {
        console.warn('Policy file count missing');
        return '';
      }
      if (item.policyFileCount === 1) {
        if (!item.policyFileName) {
          console.warn('Policy file name missing');
          return '';
        }
        return strf('DLP_FILES_WARN_MESSAGE_SINGLE', item.policyFileName);
      } else {
        return strf('DLP_FILES_WARN_MESSAGE_MULTIPLE', item.policyFileCount);
      }
    }

    if (item.state === ProgressItemState.ERROR) {
      if (!item.policyError) {
        // General error doesn't have secondary text.
        return '';
      }
      switch (item.policyError) {
        case PolicyErrorType.DLP:
          if (!item.policyFileCount) {
            console.warn('Policy file count missing');
            return '';
          }
          if (item.policyFileCount === 1) {
            if (!item.policyFileName) {
              console.warn('Policy file name missing');
              return '';
            }
            return strf(
                'DLP_FILES_BLOCKED_MESSAGE_POLICY_SINGLE', item.policyFileName);
          } else {
            return str('DLP_FILES_BLOCKED_MESSAGE_MULTIPLE');
          }
        case PolicyErrorType.ENTERPRISE_CONNECTORS:
          if (!item.policyFileCount) {
            console.warn('Policy file count missing');
            return '';
          }
          if (item.policyFileCount === 1) {
            if (!item.policyFileName) {
              console.warn('Policy file name missing');
              return '';
            }
            return strf(
                'DLP_FILES_BLOCKED_MESSAGE_CONTENT_SINGLE',
                item.policyFileName);
          } else {
            return str('DLP_FILES_BLOCKED_MESSAGE_MULTIPLE');
          }
        case PolicyErrorType.DLP_WARNING_TIMEOUT:
          switch (item.type) {
            case ProgressItemType.COPY:
              return str('DLP_FILES_COPY_TIMEOUT_MESSAGE');
            case ProgressItemType.MOVE:
              return str('DLP_FILES_MOVE_TIMEOUT_MESSAGE');
            default:
              console.warn(`Unexpected task type: ${item.type}`);
              return '';
          }
      }
    }

    const seconds = item.remainingTime;

    // Return empty string for unsupported operation (which didn't set
    // remaining time).
    if (seconds === undefined) {
      return '';
    }

    if (item.state === ProgressItemState.SCANNING) {
      return str('SCANNING_LABEL');
    }

    // Check if remaining time is valid (ie finite and positive).
    if (!(isFinite(seconds) && seconds > 0)) {
      // Return empty string for invalid remaining time in non progressing
      // state.
      return item.state === ProgressItemState.PROGRESSING ?
          str('PREPARING_LABEL') :
          '';
    }

    return util.secondsToRemainingTimeString(seconds);
  }

  /**
   * Process item updates for feedback panels.
   * @param {!ProgressCenterItem} item Item being updated.
   * @param {?ProgressCenterItem} newItem Item updating with new content.
   */
  updateFeedbackPanelItem(item, newItem) {
    let panelItem = this.feedbackHost_.findPanelItemById(item.id);
    if (newItem) {
      if (!panelItem) {
        panelItem = this.feedbackHost_.createPanelItem(item.id);
        // Show the panel only for long running operations.
        setTimeout(() => {
          this.feedbackHost_.attachPanelItem(panelItem);
        }, this.PENDING_TIME_MS_);
        if (item.type === ProgressItemType.FORMAT) {
          panelItem.panelType = panelItem.panelTypeFormatProgress;
        } else if (item.type === ProgressItemType.SYNC) {
          panelItem.panelType = panelItem.panelTypeSyncProgress;
        } else {
          panelItem.panelType = panelItem.panelTypeProgress;
        }
        // TODO(lucmult): Remove `userData`, it's only used in
        // generatePrimaryString_() which already refers to `item`.
        panelItem.userData = {
          'source': item.sourceMessage,
          'destination': item.destinationMessage,
          'count': item.itemCount,
        };
      }

      const primaryText = this.generatePrimaryString_(item, panelItem.userData);
      panelItem.secondaryText = this.generateSecondaryString_(item);
      panelItem.primaryText = primaryText;
      panelItem.setAttribute('data-progress-id', item.id);

      // Certain visual signals have the functionality to display an extra
      // button with an arbitrary callback.
      let extraButton = null;

      // On progress panels, make the cancel button aria-label more useful.
      const cancelLabel = strf('CANCEL_ACTIVITY_LABEL', primaryText);
      panelItem.closeButtonAriaLabel = cancelLabel;
      panelItem.signalCallback = (signal) => {
        if (signal === 'cancel' && item.cancelCallback) {
          item.cancelCallback();
        } else if (signal === 'dismiss') {
          this.feedbackHost_.removePanelItem(panelItem);
          this.dismissErrorItemCallback(item.id);
        } else if (
            signal === 'extra-button' && extraButton && extraButton.callback) {
          extraButton.callback();
          this.feedbackHost_.removePanelItem(panelItem);
          // The extra-button currently acts as a dismissal to invoke the
          // error item callback as well.
          this.dismissErrorItemCallback(item.id);
        }
      };
      panelItem.progress = item.progressRateInPercent.toString();
      switch (item.state) {
        case ProgressItemState.COMPLETED:
          // Create a completed panel for copies, moves, deletes and formats.
          if (item.type === ProgressItemType.COPY ||
              item.type === ProgressItemType.EXTRACT ||
              item.type === ProgressItemType.MOVE ||
              item.type === ProgressItemType.FORMAT ||
              item.type === ProgressItemType.ZIP ||
              item.type === ProgressItemType.DELETE ||
              item.type === ProgressItemType.TRASH ||
              item.type === ProgressItemType.RESTORE_TO_DESTINATION ||
              item.type === ProgressItemType.RESTORE) {
            const donePanelItem = this.feedbackHost_.addPanelItem(item.id);
            if (item.extraButton.has(ProgressItemState.COMPLETED)) {
              extraButton = item.extraButton.get(ProgressItemState.COMPLETED);
              donePanelItem.dataset.extraButtonText = extraButton.text;
            }
            donePanelItem.id = item.id;
            donePanelItem.panelType = donePanelItem.panelTypeDone;
            donePanelItem.primaryText = primaryText;
            donePanelItem.secondaryText = item.isDestinationDrive ?
                str('READY_TO_SYNC_MY_DRIVE') :
                str('COMPLETE_LABEL');
            donePanelItem.fadeSecondaryText = item.isDestinationDrive;
            donePanelItem.signalCallback = (signal) => {
              if (signal === 'dismiss') {
                this.feedbackHost_.removePanelItem(donePanelItem);
                delete this.items_[donePanelItem.id];
              } else if (
                  signal === 'extra-button' && extraButton &&
                  extraButton.callback) {
                extraButton.callback();
                this.feedbackHost_.removePanelItem(donePanelItem);
                delete this.items_[donePanelItem.id];
              }
            };
            // Delete after 4 seconds, doesn't matter if it's manually deleted
            // before the timer fires, as removePanelItem handles that case.
            setTimeout(() => {
              this.feedbackHost_.removePanelItem(donePanelItem);
              delete this.items_[donePanelItem.id];
            }, this.TIMEOUT_TO_REMOVE_MS_);
          }
          // Drop through to remove the progress panel.
        case ProgressItemState.CANCELED:
          // Remove the feedback panel when complete.
          this.feedbackHost_.removePanelItem(panelItem);
          break;
        case ProgressItemState.PAUSED:
          if (item.extraButton.has(ProgressItemState.PAUSED)) {
            extraButton = item.extraButton.get(ProgressItemState.PAUSED);
            panelItem.dataset.extraButtonText = extraButton.text;
          }
          panelItem.panelType = panelItem.panelTypeInfo;
          this.feedbackHost_.attachPanelItem(panelItem);
          break;
        case ProgressItemState.ERROR:
          if (item.extraButton.has(ProgressItemState.ERROR)) {
            extraButton = item.extraButton.get(ProgressItemState.ERROR);
            panelItem.dataset.extraButtonText = extraButton.text;
          }
          panelItem.panelType = panelItem.panelTypeError;
          // Make sure the panel is attached so it shows immediately.
          this.feedbackHost_.attachPanelItem(panelItem);
          break;
      }
    } else if (panelItem) {
      this.feedbackHost_.removePanelItem(panelItem);
    }
  }

  /**
   * Starts the item update and checks state changes.
   * @param {!ProgressCenterItem} item Item containing updated information.
   */
  updateItemState_(item) {
    // Compares the current state and the new state to check if the update is
    // valid or not.
    const previousItem = this.items_[item.id];
    switch (item.state) {
      case ProgressItemState.ERROR:
        if (previousItem &&
            (previousItem.state !== ProgressItemState.PROGRESSING ||
             previousItem.state !== ProgressItemState.PAUSED ||
             previousItem.state !== ProgressItemState.SCANNING)) {
          return;
        }
        this.items_[item.id] = item.clone();
        break;

      case ProgressItemState.PROGRESSING:
      case ProgressItemState.COMPLETED:
        if ((!previousItem && item.state === ProgressItemState.COMPLETED) ||
            (previousItem &&
             previousItem.state !== ProgressItemState.PROGRESSING)) {
          return;
        }
        this.items_[item.id] = item.clone();
        break;

      case ProgressItemState.CANCELED:
        if (!previousItem ||
            (previousItem.state !== ProgressItemState.PROGRESSING ||
             previousItem.state !== ProgressItemState.PAUSED ||
             previousItem.state !== ProgressItemState.SCANNING)) {
          return;
        }
        delete this.items_[item.id];
    }
  }

  /**
   * Updates an item to the progress center panel.
   * @param {!ProgressCenterItem} item Item including new contents.
   */
  updateItem(item) {
    this.updateItemState_(item);

    // Update an open view item.
    const newItem = this.items_[item.id] || null;
    this.updateFeedbackPanelItem(item, newItem);
  }

  /**
   * Called by background page when an error dialog is dismissed.
   * @param {string} id Item id.
   */
  dismissErrorItem(id) {
    delete this.items_[id];
  }
}
