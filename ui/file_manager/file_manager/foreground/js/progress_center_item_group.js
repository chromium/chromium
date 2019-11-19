// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Group of progress item in the progress center panels.
 *
 * This is responsible for generating the summarized item and managing lifetime
 * of error items.
 */
class ProgressCenterItemGroup {
  /**
   * @param {string} name Name of the group.
   * @param {boolean} quiet Whether the group is for quiet items or not.
   */
  constructor(name, quiet) {
    /**
     * Name of the group.
     * @private @const {string}
     */
    this.name_ = name;

    /**
     * Whether the group is for quiet items or not.
     * @private @const {boolean}
     */
    this.quiet_ = quiet;

    /**
     * State of the group.
     * @private {!ProgressCenterItemGroup.State}
     */
    this.state_ = ProgressCenterItemGroup.State.EMPTY;

    /**
     * Items that are progressing, or completed but still animated.
     * Key is item ID.
     * @private {!Object<ProgressCenterItem>}
     */
    this.items_ = {};

    /**
     * Set of animated state of items. Key is item ID and value is whether the
     * item is animated or not.
     * @private {!Object<boolean>}
     */
    this.animated_ = {};

    /**
     * Last summarized item.
     * @private {?ProgressCenterItem}
     */
    this.summarizedItem_ = null;

    /**
     * Whether the summarized item is animated or not.
     * @private {boolean}
     */
    this.summarizedItemAnimated_ = false;

    /**
     * Total maximum progress value of items already completed and removed from
     * this.items_.
     * @private {number}
     */
    this.totalProgressMax_ = 0;

    /**
     * Total progress value of items already completed and removed from
     * this.items_.
     * @private {number}
     */
    this.totalProgressValue_ = 0;
  }

  /**
   * Makes the summarized item for the groups.
   *
   * When a group has only error items, getSummarizedItem of the item returns
   * null. Basically the first result of the groups that the progress center
   * panel contains is used as a summarized item. But If all the group returns
   * null, the progress center panel generates the summarized item by using the
   * method.
   *
   * @param {...ProgressCenterItemGroup} var_args List of groups.
   * @return {ProgressCenterItem} Summarized item.
   */
  static getSummarizedErrorItem(var_args) {
    const groups = Array.prototype.slice.call(arguments);
    const errorItems = [];
    for (let i = 0; i < groups.length; i++) {
      for (const id in groups[i].items_) {
        const item = groups[i].items_[id];
        if (item.state === ProgressItemState.ERROR) {
          errorItems.push(item);
        }
      }
    }
    if (errorItems.length === 0) {
      return null;
    }

    if (errorItems.length === 1) {
      return errorItems[0].clone();
    }

    const item = new ProgressCenterItem();
    item.state = ProgressItemState.ERROR;
    item.message = strf('ERROR_PROGRESS_SUMMARY_PLURAL', errorItems.length);
    item.single = false;
    return item;
  }

  /**
   * @return {ProgressCenterItemGroup.State} State of the group.
   */
  get state() {
    return this.state_;
  }

  /**
   * @return {number} Number of error items that the group contains.
   */
  get numErrors() {
    let result = 0;
    for (const id in this.items_) {
      if (this.items_[id].state === ProgressItemState.ERROR) {
        result++;
      }
    }
    return result;
  }

  /**
   * Obtains the progressing (or completed but animated) item.
   *
   * @param {string} id Item ID.
   * @return {ProgressCenterItem} Item having the ID.
   */
  getItem(id) {
    return this.items_[id] || null;
  }

  /**
   * Obtains whether the item should be animated or not.
   * @param {string} id Item ID.
   * @return {boolean} Whether the item should be animated or not.
   */
  isAnimated(id) {
    return !!this.animated_[id];
  }

  /**
   * Obtains whether the summarized item should be animated or not.
   * @return {boolean} Whether the summarized item should be animated or not.
   */
  isSummarizedAnimated() {
    return this.summarizedItemAnimated_;
  }

  /**
   * Dismisses an error item.
   * @param {string} id Item id.
   */
  dismissErrorItem(id) {
    const errorItem = this.items_[id];

    if (!errorItem || errorItem.state !== ProgressItemState.ERROR) {
      return;
    }

    delete this.items_[id];

    this.tryToGoToNextState_();
  }

  /**
   * Starts item update.
   * Marks the given item as updating.
   * @param {ProgressCenterItem} item Item containing updated information.
   */
  update(item) {
    // Compares the current state and the new state to check if the update is
    // valid or not.
    const previousItem = this.items_[item.id];
    switch (item.state) {
      case ProgressItemState.ERROR:
        if (previousItem &&
            previousItem.state !== ProgressItemState.PROGRESSING) {
          return;
        }
        if (this.state_ === ProgressCenterItemGroup.State.EMPTY) {
          this.state_ = ProgressCenterItemGroup.State.INACTIVE;
        }
        this.items_[item.id] = item.clone();
        this.animated_[item.id] = false;
        this.summarizedItem_ = null;
        break;

      case ProgressItemState.PROGRESSING:
      case ProgressItemState.COMPLETED:
        if ((!previousItem && item.state === ProgressItemState.COMPLETED) ||
            (previousItem &&
             previousItem.state !== ProgressItemState.PROGRESSING)) {
          return;
        }
        if (this.state_ === ProgressCenterItemGroup.State.EMPTY ||
            this.state_ === ProgressCenterItemGroup.State.INACTIVE) {
          this.state_ = ProgressCenterItemGroup.State.ACTIVE;
        }
        this.items_[item.id] = item.clone();
        this.animated_[item.id] = ProgressCenterItemGroup.shouldAnimate_(
            !!this.animated_[item.id], previousItem, item,
            /* summarized */ false);
        if (!this.animated_[item.id]) {
          this.completeItemAnimation(item.id);
        }
        break;

      case ProgressItemState.CANCELED:
        if (!previousItem ||
            previousItem.state !== ProgressItemState.PROGRESSING) {
          return;
        }
        delete this.items_[item.id];
        this.animated_[item.id] = false;
        this.summarizedItem_ = null;
    }

    // Update the internal summarized item cache.
    const previousSummarizedItem = this.summarizedItem_;
    this.summarizedItem_ = this.getSummarizedItem(0);
    this.summarizedItemAnimated_ = ProgressCenterItemGroup.shouldAnimate_(
        !!this.summarizedItemAnimated_, previousSummarizedItem,
        this.summarizedItem_,
        /* summarized */ true);
    if (!this.summarizedItemAnimated_) {
      this.completeSummarizedItemAnimation();
    }
  }

  /**
   * Notifies the end of the item's animation to the group.
   * If all the items except error items completes, the group enter the inactive
   * state.
   * @param {string} id Item ID.
   */
  completeItemAnimation(id) {
    this.animated_[id] = false;
    if (this.items_[id].state === ProgressItemState.COMPLETED) {
      this.totalProgressValue_ += (this.items_[id].progressValue || 0.0);
      this.totalProgressMax_ += (this.items_[id].progressMax || 0.0);
      delete this.items_[id];
      this.tryToGoToNextState_();
    }
  }

  /**
   * Notifies the end of the summarized item's animation.
   * This may update summarized view. (1 progressing + 1 error -> 1 error)
   */
  completeSummarizedItemAnimation() {
    this.summarizedItemAnimated_ = false;
    this.tryToGoToNextState_();
  }

  /**
   * Obtains the summary of the set.
   * @param {number} numOtherErrors Number of errors contained by other groups.
   * @return {ProgressCenterItem} Item.
   */
  getSummarizedItem(numOtherErrors) {
    if (this.state_ === ProgressCenterItemGroup.State.EMPTY ||
        this.state_ === ProgressCenterItemGroup.State.INACTIVE) {
      return null;
    }

    const summarizedItem = new ProgressCenterItem();
    summarizedItem.quiet = this.quiet_;
    summarizedItem.progressMax += this.totalProgressMax_;
    summarizedItem.progressValue += this.totalProgressValue_;
    const progressingItems = [];
    const errorItems = [];
    let numItems = 0;

    for (const id in this.items_) {
      const item = this.items_[id];
      numItems++;

      // Count states.
      switch (item.state) {
        case ProgressItemState.PROGRESSING:
        case ProgressItemState.COMPLETED:
          progressingItems.push(item);
          break;
        case ProgressItemState.ERROR:
          errorItems.push(item);
          continue;
      }

      // If all of the progressing items have the same type, then use
      // it. Otherwise use TRANSFER, since it is the most generic.
      if (summarizedItem.type === null) {
        summarizedItem.type = item.type;
      } else if (summarizedItem.type !== item.type) {
        summarizedItem.type = ProgressItemType.TRANSFER;
      }

      // Sum up the progress values.
      summarizedItem.progressMax += item.progressMax;
      summarizedItem.progressValue += item.progressValue;
    }

    // Returns 1 item.
    if (progressingItems.length === 1 &&
        errorItems.length + numOtherErrors === 0) {
      summarizedItem.id = progressingItems[0].id;
      summarizedItem.cancelCallback = progressingItems[0].cancelCallback;
      summarizedItem.message = progressingItems[0].message;
      summarizedItem.state = progressingItems[0].state;
      return summarizedItem;
    }

    // Returns integrated items.
    if (progressingItems.length > 0) {
      const numErrors = errorItems.length + numOtherErrors;
      const messages = [];
      switch (summarizedItem.type) {
        case ProgressItemType.COPY:
          messages.push(str('COPY_PROGRESS_SUMMARY'));
          break;
        case ProgressItemType.MOVE:
          messages.push(str('MOVE_PROGRESS_SUMMARY'));
          break;
        case ProgressItemType.DELETE:
          messages.push(str('DELETE_PROGRESS_SUMMARY'));
          break;
        case ProgressItemType.ZIP:
          messages.push(str('ZIP_PROGRESS_SUMMARY'));
          break;
        case ProgressItemType.SYNC:
          messages.push(str('SYNC_PROGRESS_SUMMARY'));
          break;
        case ProgressItemType.TRANSFER:
          messages.push(str('TRANSFER_PROGRESS_SUMMARY'));
          break;
      }
      if (numErrors === 1) {
        messages.push(str('ERROR_PROGRESS_SUMMARY'));
      } else if (numErrors > 1) {
        messages.push(strf('ERROR_PROGRESS_SUMMARY_PLURAL', numErrors));
      }
      summarizedItem.single = false;
      summarizedItem.message = messages.join(' ');
      summarizedItem.state = ProgressItemState.PROGRESSING;
      return summarizedItem;
    }

    // Returns complete items.
    summarizedItem.state = ProgressItemState.COMPLETED;
    return summarizedItem;
  }

  /**
   * Tries to go to next state.
   * @private
   */
  tryToGoToNextState_() {
    if (this.summarizedItemAnimated_) {
      return;
    }

    // If there is no item except for error items, go to INACTIVE state.
    let hasError = false;
    for (const id in this.items_) {
      // If there is non-error item (progressing, or completed but still
      // animated), we should stay the active state.
      if (this.items_[id].state !== ProgressItemState.ERROR) {
        return;
      }
      hasError = true;
    }

    this.totalProgressValue_ = 0.0;
    this.totalProgressMax_ = 0.0;
    this.state_ = ProgressCenterItemGroup.State.INACTIVE;

    // If there is no item, go to EMPTY state.
    if (hasError) {
      return;
    }

    this.items_ = {};
    this.animated_ = {};
    this.summarizedItem_ = null;
    this.summarizedItemAnimated_ = false;
    this.state_ = ProgressCenterItemGroup.State.EMPTY;
  }

  /**
   * Obtains whether the item should be animated or not.
   * @param {boolean} previousAnimated Whether the item is previously animated
   *     or not.
   * @param {ProgressCenterItem} previousItem Item before updating.
   * @param {ProgressCenterItem} item New item.
   * @param {boolean} summarized If the item is summarized one or not.
   * @return {boolean} Whether the item should be animated or not.
   * @private
   */
  static shouldAnimate_(previousAnimated, previousItem, item, summarized) {
    // Check visibility of previous and current progress bar.
    const previousShow = previousItem && (!summarized || !previousItem.quiet);
    const currentShow = item && (!summarized || !item.quiet);
    // If previous or current item does not show progress bar, we should not
    // animate.
    if (!previousShow || !currentShow) {
      return false;
    }
    if (previousItem.progressRateInPercent < item.progressRateInPercent) {
      return true;
    }
    if (previousAnimated &&
        previousItem.progressRateInPercent === item.progressRateInPercent) {
      return true;
    }
    return false;
  }
}

/**
 * State of ProgressCenterItemGroup.
 * @enum {string}
 * @const
 */
ProgressCenterItemGroup.State = {
  // Group has no items.
  EMPTY: 'empty',
  // Group has at least 1 progressing item.
  ACTIVE: 'active',
  // Group has no progressing items but still shows error items.
  INACTIVE: 'inactive'
};
