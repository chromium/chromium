// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/ash/common/assert.js';

import {ProgressCenterItem, ProgressItemState, ProgressItemType} from '../../../common/js/progress_center_common.js';
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
     * Timeout for hiding file operations in progress.
     * @type {number}
     */
    this.PENDING_TIME_MS_ = 2000;
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
        break;
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
                strf('COPY_FILE_NAME_LONG', source, destination) :
                strf('FILE_COPIED', source);
          }
          if (item.type === ProgressItemType.EXTRACT) {
            return hasDestination ?
                strf('EXTRACT_FILE_NAME_LONG', source, destination) :
                strf('FILE_EXTRACTED', source);
          }
          if (item.type === ProgressItemType.MOVE) {
            return hasDestination ?
                strf('MOVE_FILE_NAME_LONG', source, destination) :
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
        break;
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
   * Generates remaining time message with formatted time.
   *
   * The time format in hour and minute and the durations more
   * than 24 hours also formatted in hour.
   *
   * As ICU syntax is not implemented in web ui yet (crbug/481718), the i18n
   * of time part is handled using Intl methods.
   *
   * @param {!ProgressCenterItem} item Item we're generating a message for.
   * @return {!string} Remaining time message.
   */
  generateRemainingTimeMessage(item) {
    const seconds = item.remainingTime;

    // Return empty string for unsupported operation (which didn't set
    // remaining time).
    if (seconds == null) {
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

    const locale = util.getCurrentLocaleOrDefault();
    let minutes = Math.ceil(seconds / 60);
    if (minutes <= 1) {
      // Less than one minute. Display remaining time in seconds.
      const formatter = new Intl.NumberFormat(
          locale, {style: 'unit', unit: 'second', unitDisplay: 'long'});
      return strf(
          'TIME_REMAINING_ESTIMATE', formatter.format(Math.ceil(seconds)));
    }

    const minuteFormatter = new Intl.NumberFormat(
        locale, {style: 'unit', unit: 'minute', unitDisplay: 'long'});

    const hours = Math.floor(minutes / 60);
    if (hours == 0) {
      // Less than one hour. Display remaining time in minutes.
      return strf('TIME_REMAINING_ESTIMATE', minuteFormatter.format(minutes));
    }

    minutes -= hours * 60;

    const hourFormatter = new Intl.NumberFormat(
        locale, {style: 'unit', unit: 'hour', unitDisplay: 'long'});

    if (minutes == 0) {
      // Hours but no minutes.
      return strf('TIME_REMAINING_ESTIMATE', hourFormatter.format(hours));
    }

    // Hours and minutes.
    return strf(
        'TIME_REMAINING_ESTIMATE_2', hourFormatter.format(hours),
        minuteFormatter.format(minutes));
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
      panelItem.secondaryText = this.generateRemainingTimeMessage(item);
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
            donePanelItem.secondaryText = str('COMPLETE_LABEL');
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
            }, 4000);
          }
          // Drop through to remove the progress panel.
        case ProgressItemState.CANCELED:
          // Remove the feedback panel when complete.
          this.feedbackHost_.removePanelItem(panelItem);
          break;
        case ProgressItemState.ERROR:
          if (item.extraButton.has(ProgressItemState.ERROR)) {
            extraButton = item.extraButton.get(ProgressItemState.ERROR);
            panelItem.dataset.extraButtonText = extraButton.text;
          }
          panelItem.panelType = panelItem.panelTypeError;
          panelItem.primaryText = item.message;
          panelItem.secondaryText = '';
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
            previousItem.state !== ProgressItemState.PROGRESSING) {
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
            previousItem.state !== ProgressItemState.PROGRESSING) {
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
