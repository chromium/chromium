// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Progress center panel.
 * @implements {ProgressCenterPanelInterface}
 */
class ProgressCenterPanel {
  constructor() {
    /**
     * Reference to the feedback panel host.
     * TODO(crbug.com/947388) Add closure annotation here.
     */
    this.feedbackHost_ = document.querySelector('#progress-panel');

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
   * @param {Object} info Cached information to use for formatting.
   * @return {string} String formatted based on the item state.
   */
  generateSourceString_(item, info) {
    switch (item.state) {
      case 'progressing':
        if (item.itemCount === 1) {
          if (item.type === ProgressItemType.COPY) {
            return strf('COPY_FILE_NAME', info['source']);
          } else if (item.type === ProgressItemType.MOVE) {
            return strf('MOVE_FILE_NAME', info['source']);
          } else {
            return item.message;
          }
        } else {
          if (item.type === ProgressItemType.COPY) {
            return strf('COPY_ITEMS_REMAINING', info['source']);
          } else if (item.type === ProgressItemType.MOVE) {
            return strf('MOVE_ITEMS_REMAINING', info['source']);
          } else {
            return item.message;
          }
        }
        break;
      case 'completed':
        if (info['count'] > 1) {
          return strf('FILE_ITEMS', info['source']);
        }
        return info['source'] || item.message;
      case 'error':
        return item.message;
      case 'canceled':
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
    const hasDestination = this.isNonEmptyString_(info['destination']);
    switch (item.state) {
      case 'progressing':
        // Source and primary string are the same for missing destination.
        if (!hasDestination) {
          return this.generateSourceString_(item, info);
        }
        // fall through
      case 'completed':
        if (item.itemCount === 1) {
          if (item.type === ProgressItemType.COPY) {
            if (hasDestination) {
              return strf(
                  'COPY_FILE_NAME_LONG', info['source'], info['destination']);
            } else {
              return strf('FILE_COPIED', info['source']);
            }
          } else if (item.type === ProgressItemType.MOVE) {
            if (hasDestination) {
              return strf(
                  'MOVE_FILE_NAME_LONG', info['source'], info['destination']);
            } else {
              return strf('FILE_MOVED', info['source']);
            }
          } else {
            return item.message;
          }
        } else {
          if (item.type === ProgressItemType.COPY) {
            if (hasDestination) {
              return strf(
                  'COPY_ITEMS_REMAINING_LONG', info['source'],
                  info['destination']);
            } else {
              return strf('FILE_ITEMS_COPIED', info['source']);
            }
          } else if (item.type === ProgressItemType.MOVE) {
            if (hasDestination) {
              return strf(
                  'MOVE_ITEMS_REMAINING_LONG', info['source'],
                  info['destination']);
            } else {
              return strf('FILE_ITEMS_MOVED', info['source']);
            }
          } else {
            return item.message;
          }
        }
        break;
      case 'error':
        return item.message;
      case 'canceled':
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
    if (seconds == 0 && item.state == 'progressing') {
      return str('PENDING_LABEL');
    }

    // Return empty string for not supported operation (didn't set
    // remainingTime) or 0 sec remainingTime in non progressing state.
    if (!seconds) {
      return '';
    }

    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);

    const hourFormatter = new Intl.NumberFormat(
        navigator.language, {style: 'unit', unit: 'hour', unitDisplay: 'long'});
    const minuteFormatter = new Intl.NumberFormat(
        navigator.language,
        {style: 'unit', unit: 'minute', unitDisplay: 'short'});

    if (hours > 0 && minutes > 0) {
      return strf(
          'TIME_REMAINING_ESTIMATE_2', hourFormatter.format(hours),
          minuteFormatter.format(minutes));
    } else if (hours > 0) {
      return strf('TIME_REMAINING_ESTIMATE', hourFormatter.format(hours));
    } else if (minutes > 0) {
      return strf('TIME_REMAINING_ESTIMATE', minuteFormatter.format(minutes));
    } else {
      // Round up to 1 min for short period of remaining time.
      return strf('TIME_REMAINING_ESTIMATE', minuteFormatter.format(1));
    }
  }

  /**
   * Process item updates for feedback panels.
   * @param {!ProgressCenterItem} item Item being updated.
   * @param {?ProgressCenterItem} newItem Item updating with new content.
   * @suppress {checkTypes}
   * TODO(crbug.com/947388) Remove the suppress, and fix closure compile.
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
        if (item.type === 'format') {
          panelItem.panelType = panelItem.panelTypeFormatProgress;
        } else if (item.type === 'sync') {
          panelItem.panelType = panelItem.panelTypeSyncProgress;
        } else {
          panelItem.panelType = panelItem.panelTypeProgress;
        }
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

      // On progress panels, make the cancel button aria-label more useful.
      const cancelLabel = strf('CANCEL_ACTIVITY_LABEL', primaryText);
      panelItem.closeButtonAriaLabel = cancelLabel;
      panelItem.signalCallback = (signal) => {
        if (signal === 'cancel' && item.cancelCallback) {
          item.cancelCallback();
        }
        if (signal === 'dismiss') {
          this.feedbackHost_.removePanelItem(panelItem);
          this.dismissErrorItemCallback(item.id);
        }
      };
      panelItem.progress = item.progressRateInPercent.toString();
      switch (item.state) {
        case 'completed':
          // Create a completed panel for copies, moves and formats.
          // TODO(crbug.com/947388) decide if we want these for delete, etc.
          if (item.type === 'copy' || item.type === 'move' ||
              item.type === 'format') {
            const donePanelItem = this.feedbackHost_.addPanelItem(item.id);
            donePanelItem.id = item.id;
            donePanelItem.panelType = donePanelItem.panelTypeDone;
            donePanelItem.primaryText = primaryText;
            donePanelItem.secondaryText = str('COMPLETE_LABEL');
            donePanelItem.signalCallback = (signal) => {
              if (signal === 'dismiss') {
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
        case 'canceled':
          // Remove the feedback panel when complete.
          this.feedbackHost_.removePanelItem(panelItem);
          break;
        case 'error':
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
