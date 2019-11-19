// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Watches for events in the browser such as focus changes.
 *
 */

goog.provide('cvox.ChromeVoxEventWatcher');
goog.provide('cvox.ChromeVoxEventWatcherUtil');

goog.require('cvox.ActiveIndicator');
goog.require('cvox.ApiImplementation');
goog.require('cvox.AriaUtil');
goog.require('cvox.ChromeVox');
goog.require('cvox.ChromeVoxEditableTextBase');
goog.require('cvox.ChromeVoxEventSuspender');
goog.require('cvox.ChromeVoxHTMLDateWidget');
goog.require('cvox.ChromeVoxHTMLMediaWidget');
goog.require('cvox.ChromeVoxHTMLTimeWidget');
goog.require('cvox.ChromeVoxKbHandler');
goog.require('cvox.ChromeVoxUserCommands');
goog.require('cvox.DomUtil');
goog.require('cvox.Focuser');
goog.require('cvox.History');
goog.require('cvox.LiveRegions');
goog.require('cvox.Memoize');
goog.require('cvox.NavigationSpeaker');
goog.require('cvox.PlatformFilter');  // TODO: Find a better place for this.
goog.require('cvox.PlatformUtil');
goog.require('cvox.QueueMode');
goog.require('cvox.TextHandlerInterface');
goog.require('cvox.UserEventDetail');

/**
 * @constructor
 */
cvox.ChromeVoxEventWatcher = function() {
};

/**
 * The maximum amount of time to wait before processing events.
 * A max time is needed so that even if a page is constantly updating,
 * events will still go through.
 * @const
 * @type {number}
 * @private
 */
cvox.ChromeVoxEventWatcher.MAX_WAIT_TIME_MS_ = 50;

/**
 * As long as the MAX_WAIT_TIME_ has not been exceeded, the event processor
 * will wait this long after the last event was received before starting to
 * process events.
 * @const
 * @type {number}
 * @private
 */
cvox.ChromeVoxEventWatcher.WAIT_TIME_MS_ = 10;

/**
 * Maximum number of live regions that we will attempt to process.
 * @const
 * @type {number}
 * @private
 */
cvox.ChromeVoxEventWatcher.MAX_LIVE_REGIONS_ = 5;


/**
 * Whether or not ChromeVox should echo keys.
 * It is useful to turn this off in case the system is already echoing keys (for
 * example, in Android).
 *
 * @type {boolean}
 */
cvox.ChromeVoxEventWatcher.shouldEchoKeys = true;


/**
 * Whether or not the next utterance should flush all previous speech.
 * Immediately after a key down or user action, we make the next speech
 * flush, but otherwise it's better to do a category flush, so if a single
 * user action generates both a focus change and a live region change,
 * both get spoken.
 * @type {boolean}
 */
cvox.ChromeVoxEventWatcher.shouldFlushNextUtterance = false;


/**
 * Inits the event watcher and adds listeners.
 * @param {!Document|!Window} doc The DOM document to add event listeners to.
 */
cvox.ChromeVoxEventWatcher.init = function(doc) {
  /**
   * @type {Object}
   */
  cvox.ChromeVoxEventWatcher.lastFocusedNode = null;

  /**
   * @type {Object}
   */
  cvox.ChromeVoxEventWatcher.announcedMouseOverNode = null;

  /**
   * @type {Object}
   */
  cvox.ChromeVoxEventWatcher.pendingMouseOverNode = null;

  /**
   * @type {number?}
   */
  cvox.ChromeVoxEventWatcher.mouseOverTimeoutId = null;

  /**
   * @type {string?}
   */
  cvox.ChromeVoxEventWatcher.lastFocusedNodeValue = null;

  /**
   * @type {Object}
   */
  cvox.ChromeVoxEventWatcher.eventToEat = null;

  /**
   * @type {Element}
   */
  cvox.ChromeVoxEventWatcher.currentTextControl = null;

  /**
   * @type {cvox.ChromeVoxEditableTextBase}
   */
  cvox.ChromeVoxEventWatcher.currentTextHandler = null;

  /**
   * Array of event listeners we've added so we can unregister them if needed.
   * @type {Array}
   * @private
   */
  cvox.ChromeVoxEventWatcher.listeners_ = [];

  /**
   * The mutation observer we use to listen for live regions.
   * @type {MutationObserver}
   * @private
   */
  cvox.ChromeVoxEventWatcher.mutationObserver_ = null;

  /**
   * Whether or not mouse hover events should trigger focusing.
   * @type {boolean}
   */
  cvox.ChromeVoxEventWatcher.focusFollowsMouse = false;

  /**
   * The delay before a mouseover triggers focusing or announcing anything.
   * @type {number}
   */
  cvox.ChromeVoxEventWatcher.mouseoverDelayMs = 500;

  /**
   * Array of events that need to be processed.
   * @type {Array<Event>}
   * @private
   */
  cvox.ChromeVoxEventWatcher.events_ = new Array();

  /**
   * The time when the last event was received.
   * @type {number}
   */
  cvox.ChromeVoxEventWatcher.lastEventTime = 0;

  /**
   * The timestamp for the first unprocessed event.
   * @type {number}
   */
  cvox.ChromeVoxEventWatcher.firstUnprocessedEventTime = -1;

  /**
   * Whether or not queue processing is scheduled to run.
   * @type {boolean}
   * @private
   */
  cvox.ChromeVoxEventWatcher.queueProcessingScheduled_ = false;

  /**
   * A list of callbacks to be called when the EventWatcher has
   * completed processing all events in its queue.
   * @type {Array<function()?>}
   * @private
   */
  cvox.ChromeVoxEventWatcher.readyCallbacks_ = new Array();


/**
 * tracks whether we've received two or more key up's while pass through mode
 * is active.
 * @type {boolean}
 * @private
 */
cvox.ChromeVoxEventWatcher.secondPassThroughKeyUp_ = false;

  /**
   * Whether or not the ChromeOS Search key (keyCode == 91) is being held.
   *
   * We must track this manually because on ChromeOS, the Search key being held
   * down does not cause keyEvent.metaKey to be set.
   *
   * TODO (clchen, dmazzoni): Refactor this since there are edge cases
   * where manually tracking key down and key up can fail (such as when
   * the user switches tabs before letting go of the key being held).
   *
   * @type {boolean}
   */
  cvox.ChromeVox.searchKeyHeld = false;

  /**
   * The mutation observer that listens for changes to text controls
   * that might not send other events.
   * @type {MutationObserver}
   * @private
   */
  cvox.ChromeVoxEventWatcher.textMutationObserver_ = null;

  cvox.ChromeVoxEventWatcher.addEventListeners_(doc);
};


/**
 * Stores state variables in a provided object.
 *
 * @param {Object} store The object.
 */
cvox.ChromeVoxEventWatcher.storeOn = function(store) {
  store['searchKeyHeld'] = cvox.ChromeVox.searchKeyHeld;
};

/**
 * Updates the object with state variables from an earlier storeOn call.
 *
 * @param {Object} store The object.
 */
cvox.ChromeVoxEventWatcher.readFrom = function(store) {
  cvox.ChromeVox.searchKeyHeld = store['searchKeyHeld'];
};

/**
 * Adds an event to the events queue and updates the time when the last
 * event was received.
 *
 * @param {Event} evt The event to be added to the events queue.
 */
cvox.ChromeVoxEventWatcher.addEvent = function(evt) {
  // Don't add any events to the events queue if ChromeVox is inactive or the
  // document isn't focused.
  if (!cvox.ChromeVox.isActive || !cvox.ChromeVox.documentHasFocus()) {
    if (evt.type == 'focus') {
      // If it's a focus event, update the active indicator so that it
      // properly shows and hides as focus moves to iframe and webview
      // elements.
      cvox.ChromeVox.navigationManager.activeIndicator.syncToNode(evt.target);
    }
    return;
  }
  cvox.ChromeVoxEventWatcher.events_.push(evt);
  cvox.ChromeVoxEventWatcher.lastEventTime = new Date().getTime();
  if (cvox.ChromeVoxEventWatcher.firstUnprocessedEventTime == -1) {
    cvox.ChromeVoxEventWatcher.firstUnprocessedEventTime = new Date().getTime();
  }
  if (!cvox.ChromeVoxEventWatcher.queueProcessingScheduled_) {
    cvox.ChromeVoxEventWatcher.queueProcessingScheduled_ = true;
    window.setTimeout(cvox.ChromeVoxEventWatcher.processQueue_,
        cvox.ChromeVoxEventWatcher.WAIT_TIME_MS_);
  }
};

/**
 * Adds a callback to be called when the event watcher has finished
 * processing all pending events.
 * @param {Function} cb The callback.
 */
cvox.ChromeVoxEventWatcher.addReadyCallback = function(cb) {
  cvox.ChromeVoxEventWatcher.readyCallbacks_.push(cb);
  cvox.ChromeVoxEventWatcher.maybeCallReadyCallbacks_();
};

/**
 * Returns whether or not there are pending events.
 * @return {boolean} Whether or not there are pending events.
 * @private
 */
cvox.ChromeVoxEventWatcher.hasPendingEvents_ = function() {
  return cvox.ChromeVoxEventWatcher.firstUnprocessedEventTime != -1 ||
      cvox.ChromeVoxEventWatcher.queueProcessingScheduled_;
};


/**
 * A bit used to make sure only one ready callback is pending at a time.
 * @private
 */
cvox.ChromeVoxEventWatcher.readyCallbackRunning_ = false;

/**
 * Checks if the event watcher has pending events.  If not, call the oldest
 * readyCallback in a loop until exhausted or until there are pending events.
 * @private
 */
cvox.ChromeVoxEventWatcher.maybeCallReadyCallbacks_ = function() {
  if (!cvox.ChromeVoxEventWatcher.readyCallbackRunning_) {
    cvox.ChromeVoxEventWatcher.readyCallbackRunning_ = true;
    window.setTimeout(function() {
      cvox.ChromeVoxEventWatcher.readyCallbackRunning_ = false;
      if (!cvox.ChromeVoxEventWatcher.hasPendingEvents_() &&
             !cvox.ChromeVoxEventWatcher.queueProcessingScheduled_ &&
             cvox.ChromeVoxEventWatcher.readyCallbacks_.length > 0) {
        cvox.ChromeVoxEventWatcher.readyCallbacks_.shift()();
        cvox.ChromeVoxEventWatcher.maybeCallReadyCallbacks_();
      }
    }, 5);
  }
};


/**
 * Add all of our event listeners to the document.
 * @param {!Document|!Window} doc The DOM document to add event listeners to.
 * @private
 */
cvox.ChromeVoxEventWatcher.addEventListeners_ = function(doc) {
  // We always need key down listeners to intercept activate/deactivate.
  cvox.ChromeVoxEventWatcher.addEventListener_(doc,
      'keydown', cvox.ChromeVoxEventWatcher.keyDownEventWatcher, true);

  // If ChromeVox isn't active, skip all other event listeners.
  if (!cvox.ChromeVox.isActive || cvox.ChromeVox.entireDocumentIsHidden) {
    return;
  }
  cvox.ChromeVoxEventWatcher.addEventListener_(doc,
      'keypress', cvox.ChromeVoxEventWatcher.keyPressEventWatcher, true);
  cvox.ChromeVoxEventWatcher.addEventListener_(doc,
      'keyup', cvox.ChromeVoxEventWatcher.keyUpEventWatcher, true);
  // Listen for our own events to handle public user commands if the web app
  // doesn't do it for us.
  cvox.ChromeVoxEventWatcher.addEventListener_(doc,
      cvox.UserEventDetail.Category.JUMP,
      cvox.ChromeVoxUserCommands.handleChromeVoxUserEvent,
      false);

  cvox.ChromeVoxEventWatcher.addEventListener_(doc,
      'focus', cvox.ChromeVoxEventWatcher.focusEventWatcher, true);
  cvox.ChromeVoxEventWatcher.addEventListener_(doc,
      'blur', cvox.ChromeVoxEventWatcher.blurEventWatcher, true);
  cvox.ChromeVoxEventWatcher.addEventListener_(doc,
      'change', cvox.ChromeVoxEventWatcher.changeEventWatcher, true);
  cvox.ChromeVoxEventWatcher.addEventListener_(doc,
      'copy', cvox.ChromeVoxEventWatcher.clipboardEventWatcher, true);
  cvox.ChromeVoxEventWatcher.addEventListener_(doc,
      'cut', cvox.ChromeVoxEventWatcher.clipboardEventWatcher, true);
  cvox.ChromeVoxEventWatcher.addEventListener_(doc,
      'paste', cvox.ChromeVoxEventWatcher.clipboardEventWatcher, true);
  cvox.ChromeVoxEventWatcher.addEventListener_(doc,
      'select', cvox.ChromeVoxEventWatcher.selectEventWatcher, true);

  // TODO(dtseng): Experimental, see:
  // https://developers.google.com/chrome/whitepapers/pagevisibility
  cvox.ChromeVoxEventWatcher.addEventListener_(doc, 'webkitvisibilitychange',
      cvox.ChromeVoxEventWatcher.visibilityChangeWatcher, true);
  cvox.ChromeVoxEventWatcher.events_ = new Array();
  cvox.ChromeVoxEventWatcher.queueProcessingScheduled_ = false;

  // Handle mouse events directly without going into the events queue.
  cvox.ChromeVoxEventWatcher.addEventListener_(doc,
      'mouseover', cvox.ChromeVoxEventWatcher.mouseOverEventWatcher, true);
  cvox.ChromeVoxEventWatcher.addEventListener_(doc,
      'mouseout', cvox.ChromeVoxEventWatcher.mouseOutEventWatcher, true);

  // With the exception of non-Android, click events go through the event queue.
  cvox.ChromeVoxEventWatcher.addEventListener_(doc,
      'click', cvox.ChromeVoxEventWatcher.mouseClickEventWatcher, true);

  cvox.ChromeVoxEventWatcher.mutationObserver_ =
      new window.WebKitMutationObserver(
          cvox.ChromeVoxEventWatcher.mutationHandler);
  var observerTarget = null;
  if (doc.documentElement) {
    observerTarget = doc.documentElement;
  } else if (doc.document && doc.document.documentElement) {
    observerTarget = doc.document.documentElement;
  }
  if (observerTarget) {
    cvox.ChromeVoxEventWatcher.mutationObserver_.observe(
        observerTarget,
        /** @type {!MutationObserverInit} */ ({
          childList: true,
          attributes: true,
          characterData: true,
          subtree: true,
          attributeOldValue: true,
          characterDataOldValue: true
        }));
  }
};


/**
 * Remove all registered event watchers.
 * @param {!Document|!Window} doc The DOM document to add event listeners to.
 */
cvox.ChromeVoxEventWatcher.cleanup = function(doc) {
  for (var i = 0; i < cvox.ChromeVoxEventWatcher.listeners_.length; i++) {
    var listener = cvox.ChromeVoxEventWatcher.listeners_[i];
    doc.removeEventListener(
        listener.type, listener.listener, listener.useCapture);
  }
  cvox.ChromeVoxEventWatcher.listeners_ = [];
  if (cvox.ChromeVoxEventWatcher.currentDateHandler) {
    cvox.ChromeVoxEventWatcher.currentDateHandler.shutdown();
  }
  if (cvox.ChromeVoxEventWatcher.currentTimeHandler) {
    cvox.ChromeVoxEventWatcher.currentTimeHandler.shutdown();
  }
  if (cvox.ChromeVoxEventWatcher.currentMediaHandler) {
    cvox.ChromeVoxEventWatcher.currentMediaHandler.shutdown();
  }
  if (cvox.ChromeVoxEventWatcher.mutationObserver_) {
    cvox.ChromeVoxEventWatcher.mutationObserver_.disconnect();
  }
  cvox.ChromeVoxEventWatcher.mutationObserver_ = null;
};

/**
 * Add one event listener and save the data so it can be removed later.
 * @param {!Document|!Window} doc The DOM document to add event listeners to.
 * @param {string} type The event type.
 * @param {EventListener|function(Event):(boolean|undefined)} listener
 *     The function to be called when the event is fired.
 * @param {boolean} useCapture Whether this listener should capture events
 *     before they're sent to targets beneath it in the DOM tree.
 * @private
 */
cvox.ChromeVoxEventWatcher.addEventListener_ = function(doc, type,
    listener, useCapture) {
  cvox.ChromeVoxEventWatcher.listeners_.push(
      {'type': type, 'listener': listener, 'useCapture': useCapture});
  doc.addEventListener(type, listener, useCapture);
};

/**
 * Return the last focused node.
 * @return {Object} The last node that was focused.
 */
cvox.ChromeVoxEventWatcher.getLastFocusedNode = function() {
  return cvox.ChromeVoxEventWatcher.lastFocusedNode;
};

/**
 * Sets the last focused node.
 * @param {Element} element The last focused element.
 *
 * @private
 */
cvox.ChromeVoxEventWatcher.setLastFocusedNode_ = function(element) {
  cvox.ChromeVoxEventWatcher.lastFocusedNode = element;
  cvox.ChromeVoxEventWatcher.lastFocusedNodeValue = !element ? null :
      cvox.DomUtil.getControlValueAndStateString(element);
};

/**
 * Called when there's any mutation of the document. We use this to
 * handle live region updates.
 * @param {Array<MutationRecord>} mutations The mutations.
 * @return {boolean} True if the default action should be performed.
 */
cvox.ChromeVoxEventWatcher.mutationHandler = function(mutations) {
  if (cvox.ChromeVoxEventSuspender.areEventsSuspended()) {
    return true;
  }

  cvox.ChromeVox.navigationManager.updateIndicatorIfChanged();

  cvox.LiveRegions.processMutations(
      mutations,
      function(assertive, navDescriptions) {
        var evt = new window.Event('LiveRegion');
        evt.navDescriptions = navDescriptions;
        evt.assertive = assertive;
        cvox.ChromeVoxEventWatcher.addEvent(evt);
        return true;
      });
  return false;
};


/**
 * Handles mouseclick events.
 * Mouseclick events are only triggered if the user touches the mouse;
 * we use it to determine whether or not we should bother trying to sync to a
 * selection.
 * @param {Event} evt The mouseclick event to process.
 * @return {boolean} True if the default action should be performed.
 */
cvox.ChromeVoxEventWatcher.mouseClickEventWatcher = function(evt) {
  if (evt.fromCvox) {
    return true;
  }

  if (cvox.ChromeVox.host.mustRedispatchClickEvent()) {
    cvox.ChromeVoxUserCommands.wasMouseClicked = true;
    evt.stopPropagation();
    evt.preventDefault();
    // Since the click event was caught and we are re-dispatching it, we also
    // need to refocus the current node because the current node has already
    // been blurred by the window getting the click event in the first place.
    // Failing to restore focus before clicking can cause odd problems such as
    // the soft IME not coming up in Android (it only shows up if the click
    // happens in a focused text field).
    cvox.Focuser.setFocus(cvox.ChromeVox.navigationManager.getCurrentNode());
    cvox.ChromeVox.tts.speak(
        Msgs.getMsg('element_clicked'),
        cvox.ChromeVoxEventWatcher.queueMode_(),
        cvox.AbstractTts.PERSONALITY_ANNOTATION);
    var targetNode = cvox.ChromeVox.navigationManager.getCurrentNode();
    // If the targetNode has a defined onclick function, just call it directly
    // rather than try to generate a click event and dispatching it.
    // While both work equally well on standalone Chrome, when dealing with
    // embedded WebViews, generating a click event and sending it is not always
    // reliable since the framework may swallow the event.
    cvox.DomUtil.clickElem(targetNode, false, true);
    return false;
  } else {
    cvox.ChromeVoxEventWatcher.addEvent(evt);
  }
  cvox.ChromeVoxUserCommands.wasMouseClicked = true;
  return true;
};

/**
 * Handles mouseover events.
 * Mouseover events are only triggered if the user touches the mouse, so
 * for users who only use the keyboard, this will have no effect.
 *
 * @param {Event} evt The mouseover event to process.
 * @return {boolean} True if the default action should be performed.
 */
cvox.ChromeVoxEventWatcher.mouseOverEventWatcher = function(evt) {
  // Chrome simulates the meta key for mouse events generated from
  // touch exploration.
  var isTouchEvent = (evt.metaKey);

  var mouseoverDelayMs = cvox.ChromeVoxEventWatcher.mouseoverDelayMs;
  if (isTouchEvent) {
    mouseoverDelayMs = 0;
  } else if (!cvox.ChromeVoxEventWatcher.focusFollowsMouse) {
    return true;
  }

  if (cvox.DomUtil.isDescendantOfNode(
      cvox.ChromeVoxEventWatcher.announcedMouseOverNode, evt.target)) {
    return true;
  }

  if (evt.target == cvox.ChromeVoxEventWatcher.pendingMouseOverNode) {
    return true;
  }

  cvox.ChromeVoxEventWatcher.pendingMouseOverNode = evt.target;
  if (cvox.ChromeVoxEventWatcher.mouseOverTimeoutId) {
    window.clearTimeout(cvox.ChromeVoxEventWatcher.mouseOverTimeoutId);
    cvox.ChromeVoxEventWatcher.mouseOverTimeoutId = null;
  }

  if (evt.target.tagName && (evt.target.tagName == 'BODY')) {
    cvox.ChromeVoxEventWatcher.pendingMouseOverNode = null;
    cvox.ChromeVoxEventWatcher.announcedMouseOverNode = null;
    return true;
  }

  // Only focus and announce if the mouse stays over the same target
  // for longer than the given delay.
  cvox.ChromeVoxEventWatcher.mouseOverTimeoutId = window.setTimeout(
      function() {
        cvox.ChromeVoxEventWatcher.mouseOverTimeoutId = null;
        if (evt.target != cvox.ChromeVoxEventWatcher.pendingMouseOverNode) {
          return;
        }

        cvox.Memoize.scope(function() {
          cvox.ChromeVoxEventWatcher.shouldFlushNextUtterance = true;
          cvox.ChromeVox.navigationManager.stopReading(true);
          var target = /** @type {Node} */(evt.target);
          cvox.Focuser.setFocus(target);
          cvox.ApiImplementation.syncToNode(
              target, true, cvox.ChromeVoxEventWatcher.queueMode_());
          cvox.ChromeVoxEventWatcher.announcedMouseOverNode = target;
        });
      }, mouseoverDelayMs);

  return true;
};

/**
 * Handles mouseout events.
 *
 * @param {Event} evt The mouseout event to process.
 * @return {boolean} True if the default action should be performed.
 */
cvox.ChromeVoxEventWatcher.mouseOutEventWatcher = function(evt) {
  if (evt.target == cvox.ChromeVoxEventWatcher.pendingMouseOverNode) {
    cvox.ChromeVoxEventWatcher.pendingMouseOverNode = null;
    if (cvox.ChromeVoxEventWatcher.mouseOverTimeoutId) {
      window.clearTimeout(cvox.ChromeVoxEventWatcher.mouseOverTimeoutId);
      cvox.ChromeVoxEventWatcher.mouseOverTimeoutId = null;
    }
  }

  return true;
};


/**
 * Watches for focus events.
 *
 * @param {Event} evt The focus event to add to the queue.
 * @return {boolean} True if the default action should be performed.
 */
cvox.ChromeVoxEventWatcher.focusEventWatcher = function(evt) {
  // First remove any dummy spans. We create dummy spans in UserCommands in
  // order to sync the browser's default tab action with the user's current
  // navigation position.
  cvox.ChromeVoxUserCommands.removeTabDummySpan();

  if (!cvox.ChromeVoxEventSuspender.areEventsSuspended()) {
    cvox.ChromeVoxEventWatcher.addEvent(evt);
  } else if (evt.target && evt.target.nodeType == Node.ELEMENT_NODE) {
    cvox.ChromeVoxEventWatcher.setLastFocusedNode_(
        /** @type {Element} */(evt.target));
  }
  return true;
};

/**
 * Handles for focus events passed to it from the events queue.
 *
 * @param {Event} evt The focus event to handle.
 */
cvox.ChromeVoxEventWatcher.focusHandler = function(evt) {
  if (!cvox.ChromeVox.documentHasFocus()) {
    return;
  }
  if (evt.target &&
      evt.target.hasAttribute &&
      evt.target.getAttribute('aria-hidden') == 'true' &&
      evt.target.getAttribute('chromevoxignoreariahidden') != 'true') {
    cvox.ChromeVoxEventWatcher.setLastFocusedNode_(null);
    cvox.ChromeVoxEventWatcher.setUpTextHandler();
    return;
  }
  if (evt.target && evt.target != window) {
    var target = /** @type {Element} */(evt.target);
    var parentControl = cvox.DomUtil.getSurroundingControl(target);
    if (parentControl &&
        parentControl == cvox.ChromeVoxEventWatcher.lastFocusedNode) {
      cvox.ChromeVoxEventWatcher.handleControlChanged(target);
      return;
    }

    if (parentControl) {
      cvox.ChromeVoxEventWatcher.setLastFocusedNode_(
          /** @type {Element} */(parentControl));
    } else {
      cvox.ChromeVoxEventWatcher.setLastFocusedNode_(target);
    }

    var queueMode = cvox.ChromeVoxEventWatcher.queueMode_();

    if (cvox.ChromeVoxEventWatcher.getInitialVisibility() ||
        cvox.ChromeVoxEventWatcher.handleDialogFocus(target)) {
      queueMode = cvox.QueueMode.QUEUE;
    }

    if (cvox.ChromeVox.navigationManager.clearPageSel(true)) {
      queueMode = cvox.QueueMode.QUEUE;
    }

    // Navigate to this control so that it will be the same for focus as for
    // regular navigation.
    cvox.ApiImplementation.syncToNode(
        target, !document.webkitHidden, queueMode);

    if ((evt.target.constructor == HTMLVideoElement) ||
        (evt.target.constructor == HTMLAudioElement)) {
      cvox.ChromeVoxEventWatcher.setUpMediaHandler_();
      return;
    }
    if (evt.target.hasAttribute) {
      var inputType = evt.target.getAttribute('type');
      switch (inputType) {
        case 'time':
          cvox.ChromeVoxEventWatcher.setUpTimeHandler_();
          return;
        case 'date':
        case 'month':
        case 'week':
          cvox.ChromeVoxEventWatcher.setUpDateHandler_();
          return;
      }
    }
    cvox.ChromeVoxEventWatcher.setUpTextHandler();
  } else {
    cvox.ChromeVoxEventWatcher.setLastFocusedNode_(null);
  }
  return;
};

/**
 * Watches for blur events.
 *
 * @param {Event} evt The blur event to add to the queue.
 * @return {boolean} True if the default action should be performed.
 */
cvox.ChromeVoxEventWatcher.blurEventWatcher = function(evt) {
  window.setTimeout(function() {
    if (!document.activeElement) {
      cvox.ChromeVoxEventWatcher.setLastFocusedNode_(null);
      cvox.ChromeVoxEventWatcher.addEvent(evt);
    }
  }, 0);
  return true;
};

/**
 * Watches for key down events.
 *
 * @param {Event} evt The keydown event to add to the queue.
 * @return {boolean} True if the default action should be performed.
 */
cvox.ChromeVoxEventWatcher.keyDownEventWatcher = function(evt) {
  return /** @type {boolean} */ (cvox.Memoize.scope(
      cvox.ChromeVoxEventWatcher.doKeyDownEventWatcher_.bind(this, evt)));
};

/**
 * Implementation of |keyDownEventWatcher|.
 *
 * @param {Event} evt The keydown event to add to the queue.
 * @return {boolean} True if the default action should be performed.
 * @private
 */
cvox.ChromeVoxEventWatcher.doKeyDownEventWatcher_ = function(evt) {
  cvox.ChromeVoxEventWatcher.shouldFlushNextUtterance = true;

  if (cvox.ChromeVox.passThroughMode) {
    return true;
  }

  if (cvox.ChromeVox.isChromeOS && evt.keyCode == 91) {
    cvox.ChromeVox.searchKeyHeld = true;
  }

  // Store some extra ChromeVox-specific properties in the event.
  evt.searchKeyHeld =
      cvox.ChromeVox.searchKeyHeld && cvox.ChromeVox.isActive;
  evt.stickyMode = cvox.ChromeVox.isStickyModeOn() && cvox.ChromeVox.isActive;
  evt.keyPrefix = cvox.ChromeVox.keyPrefixOn && cvox.ChromeVox.isActive;

  cvox.ChromeVox.keyPrefixOn = false;

  cvox.ChromeVoxEventWatcher.eventToEat = null;
  if (!cvox.ChromeVoxKbHandler.basicKeyDownActionsListener(evt) ||
      cvox.ChromeVoxEventWatcher.handleControlAction(evt)) {
    // Swallow the event immediately to prevent the arrow keys
    // from driving controls on the web page.
    evt.preventDefault();
    evt.stopPropagation();
    // Also mark this as something to be swallowed when the followup
    // keypress/keyup counterparts to this event show up later.
    cvox.ChromeVoxEventWatcher.eventToEat = evt;
    return false;
  }
  cvox.ChromeVoxEventWatcher.addEvent(evt);
  return true;
};

/**
 * Watches for key up events.
 *
 * @param {Event} evt The event to add to the queue.
 * @return {boolean} True if the default action should be performed.
 * @this {cvox.ChromeVoxEventWatcher}
 */
cvox.ChromeVoxEventWatcher.keyUpEventWatcher = function(evt) {
  if (evt.keyCode == 91) {
    cvox.ChromeVox.searchKeyHeld = false;
  }

  if (cvox.ChromeVox.passThroughMode) {
    if (!evt.ctrlKey && !evt.altKey && !evt.metaKey && !evt.shiftKey &&
        !cvox.ChromeVox.searchKeyHeld) {
      // Only reset pass through on the second key up without modifiers since
      // the first one is from the pass through shortcut itself.
      if (this.secondPassThroughKeyUp_) {
        this.secondPassThroughKeyUp_ = false;
        cvox.ChromeVox.passThroughMode = false;
      } else {
        this.secondPassThroughKeyUp_ = true;
      }
    }
    return true;
  }

  if (cvox.ChromeVoxEventWatcher.eventToEat &&
      evt.keyCode == cvox.ChromeVoxEventWatcher.eventToEat.keyCode) {
    evt.stopPropagation();
    evt.preventDefault();
    return false;
  }

  cvox.ChromeVoxEventWatcher.addEvent(evt);

  return true;
};

/**
 * Watches for key press events.
 *
 * @param {Event} evt The event to add to the queue.
 * @return {boolean} True if the default action should be performed.
 */
cvox.ChromeVoxEventWatcher.keyPressEventWatcher = function(evt) {
  var url = document.location.href;
  // Use ChromeVox.typingEcho as default value.
  var speakChar = cvox.TypingEcho.shouldSpeakChar(cvox.ChromeVox.typingEcho);

  if (typeof cvox.ChromeVox.keyEcho[url] !== 'undefined') {
    speakChar = cvox.ChromeVox.keyEcho[url];
  }

  // Directly handle typed characters here while key echo is on. This
  // skips potentially costly computations (especially for content editable).
  // This is done deliberately for the sake of responsiveness and in some cases
  // (e.g. content editable), to have characters echoed properly.
  if (cvox.ChromeVoxEditableTextBase.eventTypingEcho && (speakChar &&
          cvox.DomPredicates.editTextPredicate([document.activeElement])) &&
      document.activeElement.type !== 'password') {
    cvox.ChromeVox.tts.speak(String.fromCharCode(evt.charCode),
                             cvox.QueueMode.FLUSH);
  }
  cvox.ChromeVoxEventWatcher.addEvent(evt);
  if (cvox.ChromeVoxEventWatcher.eventToEat &&
      evt.keyCode == cvox.ChromeVoxEventWatcher.eventToEat.keyCode) {
    evt.preventDefault();
    evt.stopPropagation();
    return false;
  }
  return true;
};

/**
 * Watches for change events.
 *
 * @param {Event} evt The event to add to the queue.
 * @return {boolean} True if the default action should be performed.
 */
cvox.ChromeVoxEventWatcher.changeEventWatcher = function(evt) {
  cvox.ChromeVoxEventWatcher.addEvent(evt);
  return true;
};

// TODO(dtseng): ChromeVoxEditableText interrupts cut and paste announcements.
/**
 * Watches for cut, copy, and paste events.
 *
 * @param {Event} evt The event to process.
 * @return {boolean} True if the default action should be performed.
 */
cvox.ChromeVoxEventWatcher.clipboardEventWatcher = function(evt) {
  // Don't announce anything unless this document has focus and the
  // editable element that's the target of the clipboard event is visible.
  var targetNode = /** @type {Node} */(evt.target);
  if (!cvox.ChromeVox.documentHasFocus() ||
      !targetNode ||
      !cvox.DomUtil.isVisible(targetNode) ||
      cvox.AriaUtil.isHidden(targetNode)) {
    return true;
  }

  cvox.ChromeVox.tts.speak(Msgs.getMsg(evt.type).toLowerCase(),
                           cvox.QueueMode.QUEUE);
  var text = '';
  switch (evt.type) {
  case 'paste':
    text = evt.clipboardData.getData('text');
    break;
  case 'copy':
  case 'cut':
    text = window.getSelection().toString();
    break;
  }
  cvox.ChromeVox.tts.speak(text, cvox.QueueMode.QUEUE);
  cvox.ChromeVox.navigationManager.clearPageSel();
  return true;
};

/**
 * Handles change events passed to it from the events queue.
 *
 * @param {Event} evt The event to handle.
 */
cvox.ChromeVoxEventWatcher.changeHandler = function(evt) {
  if (cvox.ChromeVoxEventWatcher.setUpTextHandler()) {
    return;
  }
  if (document.activeElement == evt.target) {
    cvox.ChromeVoxEventWatcher.handleControlChanged(document.activeElement);
  }
};

/**
 * Watches for select events.
 *
 * @param {Event} evt The event to add to the queue.
 * @return {boolean} True if the default action should be performed.
 */
cvox.ChromeVoxEventWatcher.selectEventWatcher = function(evt) {
  cvox.ChromeVoxEventWatcher.addEvent(evt);
  return true;
};

/**
 * Listens for WebKit visibility change events.
 */
cvox.ChromeVoxEventWatcher.visibilityChangeWatcher = function() {
  cvox.ChromeVoxEventWatcher.initialVisibility = !document.webkitHidden;
  if (document.webkitHidden) {
    cvox.ChromeVox.navigationManager.stopReading(true);
  }
};

/**
 * Gets the initial visibility of the page.
 * @return {boolean} True if the page is visible and this is the first request
 * for visibility state.
 */
cvox.ChromeVoxEventWatcher.getInitialVisibility = function() {
  var ret = cvox.ChromeVoxEventWatcher.initialVisibility;
  cvox.ChromeVoxEventWatcher.initialVisibility = false;
  return ret;
};

/**
 * Speaks the text of one live region.
 * @param {boolean} assertive True if it's an assertive live region.
 * @param {Array<cvox.NavDescription>} messages An array of navDescriptions
 *    representing the description of the live region changes.
 * @private
 */
cvox.ChromeVoxEventWatcher.speakLiveRegion_ = function(
    assertive, messages) {
  var queueMode = cvox.ChromeVoxEventWatcher.queueMode_();
  var descSpeaker = new cvox.NavigationSpeaker();
  descSpeaker.speakDescriptionArray(messages, queueMode, null);
};

/**
 * Sets up the text handler.
 * @return {boolean} True if an editable text control has focus.
 */
cvox.ChromeVoxEventWatcher.setUpTextHandler = function() {
  var currentFocus = document.activeElement;
  if (currentFocus &&
      currentFocus.hasAttribute &&
      currentFocus.getAttribute('aria-hidden') == 'true' &&
      currentFocus.getAttribute('chromevoxignoreariahidden') != 'true') {
    currentFocus = null;
  }

  if (currentFocus != cvox.ChromeVoxEventWatcher.currentTextControl) {
    if (cvox.ChromeVoxEventWatcher.currentTextControl) {
      cvox.ChromeVoxEventWatcher.currentTextControl.removeEventListener(
          'input', cvox.ChromeVoxEventWatcher.changeEventWatcher, false);
      cvox.ChromeVoxEventWatcher.currentTextControl.removeEventListener(
          'click', cvox.ChromeVoxEventWatcher.changeEventWatcher, false);
      if (cvox.ChromeVoxEventWatcher.textMutationObserver_) {
        cvox.ChromeVoxEventWatcher.textMutationObserver_.disconnect();
        cvox.ChromeVoxEventWatcher.textMutationObserver_ = null;
      }
    }
    cvox.ChromeVoxEventWatcher.currentTextControl = null;
    if (cvox.ChromeVoxEventWatcher.currentTextHandler) {
      cvox.ChromeVoxEventWatcher.currentTextHandler.teardown();
      cvox.ChromeVoxEventWatcher.currentTextHandler = null;
    }
    if (currentFocus == null) {
      return false;
    }
    if (currentFocus.constructor == HTMLInputElement &&
        cvox.DomUtil.isInputTypeText(currentFocus) &&
        cvox.ChromeVoxEventWatcher.shouldEchoKeys) {
      cvox.ChromeVoxEventWatcher.currentTextControl = currentFocus;
      cvox.ChromeVoxEventWatcher.currentTextHandler =
          new cvox.ChromeVoxEditableHTMLInput(currentFocus, cvox.ChromeVox.tts);
    } else if ((currentFocus.constructor == HTMLTextAreaElement) &&
        cvox.ChromeVoxEventWatcher.shouldEchoKeys) {
      cvox.ChromeVoxEventWatcher.currentTextControl = currentFocus;
      cvox.ChromeVoxEventWatcher.currentTextHandler =
          new cvox.ChromeVoxEditableTextArea(currentFocus, cvox.ChromeVox.tts);
    } else if (currentFocus.isContentEditable ||
               currentFocus.getAttribute('role') == 'textbox') {
      cvox.ChromeVoxEventWatcher.currentTextControl = currentFocus;
      cvox.ChromeVoxEventWatcher.currentTextHandler =
          new cvox.ChromeVoxEditableContentEditable(currentFocus,
              cvox.ChromeVox.tts);
    }

    if (cvox.ChromeVoxEventWatcher.currentTextControl) {
      cvox.ChromeVoxEventWatcher.currentTextControl.addEventListener(
          'input', cvox.ChromeVoxEventWatcher.changeEventWatcher, false);
      cvox.ChromeVoxEventWatcher.currentTextControl.addEventListener(
          'click', cvox.ChromeVoxEventWatcher.changeEventWatcher, false);
      if (window.WebKitMutationObserver) {
        cvox.ChromeVoxEventWatcher.textMutationObserver_ =
            new window.WebKitMutationObserver(
                cvox.ChromeVoxEventWatcher.onTextMutation);
        cvox.ChromeVoxEventWatcher.textMutationObserver_.observe(
            cvox.ChromeVoxEventWatcher.currentTextControl,
            /** @type {!MutationObserverInit} */ ({
              childList: true,
              attributes: true,
              subtree: true,
              attributeOldValue: false,
              characterDataOldValue: false
            }));
      }
      if (!cvox.ChromeVoxEventSuspender.areEventsSuspended()) {
        cvox.ChromeVox.navigationManager.updateSel(
            cvox.CursorSelection.fromNode(
                cvox.ChromeVoxEventWatcher.currentTextControl));
      }
    }

    return (null != cvox.ChromeVoxEventWatcher.currentTextHandler);
  }
  return false;
};

/**
 * Speaks updates to editable text controls as needed.
 *
 * @param {boolean} isKeypress Was this change triggered by a keypress?
 * @return {boolean} True if an editable text control has focus.
 */
cvox.ChromeVoxEventWatcher.handleTextChanged = function(isKeypress) {
  if (cvox.ChromeVoxEventWatcher.currentTextHandler) {
    var handler = cvox.ChromeVoxEventWatcher.currentTextHandler;
    var shouldFlush = false;
    if (isKeypress && cvox.ChromeVoxEventWatcher.shouldFlushNextUtterance) {
      shouldFlush = true;
      cvox.ChromeVoxEventWatcher.shouldFlushNextUtterance = false;
    }
    handler.update(shouldFlush);
    cvox.ChromeVoxEventWatcher.shouldFlushNextUtterance = false;
    return true;
  }
  return false;
};

/**
 * Called when an editable text control has focus, because many changes
 * to a text box don't ever generate events - e.g. if the page's javascript
 * changes the contents of the text box after some delay, or if it's
 * contentEditable or a generic div with role="textbox".
 */
cvox.ChromeVoxEventWatcher.onTextMutation = function() {
  if (cvox.ChromeVoxEventWatcher.currentTextHandler) {
    window.setTimeout(function() {
      cvox.ChromeVoxEventWatcher.handleTextChanged(false);
    }, cvox.ChromeVoxEventWatcher.MAX_WAIT_TIME_MS_);
  }
};

/**
 * Speaks updates to other form controls as needed.
 * @param {Element} control The target control.
 */
cvox.ChromeVoxEventWatcher.handleControlChanged = function(control) {
  var newValue = cvox.DomUtil.getControlValueAndStateString(control);
  var parentControl = cvox.DomUtil.getSurroundingControl(control);
  var announceChange = false;

  if (control != cvox.ChromeVoxEventWatcher.lastFocusedNode &&
      (parentControl == null ||
       parentControl != cvox.ChromeVoxEventWatcher.lastFocusedNode)) {
    cvox.ChromeVoxEventWatcher.setLastFocusedNode_(control);
  } else if (newValue == cvox.ChromeVoxEventWatcher.lastFocusedNodeValue) {
    return;
  }

  cvox.ChromeVoxEventWatcher.lastFocusedNodeValue = newValue;
  if (cvox.DomPredicates.checkboxPredicate([control]) ||
      cvox.DomPredicates.radioPredicate([control])) {
    // Always announce changes to checkboxes and radio buttons.
    announceChange = true;
    // Play earcons for checkboxes and radio buttons
    if (control.checked) {
      cvox.ChromeVox.earcons.playEarcon(cvox.Earcon.CHECK_ON);
    } else {
      cvox.ChromeVox.earcons.playEarcon(cvox.Earcon.CHECK_OFF);
    }
  }

  if (control.tagName == 'SELECT') {
    announceChange = true;
  }

  if (control.tagName == 'INPUT') {
    switch (control.type) {
      case 'color':
      case 'datetime':
      case 'datetime-local':
      case 'range':
        announceChange = true;
        break;
      default:
        break;
    }
  }

  // Always announce changes for anything with an ARIA role.
  if (control.hasAttribute && control.hasAttribute('role')) {
    announceChange = true;
  }

  var activeDescendant = cvox.AriaUtil.getActiveDescendant(control);
  if ((parentControl &&
      parentControl != control &&
      document.activeElement == control)) {
    // Sync ChromeVox to the newly selected control.
    cvox.ApiImplementation.syncToNode(
        activeDescendant || control, true,
        cvox.ChromeVoxEventWatcher.queueMode_());
    announceChange = false;
  } else if (activeDescendant) {
    cvox.ChromeVox.navigationManager.updateSelToArbitraryNode(
        activeDescendant,
        true);

    announceChange = true;
  }

  if (newValue && announceChange &&
      !cvox.ChromeVoxEventSuspender.areEventsSuspended()) {
    cvox.ChromeVox.tts.speak(newValue,
                             cvox.ChromeVoxEventWatcher.queueMode_(),
                             null);
    cvox.ChromeVox.braille.write(cvox.NavBraille.fromText(newValue));
  }
};

/**
 * Handle actions on form controls triggered by key presses.
 * @param {Object} evt The event.
 * @return {boolean} True if this key event was handled.
 */
cvox.ChromeVoxEventWatcher.handleControlAction = function(evt) {
  // Ignore the control action if ChromeVox is not active.
  if (!cvox.ChromeVox.isActive) {
    return false;
  }
  var control = evt.target;

  if (control.tagName == 'SELECT' && (control.size <= 1) &&
      (evt.keyCode == 13 || evt.keyCode == 32)) { // Enter or Space
    // TODO (dmazzoni, clchen): Remove this workaround once accessibility
    // APIs make browser based popups accessible.
    //
    // Do nothing, but eat this keystroke when the SELECT control
    // has a dropdown style since if we don't, it will generate
    // a browser popup menu which is not accessible.
    // List style SELECT controls are fine and don't need this workaround.
    evt.preventDefault();
    evt.stopPropagation();
    return true;
  }

  if (control.tagName == 'INPUT' && control.type == 'range') {
    var value = parseFloat(control.value);
    var step;
    if (control.step && control.step > 0.0) {
      step = control.step;
    } else if (control.min && control.max) {
      var range = (control.max - control.min);
      if (range > 2 && range < 31) {
        step = 1;
      } else {
        step = (control.max - control.min) / 10;
      }
    } else {
      step = 1;
    }

    if (evt.keyCode == 37 || evt.keyCode == 38) {  // left or up
      value -= step;
    } else if (evt.keyCode == 39 || evt.keyCode == 40) {  // right or down
      value += step;
    }

    if (control.max && value > control.max) {
      value = control.max;
    }
    if (control.min && value < control.min) {
      value = control.min;
    }

    control.value = value;
  }
  return false;
};

/**
 * When an element receives focus, see if we've entered or left a dialog
 * and return a string describing the event.
 *
 * @param {Element} target The element that just received focus.
 * @return {boolean} True if an announcement was spoken.
 */
cvox.ChromeVoxEventWatcher.handleDialogFocus = function(target) {
  var dialog = target;
  var role = '';
  while (dialog) {
    if (dialog.hasAttribute) {
      role = dialog.getAttribute('role');
      if (role == 'dialog' || role == 'alertdialog') {
        break;
      }
    }
    dialog = dialog.parentElement;
  }

  if (dialog == cvox.ChromeVox.navigationManager.currentDialog) {
    return false;
  }

  if (cvox.ChromeVox.navigationManager.currentDialog && !dialog) {
    if (!cvox.DomUtil.isDescendantOfNode(
        document.activeElement,
        cvox.ChromeVox.navigationManager.currentDialog)) {
      cvox.ChromeVox.navigationManager.currentDialog = null;

      cvox.ChromeVoxEventWatcher.speakAnnotationWithCategory_(
          Msgs.getMsg('exiting_dialog'),
          cvox.TtsCategory.NAV);
      return true;
    }
  } else {
    if (dialog) {
      cvox.ChromeVox.navigationManager.currentDialog = dialog;
      cvox.ChromeVoxEventWatcher.speakAnnotationWithCategory_(
          Msgs.getMsg('entering_dialog'),
          cvox.TtsCategory.NAV);

      if (role == 'alertdialog') {
        var dialogDescArray =
            cvox.DescriptionUtil.getFullDescriptionsFromChildren(null, dialog);
        var descSpeaker = new cvox.NavigationSpeaker();
        descSpeaker.speakDescriptionArray(dialogDescArray,
                                          cvox.QueueMode.QUEUE,
                                          null);
      }
      return true;
    }
  }
  return false;
};

/**
 * Speak the given text with the annotation personality and the given
 * speech queue utterance category.
 * @param {string} text The text to speak.
 * @param {string} category The category of text, used by the speech queue
 *     when flushing all speech from the same category while leaving other
 *     speech in the queue.
 * @private
 */
cvox.ChromeVoxEventWatcher.speakAnnotationWithCategory_ = function(
    text, category) {
  var properties = {};
  var src = cvox.AbstractTts.PERSONALITY_ANNOTATION;
  for (var key in src) {
    properties[key] = src[key];
  }
  properties['category'] = category;
  cvox.ChromeVox.tts.speak(
      text,
      cvox.ChromeVoxEventWatcher.queueMode_(),
      properties);
};

/**
 * Returns true if we should wait to process events.
 * @param {number} lastFocusTimestamp The timestamp of the last focus event.
 * @param {number} firstTimestamp The timestamp of the first event.
 * @param {number} currentTime The current timestamp.
 * @return {boolean} True if we should wait to process events.
 */
cvox.ChromeVoxEventWatcherUtil.shouldWaitToProcess = function(
    lastFocusTimestamp, firstTimestamp, currentTime) {
  var timeSinceFocusEvent = currentTime - lastFocusTimestamp;
  var timeSinceFirstEvent = currentTime - firstTimestamp;
  return timeSinceFocusEvent < cvox.ChromeVoxEventWatcher.WAIT_TIME_MS_ &&
      timeSinceFirstEvent < cvox.ChromeVoxEventWatcher.MAX_WAIT_TIME_MS_;
};


/**
 * Returns the queue mode to use for the next utterance spoken as
 * a result of an event or navigation. The first utterance that's spoken
 * after an explicit user action like a key press will flush, and
 * subsequent events will return a category flush.
 * @return {cvox.QueueMode} The queue mode.
 * @private
 */
cvox.ChromeVoxEventWatcher.queueMode_ = function() {
  if (cvox.ChromeVoxEventWatcher.shouldFlushNextUtterance) {
    cvox.ChromeVoxEventWatcher.shouldFlushNextUtterance = false;
    return cvox.QueueMode.FLUSH;
  }
  return cvox.QueueMode.CATEGORY_FLUSH;
};


/**
 * Processes the events queue.
 *
 * @private
 */
cvox.ChromeVoxEventWatcher.processQueue_ = function() {
  cvox.Memoize.scope(cvox.ChromeVoxEventWatcher.doProcessQueue_);
};

/**
 * Implementation of |processQueue_|.
 *
 * @private
 */
cvox.ChromeVoxEventWatcher.doProcessQueue_ = function() {
  // Return now if there are no events in the queue.
  if (cvox.ChromeVoxEventWatcher.events_.length == 0) {
    return;
  }

  // Look for the most recent focus event and delete any preceding event
  // that applied to whatever was focused previously.
  var events = cvox.ChromeVoxEventWatcher.events_;
  var lastFocusIndex = -1;
  var lastFocusTimestamp = 0;
  var evt;
  var i;
  for (i = 0; evt = events[i]; i++) {
    if (evt.type == 'focus') {
      lastFocusIndex = i;
      lastFocusTimestamp = evt.timeStamp;
    }
  }
  cvox.ChromeVoxEventWatcher.events_ = [];
  for (i = 0; evt = events[i]; i++) {
    var prevEvt = events[i - 1] || {};
    if ((i >= lastFocusIndex || evt.type == 'LiveRegion') &&
        (prevEvt.type != 'focus' || evt.type != 'change')) {
      cvox.ChromeVoxEventWatcher.events_.push(evt);
    }
  }

  cvox.ChromeVoxEventWatcher.events_.sort(function(a, b) {
    if (b.type != 'LiveRegion' && a.type == 'LiveRegion') {
      return 1;
    }
    return -1;
  });

  // If the most recent focus event was very recent, wait for things to
  // settle down before processing events, unless the max wait time has
  // passed.
  var currentTime = new Date().getTime();
  if (lastFocusIndex >= 0 &&
      cvox.ChromeVoxEventWatcherUtil.shouldWaitToProcess(
          lastFocusTimestamp,
          cvox.ChromeVoxEventWatcher.firstUnprocessedEventTime,
          currentTime)) {
    window.setTimeout(cvox.ChromeVoxEventWatcher.processQueue_,
                      cvox.ChromeVoxEventWatcher.WAIT_TIME_MS_);
    return;
  }

  // Process the remaining events in the queue, in order.
  for (i = 0; evt = cvox.ChromeVoxEventWatcher.events_[i]; i++) {
    cvox.ChromeVoxEventWatcher.handleEvent_(evt);
  }
  cvox.ChromeVoxEventWatcher.events_ = new Array();
  cvox.ChromeVoxEventWatcher.firstUnprocessedEventTime = -1;
  cvox.ChromeVoxEventWatcher.queueProcessingScheduled_ = false;
  cvox.ChromeVoxEventWatcher.maybeCallReadyCallbacks_();
};

/**
 * Handle events from the queue by routing them to their respective handlers.
 *
 * @private
 * @param {Event} evt The event to be handled.
 */
cvox.ChromeVoxEventWatcher.handleEvent_ = function(evt) {
  switch (evt.type) {
    case 'keydown':
    case 'input':
      cvox.ChromeVoxEventWatcher.setUpTextHandler();
      if (cvox.ChromeVoxEventWatcher.currentTextControl) {
        cvox.ChromeVoxEventWatcher.handleTextChanged(true);

        var editableText = /** @type {cvox.ChromeVoxEditableTextBase} */
            (cvox.ChromeVoxEventWatcher.currentTextHandler);
        if (editableText && editableText.lastChangeDescribed) {
          break;
        }
      }
      // We're either not on a text control, or we are on a text control but no
      // text change was described. Let's try describing the state instead.
      cvox.ChromeVoxEventWatcher.handleControlChanged(document.activeElement);
      break;
    case 'keyup':
      // Some controls change only after key up.
      cvox.ChromeVoxEventWatcher.handleControlChanged(document.activeElement);
      break;
    case 'keypress':
      cvox.ChromeVoxEventWatcher.setUpTextHandler();
      break;
    case 'click':
      cvox.ApiImplementation.syncToNode(/** @type {Node} */(evt.target), true);
      break;
    case 'focus':
      cvox.ChromeVoxEventWatcher.focusHandler(evt);
      break;
    case 'blur':
      cvox.ChromeVoxEventWatcher.setUpTextHandler();
      break;
    case 'change':
      cvox.ChromeVoxEventWatcher.changeHandler(evt);
      break;
    case 'select':
      cvox.ChromeVoxEventWatcher.setUpTextHandler();
      break;
    case 'LiveRegion':
      cvox.ChromeVoxEventWatcher.speakLiveRegion_(
          evt.assertive, evt.navDescriptions);
      break;
  }
};


/**
 * Sets up the time handler.
 * @return {boolean} True if a time control has focus.
 * @private
 */
cvox.ChromeVoxEventWatcher.setUpTimeHandler_ = function() {
  var currentFocus = document.activeElement;
  if (currentFocus &&
      currentFocus.hasAttribute &&
      currentFocus.getAttribute('aria-hidden') == 'true' &&
      currentFocus.getAttribute('chromevoxignoreariahidden') != 'true') {
    currentFocus = null;
  }
  if (currentFocus.constructor == HTMLInputElement &&
      currentFocus.type && (currentFocus.type == 'time')) {
    cvox.ChromeVoxEventWatcher.currentTimeHandler =
        new cvox.ChromeVoxHTMLTimeWidget(currentFocus, cvox.ChromeVox.tts);
    } else {
      cvox.ChromeVoxEventWatcher.currentTimeHandler = null;
    }
  return (null != cvox.ChromeVoxEventWatcher.currentTimeHandler);
};


/**
 * Sets up the media (video/audio) handler.
 * @return {boolean} True if a media control has focus.
 * @private
 */
cvox.ChromeVoxEventWatcher.setUpMediaHandler_ = function() {
  var currentFocus = document.activeElement;
  if (currentFocus &&
      currentFocus.hasAttribute &&
      currentFocus.getAttribute('aria-hidden') == 'true' &&
      currentFocus.getAttribute('chromevoxignoreariahidden') != 'true') {
    currentFocus = null;
  }
  if ((currentFocus.constructor == HTMLVideoElement) ||
      (currentFocus.constructor == HTMLAudioElement)) {
    cvox.ChromeVoxEventWatcher.currentMediaHandler =
        new cvox.ChromeVoxHTMLMediaWidget(currentFocus, cvox.ChromeVox.tts);
    } else {
      cvox.ChromeVoxEventWatcher.currentMediaHandler = null;
    }
  return (null != cvox.ChromeVoxEventWatcher.currentMediaHandler);
};

/**
 * Sets up the date handler.
 * @return {boolean} True if a date control has focus.
 * @private
 */
cvox.ChromeVoxEventWatcher.setUpDateHandler_ = function() {
  var currentFocus = document.activeElement;
  if (currentFocus &&
      currentFocus.hasAttribute &&
      currentFocus.getAttribute('aria-hidden') == 'true' &&
      currentFocus.getAttribute('chromevoxignoreariahidden') != 'true') {
    currentFocus = null;
  }
  if (currentFocus.constructor == HTMLInputElement &&
      currentFocus.type &&
      ((currentFocus.type == 'date') ||
      (currentFocus.type == 'month') ||
      (currentFocus.type == 'week'))) {
    cvox.ChromeVoxEventWatcher.currentDateHandler =
        new cvox.ChromeVoxHTMLDateWidget(currentFocus, cvox.ChromeVox.tts);
    } else {
      cvox.ChromeVoxEventWatcher.currentDateHandler = null;
    }
  return (null != cvox.ChromeVoxEventWatcher.currentDateHandler);
};
