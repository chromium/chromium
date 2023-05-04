// Copyright 2015 The ChromeOS IME Authors. All Rights Reserved.
// limitations under the License.
// See the License for the specific language governing permissions and
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// distributed under the License is distributed on an "AS-IS" BASIS,
// Unless required by applicable law or agreed to in writing, software
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// You may obtain a copy of the License at
// you may not use this file except in compliance with the License.
// Licensed under the Apache License, Version 2.0 (the "License");
//
goog.provide('i18n.input.chrome.inputview.elements.content.SwipeView');

goog.require('goog.dom.TagName');
goog.require('goog.dom.classlist');
goog.require('goog.events');
goog.require('goog.fx.Transition');
goog.require('goog.fx.dom.FadeOut');
goog.require('goog.fx.dom.PredefinedEffect');
goog.require('goog.fx.easing');
goog.require('goog.log');
goog.require('goog.style');
goog.require('i18n.input.chrome.ElementType');
goog.require('i18n.input.chrome.Statistics');
goog.require('i18n.input.chrome.events.KeyCodes');
goog.require('i18n.input.chrome.inputview.Css');
goog.require('i18n.input.chrome.inputview.SwipeDirection');
goog.require('i18n.input.chrome.inputview.elements.Element');
goog.require('i18n.input.chrome.inputview.events.EventType');
goog.require('i18n.input.chrome.inputview.handler.PointerHandler');
goog.require('i18n.input.chrome.inputview.util');
goog.require('i18n.input.chrome.message.ContextType');

goog.scope(function() {
var CandidateView = i18n.input.chrome.inputview.elements.content.CandidateView;
var ContextType = i18n.input.chrome.message.ContextType;
var Css = i18n.input.chrome.inputview.Css;
var ElementType = i18n.input.chrome.ElementType;
var EventType = i18n.input.chrome.inputview.events.EventType;
var KeyCodes = i18n.input.chrome.events.KeyCodes;
var SwipeDirection = i18n.input.chrome.inputview.SwipeDirection;
var content = i18n.input.chrome.inputview.elements.content;



/**
 * The view used to display the selection and deletion swipe tracks.
 *
 * @param {!i18n.input.chrome.inputview.Adapter} adapter .
 * @param {!CandidateView} candidateView .
 * @param {goog.events.EventTarget=} opt_eventTarget The parent event target.
 * @constructor
 * @extends {i18n.input.chrome.inputview.elements.Element}
 */
i18n.input.chrome.inputview.elements.content.SwipeView = function(
    adapter, candidateView, opt_eventTarget) {
  i18n.input.chrome.inputview.elements.content.SwipeView.base(
      this, 'constructor', '', ElementType.SWIPE_VIEW, opt_eventTarget);

  /**
   * The inputview adapter.
   *
   * @private {!i18n.input.chrome.inputview.Adapter}
   */
  this.adapter_ = adapter;

  /**
   * The candidate view.
   *
   * @private {!CandidateView}
   */
  this.candidateView_ = candidateView;

  /**
   * The swipe elements.
   *
   * @private {!Array<!Element>}
   */
  this.trackElements_ = [];

  /**
   * The text before the current focus.
   *
   * @private {string}
   */
  this.surroundingText_ = '';

  /**
   * The offset position of the surrounding text. This value
   * indicates the absolute position of the first character of surrounding text.
   *
   * @private {number}
   */
  this.surroundingTextOffset_ = 0;

  /**
   * List of recent words that have been deleted in the order that they
   * were deleted.
   *
   * @private {!Array<string>}
   */
  this.deletedWords_ = [];

  /**
   * The pointer handler.
   *
   * @private {!i18n.input.chrome.inputview.handler.PointerHandler}
   */
  this.pointerHandler_ = new i18n.input.chrome.inputview.handler
      .PointerHandler();

  /**
   * The cover element.
   * Note: The reason we use a separate cover element instead of the view is
   * because of the opacity. We can not reassign the opacity in child element.
   *
   * @private {!Element}
   */
  this.coverElement_;

  /**
   * The finger tracker affordance.
   *
   * @private {!Element}
   */
  this.fingerTracker_;

  /**
   * The ripple element.
   *
   * @private {!Element}
   */
  this.ripple_;

  /**
   * The index of the alternative element which is highlighted.
   *
   * @private {number}
   */
  this.highlightIndex_ = SwipeView.INVALID_INDEX_;

  /**
   * The key which triggered this view to be shown.
   *
   * @type {i18n.input.chrome.inputview.elements.content.SoftKey|undefined}
   */
  this.triggeredBy;

  /**
   * Whether finger movement is being tracked.
   *
   * @private {boolean}
   */
  this.tracking_ = false;

  /**
   * Whether to deploy the tracker on swipe events.
   *
   * @private {boolean}
   */
  this.armed_ = false;

  /**
   * Whether to handle swipe events.
   *
   * @type {boolean}
   */
  this.enabled = false;

  /**
   * Whether the current keyset supports swipe editing.
   *
   * @private {boolean}
   */
  this.isKeysetSupported_ = false;

  /**
   * Fade animation for the finger affordance.
   *
   * @private {!goog.fx.dom.PredefinedEffect}
   */
  this.fadeAnimation_;

  /**
   * Scale animation for the finger affordance.
   *
   * @private {!goog.fx.dom.PredefinedEffect}
   */
  this.scaleAnimation_;

  /**
   * The statistics object for recording metrics values.
   *
   * @type {!i18n.input.chrome.Statistics}
   * @private
   */
  this.statistics_ = i18n.input.chrome.Statistics.getInstance();

  /**
   * Whether the current track is selection or deletion.
   *
   * @private {boolean}
   */
  this.isSelection_ = true;

  /**
   * Triggering event identifier.
   *
   * @private {number|undefined}
   */
  this.eventIdentifier_;


  /**
   * Relative surrounding text when the deletion track first shows.
   *
   * @private {!string}
   */
  this.initialSurroundingText_ = '';

  /**
   * Total surrounding text length when the deletion track first shows.
   *
   * @private {!number}
   */
  this.initialSurroundingTextLength_ = 0;

  /**
   * Number of deletion noops.
   *
   * @private {!number}
   */
  this.noopCount_ = 0;

  /**
   * Logger for SwipeView.
   * @private {goog.log.Logger}
   */
  this.logger_ = goog.log.getLogger(
      'i18n.input.chrome.inputview.elements.content.SwipeView');
};
goog.inherits(i18n.input.chrome.inputview.elements.content.SwipeView,
    i18n.input.chrome.inputview.elements.Element);
var SwipeView = i18n.input.chrome.inputview.elements.content.SwipeView;


/**
 * The number of swipe elements to display.
 *
 * @private {number}
 * @const
 */
SwipeView.LENGTH_ = 15;


/**
 * Index representing no swipe element currently being highlighted.
 *
 * @private {number}
 * @const
 */
SwipeView.INVALID_INDEX_ = -1;


/**
 * The maximum distance the users finger can move from the track view without
 * dismissing it.
 *
 * @private {number}
 * @const
 */
SwipeView.FINGER_DISTANCE_TO_CANCEL_SWIPE_ = 100;


/**
 * The width of a regular track segment.
 *
 * @private {number}
 * @const
 */
SwipeView.SEGMENT_WIDTH_ = 40;


/**
 * The string representation of &nbsp.
 *
 * @private {string}
 * @const
 */
SwipeView.NBSP_CHAR_ = String.fromCharCode(160);


/**
 * The width of the finger affordance.
 *
 * @private {number}
 * @const
 */
SwipeView.FINGER_TRACKER_WIDTH_ = 64;


/**
 * The initial width of the ripple.
 *
 * @private {number}
 * @const
 */
SwipeView.RIPPLE_WIDTH_ = 22;


/**
 * The ripple scale animation duration in ms.
 *
 * @private {number}
 * @const
 */
SwipeView.SCALE_ANIMATION_TIME_ = 220;


/**
 * The ripple scale factor.
 *
 * @private {number}
 * @const
 */
SwipeView.RIPPLE_SCALE_FACTOR_ = 24;


/**
 * The ripple fade animation duration in ms.
 *
 * @private {number}
 * @const
 */
SwipeView.FADE_ANIMATION_TIME_ = 200;


/**
 * Tooltips to display in the candidate window during gesture editing.
 *
 * @enum {string}
 */
SwipeView.Tooltip = {
  SELECTION: chrome.i18n.getMessage('SWIPE_SELECTION_TOOLTIP'),
  DELETION: chrome.i18n.getMessage('SWIPE_DELETION_TOOLTIP'),
  RESTORATION: chrome.i18n.getMessage('SWIPE_RESTORATION_TOOLTIP')
};
var Tooltip = SwipeView.Tooltip;


/**
 * Returns whether the tracker will be deployed on future swipe events.
 *
 * @return {boolean}
 */
SwipeView.prototype.isArmed = function() {
  return this.armed_;
};


/**
 * Handles a SurroundingTextChanged event. Keeps track of text that has been
 * deleted so that it can be restored if necessary.
 *
 * @param {!i18n.input.chrome.inputview.events.SurroundingTextChangedEvent} e .
 * @private
 */
SwipeView.prototype.onSurroundingTextChanged_ = function(e) {
  if (this.adapter_.isPasswordBox()) {
    this.surroundingText_ = '';
    this.surroundingTextOffset_ = 0;
    return;
  }
  // Extract text before the cursor.
  var text = e.textBeforeCursor || '';
  if (this.surroundingText_ == text &&
      this.surroundingTextOffset_ == e.offset) {
    // Duplicate event.
    return;
  }
  // Cache old values.
  var oldText = this.surroundingText_;
  var oldOffset = this.surroundingTextOffset_;
  // Update stored values.
  this.surroundingTextOffset_ = e.offset;
  this.surroundingText_ = text;
  // Check for selection in progress.
  if (e.anchor != e.focus) {
    return;
  }
  var diff = '';
  var delta = (oldText.length + oldOffset) -
      (text.length + e.offset);
  if (delta > 0) {
    // Deletion occurred.
    if (delta <= oldText.length) {
      diff = oldText.slice(-delta);
    } else {
      // First OSTC event, ignore.
      return;
    }
  } else if (delta < 0) {
    // Text inserted.
    // Handle blink bug where ctrl+delete deletes a space and inserts
    // a &nbsp.
    // Convert &nbsp to ' ' and remove from delete words since blink
    // did a minirestore for us.
    var restored = text.slice(delta);
    var letter = restored[0];
    if (letter == SwipeView.NBSP_CHAR_) {
      var lastDelete = this.deletedWords_.pop();
      if (lastDelete) {
        var firstChar = lastDelete[0] || '';
        if (firstChar != ' ') {
          // Not in the edge case mentioned, restore the deletion.
          this.deletedWords_.push(lastDelete);
        } else {
          // First character was the extra ' '.
          this.deletedWords_.push(lastDelete.slice(1));
        }
      }
    }
  } else {
    goog.log.warning(this.logger_, 'Unexpected OSTC event.');
  }
  if (diff) {
    this.deletedWords_.push(diff);
  // Do not reset while swiping.
  } else if (!this.isVisible()) {
    this.deletedWords_ = [];
  }
};


/**
 * Attempts to restore the original text input.
 *
 * @private
 * @return {boolean} Whether it was successful.
 */
SwipeView.prototype.restoreOriginalText_ = function() {
  var restoreLength = (this.initialSurroundingTextLength_) -
      (this.surroundingText_.length + this.surroundingTextOffset_);
  // Native undo does not work well with composition text. First try to
  // compute the delta between what the text was when gesture deletion was
  // triggered, and what it is now.
  if (restoreLength > 0 &&
      restoreLength <= this.initialSurroundingText_.length) {
    this.adapter_.commitText(
        this.initialSurroundingText_.slice(-restoreLength));
    // Prevent using this again.
    this.initialSurroundingText_ = '';
    this.initialSurroundingTextLength_ = 0;
    return true;
  }
  return false;
};


/**
 * Handles swipe actions on the deletion track. Leftward swipes on the deletion
 * track deletes words, while rightward swipes restore them.
 *
 * @param {!i18n.input.chrome.inputview.events.SwipeEvent} e The swipe event.
 * @private
 */
SwipeView.prototype.swipeToDelete_ = function(e) {
  // Cache whether we were tracking.
  var alreadyTracking = this.tracking_;
  var previousIndex = this.getHighlightedIndex();
  var direction = this.swipeOnTrack(e.x, e.y);
  // Did not move segments.
  if (!direction) {
    // First gesture.
    if (!alreadyTracking) {
      // All previous deletions count as one now.
      this.deletedWords_.reverse();
      var word = this.deletedWords_.join('');
      this.deletedWords_ = [word];
      // Swiped right, cancel the deletion.
      if (e.direction & SwipeDirection.RIGHT) {
        word = this.deletedWords_.pop();
        if (word) {
          this.adapter_.commitText(word);
        }
      } else {
        // Change the tooltip to show restoration instructions.
        this.candidateView_.showTooltip(Tooltip.RESTORATION);
      }
    }
    return;
  }
  // Always show restoration tooltip after the trackIndex changes.
  this.candidateView_.showTooltip(Tooltip.RESTORATION);
  // Some finger swipes jump tracks, compensate for this.
  var delta = Math.abs(this.getHighlightedIndex() - previousIndex);
  if (direction & SwipeDirection.LEFT) {
    for (var i = 0; i < delta; i++) {
      if (this.surroundingText_ == '') {
        // Empty text, nothing to delete.
        this.noopCount_++;
      } else {
        this.adapter_.sendKeyDownAndUpEvent(
            '\u0008', KeyCodes.BACKSPACE, undefined, undefined, {
              ctrl: true,
              shift: false
            });
      }
    }
  } else if (direction & SwipeDirection.RIGHT) {
    for (var i = 0; i < delta; i++) {
      // Restore text we deleted before the track came up, but part of the
      // same gesture.
      if (this.isAtOrigin()) {
        if (!this.restoreOriginalText_() && this.noopCount_ == 0) {
          // Unable to use onSurroundingText, as long as there are no noop undos
          // use a native undo for the final undo.
          this.adapter_.sendKeyDownAndUpEvent(
              '\u007a', KeyCodes.KEY_Z, undefined, undefined, {
                ctrl: true,
                shift: false
              });
        }
        this.noopCount_ = 0;
        break;
      }
      if (this.noopCount_ > 0) {
        this.noopCount_--;
      } else {
        this.adapter_.sendKeyDownAndUpEvent(
            '\u007a', KeyCodes.KEY_Z, undefined, undefined, {
              ctrl: true,
              shift: false
            });
      }
    }
  } else {
    goog.log.warning(this.logger_, 'Unexpected swipe direction: ' + direction);
  }
};


/**
 * Sets whether the current keyset supports swipe editting.
 *
 * @param {boolean} supported .
 */
SwipeView.prototype.setKeysetSupported = function(supported) {
  this.isKeysetSupported_ = supported;
};


/**
 * Handles swipe actions on the selection track. Swipes cause the cursor to move
 * to the next blank space in the direction of the swipe.
 *
 * @param {!i18n.input.chrome.inputview.events.SwipeEvent} e The swipe event.
 * @private
 */
SwipeView.prototype.swipeToSelect_ = function(e) {
  var previousIndex = this.getHighlightedIndex();
  var direction = this.swipeOnTrack(e.x, e.y);
  // Swipe did not change track index, ignore.
  if (!direction) {
    return;
  }
  var index = this.getHighlightedIndex();
  if (index == -1) {
    goog.log.warning(this.logger_, 'Invalid track index.');
    return;
  }
  // TODO: Set selectWord to true if the shift key is currently pressed.
  var selectWord = false;
  var code;
  if (direction & SwipeDirection.LEFT) {
    code = KeyCodes.ARROW_LEFT;
  } else if (direction & SwipeDirection.RIGHT) {
    code = KeyCodes.ARROW_RIGHT;
  } else {
    goog.log.warning(this.logger_, 'Unexpected swipe direction: ' + direction);
    return;
  }
  // Finger swipes sometimes go over multiple tracks. Complete the action for
  // each.
  var delta = Math.abs(index - previousIndex);
  if (delta < 0) {
    goog.log.warning(this.logger_, 'Swipe index did not change.');
  }
  // TODO: Investigate why pointerbundle skips some swipe events.
  for (var i = 0; i < delta; i++) {
    this.adapter_.sendKeyDownAndUpEvent(
        '', code, undefined, undefined, {
          ctrl: true,
          shift: selectWord
        });
  }
};


/**
 * Handles the swipe action. Swipes on the deletion track edits the surrounding
 * text, while swipes on the selection track navigates it.
 *
 * @param {!i18n.input.chrome.inputview.events.SwipeEvent} e The swipe event.
 * @private
 */
SwipeView.prototype.handleSwipeAction_ = function(e) {
  if (this.eventIdentifier_ != undefined &&
      this.eventIdentifier_ != e.identifier) {
    return;
  }
  if (this.isVisible()) {
    if (e.view.type == ElementType.BACKSPACE_KEY) {
      this.swipeToDelete_(e);
      return;
    }
    if (e.view.type == ElementType.SELECT_VIEW) {
      this.swipeToSelect_(e);
      return;
    }
  }

  // User swiped on backspace key before swipeview was visible.
  if (e.view.type == ElementType.BACKSPACE_KEY) {
    if (!this.armed_) {
      // Prevents reshowing the track after it is hidden as part of the same
      // finger movement.
      return;
    }
    if (e.direction & SwipeDirection.LEFT) {
      var key = /** @type {!content.FunctionalKey} */ (e.view);
      // Equivalent to a longpress.
      if (this.isDeletionEnabled()) {
        this.showDeletionTrack(key, e.identifier, true);
      }
    } else if (e.direction & SwipeDirection.RIGHT) {
      this.restoreOriginalText_();
      this.armed_ = false;
    }
    return;
  }
};


/**
 * Handles the pointer action.
 *
 * @param {!i18n.input.chrome.inputview.events.PointerEvent} e .
 * @private
 */
SwipeView.prototype.handlePointerAction_ = function(e) {
  if (!e.view) {
    return;
  }
  if (this.eventIdentifier_ != undefined &&
      e.identifier != this.eventIdentifier_) {
    return;
  }
  switch (e.view.type) {
    case ElementType.BACKSPACE_KEY:
      var key = /** @type {!content.FunctionalKey} */ (e.view);
      if (e.type == EventType.POINTER_DOWN) {
        if (this.adapter_.contextType != ContextType.URL) {
          this.armed_ = true;
          this.initialSurroundingText_ = this.surroundingText_;
          this.initialSurroundingTextLength_ =
              this.surroundingText_.length + this.surroundingTextOffset_;
        }
        this.deletedWords_ = [];
      } else if (e.type == EventType.POINTER_UP ||
                 e.type == EventType.POINTER_OUT) {
        if (!this.isVisible()) {
          this.armed_ = false;
        }
      } else if (e.type == EventType.LONG_PRESS) {
        if (this.isDeletionEnabled()) {
          this.showDeletionTrack(key, e.identifier, false);
        }
      }
      break;
    case ElementType.SWIPE_VIEW:
      if (e.type == EventType.POINTER_DOWN &&
          e.target == this.getCoverElement()) {
        this.hide_();
      } else if (e.type == EventType.POINTER_UP ||
                 e.type == EventType.POINTER_OUT) {
        this.hide_();
        // Reset the deleted words.
        this.deletedWords_ = [];
      }
      break;
    case ElementType.SELECT_VIEW:
      if (e.type == EventType.POINTER_DOWN) {
        this.showSelectionTrack(e.x, e.y, e.identifier);
      }
      if (e.type == EventType.POINTER_UP) {
        this.hide_();
      }
      break;
  }
};


/** @override */
SwipeView.prototype.createDom = function() {
  goog.base(this, 'createDom');

  var dom = this.getDomHelper();
  var elem = this.getElement();
  goog.style.setElementShown(elem, false);
  goog.dom.classlist.add(elem, i18n.input.chrome.inputview.Css.SWIPE_VIEW);
  this.coverElement_ = dom.createDom(goog.dom.TagName.DIV,
      i18n.input.chrome.inputview.Css.TRACK_COVER);
  dom.appendChild(document.body, this.coverElement_);
  goog.style.setElementShown(this.coverElement_, false);
  this.coverElement_['view'] = this;
  // Cache finger affordance.
  this.fingerTracker_ = dom.createDom(goog.dom.TagName.DIV,
      i18n.input.chrome.inputview.Css.GESTURE_EDITING_FINGER_TRACKER);
  goog.style.setSize(this.fingerTracker_,
      SwipeView.FINGER_TRACKER_WIDTH_ + 'px',
      SwipeView.FINGER_TRACKER_WIDTH_ + 'px');
  this.ripple_ = dom.createDom(goog.dom.TagName.DIV,
      i18n.input.chrome.inputview.Css.GESTURE_RIPPLE);
  dom.appendChild(this.coverElement_, this.ripple_);
  // Cache ripple animations.
  this.fadeAnimation_ = new goog.fx.dom.FadeOut(this.ripple_,
      SwipeView.FADE_ANIMATION_TIME_, goog.fx.easing.easeIn);
  this.scaleAnimation_ = new ScaleAtPoint(this.ripple_,
      [1, 1],
      [SwipeView.RIPPLE_SCALE_FACTOR_, SwipeView.RIPPLE_SCALE_FACTOR_],
      [-SwipeView.RIPPLE_WIDTH_, -SwipeView.RIPPLE_WIDTH_],
      SwipeView.SCALE_ANIMATION_TIME_,
      goog.fx.easing.easeOut);
};


/** @override */
SwipeView.prototype.enterDocument = function() {
  goog.base(this, 'enterDocument');
  this.getHandler()
      .listen(this.adapter_,
          i18n.input.chrome.inputview.events.EventType
              .SURROUNDING_TEXT_CHANGED,
          this.onSurroundingTextChanged_)
      .listen(this.pointerHandler_, [
        EventType.SWIPE], this.handleSwipeAction_)
      .listen(this.pointerHandler_, [
        EventType.LONG_PRESS,
        EventType.POINTER_UP,
        EventType.POINTER_DOWN,
        EventType.POINTER_OUT], this.handlePointerAction_);
};


/**
 * Shows the deletion swipe tracker.
 *
 * @param {number} x
 * @param {number} y
 * @param {number} width The width of a key.
 * @param {number} height The height of a key.
 * @private
 */
SwipeView.prototype.showDeletionTrack_ = function(x, y, width, height) {
  this.tracking_ = false;
  goog.style.setElementShown(this.getElement(), true);
  this.getDomHelper().removeChildren(this.getElement());
  goog.dom.classlist.add(this.getElement(), Css.DELETION_TRACK);
  // Each key except last has a separator.
  var totalWidth = ((2 * SwipeView.LENGTH_) - 1) * width;

  this.ltr = true;
  this.highlightIndex_ = 0;
  if ((x + totalWidth) > screen.width) {
    // If not enough space at the right, then make it to the left.
    x -= totalWidth;
    this.ltr = false;
    this.highlightIndex_ = SwipeView.LENGTH_ - 1;
  }
  if (width == 0) {
    this.highlightIndex_ = SwipeView.INVALID_INDEX_;
  }
  if (this.ltr) {
    goog.dom.classlist.add(this.getElement(), Css.LEFT_TO_RIGHT);
  }
  for (var i = 0; i < SwipeView.LENGTH_; i++) {
    var keyElem = this.addKey_();
    goog.style.setSize(keyElem, width, height);
    this.trackElements_.push(keyElem);

    if (i != (SwipeView.LENGTH_ - 1)) {
      this.addSeparator_(width, height);
    }
  }
  // Set position only changes the left and top values, which is problematic.
  // Manually modify css rules instead.
  if (this.ltr) {
    goog.style.setStyle(this.getElement(), {
      'right': 'initial',
      'left': x,
      'top': y
    });
  } else {
    goog.style.setStyle(this.getElement(), {
      'left': 'initial',
      'right': screen.width - x - totalWidth,
      'top': y
    });
  }
  // Highlight selected element if it's index is valid.
  if (this.highlightIndex_ != SwipeView.INVALID_INDEX_) {
    var elem = this.trackElements_[this.highlightIndex_];
    this.setElementBackground_(elem, true);
  }
  if (this.adapter_.contextType == ContextType.NUMBER ||
      this.adapter_.contextType == ContextType.PHONE) {
    goog.dom.classlist.add(this.coverElement_, Css.NUMERIC_LAYOUT);
  } else {
    goog.dom.classlist.remove(this.coverElement_, Css.NUMERIC_LAYOUT);
  }
  goog.style.setElementShown(this.coverElement_, true);
  this.triggeredBy && this.triggeredBy.setHighlighted(true);
};


/**
 * Shows the current finger location at the coordinates provided.
 *
 * @param {number} x .
 * @param {number} y .
 * @private
 */
SwipeView.prototype.showFinger_ = function(x, y) {
  var dom = this.getDomHelper();
  dom.appendChild(this.getElement(), this.fingerTracker_);
  goog.style.setPosition(this.fingerTracker_,
      x - (SwipeView.FINGER_TRACKER_WIDTH_ / 2),
      y - (SwipeView.FINGER_TRACKER_WIDTH_ / 2));
  goog.style.setElementShown(this.fingerTracker_, true);
};


/**
 * Shows the selection swipe tracker.
 *
 * @param {number} x
 * @param {number} y
 * @param {number} width The width of a key.
 * @param {number} height The height of a key.
 * @private
 */
SwipeView.prototype.showSelectionTrack_ = function(x, y, width, height) {
  this.tracking_ = false;
  goog.style.setElementShown(this.getElement(), true);
  this.getDomHelper().removeChildren(this.getElement());
  goog.dom.classlist.add(this.getElement(), Css.SELECTION_TRACK);
  // Each key has a separator.
  var totalWidth = ((2 * SwipeView.LENGTH_)) * width;

  this.ltr = true;
  this.highlightIndex_ = SwipeView.INVALID_INDEX_;
  if ((x + totalWidth) > screen.width) {
    // If not enough space at the right, then make it to the left.
    x -= totalWidth;
    this.ltr = false;
  }
  if (this.ltr) {
    goog.dom.classlist.add(this.getElement(), Css.LEFT_TO_RIGHT);
  }

  for (var i = 0; i < SwipeView.LENGTH_; i++) {
    var keyElem;
    if (!this.ltr) {
      keyElem = this.addKey_();
      goog.style.setSize(keyElem, width, height);
      this.trackElements_.push(keyElem);
    }

    keyElem = this.addSeparator_(width, height);
    goog.style.setSize(keyElem, width, height);
    this.trackElements_.push(keyElem || undefined);

    if (this.ltr) {
      keyElem = this.addKey_();
      goog.style.setSize(keyElem, width, height);
      this.trackElements_.push(keyElem);
    }
  }
  // Set position only changes the left and top values, which is problematic.
  // Manually modify css rules instead.
  if (this.ltr) {
    goog.style.setStyle(this.getElement(), {
      'right': 'initial',
      'left': x,
      'top': y
    });
  } else {
    goog.style.setStyle(this.getElement(), {
      'left': 'initial',
      'right': 0,
      'top': y
    });
  }
  if (this.adapter_.contextType == ContextType.NUMBER ||
      this.adapter_.contextType == ContextType.PHONE) {
    goog.dom.classlist.add(this.coverElement_, Css.NUMERIC_LAYOUT);
  } else {
    goog.dom.classlist.remove(this.coverElement_, Css.NUMERIC_LAYOUT);
  }
  goog.style.setElementShown(this.coverElement_, true);
  this.triggeredBy && this.triggeredBy.setHighlighted(true);
};


/**
 * Shows the deletion track.
 *
 * @param {!i18n.input.chrome.inputview.elements.content.SoftKey} key
 *   The key triggered this track view.
 * @param {!number} id The triggering event id.
 * @param {boolean} isSwipe Indicates the trigger was a swipe.
 */
SwipeView.prototype.showDeletionTrack = function(key, id, isSwipe) {
  this.eventIdentifier_ = id;
  this.isSelection_ = false;
  this.adapter_.setGestureEditingInProgress(true, isSwipe);
  this.candidateView_.showTooltip(Tooltip.DELETION);
  this.triggeredBy = key;
  var coordinate = goog.style.getClientPosition(key.getElement());
  if (key.type == ElementType.BACKSPACE_KEY) {
    this.showDeletionTrack_(
        coordinate.x + key.availableWidth,
        coordinate.y,
        SwipeView.SEGMENT_WIDTH_,
        SwipeView.SEGMENT_WIDTH_);
  }
  var centerX = coordinate.x + (key.availableWidth / 2);
  var centerY = coordinate.y + (key.availableHeight / 2);
  this.animateRipple_(centerX, centerY);
  this.showFinger_(centerX, centerY);
};


/**
 * Shows the selection track.
 *
 * @param {number} x
 * @param {number} y
 * @param {number} id The triggering event id.
 */
SwipeView.prototype.showSelectionTrack = function(x, y, id) {
  this.eventIdentifier_ = id;
  this.isSelection_ = true;
  this.adapter_.setGestureEditingInProgress(true);
  this.candidateView_.showTooltip(Tooltip.SELECTION);
  var ltr = (x <= (screen.width / 2));
  var halfWidth = SwipeView.SEGMENT_WIDTH_ / 2;
  // Center track on finger but force containment.
  var trackY = Math.max(y - halfWidth, halfWidth);
  trackY = Math.min(trackY, window.innerHeight - 3 * halfWidth);
  this.showSelectionTrack_(
      ltr ? 0 : screen.width,
      trackY,
      SwipeView.SEGMENT_WIDTH_,
      SwipeView.SEGMENT_WIDTH_);
  this.showFinger_(x, y);
};


/**
 * Creates an animation object that will scale an element at a point.
 *
 * Start, end and origin should be 2 dimensional arrays
 *
 * @param {Element} element Dom Node to be used in the animation.
 * @param {Array<number>} start 2D array for start x-scale and y-scale.
 * @param {Array<number>} end 2D array for end x-scale and y-scale.
 * @param {Array<number>} origin 2D array for origin relative to the elements
 *     current position.
 * @param {number} time Length of animation in milliseconds.
 * @param {Function=} opt_acc Acceleration function, returns 0-1 for inputs 0-1.
 * @extends {goog.fx.dom.PredefinedEffect}
 * @constructor
 */
// TODO: Migrate this to the closure animation library.
var ScaleAtPoint = function(element, start, end, origin, time, opt_acc) {
  if (start.length != 2 || end.length != 2 || origin.length != 2) {
    throw Error('Start, end and origin arrays must be 2D');
  }

  /**
   * Point at which the animation should scale relative to the elements current
   * location.
   *
   * @private {Array<number>}
   */
  this.origin_ = origin;

  ScaleAtPoint.base(this, 'constructor', element, start, end, time, opt_acc);
};
goog.inherits(ScaleAtPoint, goog.fx.dom.PredefinedEffect);


/** override */
ScaleAtPoint.prototype.updateStyle = function() {
  var transform = [
    'translate(', this.origin_[0], 'px, ', this.origin_[1], 'px) ',
    'scale(', this.coords[0], ', ', this.coords[1], ')'
  ].join('');
  goog.style.setStyle(this.element, 'transform', transform);
};


/**
 * Handles the fade start event on the ripple.
 *
 * @private
 */
SwipeView.prototype.onFadeStarted_ = function() {
  goog.events.unlisten(this.fadeAnimation_,
      goog.fx.Transition.EventType.BEGIN,
      this.onFadeStarted_);
  this.scaleAnimation_.play();
};


/**
 * Animates the ripple effect centered on the coordinates provided.
 *
 * @param {number} x
 * @param {number} y
 * @private
 */
SwipeView.prototype.animateRipple_ = function(x, y) {
  goog.style.setPosition(this.ripple_, x, y);
  goog.style.setStyle(this.ripple_, 'transform', '');
  goog.style.setElementShown(this.ripple_, true);
  goog.events.listen(this.fadeAnimation_, goog.fx.Transition.EventType.BEGIN,
      this.onFadeStarted_.bind(this));
  this.fadeAnimation_.play();
};


/**
 * Hides the swipe view.
 *
 * @private
 */
SwipeView.prototype.hide_ = function() {
  this.adapter_.setGestureEditingInProgress(false);
  this.candidateView_.hideTooltip();
  goog.style.setElementShown(this.ripple_, false);
  this.armed_ = false;
  this.trackElements_ = [];
  this.tracking_ = false;
  this.eventIdentifier_ = undefined;
  this.noopCount_ = 0;
  this.initialSurroundingText_ = '';
  this.initialSurroundingTextLength_ = 0;
  if (this.triggeredBy) {
    this.triggeredBy.setHighlighted(false);
  }
  this.triggeredBy = undefined;
  goog.style.setElementShown(this.getElement(), false);
  goog.style.setElementShown(this.coverElement_, false);
  this.highlightIndex_ = SwipeView.INVALID_INDEX_;
  goog.dom.classlist.removeAll(this.getElement(), [
    Css.SWIPE_ACTIVE, Css.SELECTION_TRACK, Css.DELETION_TRACK,
    Css.LEFT_TO_RIGHT
  ]);
};


/**
 * Returns whether the current track counter is at the first element.
 *
 * @return {boolean}
 */
SwipeView.prototype.isAtOrigin = function() {
  return this.ltr ? this.highlightIndex_ == 0 :
      this.highlightIndex_ == SwipeView.LENGTH_ - 1;
};


/**
 * Swipes to the coordinates specified on the track.
 *
 * @param {number} x .
 * @param {number} y .
 * @return {SwipeDirection|undefined} Direction swiped else undefined if there
 *    there was no change.
 */
SwipeView.prototype.swipeOnTrack = function(x, y) {
  var previousIndex = this.highlightIndex_;
  for (var i = 0; i < this.trackElements_.length; i++) {
    var elem = this.trackElements_[i];
    var coordinate = goog.style.getClientPosition(elem);
    var size = goog.style.getSize(elem);
    var visible = (goog.style.getComputedStyle(elem, 'display') != 'none');
    if (visible && coordinate.x < x && (coordinate.x + size.width) > x) {
      this.highlightIndex_ = i;
      this.clearAllHighlights_();
      this.setElementBackground_(elem, true);
      goog.dom.classlist.add(this.getElement(), Css.SWIPE_ACTIVE);
    }
  }
  var changed = previousIndex != this.highlightIndex_;
  this.tracking_ = this.tracking_ || changed;
  // Update finger affordance.
  goog.style.setPosition(this.fingerTracker_,
      x - (SwipeView.FINGER_TRACKER_WIDTH_ / 2),
      y - (SwipeView.FINGER_TRACKER_WIDTH_ / 2));
  if (!changed || previousIndex == SwipeView.INVALID_INDEX_) {
    return undefined;
  }
  return previousIndex < this.highlightIndex_ ?
      SwipeDirection.RIGHT : SwipeDirection.LEFT;
};


/**
 * Clears all the highlights.
 *
 * @private
 */
SwipeView.prototype.clearAllHighlights_ = function() {
  for (var i = 0; i < this.trackElements_.length; i++) {
    this.setElementBackground_(this.trackElements_[i], false);
  }
};


/**
 * Sets the background style of the element.
 *
 * @param {!Element} element The element.
 * @param {boolean} highlight True to highlight the element.
 * @private
 */
SwipeView.prototype.setElementBackground_ =
    function(element, highlight) {
  if (highlight) {
    goog.dom.classlist.add(element, Css.ELEMENT_HIGHLIGHT);
  } else {
    goog.dom.classlist.remove(element, Css.ELEMENT_HIGHLIGHT);
  }
};


/**
 * Adds a swipable key into the view.
 *
 * @param {string=} opt_character The character.
 * @param {Css=} opt_icon_css
 * @return {!Element} The create key element.
 * @private
 */
SwipeView.prototype.addKey_ = function(opt_character, opt_icon_css) {
  var dom = this.getDomHelper();
  var character = opt_character &&
      i18n.input.chrome.inputview.util.getVisibleCharacter(opt_character);
  var keyElem;
  if (character) {
    keyElem = dom.createDom(goog.dom.TagName.DIV, Css.SWIPE_KEY, character);
  } else {
    keyElem = dom.createDom(goog.dom.TagName.DIV, Css.SWIPE_KEY);
  }
  goog.dom.classlist.add(keyElem, Css.SWIPE_PIECE);
  if (opt_icon_css) {
    var child = dom.createDom(goog.dom.TagName.DIV, opt_icon_css);
    dom.appendChild(keyElem, child);
  }
  dom.appendChild(this.getElement(), keyElem);
  return keyElem;
};


/**
 * Adds a separator.
 *
 * @param {number} width .
 * @param {number} height .
 * @return {Element}
 * @private
 */
SwipeView.prototype.addSeparator_ = function(width, height) {
  var dom = this.getDomHelper();
  var separator = dom.createDom(goog.dom.TagName.DIV, Css.SWIPE_SEPARATOR);
  goog.style.setSize(separator, width + 'px', height + 'px');
  goog.dom.classlist.add(separator, Css.SWIPE_PIECE);
  dom.appendChild(this.getElement(), separator);
  return separator;
};


/**
 * Gets the cover element.
 *
 * @return {!Element} The cover element.
 */
SwipeView.prototype.getCoverElement = function() {
  return this.coverElement_;
};


/**
 * Returns the index of the current highlighted swipe element.
 *
 * @return {number}
 */
SwipeView.prototype.getHighlightedIndex = function() {
  if (this.highlightIndex_ == SwipeView.INVALID_INDEX_) {
    return SwipeView.INVALID_INDEX_;
  }
  if (this.ltr) {
    return this.highlightIndex_;
  }
  return this.trackElements_.length - this.highlightIndex_ - 1;
};


/**
 * Returns whether gesture deletion is enabled for the current context.
 *
 * @return {boolean}
 */
SwipeView.prototype.isDeletionEnabled = function() {
  // TODO: Omni bar sends wrong anchor/focus when autocompleting
  // URLs. Re-enable when that is fixed.
  if (this.adapter_.contextType == ContextType.URL) {
    return false;
  }
  // TODO(rsadam): Re-enable when ctrl+z is fixed on gmail.
  if (this.adapter_.isGoogleMail()) {
    return false;
  }
  if (this.adapter_.isPasswordBox()) {
    return false;
  }
  if (this.adapter_.isA11yMode) {
    return false;
  }
  if (!this.isKeysetSupported_) {
    return false;
  }
  // TODO: Disable if the current layout is emoji or handwriting.
  return this.enabled;
};


/** @override */
SwipeView.prototype.resize = function(width, height) {
  goog.base(this, 'resize', width, height);
  goog.style.setSize(this.coverElement_, width, height);
};


/**
 * Resets the swipeview.
 *
 */
SwipeView.prototype.reset = function() {
  this.deletedWords_ = [];
  this.surroundingText_ = '';
  this.hide_();
};


/** @override */
SwipeView.prototype.disposeInternal = function() {
  goog.dispose(this.pointerHandler_);

  goog.base(this, 'disposeInternal');
};

});  // goog.scope
