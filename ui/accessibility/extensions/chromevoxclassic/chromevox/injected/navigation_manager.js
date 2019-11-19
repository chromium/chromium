// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Manages navigation within a page.
 * This unifies navigation by the DOM walker and by WebKit selection.
 * NOTE: the purpose of this class is only to hold state
 * and delegate all of its functionality to mostly stateless classes that
 * are easy to test.
 *
 */


goog.provide('cvox.NavigationManager');

goog.require('cvox.ActiveIndicator');
goog.require('cvox.ChromeVox');
goog.require('cvox.ChromeVoxEventSuspender');
goog.require('cvox.CursorSelection');
goog.require('cvox.DescriptionUtil');
goog.require('cvox.DomUtil');
goog.require('cvox.FindUtil');
goog.require('cvox.Focuser');
goog.require('cvox.Interframe');
goog.require('cvox.MathShifter');
goog.require('cvox.NavBraille');
goog.require('cvox.NavDescription');
goog.require('cvox.NavigationHistory');
goog.require('cvox.NavigationShifter');
goog.require('cvox.NavigationSpeaker');
goog.require('cvox.PageSelection');
goog.require('cvox.SelectionUtil');
goog.require('cvox.TableShifter');
goog.require('cvox.TraverseMath');
goog.require('cvox.Widget');


/**
 * @constructor
 */
cvox.NavigationManager = function() {
  this.addInterframeListener_();

  this.reset();

  this.iframeRetries_ = 0;
};

/**
 * Stores state variables in a provided object.
 *
 * @param {Object} store The object.
 */
cvox.NavigationManager.prototype.storeOn = function(store) {
  store['reversed'] = this.isReversed();
  store['keepReading'] = this.keepReading_;
  store['findNext'] = this.predicate_;
  this.shifter_.storeOn(store);
};

/**
 * Updates the object with state variables from an earlier storeOn call.
 *
 * @param {Object} store The object.
 */
cvox.NavigationManager.prototype.readFrom = function(store) {
  this.curSel_.setReversed(store['reversed']);
  this.shifter_.readFrom(store);
  this.keepReading_ = store['keepReading'];
};

/**
 * Resets the navigation manager to the top of the page.
 */
cvox.NavigationManager.prototype.reset = function() {
  /**
   * @type {!cvox.NavigationSpeaker}
   * @private
   */
  this.navSpeaker_ = new cvox.NavigationSpeaker();

  /**
   * @type {!Array<Object>}
   * @private
   */
  this.shifterTypes_ = [cvox.NavigationShifter,
                        cvox.TableShifter,
                        cvox.MathShifter];

  /**
   * @type {!Array<!cvox.AbstractShifter>}
  */
  this.shifterStack_ = [];

  /**
   * The active shifter.
   * @type {!cvox.AbstractShifter}
   * @private
  */
  this.shifter_ = new cvox.NavigationShifter();

  // NOTE(deboer): document.activeElement can not be null (c.f.
  // https://developer.mozilla.org/en-US/docs/DOM/document.activeElement)
  // Instead, if there is no active element, activeElement is set to
  // document.body.
  /**
   * If there is an activeElement, use it.  Otherwise, sync to the page
   * beginning.
   * @type {!cvox.CursorSelection}
   * @private
   */
  this.curSel_ = document.activeElement != document.body ?
      /** @type {!cvox.CursorSelection} */
      (cvox.CursorSelection.fromNode(document.activeElement)) :
      this.shifter_.begin(this.curSel_, {reversed: false});

  /**
   * @type {!cvox.CursorSelection}
   * @private
   */
  this.prevSel_ = this.curSel_.clone();

  /**
   * Keeps track of whether we have skipped while "reading from here"
   * so that we can insert an earcon.
   * @type {boolean}
   * @private
   */
  this.skipped_ = false;

  /**
   * Keeps track of whether we have recovered from dropped focus
   * so that we can insert an earcon.
   * @type {boolean}
   * @private
   */
  this.recovered_ = false;

  /**
   * True if in "reading from here" mode.
   * @type {boolean}
   * @private
   */
  this.keepReading_ = false;

  /**
   * True if we are at the end of the page and we wrap around.
   * @type {boolean}
   * @private
   */
  this.pageEnd_ = false;

  /**
   * True if we have already announced that we will wrap around.
   * @type {boolean}
   * @private
   */
  this.pageEndAnnounced_ = false;

  /**
   * True if we entered into a shifter.
   * @type {boolean}
   * @private
   */
  this.enteredShifter_ = false;

  /**
   * True if we exited a shifter.
   * @type {boolean}
   * @private
   */
  this.exitedShifter_ = false;

  /**
   * True if we want to ignore iframes no matter what.
   * @type {boolean}
   * @private
   */
  this.ignoreIframesNoMatterWhat_ = false;

  /**
   * @type {cvox.PageSelection}
   * @private
   */
  this.pageSel_ = null;

  /** @type {string} */
  this.predicate_ = '';

  /** @type {cvox.CursorSelection} */
  this.saveSel_ = null;

  // TODO(stoarca): This seems goofy. Why are we doing this?
  if (this.activeIndicator) {
    this.activeIndicator.removeFromDom();
  }
  this.activeIndicator = new cvox.ActiveIndicator();

  /**
   * Makes sure focus doesn't get lost.
   * @type {!cvox.NavigationHistory}
   * @private
   */
  this.navigationHistory_ = new cvox.NavigationHistory();

  /** @type {boolean} */
  this.focusRecovery_ = window.location.protocol != 'chrome:';

  this.iframeIdMap = {};
  this.nextIframeId = 1;

  // Only sync if the activeElement is not document.body; which is shorthand for
  // 'no selection'.  Currently the walkers don't deal with the no selection
  // case -- and it is not clear that they should.
  if (document.activeElement != document.body) {
    this.sync();
  }

  // This object is effectively empty when no math is in the page.
  cvox.TraverseMath.getInstance();
};


/**
 * Determines if we are navigating from a valid node. If not, ask navigation
 * history for an acceptable restart point and go there.
 * @param {function(Node)=} opt_predicate A function that takes in a node and
 *     returns true if it is a valid recovery candidate.
 * @return {boolean} True if we should continue navigation normally.
 */
cvox.NavigationManager.prototype.resolve = function(opt_predicate) {
  if (!this.getFocusRecovery()) {
    return true;
  }

  var current = this.getCurrentNode();

  if (!this.navigationHistory_.becomeInvalid(current)) {
    return true;
  }

  // Only attempt to revert if going next will cause us to restart at the top
  // of the page.
  if (this.hasNext_()) {
    return true;
  }

  // Our current node was invalid. Revert to history.
  var revert = this.navigationHistory_.revert(opt_predicate);

  // If the history is empty, revert.current will be null.  In that case,
  // it is best to continue navigating normally.
  if (!revert.current) {
    return true;
  }

  // Convert to selections.
  var newSel = cvox.CursorSelection.fromNode(revert.current);
  var context = cvox.CursorSelection.fromNode(revert.previous);

  // Default to document body if selections are null.
  newSel = newSel || cvox.CursorSelection.fromBody();
  context = context || cvox.CursorSelection.fromBody();
  newSel.setReversed(this.isReversed());

  this.updateSel(newSel, context);
  this.recovered_ = true;
  return false;
};


/**
 * Gets the state of focus recovery.
 * @return {boolean} True if focus recovery is on; false otherwise.
 */
cvox.NavigationManager.prototype.getFocusRecovery = function() {
  return this.focusRecovery_;
};


/**
 * Enables or disables focus recovery.
 * @param {boolean} value True to enable, false to disable.
 */
cvox.NavigationManager.prototype.setFocusRecovery = function(value) {
  this.focusRecovery_ = value;
};


/**
 * Delegates to NavigationShifter with current page state.
 * @param {boolean=} iframes Jump in and out of iframes if true. Default false.
 * @return {boolean} False if end of document has been reached.
 * @private
 */
cvox.NavigationManager.prototype.next_ = function(iframes) {
  if (this.tryBoundaries_(this.shifter_.next(this.curSel_), iframes)) {
    // TODO(dtseng): An observer interface would help to keep logic like this
    // to a minimum.
    this.pageSel_ && this.pageSel_.extend(this.curSel_);
    return true;
  }
  return false;
};

/**
 * Looks ahead to see if it is possible to navigate forward from the current
 * position.
 * @return {boolean} True if it is possible to navigate forward.
 * @private
 */
cvox.NavigationManager.prototype.hasNext_ = function() {
  // Non-default shifters validly end before page end.
  if (this.shifterStack_.length > 0) {
    return true;
  }
  var dummySel = this.curSel_.clone();
  var result = false;
  var dummyNavShifter = new cvox.NavigationShifter();
  dummyNavShifter.setGranularity(this.shifter_.getGranularity());
  dummyNavShifter.sync(dummySel);
  if (dummyNavShifter.next(dummySel)) {
    result = true;
  }
  return result;
};


/**
 * Delegates to NavigationShifter with current page state.
 * @param {function(Array<Node>)} predicate A function taking an array
 *     of unique ancestor nodes as a parameter and returning a desired node.
 *     It returns null if that node can't be found.
 * @param {string=} opt_predicateName The programmatic name that exists in
 * cvox.DomPredicates. Used to dispatch calls across iframes since functions
 * cannot be stringified.
 * @param {boolean=} opt_initialNode Whether to start the search from node
 * (true), or the next node (false); defaults to false.
 * @return {cvox.CursorSelection} The newly found selection.
 */
cvox.NavigationManager.prototype.findNext = function(
    predicate, opt_predicateName, opt_initialNode) {
  this.predicate_ = opt_predicateName || '';
  this.resolve();
  this.shifter_ = this.shifterStack_[0] || this.shifter_;
  this.shifterStack_ = [];
  var ret = cvox.FindUtil.findNext(this.curSel_, predicate, opt_initialNode);
  if (!this.ignoreIframesNoMatterWhat_) {
    this.tryIframe_(ret && ret.start.node);
  }
  if (ret) {
    this.updateSelToArbitraryNode(ret.start.node);
  }
  this.predicate_ = '';
  return ret;
};


/**
 * Delegates to NavigationShifter with current page state.
 */
cvox.NavigationManager.prototype.sync = function() {
  this.resolve();
  var ret = this.shifter_.sync(this.curSel_);
  if (ret) {
    this.curSel_ = ret;
  }
};

/**
 * Sync's all possible cursors:
 * - focus
 * - ActiveIndicator
 * - CursorSelection
 * @param {boolean=} opt_skipText Skips focus on text nodes; defaults to false.
 */
cvox.NavigationManager.prototype.syncAll = function(opt_skipText) {
  this.sync();
  this.setFocus(opt_skipText);
  this.updateIndicator();
};


/**
 * Clears a DOM selection made via a CursorSelection.
 * @param {boolean=} opt_announce True to announce the clearing.
 * @return {boolean} If a selection was cleared.
 */
cvox.NavigationManager.prototype.clearPageSel = function(opt_announce) {
  var hasSel = !!this.pageSel_;
  if (hasSel && opt_announce) {
    var announcement = Msgs.getMsg('clear_page_selection');
    cvox.ChromeVox.tts.speak(announcement, cvox.QueueMode.FLUSH,
                             cvox.AbstractTts.PERSONALITY_ANNOTATION);
  }
  this.pageSel_ = null;
  return hasSel;
};


/**
 * Begins or finishes a DOM selection at the current CursorSelection in the
 * document.
 * @return {boolean} Whether selection is on or off after this call.
 */
cvox.NavigationManager.prototype.togglePageSel = function() {
  this.pageSel_ = this.pageSel_ ? null :
      new cvox.PageSelection(this.curSel_.setReversed(false));
  return !!this.pageSel_;
};


// TODO(stoarca): getDiscription is split awkwardly between here and the
// walkers. The walkers should have getBaseDescription() which requires
// very little context, and then this method should tack on everything
// which requires any extensive knowledge.
/**
 * Delegates to NavigationShifter with the current page state.
 * @return {Array<cvox.NavDescription>} The summary of the current position.
 */
cvox.NavigationManager.prototype.getDescription = function() {
  // Handle description of special content. Consider moving to DescriptionUtil.
  // Specially annotated nodes.
  if (this.getCurrentNode().hasAttribute &&
      this.getCurrentNode().hasAttribute('cvoxnodedesc')) {
    var preDesc = cvox.ChromeVoxJSON.parse(
        this.getCurrentNode().getAttribute('cvoxnodedesc'));
    var currentDesc = new Array();
    for (var i = 0; i < preDesc.length; ++i) {
      var inDesc = preDesc[i];
      // TODO: this can probably be replaced with just NavDescription(inDesc)
      // need test case to ensure this change will work
      currentDesc.push(new cvox.NavDescription({
        context: inDesc.context,
        text: inDesc.text,
        userValue: inDesc.userValue,
        annotation: inDesc.annotation
      }));
    }
    return currentDesc;
  }

  // Selected content.
  var desc = this.pageSel_ ? this.pageSel_.getDescription(
          this.shifter_, this.prevSel_, this.curSel_) :
      this.shifter_.getDescription(this.prevSel_, this.curSel_);
  var earcons = [];

  // Earcons.
  if (this.skipped_) {
    earcons.push(cvox.Earcon.SKIP);
    this.skipped_ = false;
  }
  if (this.recovered_) {
    earcons.push(cvox.Earcon.RECOVER_FOCUS);
    this.recovered_ = false;
  }
  if (this.pageEnd_) {
    earcons.push(cvox.Earcon.WRAP);
    this.pageEnd_ = false;
  }
  if (this.enteredShifter_) {
    earcons.push(cvox.Earcon.OBJECT_ENTER);
    this.enteredShifter_ = false;
  }
  if (this.exitedShifter_) {
    earcons.push(cvox.Earcon.OBJECT_EXIT);
    this.exitedShifter_ = false;
  }
  if (earcons.length > 0 && desc.length > 0) {
    earcons.forEach(function(earcon) {
        desc[0].pushEarcon(earcon);
    });
  }
  return desc;
};


/**
 * Delegates to NavigationShifter with the current page state.
 * @return {!cvox.NavBraille} The braille description.
 */
cvox.NavigationManager.prototype.getBraille = function() {
  return this.shifter_.getBraille(this.prevSel_, this.curSel_);
};

/**
 * Delegates an action to the current walker.
 * @param {string} name Action name.
 * @return {boolean} True if action performed.
 */
cvox.NavigationManager.prototype.performAction = function(name) {
  var newSel = null;
  switch (name) {
    case 'enterShifter':
    case 'enterShifterSilently':
      for (var i = this.shifterTypes_.length - 1, shifterType;
           shifterType = this.shifterTypes_[i];
           i--) {
        var shifter = shifterType.create(this.curSel_);
        if (shifter && shifter.getName() != this.shifter_.getName()) {
          this.shifterStack_.push(this.shifter_);
          this.shifter_ = shifter;
          this.sync();
          this.enteredShifter_ = name != 'enterShifterSilently';
          break;
        } else if (shifter && this.shifter_.getName() == shifter.getName()) {
          break;
        }
      }
      break;
    case 'exitShifter':
      if (this.shifterStack_.length == 0) {
        return false;
      }
      this.shifter_ = this.shifterStack_.pop();
      this.sync();
      this.exitedShifter_ = true;
      break;
    case 'exitShifterContent':
      if (this.shifterStack_.length == 0) {
        return false;
      }
      this.updateSel(this.shifter_.performAction(name, this.curSel_));
      this.shifter_ = this.shifterStack_.pop() || this.shifter_;
      this.sync();
      this.exitedShifter_ = true;
      break;
      default:
        if (this.shifter_.hasAction(name)) {
          return this.updateSel(
              this.shifter_.performAction(name, this.curSel_));
        } else {
          return false;
        }
    }
  return true;
};


/**
 * Returns the current navigation strategy.
 *
 * @return {string} The name of the strategy used.
 */
cvox.NavigationManager.prototype.getGranularityMsg = function() {
  return this.shifter_.getGranularityMsg();
};


/**
 * Delegates to NavigationShifter.
 * @param {boolean=} opt_persist Persist the granularity to all running tabs;
 * defaults to true.
 */
cvox.NavigationManager.prototype.makeMoreGranular = function(opt_persist) {
  this.shifter_.makeMoreGranular();
  this.sync();
  this.persistGranularity_(opt_persist);
};


/**
 * Delegates to current shifter.
 * @param {boolean=} opt_persist Persist the granularity to all running tabs;
 * defaults to true.
 */
cvox.NavigationManager.prototype.makeLessGranular = function(opt_persist) {
  this.shifter_.makeLessGranular();
  this.sync();
  this.persistGranularity_(opt_persist);
};


/**
 * Delegates to navigation shifter. Behavior is not defined if granularity
 * was not previously gotten from a call to getGranularity(). This method is
 * only supported by NavigationShifter which exposes a random access
 * iterator-like interface. The caller has the option to force granularity
  which results in exiting any entered shifters. If not forced, and there has
 * been a shifter entered, setting granularity is a no-op.
 * @param {number} granularity The desired granularity.
 * @param {boolean=} opt_force Forces current shifter to NavigationShifter;
 * false by default.
 * @param {boolean=} opt_persist Persists setting to all running tabs; defaults
 * to false.
 */
cvox.NavigationManager.prototype.setGranularity = function(
    granularity, opt_force, opt_persist) {
  if (!opt_force && this.shifterStack_.length > 0) {
    return;
  }
  this.shifter_ = this.shifterStack_.shift() || this.shifter_;
  this.shifters_ = [];
  this.shifter_.setGranularity(granularity);
  this.persistGranularity_(opt_persist);
};


/**
 * Delegates to NavigationShifter.
 * @return {number} The current granularity.
 */
cvox.NavigationManager.prototype.getGranularity = function() {
  var shifter = this.shifterStack_[0] || this.shifter_;
  return shifter.getGranularity();
};


/**
 * Delegates to NavigationShifter.
 */
cvox.NavigationManager.prototype.ensureSubnavigating = function() {
  if (!this.shifter_.isSubnavigating()) {
    this.shifter_.ensureSubnavigating();
    this.sync();
  }
};


/**
 * Stops subnavigating, specifying that we should navigate at a less granular
 * level than the current navigation strategy.
 */
cvox.NavigationManager.prototype.ensureNotSubnavigating = function() {
  if (this.shifter_.isSubnavigating()) {
    this.shifter_.ensureNotSubnavigating();
    this.sync();
  }
};


/**
 * Delegates to NavigationSpeaker.
 * @param {Array<cvox.NavDescription>} descriptionArray The array of
 *     NavDescriptions to speak.
 * @param {cvox.QueueMode} initialQueueMode The initial queue mode.
 * @param {Function} completionFunction Function to call when finished speaking.
 * @param {Object=} opt_personality Optional personality for all descriptions.
 * @param {string=} opt_category Optional category for all descriptions.
 */
cvox.NavigationManager.prototype.speakDescriptionArray = function(
    descriptionArray,
    initialQueueMode,
    completionFunction,
    opt_personality,
    opt_category) {
  if (opt_personality) {
    descriptionArray.forEach(function(desc) {
      if (!desc.personality) {
        desc.personality = opt_personality;
      }
    });
  }
  if (opt_category) {
    descriptionArray.forEach(function(desc) {
      if (!desc.category) {
        desc.category = opt_category;
      }
    });
  }

  this.navSpeaker_.speakDescriptionArray(
      descriptionArray, initialQueueMode, completionFunction);
};

/**
 * Add the position of the node on the page.
 * @param {Node} node The node that ChromeVox should update the position.
 */
cvox.NavigationManager.prototype.updatePosition = function(node) {
  var msg = cvox.ChromeVox.position;
  msg[document.location.href] =
      cvox.DomUtil.elementToPoint(node);

  cvox.ChromeVox.host.sendToBackgroundPage({
    'target': 'Prefs',
    'action': 'setPref',
    'pref': 'position',
    'value': JSON.stringify(msg)
  });
};


// TODO(stoarca): The stuff below belongs in its own layer.
/**
 * Perform all of the actions that should happen at the end of any
 * navigation operation: update the lens, play earcons, and speak the
 * description of the object that was navigated to.
 *
 * @param {string=} opt_prefix The string to be prepended to what
 * is spoken to the user.
 * @param {boolean=} opt_setFocus Whether or not to focus the current node.
 * Defaults to true.
 * @param {cvox.QueueMode=} opt_queueMode Initial queue mode to use.
 * @param {function(): ?=} opt_callback Function to call after speaking.
 */
cvox.NavigationManager.prototype.finishNavCommand = function(
    opt_prefix, opt_setFocus, opt_queueMode, opt_callback) {
  if (this.pageEnd_ && !this.pageEndAnnounced_) {
    this.pageEndAnnounced_ = true;
    cvox.ChromeVox.tts.stop();
    cvox.ChromeVox.earcons.playEarcon(cvox.Earcon.WRAP);
    if (cvox.ChromeVox.verbosity === cvox.VERBOSITY_VERBOSE) {
      var msg = Msgs.getMsg('wrapped_to_top');
      if (this.isReversed()) {
        msg = Msgs.getMsg('wrapped_to_bottom');
      }
      cvox.ChromeVox.tts.speak(msg, cvox.QueueMode.QUEUE,
          cvox.AbstractTts.PERSONALITY_ANNOTATION);
    }
    return;
  }

  if (this.enteredShifter_ || this.exitedShifter_) {
    opt_prefix = Msgs.getMsg('enter_content_say', [this.shifter_.getName()]);
  }

  var descriptionArray = cvox.ChromeVox.navigationManager.getDescription();

  opt_setFocus = opt_setFocus === undefined ? true : opt_setFocus;

  if (opt_setFocus) {
    this.setFocus();
  }
  this.updateIndicator();

  var queueMode = opt_queueMode || cvox.QueueMode.FLUSH;

  if (opt_prefix) {
    cvox.ChromeVox.tts.speak(
        opt_prefix, queueMode, cvox.AbstractTts.PERSONALITY_ANNOTATION);
    queueMode = cvox.QueueMode.QUEUE;
  }
  this.speakDescriptionArray(descriptionArray,
                             queueMode,
                             opt_callback || null,
                             null,
                             cvox.TtsCategory.NAV);

  cvox.ChromeVox.braille.write(this.getBraille());

  this.updatePosition(this.getCurrentNode());
};


/**
 * Moves forward. Stops any subnavigation.
 * @param {boolean=} opt_ignoreIframes Ignore iframes when navigating. Defaults
 * to not ignore iframes.
 * @param {number=} opt_granularity Optionally, switches to granularity before
 * navigation.
 * @return {boolean} False if end of document reached.
 */
cvox.NavigationManager.prototype.navigate = function(
    opt_ignoreIframes, opt_granularity) {
  this.pageEndAnnounced_ = false;
  if (this.pageEnd_) {
    this.pageEnd_ = false;
    this.syncToBeginning(opt_ignoreIframes);
    return true;
  }
  if (!this.resolve()) {
    return false;
  }
  this.ensureNotSubnavigating();
  if (opt_granularity !== undefined &&
      (opt_granularity !== this.getGranularity() ||
          this.shifterStack_.length > 0)) {
    this.setGranularity(opt_granularity, true);
    this.sync();
  }
  return this.next_(!opt_ignoreIframes);
};


/**
 * Moves forward after switching to a lower granularity until the next
 * call to navigate().
 */
cvox.NavigationManager.prototype.subnavigate = function() {
  this.pageEndAnnounced_ = false;
  if (!this.resolve()) {
    return;
  }
  this.ensureSubnavigating();
  this.next_(true);
};


/**
 * Moves forward. Starts reading the page from that node.
 * Uses QUEUE_MODE_FLUSH to flush any previous speech.
 * @return {boolean} False if not "reading from here". True otherwise.
 */
cvox.NavigationManager.prototype.skip = function() {
  if (!this.keepReading_) {
    return false;
  }
  if (cvox.ChromeVox.host.hasTtsCallback()) {
    this.skipped_ = true;
    this.setReversed(false);
    this.startCallbackReading_(cvox.QueueMode.FLUSH);
  }
  return true;
};


/**
 * Starts reading the page from the current selection.
 * @param {cvox.QueueMode} queueMode Either flush or queue.
 */
cvox.NavigationManager.prototype.startReading = function(queueMode) {
  this.keepReading_ = true;
  if (cvox.ChromeVox.host.hasTtsCallback()) {
    this.startCallbackReading_(queueMode);
  } else {
    this.startNonCallbackReading_(queueMode);
  }
  cvox.ChromeVox.stickyOverride = true;
};

/**
 * Stops continuous read.
 * @param {boolean} stopTtsImmediately True if the TTS should immediately stop
 * speaking.
 */
cvox.NavigationManager.prototype.stopReading = function(stopTtsImmediately) {
  this.keepReading_ = false;
  this.navSpeaker_.stopReading = true;
  if (stopTtsImmediately) {
    cvox.ChromeVox.tts.stop();
  }
  cvox.ChromeVox.stickyOverride = null;
};


/**
 * The current current state of continuous read.
 * @return {boolean} The state.
 */
cvox.NavigationManager.prototype.isReading = function() {
  return this.keepReading_;
};


/**
 * Starts reading the page from the current selection if there are callbacks.
 * @param {cvox.QueueMode} queueMode Either flush or queue.
 * @private
 */
cvox.NavigationManager.prototype.startCallbackReading_ =
    cvox.ChromeVoxEventSuspender.withSuspendedEvents(function(queueMode) {
  this.finishNavCommand('', true, queueMode, goog.bind(function() {
    if (this.prevReadingSel_ == this.curSel_) {
      this.stopReading();
      return;
    }
    this.prevReadingSel_ = this.curSel_;
    if (this.next_(true) && this.keepReading_) {
      this.startCallbackReading_(cvox.QueueMode.QUEUE);
    }
  }, this));
});


/**
 * Starts reading the page from the current selection if there are no callbacks.
 * With this method, we poll the keepReading_ var and stop when it is false.
 * @param {cvox.QueueMode} queueMode Either flush or queue.
 * @private
 */
cvox.NavigationManager.prototype.startNonCallbackReading_ =
    cvox.ChromeVoxEventSuspender.withSuspendedEvents(function(queueMode) {
  if (!this.keepReading_) {
    return;
  }

  if (!cvox.ChromeVox.tts.isSpeaking()) {
    this.finishNavCommand('', true, queueMode, null);
    if (!this.next_(true)) {
      this.keepReading_ = false;
    }
  }
  window.setTimeout(goog.bind(this.startNonCallbackReading_, this), 1000);
});


/**
 * Returns a complete description of the current position, including
 * the text content and annotations such as "link", "button", etc.
 * Unlike getDescription, this does not shorten the position based on the
 * previous position.
 *
 * @return {Array<cvox.NavDescription>} The summary of the current position.
 */
cvox.NavigationManager.prototype.getFullDescription = function() {
  if (this.pageSel_) {
    return this.pageSel_.getFullDescription();
  }
  return [cvox.DescriptionUtil.getDescriptionFromAncestors(
      cvox.DomUtil.getAncestors(this.curSel_.start.node),
      true,
      cvox.ChromeVox.verbosity)];
};


/**
 * Sets the browser's focus to the current node.
 * @param {boolean=} opt_skipText Skips focusing text nodes or any of their
 * ancestors; defaults to false.
 */
cvox.NavigationManager.prototype.setFocus = function(opt_skipText) {
  // TODO(dtseng): cvox.DomUtil.setFocus() totally destroys DOM ranges that have
  // been set on the page; this requires further investigation, but
  // PageSelection won't work without this.
  if (this.pageSel_ ||
      (opt_skipText && this.curSel_.start.node.constructor == Text)) {
    return;
  }
  cvox.Focuser.setFocus(this.curSel_.start.node);
};


/**
 * Returns the node of the directed start of the selection.
 * @return {Node} The current node.
 */
cvox.NavigationManager.prototype.getCurrentNode = function() {
  return this.curSel_.absStart().node;
};


/**
 * Listen to messages from other frames and respond to messages that
 * tell our frame to take focus and preseve the navigation granularity
 * from the other frame.
 * @private
 */
cvox.NavigationManager.prototype.addInterframeListener_ = function() {
  /**
   * @type {!cvox.NavigationManager}
   */
  var self = this;

  cvox.Interframe.addListener(function(message) {
    if (message['command'] != 'enterIframe' &&
        message['command'] != 'exitIframe') {
      return;
    }
    cvox.ChromeVox.serializer.readFrom(message);

    cvox.ChromeVoxEventSuspender.withSuspendedEvents(function() {
      window.focus();
      if (message['findNext']) {
        var predicateName = message['findNext'];
        var predicate = cvox.DomPredicates[predicateName];
        var found = self.findNext(predicate, predicateName, true);
        if (predicate && (!found || found.start.node.tagName == 'IFRAME')) {
          return;
        }
      } else if (message['command'] == 'exitIframe') {
        var id = message['sourceId'];
        var iframeElement = self.iframeIdMap[id];
        var reversed = message['reversed'];
        var granularity = message['granularity'];
        if (iframeElement) {
          self.updateSel(cvox.CursorSelection.fromNode(iframeElement));
        }
        self.setReversed(reversed);
        self.sync();
        self.navigate();
      } else {
        self.syncToBeginning();

        // if we have an empty body, then immediately exit the iframe
        if (!cvox.DomUtil.hasContent(document.body)) {
          self.tryIframe_(null);
          return;
        }
      }

      // Now speak what ended up being selected.
      // TODO(deboer): Some of this could be moved to readFrom
      self.finishNavCommand('', true);
      if (self.keepReading_) {
        self.startReading(cvox.QueueMode.FLUSH);
      }
    })();
  });
};


/**
 * Update the active indicator to reflect the current node or selection.
 */
cvox.NavigationManager.prototype.updateIndicator = function() {
  this.activeIndicator.syncToCursorSelection(this.curSel_);
};


/**
 * Update the active indicator in case the active object moved or was
 * removed from the document.
 */
cvox.NavigationManager.prototype.updateIndicatorIfChanged = function() {
  this.activeIndicator.updateIndicatorIfChanged();
};


/**
 * Show or hide the active indicator based on whether ChromeVox is
 * active or not.
 *
 * If 'active' is true, cvox.NavigationManager does not do anything.
 * However, callers to showOrHideIndicator also need to call updateIndicator
 * to update the indicator -- which also does the work to show the
 * indicator.
 *
 * @param {boolean} active True if we should show the indicator, false
 *     if we should hide the indicator.
 */
cvox.NavigationManager.prototype.showOrHideIndicator = function(active) {
  if (!active) {
    this.activeIndicator.removeFromDom();
  }
};


/**
 * Collapses the selection to directed cursor start.
 */
cvox.NavigationManager.prototype.collapseSelection = function() {
  this.curSel_.collapse();
};


/**
 * This is used to update the selection to arbitrary nodes because there are
 * browser events, cvox API's, and user commands that require selection around a
 * precise node. As a consequence, calling this method will result in a shift to
 * object granularity without explicit user action or feedback. Also, note that
 * this selection will be sync'ed to ObjectWalker by default unless explicitly
 * ttold not to. We assume object walker can describe the node in the latter
 * case.
 * @param {Node} node The node to update to.
 * @param {boolean=} opt_precise Whether selection will sync exactly to the
 * given node. Defaults to false (and selection will sync according to object
 * walker).
 */
cvox.NavigationManager.prototype.updateSelToArbitraryNode = function(
    node, opt_precise) {
  if (node) {
    this.setGranularity(cvox.NavigationShifter.GRANULARITIES.OBJECT, true);
    this.updateSel(cvox.CursorSelection.fromNode(node));
    if (!opt_precise) {
      this.sync();
    }
  } else {
    this.syncToBeginning();
  }
};


/**
 * Updates curSel_ to the new selection and sets prevSel_ to the old curSel_.
 * This should be called exactly when something user-perceivable happens.
 * @param {cvox.CursorSelection} sel The selection to update to.
 * @param {cvox.CursorSelection=} opt_context An optional override for prevSel_.
 * Used to override both curSel_ and prevSel_ when jumping back in nav history.
 * @return {boolean} False if sel is null. True otherwise.
 */
cvox.NavigationManager.prototype.updateSel = function(sel, opt_context) {
  if (sel) {
    this.prevSel_ = opt_context || this.curSel_;
    this.curSel_ = sel;
  }
  // Only update the history if we aren't just trying to peek ahead.
  var currentNode = this.getCurrentNode();
  this.navigationHistory_.update(currentNode);
  return !!sel;
};


/**
 * Sets the direction.
 * @param {!boolean} r True to reverse.
 */
cvox.NavigationManager.prototype.setReversed = function(r) {
  this.curSel_.setReversed(r);
};


/**
 * Returns true if currently reversed.
 * @return {boolean} True if reversed.
 */
cvox.NavigationManager.prototype.isReversed = function() {
  return this.curSel_.isReversed();
};


/**
 * Checks if boundary conditions are met and updates the selection.
 * @param {cvox.CursorSelection} sel The selection.
 * @param {boolean=} iframes If true, tries to enter iframes. Default false.
 * @return {boolean} False if end of page is reached.
 * @private
 */
cvox.NavigationManager.prototype.tryBoundaries_ = function(sel, iframes) {
  iframes = (!!iframes && !this.ignoreIframesNoMatterWhat_) || false;
  this.pageEnd_ = false;
  if (iframes && this.tryIframe_(sel && sel.start.node)) {
    return true;
  }
  if (sel) {
    this.updateSel(sel);
    return true;
  }
  if (this.shifterStack_.length > 0) {
    return true;
  }
  this.syncToBeginning(!iframes);
  this.clearPageSel(true);
  this.stopReading(true);
  this.pageEnd_ = true;
  return false;
};


/**
 * Given a node that we just navigated to, try to jump in and out of iframes
 * as needed. If the node is an iframe, jump into it. If the node is null,
 * assume we reached the end of an iframe and try to jump out of it.
 * @param {Node} node The node to try to jump into.
 * @return {boolean} True if we jumped into an iframe.
 * @private
 */
cvox.NavigationManager.prototype.tryIframe_ = function(node) {
  if (node == null && cvox.Interframe.isIframe()) {
    var message = {
      'command': 'exitIframe',
      'reversed': this.isReversed(),
      'granularity': this.getGranularity()
    };
    cvox.ChromeVox.serializer.storeOn(message);
    cvox.Interframe.sendMessageToParentWindow(message);
    this.keepReading_ = false;
    return true;
  }

  if (node == null || node.tagName != 'IFRAME' || !node.src) {
    return false;
  }
  var iframeElement = /** @type {HTMLIFrameElement} */(node);

  var iframeId = undefined;
  for (var id in this.iframeIdMap) {
    if (this.iframeIdMap[id] == iframeElement) {
      iframeId = id;
      break;
    }
  }
  if (iframeId == undefined) {
    iframeId = this.nextIframeId;
    this.nextIframeId++;
    cvox.Interframe.sendIdToIFrame(iframeId, iframeElement, function() {
      this.iframeIdMap[iframeId] = iframeElement;
      this.iframeRetries_ = 0;
    }.bind(this));
  }

  // We never received an ack from the iframe.
  if (!this.iframeIdMap[iframeId]) {
    this.iframeRetries_++;
    if (this.iframeRetries_ > 5) {
      // Give up.
      this.iframeRetries_ = 0;
      return false;
    }
  }

  var message = {
    'command': 'enterIframe',
    'id': iframeId
  };
  cvox.ChromeVox.serializer.storeOn(message);
  cvox.Interframe.sendMessageToIFrame(message, iframeElement);
  return true;
};


/**
 * Delegates to NavigationShifter. Tries to enter any iframes or tables if
 * requested.
 * @param {boolean=} opt_skipIframe True to skip iframes.
 */
cvox.NavigationManager.prototype.syncToBeginning = function(opt_skipIframe) {
  var ret = this.shifter_.begin(this.curSel_, {
      reversed: this.curSel_.isReversed()
  });
  if (!opt_skipIframe && this.tryIframe_(ret && ret.start.node)) {
    return;
  }
  this.updateSel(ret);
};


/**
 * Used during testing since there are iframes and we don't always want to
 * interact with them so that we can test certain features.
 */
cvox.NavigationManager.prototype.ignoreIframesNoMatterWhat = function() {
  this.ignoreIframesNoMatterWhat_ = true;
};


/**
 * Save a cursor selection during an excursion.
 */
cvox.NavigationManager.prototype.saveSel = function() {
  this.saveSel_ = this.curSel_;
};


/**
 * Save a cursor selection after an excursion.
 */
cvox.NavigationManager.prototype.restoreSel = function() {
  this.curSel_ = this.saveSel_ || this.curSel_;
};


/**
 * @param {boolean=} opt_persist Persist the granularity to all running tabs;
 * defaults to false.
 * @private
 */
cvox.NavigationManager.prototype.persistGranularity_ = function(opt_persist) {
  opt_persist = opt_persist === undefined ? false : opt_persist;
  if (opt_persist) {
    cvox.ChromeVox.host.sendToBackgroundPage({
      'target': 'Prefs',
      'action': 'setPref',
      'pref': 'granularity',
      'value': this.getGranularity()
    });
  }
};
