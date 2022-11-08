// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This implements a splitter element which can be used to resize
 * elements in split panes.
 *
 * The parent of the splitter should be an hbox (display: -webkit-box) with at
 * least one previous element sibling. The splitter controls the width of the
 * element before it.
 *
 * <div class=split-pane>
 *   <div class=left>...</div>
 *   <div class=splitter></div>
 *   ...
 * </div>
 *
 */

import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';

import {define as crUiDefine} from '../../../common/js/ui.js';

/**
 * Creates a new splitter element.
 * @param {Object=} opt_propertyBag Optional properties.
 * @constructor
 * @extends {HTMLDivElement}
 */
export const Splitter = crUiDefine('div');

Splitter.prototype = {
  __proto__: HTMLDivElement.prototype,

  /**
   * Initializes the element.
   */
  decorate() {
    this.addEventListener('mousedown', this.handleMouseDown_.bind(this), true);
    this.addEventListener(
        'touchstart', this.handleTouchStart_.bind(this), true);
    this.resizeNextElement_ = false;
  },

  /**
   * @param {boolean} resizeNext True if resize the next element.
   *     By default, splitter resizes previous (left) element.
   */
  set resizeNextElement(resizeNext) {
    this.resizeNextElement_ = resizeNext;
  },

  /**
   * Starts the dragging of the splitter. Adds listeners for mouse or touch
   * events and calls splitter drag start handler.
   * @param {number} clientX X position of the mouse or touch event that
   *                         started the drag.
   * @param {boolean} isTouchEvent True if the drag started by touch event.
   */
  startDrag(clientX, isTouchEvent) {
    if (this.handlers_) {
      // Case of concurrent drags.
      this.endDrag_();
    }
    if (isTouchEvent) {
      const endDragBound = this.endDrag_.bind(this);
      this.handlers_ = {
        'touchmove': this.handleTouchMove_.bind(this),
        'touchend': endDragBound,
        'touchcancel': endDragBound,

        // Another touch start (we somehow missed touchend or touchcancel).
        'touchstart': endDragBound,
      };
    } else {
      this.handlers_ = {
        'mousemove': this.handleMouseMove_.bind(this),
        'mouseup': this.handleMouseUp_.bind(this),
      };
    }

    const doc = this.ownerDocument;

    // Use capturing events on the document to get events when the mouse
    // leaves the document.
    for (const eventType in this.handlers_) {
      doc.addEventListener(eventType, this.handlers_[eventType], true);
    }

    this.startX_ = clientX;
    this.handleSplitterDragStart();
  },

  /**
   * Ends the dragging of the splitter. Removes listeners set in startDrag
   * and calls splitter drag end handler.
   * @private
   */
  endDrag_() {
    const doc = this.ownerDocument;
    for (const eventType in this.handlers_) {
      doc.removeEventListener(eventType, this.handlers_[eventType], true);
    }
    this.handlers_ = null;
    this.handleSplitterDragEnd();
  },

  /**
   * @return {Element}
   * @private
   */
  getResizeTarget_() {
    return this.resizeNextElement_ ? this.nextElementSibling :
                                     this.previousElementSibling;
  },

  /**
   * Calculate width to resize target element.
   * @param {number} deltaX horizontal drag amount
   * @return {number}
   * @private
   */
  calcDeltaX_(deltaX) {
    return this.resizeNextElement_ ? -deltaX : deltaX;
  },

  /**
   * Handles the mousedown event which starts the dragging of the splitter.
   * @param {!Event} e The mouse event.
   * @private
   */
  handleMouseDown_(e) {
    e = /** @type {!MouseEvent} */ (e);
    if (e.button) {
      return;
    }
    this.startDrag(e.clientX, false);
    // Default action is to start selection and to move focus.
    e.preventDefault();
  },

  /**
   * Handles the touchstart event which starts the dragging of the splitter.
   * @param {!Event} e The touch event.
   * @private
   */
  handleTouchStart_(e) {
    e = /** @type {!TouchEvent} */ (e);
    if (e.touches.length === 1) {
      this.startDrag(e.touches[0].clientX, true);
      if (e.cancelable) {
        e.preventDefault();
      }
    }
  },

  /**
   * Handles the mousemove event which moves the splitter as the user moves
   * the mouse.
   * @param {!MouseEvent} e The mouse event.
   * @private
   */
  handleMouseMove_(e) {
    this.handleMove_(e.clientX);
  },

  /**
   * Handles the touch move event.
   * @param {!TouchEvent} e The touch event.
   */
  handleTouchMove_(e) {
    if (e.touches.length === 1) {
      this.handleMove_(e.touches[0].clientX);
    }
  },

  /**
   * Common part of handling mousemove and touchmove. Calls splitter drag
   * move handler.
   * @param {number} clientX X position of the mouse or touch event.
   * @private
   */
  handleMove_(clientX) {
    const rtl =
        this.ownerDocument.defaultView.getComputedStyle(this).direction ===
        'rtl';
    const dirMultiplier = rtl ? -1 : 1;
    const deltaX = dirMultiplier * (clientX - this.startX_);
    this.handleSplitterDragMove(deltaX);
  },

  /**
   * Handles the mouse up event which ends the dragging of the splitter.
   * @param {!MouseEvent} e The mouse event.
   * @private
   */
  handleMouseUp_(e) {
    this.endDrag_();
  },

  /**
   * Handles start of the splitter dragging. Saves current width of the
   * element being resized.
   */
  handleSplitterDragStart() {
    // Use the computed width style as the base so that we can ignore what
    // box sizing the element has. Add the difference between offset and
    // client widths to account for any scrollbars.
    const targetElement = this.getResizeTarget_();
    const doc = targetElement.ownerDocument;
    this.startWidth_ =
        parseFloat(doc.defaultView.getComputedStyle(targetElement).width) +
        targetElement.offsetWidth - targetElement.clientWidth;

    this.classList.add('splitter-active');
  },

  /**
   * Handles splitter moves. Updates width of the element being resized.
   * @param {number} deltaX The change of splitter horizontal position.
   */
  handleSplitterDragMove(deltaX) {
    const targetElement = this.getResizeTarget_();
    const newWidth = this.startWidth_ + this.calcDeltaX_(deltaX);
    targetElement.style.width = newWidth + 'px';
    dispatchSimpleEvent(this, 'dragmove');
  },

  /**
   * Handles end of the splitter dragging. This fires a 'resize' event if the
   * size changed.
   */
  handleSplitterDragEnd() {
    // Check if the size changed.
    const targetElement = this.getResizeTarget_();
    const doc = targetElement.ownerDocument;
    const computedWidth =
        parseFloat(doc.defaultView.getComputedStyle(targetElement).width);
    if (this.startWidth_ !== computedWidth) {
      dispatchSimpleEvent(this, 'resize');
    }

    this.classList.remove('splitter-active');
  },
};
