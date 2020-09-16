// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Item element of the progress center.
 * @extends HTMLDivElement
 */
class ProgressCenterItemElement {
  /**
   * @param {Document} document Document which the new item belongs to.
   */
  constructor(document) {
    /** @private {?ProgressItemState} */
    this.state_ = null;

    /** @private {?function()} */
    this.cancelTransition_ = null;

    /** @private {?Element} */
    this.track_ = null;

    const label = document.createElement('label');
    label.className = 'label';

    const progressBarIndicator = document.createElement('div');
    progressBarIndicator.className = 'progress-track';

    const progressBar = document.createElement('div');
    progressBar.className = 'progress-bar';
    progressBar.appendChild(progressBarIndicator);

    const progressFrame = document.createElement('div');
    progressFrame.className = 'progress-frame';
    progressFrame.appendChild(label);
    progressFrame.appendChild(progressBar);

    const cancelButton = document.createElement('button');
    cancelButton.className = 'cancel';
    cancelButton.setAttribute('tabindex', '-1');

    // Dismiss button is shown for error item.
    const dismissButton = document.createElement('button');
    dismissButton.classList.add('dismiss');
    dismissButton.setAttribute('tabindex', '-1');

    const buttonFrame = document.createElement('div');
    buttonFrame.className = 'button-frame';
    buttonFrame.appendChild(cancelButton);
    buttonFrame.appendChild(dismissButton);

    const itemElement = document.createElement('li');
    itemElement.appendChild(progressFrame);
    itemElement.appendChild(buttonFrame);

    return ProgressCenterItemElement.decorate(itemElement);
  }

  /**
   * Ensures the animation triggers.
   *
   * @param {function(?)} callback Function to set the transition end
   *     properties.
   * @return {function()} Function to cancel the request.
   * @private
   */
  static safelySetAnimation_(callback) {
    let requestId = window.requestAnimationFrame(() => {
      // The transition start properties currently set are rendered at this
      // frame. And the transition end properties set by the callback is
      // rendered at the next frame.
      requestId = window.requestAnimationFrame(callback);
    });
    return () => {
      window.cancelAnimationFrame(requestId);
    };
  }

  /**
   * Decorates the given element as a progress item.
   * @param {!Element} element Item to be decorated.
   * @return {!ProgressCenterItemElement} Decorated item.
   */
  static decorate(element) {
    element.__proto__ = ProgressCenterItemElement.prototype;
    element = /** @type {!ProgressCenterItemElement} */ (element);
    element.state_ = ProgressItemState.PROGRESSING;
    element.track_ = element.querySelector('.progress-track');
    element.track_.addEventListener(
        'transitionend', element.onTransitionEnd_.bind(element));
    element.cancelTransition_ = null;
    return element;
  }

  get quiet() {
    return this.classList.contains('quiet');
  }

  /**
   * Updates the element view according to the item.
   * @param {ProgressCenterItem} item Item to be referred for the update.
   * @param {boolean} animated Whether the progress width is applied as animated
   *     or not.
   */
  update(item, animated) {
    // Set element attributes.
    this.state_ = item.state;
    this.setAttribute('data-progress-id', item.id);
    this.classList.toggle('error', item.state === ProgressItemState.ERROR);
    this.classList.toggle('cancelable', item.cancelable);
    this.classList.toggle('single', item.single);
    this.classList.toggle('quiet', item.quiet);

    // Set label.
    if (this.state_ === ProgressItemState.PROGRESSING ||
        this.state_ === ProgressItemState.ERROR) {
      this.querySelector('label').textContent = item.message;
    } else if (this.state_ === ProgressItemState.CANCELED) {
      this.querySelector('label').textContent = '';
    }

    // Cancel the previous property set.
    if (this.cancelTransition_) {
      this.cancelTransition_();
      this.cancelTransition_ = null;
    }

    // Set track width.
    const setWidth = ((nextWidthFrame) => {
                       const currentWidthRate =
                           parseInt(this.track_.style.width, 10);
                       // Prevent assigning the same width to avoid stopping the
                       // animation. animated == false may be intended to cancel
                       // the animation, so in that case, the assignment should
                       // be done.
                       if (currentWidthRate === nextWidthFrame && animated) {
                         return;
                       }
                       this.track_.hidden = false;
                       this.track_.style.width = nextWidthFrame + '%';
                       this.track_.classList.toggle('animated', animated);
                     }).bind(null, item.progressRateInPercent);

    if (animated) {
      this.cancelTransition_ =
          ProgressCenterItemElement.safelySetAnimation_(setWidth);
    } else {
      // For animated === false, we should call setWidth immediately to cancel
      // the animation, otherwise the animation may complete before canceling
      // it.
      setWidth();
    }
  }

  /**
   * Resets the item.
   */
  reset() {
    this.track_.hidden = true;
    this.track_.width = '';
    this.state_ = ProgressItemState.PROGRESSING;
  }

  /**
   * Handles transition end events.
   * @param {Event} event Transition end event.
   * @private
   */
  onTransitionEnd_(event) {
    if (event.propertyName !== 'width') {
      return;
    }
    this.track_.classList.remove('animated');
    this.dispatchEvent(new Event(
        ProgressCenterItemElement.PROGRESS_ANIMATION_END_EVENT,
        {bubbles: true}));
  }
}

ProgressCenterItemElement.prototype.__proto__ = HTMLDivElement.prototype;

/**
 * Event triggered when the item should be dismissed.
 * @const {string}
 */
ProgressCenterItemElement.PROGRESS_ANIMATION_END_EVENT = 'progressAnimationEnd';

/**
 * Progress center panel.
 * @implements {ProgressCenterPanelInterface}
 */
class ProgressCenterPanel {
  /**
   * @param {!Element} element DOM Element of the process center panel.
   */
  constructor(element) {
    /**
     * Root element of the progress center.
     * @type {!Element}
     * @private
     */
    this.element_ = element;

    /**
     * Open view containing multiple progress items.
     * @type {!HTMLDivElement}
     * @private
     */
    this.openView_ = assertInstanceof(
        queryRequiredElement('#progress-center-open-view', this.element_),
        HTMLDivElement);

    /**
     * Reference to the feedback panel host.
     * TODO(crbug.com/947388) Add closure annotation here.
     */
    this.feedbackHost_ = document.querySelector('#progress-panel');

    /**
     * Close view that is a summarized progress item.
     * @type {ProgressCenterItemElement}
     * @private
     */
    this.closeView_ = ProgressCenterItemElement.decorate(
        assert(this.element_.querySelector('#progress-center-close-view')));

    /**
     * Toggle animation rule of the progress center.
     * @type {CSSKeyframesRule}
     * @private
     */
    this.toggleAnimation_ =
        ProgressCenterPanel.getToggleAnimation_(element.ownerDocument);

    /**
     * Item group for normal priority items.
     * @type {ProgressCenterItemGroup}
     * @private
     */
    this.normalItemGroup_ = new ProgressCenterItemGroup('normal', false);

    /**
     * Item group for low priority items.
     * @type {ProgressCenterItemGroup}
     * @private
     */
    this.quietItemGroup_ = new ProgressCenterItemGroup('quiet', true);

    /**
     * Queries to obtains items for each group.
     * @type {Object<string>}
     * @private
     */
    this.itemQuery_ =
        Object.preventExtensions({normal: 'li:not(.quiet)', quiet: 'li.quiet'});

    /**
     * Timeout IDs of the inactive state of each group.
     * @type {Object<?number>}
     * @private
     */
    this.timeoutId_ = Object.preventExtensions({normal: null, quiet: null});

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

    // Register event handlers.
    element.addEventListener('click', this.onClick_.bind(this));
    element.addEventListener(
        'animationend', this.onToggleAnimationEnd_.bind(this));
    element.addEventListener(
        ProgressCenterItemElement.PROGRESS_ANIMATION_END_EVENT,
        this.onItemAnimationEnd_.bind(this));
  }

  /**
   * Obtains the toggle animation keyframes rule from the document.
   * @param {Document} document Document containing the rule.
   * @return {CSSKeyframesRule} Animation rule.
   * @private
   */
  static getToggleAnimation_(document) {
    for (let i = 0; i < document.styleSheets.length; i++) {
      const styleSheet = document.styleSheets[i];
      let rules = null;
      // External stylesheets may not be accessible due to CORS restrictions.
      // This try/catch is the only way avoid an exception when iterating over
      // stylesheets that include chrome://resources.
      // See https://crbug.com/775525/ for details.
      try {
        rules = styleSheet.cssRules;
      } catch (err) {
        if (err.name == 'SecurityError') {
          continue;
        }
        throw err;
      }

      for (let j = 0; j < rules.length; j++) {
        // HACK: closure does not define experimental CSSRules.
        const keyFramesRule = CSSRule.KEYFRAMES_RULE || 7;
        const rule = rules[j];
        if (rule.type === keyFramesRule &&
            rule.name === 'progress-center-toggle') {
          return rule;
        }
      }
    }

    throw new Error('The progress-center-toggle rules is not found.');
  }

  /**
   * Root element of the progress center.
   * @type {HTMLElement}
   */
  get element() {
    return this.element_;
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
   * Generate destination string for display on the feedback panel.
   * @param {!ProgressCenterItem} item Item we're generating a message for.
   * @param {Object} info Cached information to use for formatting.
   * @return {string} String formatted based on the item state.
   */
  generateDestinationString_(item, info) {
    const hasDestination = this.isNonEmptyString_(info['destination']);
    switch (item.state) {
      case 'progressing':
        if (hasDestination) {
          return strf('TO_FOLDER_NAME', info['destination']);
        }
        break;
      case 'completed':
        if (item.type === ProgressItemType.COPY) {
          if (hasDestination) {
            return strf('COPIED_TO', info['destination']);
          } else {
            return str('COPIED');
          }
        } else if (item.type === ProgressItemType.MOVE) {
          if (hasDestination) {
            return strf('MOVED_TO', info['destination']);
          } else {
            return str('MOVED');
          }
        }
        break;
      case 'error':
      case 'canceled':
        break;
      default:
        assertNotReached();
        break;
    }
    return '';
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

      let primaryText, secondaryText;
      if (util.isTransferDetailsEnabled()) {
        primaryText = this.generatePrimaryString_(item, panelItem.userData);
        panelItem.secondaryText = this.generateRemainingTimeMessage(item);
      } else {
        primaryText = this.generateSourceString_(item, panelItem.userData);
        if (item.destinationMessage) {
          panelItem.secondaryText =
              strf('TO_FOLDER_NAME', item.destinationMessage);
        }
      }
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
            donePanelItem.panelType = donePanelItem.panelTypeDone;
            donePanelItem.primaryText = primaryText;
            if (util.isTransferDetailsEnabled()) {
              donePanelItem.secondaryText = str('COMPLETE_LABEL');
            } else {
              donePanelItem.secondaryText =
                  this.generateDestinationString_(item, panelItem.userData);
            }
            donePanelItem.signalCallback = (signal) => {
              if (signal === 'dismiss') {
                this.feedbackHost_.removePanelItem(donePanelItem);
              }
            };
            // Delete after 4 seconds, doesn't matter if it's manually deleted
            // before the timer fires, as removePanelItem handles that case.
            setTimeout(() => {
              this.feedbackHost_.removePanelItem(donePanelItem);
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
          // Make sure the panel is attached so it shows immediately.
          this.feedbackHost_.attachPanelItem(panelItem);
          break;
      }
    } else if (panelItem) {
      this.feedbackHost_.removePanelItem(panelItem);
    }
  }

  /**
   * Updates an item to the progress center panel.
   * @param {!ProgressCenterItem} item Item including new contents.
   */
  updateItem(item) {
    const targetGroup = this.getGroupForItem_(item);

    // Update the item.
    targetGroup.update(item);

    // Update an open view item.
    const newItem = targetGroup.getItem(item.id);
    this.updateFeedbackPanelItem(item, newItem);
  }

  /**
   * Handles the item animation end.
   * @param {Event} event Item animation end event.
   * @private
   * @suppress {checkTypes}
   * TODO(crbug.com/947388) Remove the suppress, and fix closure compile.
   */
  onItemAnimationEnd_(event) {
    const targetGroup = event.target.classList.contains('quiet') ?
        this.quietItemGroup_ :
        this.normalItemGroup_;
    if (event.target === this.closeView_) {
      targetGroup.completeSummarizedItemAnimation();
    } else {
      const itemId = event.target.getAttribute('data-progress-id');
      targetGroup.completeItemAnimation(itemId);
      const panelItem = this.feedbackHost_.findPanelItemById(itemId);
      if (panelItem) {
        this.feedbackHost_.removePanelItem(panelItem);
      }
    }
    this.updateCloseView_();
  }

  /**
   * Requests all item groups to dismiss an error item.
   * @param {string} id Item id.
   */
  dismissErrorItem(id) {
    this.normalItemGroup_.dismissErrorItem(id);
    this.quietItemGroup_.dismissErrorItem(id);

    const element = this.getItemElement_(id);
    if (element) {
      this.openView_.removeChild(element);
    }
    this.updateCloseView_();
  }

  /**
   * Updates the close view.
   * @private
   */
  updateCloseView_() {
    // Try to use the normal summarized item.
    const normalSummarizedItem =
        this.normalItemGroup_.getSummarizedItem(this.quietItemGroup_.numErrors);
    if (normalSummarizedItem) {
      // If the quiet animation is overridden by normal summarized item, discard
      // the quiet animation.
      if (this.quietItemGroup_.isSummarizedAnimated()) {
        this.quietItemGroup_.completeSummarizedItemAnimation();
      }

      // Update the view state.
      this.closeView_.update(
          normalSummarizedItem, this.normalItemGroup_.isSummarizedAnimated());
      this.element_.hidden = false;
      return;
    }

    // Try to use the quiet summarized item.
    const quietSummarizedItem =
        this.quietItemGroup_.getSummarizedItem(this.normalItemGroup_.numErrors);
    if (quietSummarizedItem) {
      this.closeView_.update(
          quietSummarizedItem, this.quietItemGroup_.isSummarizedAnimated());
      this.element_.hidden = false;
      return;
    }

    // Try to use the error summarized item.
    const errorSummarizedItem = ProgressCenterItemGroup.getSummarizedErrorItem(
        this.normalItemGroup_, this.quietItemGroup_);
    if (errorSummarizedItem) {
      this.closeView_.update(errorSummarizedItem, false);
      this.element_.hidden = false;
      return;
    }

    // Hide the progress center because there is no items to show.
    this.closeView_.reset();
    this.element_.hidden = true;
    this.element_.classList.remove('opened');
  }

  /**
   * Gets an item element having the specified ID.
   * @param {string} id progress item ID.
   * @return {ProgressCenterItemElement} Item element having the ID.
   * @private
   */
  getItemElement_(id) {
    const query = 'li[data-progress-id="' + id + '"]';
    return /** @type {ProgressCenterItemElement} */ (
        this.openView_.querySelector(query));
  }

  /**
   * Obtains the group for the item.
   * @param {ProgressCenterItem} item Progress item.
   * @return {ProgressCenterItemGroup} Item group that should contain the item.
   * @private
   */
  getGroupForItem_(item) {
    return item.quiet ? this.quietItemGroup_ : this.normalItemGroup_;
  }

  /**
   * Handles the animation end event of the progress center.
   * @param {Event} event Animation end event.
   * @private
   */
  onToggleAnimationEnd_(event) {
    // Transition end of the root element's height.
    if (event.target === this.element_ &&
        event.animationName === 'progress-center-toggle') {
      this.element_.classList.remove('animated');
      return;
    }
  }

  /**
   * Handles the click event.
   * @param {Event} event Click event.
   * @private
   */
  onClick_(event) {
    // Toggle button.
    if (event.target.classList.contains('open') ||
        event.target.classList.contains('close')) {
      // If the progress center has already animated, just return.
      if (this.element_.classList.contains('animated')) {
        return;
      }

      // Obtains current and target height.
      let currentHeight;
      let targetHeight;
      if (this.element_.classList.contains('opened')) {
        currentHeight = this.openView_.getBoundingClientRect().height;
        targetHeight = this.closeView_.getBoundingClientRect().height;
      } else {
        currentHeight = this.closeView_.getBoundingClientRect().height;
        targetHeight = this.openView_.getBoundingClientRect().height;
      }

      // Set styles for animation.
      this.toggleAnimation_.cssRules[0].style.height = currentHeight + 'px';
      this.toggleAnimation_.cssRules[1].style.height = targetHeight + 'px';
      this.element_.classList.add('animated');
      this.element_.classList.toggle('opened');
      return;
    }

    if (event.target.classList.contains('dismiss')) {
      // To dismiss the error item in all windows, we send this to progress
      // center in background page.
      const itemElement = event.target.parentNode.parentNode;
      const id = itemElement.getAttribute('data-progress-id');
      this.dismissErrorItemCallback(id);
    }

    // Cancel button.
    if (event.target.classList.contains('cancel')) {
      const itemElement = event.target.parentNode.parentNode;
      if (this.cancelCallback) {
        const id = itemElement.getAttribute('data-progress-id');
        this.cancelCallback(id);
      }
    }
  }
}
