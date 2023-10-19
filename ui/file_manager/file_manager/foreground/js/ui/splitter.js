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
// @ts-ignore: error TS8022: JSDoc '@extends' is not attached to a class.
export const Splitter = crUiDefine('div');

Splitter.prototype = {
  __proto__: HTMLDivElement.prototype,

  /**
   * Initializes the element.
   */
  decorate() {
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type '{ __proto__: HTMLDivElement; decorate(): void; resizeNextElement:
    // boolean; startDrag(clientX: number, isTouchEvent: boolean): void;
    // endDrag_(): void; getResizeTarget_(): Element; ... 9 more ...;
    // handleSplitterDragEnd(): void; }'.
    this.addEventListener('mousedown', this.handleMouseDown_.bind(this), true);
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type '{ __proto__: HTMLDivElement; decorate(): void; resizeNextElement:
    // boolean; startDrag(clientX: number, isTouchEvent: boolean): void;
    // endDrag_(): void; getResizeTarget_(): Element; ... 9 more ...;
    // handleSplitterDragEnd(): void; }'.
    this.addEventListener(
        'touchstart', this.handleTouchStart_.bind(this), true);
    // @ts-ignore: error TS2551: Property 'resizeNextElement_' does not exist on
    // type '{ __proto__: HTMLDivElement; decorate(): void; resizeNextElement:
    // boolean; startDrag(clientX: number, isTouchEvent: boolean): void;
    // endDrag_(): void; getResizeTarget_(): Element; ... 9 more ...;
    // handleSplitterDragEnd(): void; }'. Did you mean 'resizeNextElement'?
    this.resizeNextElement_ = false;
  },

  /**
   * @param {boolean} resizeNext True if resize the next element.
   *     By default, splitter resizes previous (left) element.
   */
  set resizeNextElement(resizeNext) {
    // @ts-ignore: error TS2551: Property 'resizeNextElement_' does not exist on
    // type '{ __proto__: HTMLDivElement; decorate(): void; resizeNextElement:
    // boolean; startDrag(clientX: number, isTouchEvent: boolean): void;
    // endDrag_(): void; getResizeTarget_(): Element; ... 9 more ...;
    // handleSplitterDragEnd(): void; }'. Did you mean 'resizeNextElement'?
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
    // @ts-ignore: error TS2339: Property 'handlers_' does not exist on type '{
    // __proto__: HTMLDivElement; decorate(): void; resizeNextElement: boolean;
    // startDrag(clientX: number, isTouchEvent: boolean): void; endDrag_():
    // void; getResizeTarget_(): Element; ... 9 more ...;
    // handleSplitterDragEnd(): void; }'.
    if (this.handlers_) {
      // Case of concurrent drags.
      this.endDrag_();
    }
    if (isTouchEvent) {
      const endDragBound = this.endDrag_.bind(this);
      // @ts-ignore: error TS2339: Property 'handlers_' does not exist on type
      // '{ __proto__: HTMLDivElement; decorate(): void; resizeNextElement:
      // boolean; startDrag(clientX: number, isTouchEvent: boolean): void;
      // endDrag_(): void; getResizeTarget_(): Element; ... 9 more ...;
      // handleSplitterDragEnd(): void; }'.
      this.handlers_ = {
        'touchmove': this.handleTouchMove_.bind(this),
        'touchend': endDragBound,
        'touchcancel': endDragBound,

        // Another touch start (we somehow missed touchend or touchcancel).
        'touchstart': endDragBound,
      };
    } else {
      // @ts-ignore: error TS2339: Property 'handlers_' does not exist on type
      // '{ __proto__: HTMLDivElement; decorate(): void; resizeNextElement:
      // boolean; startDrag(clientX: number, isTouchEvent: boolean): void;
      // endDrag_(): void; getResizeTarget_(): Element; ... 9 more ...;
      // handleSplitterDragEnd(): void; }'.
      this.handlers_ = {
        'mousemove': this.handleMouseMove_.bind(this),
        'mouseup': this.handleMouseUp_.bind(this),
      };
    }

    // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on type
    // '{ __proto__: HTMLDivElement; decorate(): void; resizeNextElement:
    // boolean; startDrag(clientX: number, isTouchEvent: boolean): void;
    // endDrag_(): void; getResizeTarget_(): Element; ... 9 more ...;
    // handleSplitterDragEnd(): void; }'.
    const doc = this.ownerDocument;

    // Use capturing events on the document to get events when the mouse
    // leaves the document.
    // @ts-ignore: error TS2339: Property 'handlers_' does not exist on type '{
    // __proto__: HTMLDivElement; decorate(): void; resizeNextElement: boolean;
    // startDrag(clientX: number, isTouchEvent: boolean): void; endDrag_():
    // void; getResizeTarget_(): Element; ... 9 more ...;
    // handleSplitterDragEnd(): void; }'.
    for (const eventType in this.handlers_) {
      // @ts-ignore: error TS2339: Property 'handlers_' does not exist on type
      // '{ __proto__: HTMLDivElement; decorate(): void; resizeNextElement:
      // boolean; startDrag(clientX: number, isTouchEvent: boolean): void;
      // endDrag_(): void; getResizeTarget_(): Element; ... 9 more ...;
      // handleSplitterDragEnd(): void; }'.
      doc.addEventListener(eventType, this.handlers_[eventType], true);
    }

    // @ts-ignore: error TS2339: Property 'startX_' does not exist on type '{
    // __proto__: HTMLDivElement; decorate(): void; resizeNextElement: boolean;
    // startDrag(clientX: number, isTouchEvent: boolean): void; endDrag_():
    // void; getResizeTarget_(): Element; ... 9 more ...;
    // handleSplitterDragEnd(): void; }'.
    this.startX_ = clientX;
    this.handleSplitterDragStart();
  },

  /**
   * Ends the dragging of the splitter. Removes listeners set in startDrag
   * and calls splitter drag end handler.
   * @private
   */
  endDrag_() {
    // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on type
    // '{ __proto__: HTMLDivElement; decorate(): void; resizeNextElement:
    // boolean; startDrag(clientX: number, isTouchEvent: boolean): void;
    // endDrag_(): void; getResizeTarget_(): Element; ... 9 more ...;
    // handleSplitterDragEnd(): void; }'.
    const doc = this.ownerDocument;
    // @ts-ignore: error TS2339: Property 'handlers_' does not exist on type '{
    // __proto__: HTMLDivElement; decorate(): void; resizeNextElement: boolean;
    // startDrag(clientX: number, isTouchEvent: boolean): void; endDrag_():
    // void; getResizeTarget_(): Element; ... 9 more ...;
    // handleSplitterDragEnd(): void; }'.
    for (const eventType in this.handlers_) {
      // @ts-ignore: error TS2339: Property 'handlers_' does not exist on type
      // '{ __proto__: HTMLDivElement; decorate(): void; resizeNextElement:
      // boolean; startDrag(clientX: number, isTouchEvent: boolean): void;
      // endDrag_(): void; getResizeTarget_(): Element; ... 9 more ...;
      // handleSplitterDragEnd(): void; }'.
      doc.removeEventListener(eventType, this.handlers_[eventType], true);
    }
    // @ts-ignore: error TS2339: Property 'handlers_' does not exist on type '{
    // __proto__: HTMLDivElement; decorate(): void; resizeNextElement: boolean;
    // startDrag(clientX: number, isTouchEvent: boolean): void; endDrag_():
    // void; getResizeTarget_(): Element; ... 9 more ...;
    // handleSplitterDragEnd(): void; }'.
    this.handlers_ = null;
    this.handleSplitterDragEnd();
  },

  /**
   * @return {Element}
   * @private
   */
  getResizeTarget_() {
    // @ts-ignore: error TS2339: Property 'nextElementSibling' does not exist on
    // type '{ __proto__: HTMLDivElement; decorate(): void; resizeNextElement:
    // boolean; startDrag(clientX: number, isTouchEvent: boolean): void;
    // endDrag_(): void; getResizeTarget_(): Element; ... 9 more ...;
    // handleSplitterDragEnd(): void; }'.
    return this.resizeNextElement_ ?
        // @ts-ignore: error TS2339: Property 'nextElementSibling' does not
        // exist on type '{ __proto__: HTMLDivElement;...
        this.nextElementSibling :
        // @ts-ignore: error TS2339: Property 'previousElementSibling' does not
        // exist on type '{ __proto__: HTMLDivElement; decorate(): void;
        // resizeNextElement: boolean; startDrag(clientX: number, isTouchEvent:
        // boolean): void; endDrag_(): void; getResizeTarget_(): Element; ... 9
        // more ...; handleSplitterDragEnd(): void; }'.
        this.previousElementSibling;
  },

  /**
   * Calculate width to resize target element.
   * @param {number} deltaX horizontal drag amount
   * @return {number}
   * @private
   */
  calcDeltaX_(deltaX) {
    // @ts-ignore: error TS2551: Property 'resizeNextElement_' does not exist on
    // type '{ __proto__: HTMLDivElement; decorate(): void; resizeNextElement:
    // boolean; startDrag(clientX: number, isTouchEvent: boolean): void;
    // endDrag_(): void; getResizeTarget_(): Element; ... 9 more ...;
    // handleSplitterDragEnd(): void; }'. Did you mean 'resizeNextElement'?
    return this.resizeNextElement_ ? -deltaX : deltaX;
  },

  /**
   * Handles the mousedown event which starts the dragging of the splitter.
   * @param {!Event} e The mouse event.
   * @private
   */
  handleMouseDown_(e) {
    e = /** @type {!MouseEvent} */ (e);
    // @ts-ignore: error TS2339: Property 'button' does not exist on type
    // 'Event'.
    if (e.button) {
      return;
    }
    // @ts-ignore: error TS2339: Property 'clientX' does not exist on type
    // 'Event'.
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
    // @ts-ignore: error TS2339: Property 'touches' does not exist on type
    // 'Event'.
    if (e.touches.length === 1) {
      // @ts-ignore: error TS2339: Property 'touches' does not exist on type
      // 'Event'.
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
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
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
        // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
        // type '{ __proto__: HTMLDivElement; decorate(): void;
        // resizeNextElement: boolean; startDrag(clientX: number, isTouchEvent:
        // boolean): void; endDrag_(): void; getResizeTarget_(): Element; ... 9
        // more ...; handleSplitterDragEnd(): void; }'.
        this.ownerDocument.defaultView.getComputedStyle(this).direction ===
        'rtl';
    const dirMultiplier = rtl ? -1 : 1;
    // @ts-ignore: error TS2339: Property 'startX_' does not exist on type '{
    // __proto__: HTMLDivElement; decorate(): void; resizeNextElement: boolean;
    // startDrag(clientX: number, isTouchEvent: boolean): void; endDrag_():
    // void; getResizeTarget_(): Element; ... 9 more ...;
    // handleSplitterDragEnd(): void; }'.
    const deltaX = dirMultiplier * (clientX - this.startX_);
    this.handleSplitterDragMove(deltaX);
  },

  /**
   * Handles the mouse up event which ends the dragging of the splitter.
   * @param {!MouseEvent} e The mouse event.
   * @private
   */
  // @ts-ignore: error TS6133: 'e' is declared but its value is never read.
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
    // @ts-ignore: error TS2339: Property 'startWidth_' does not exist on type
    // '{ __proto__: HTMLDivElement; decorate(): void; resizeNextElement:
    // boolean; startDrag(clientX: number, isTouchEvent: boolean): void;
    // endDrag_(): void; getResizeTarget_(): Element; ... 9 more ...;
    // handleSplitterDragEnd(): void; }'.
    this.startWidth_ =
        // @ts-ignore: error TS18047: 'doc.defaultView' is possibly 'null'.
        parseFloat(doc.defaultView.getComputedStyle(targetElement).width) +
        // @ts-ignore: error TS2339: Property 'offsetWidth' does not exist on
        // type 'Element'.
        targetElement.offsetWidth - targetElement.clientWidth;

    // @ts-ignore: error TS2339: Property 'classList' does not exist on type '{
    // __proto__: HTMLDivElement; decorate(): void; resizeNextElement: boolean;
    // startDrag(clientX: number, isTouchEvent: boolean): void; endDrag_():
    // void; getResizeTarget_(): Element; ... 9 more ...;
    // handleSplitterDragEnd(): void; }'.
    this.classList.add('splitter-active');
  },

  /**
   * Handles splitter moves. Updates width of the element being resized.
   * @param {number} deltaX The change of splitter horizontal position.
   */
  handleSplitterDragMove(deltaX) {
    const targetElement = this.getResizeTarget_();
    // @ts-ignore: error TS2339: Property 'startWidth_' does not exist on type
    // '{ __proto__: HTMLDivElement; decorate(): void; resizeNextElement:
    // boolean; startDrag(clientX: number, isTouchEvent: boolean): void;
    // endDrag_(): void; getResizeTarget_(): Element; ... 9 more ...;
    // handleSplitterDragEnd(): void; }'.
    const newWidth = this.startWidth_ + this.calcDeltaX_(deltaX);
    // @ts-ignore: error TS2339: Property 'style' does not exist on type
    // 'Element'.
    targetElement.style.width = newWidth + 'px';
    // @ts-ignore: error TS2345: Argument of type '{ __proto__: HTMLDivElement;
    // decorate(): void; resizeNextElement: boolean; startDrag(clientX: number,
    // isTouchEvent: boolean): void; endDrag_(): void; getResizeTarget_():
    // Element; ... 9 more ...; handleSplitterDragEnd(): void; }' is not
    // assignable to parameter of type 'EventTarget'.
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
        // @ts-ignore: error TS18047: 'doc.defaultView' is possibly 'null'.
        parseFloat(doc.defaultView.getComputedStyle(targetElement).width);
    // @ts-ignore: error TS2339: Property 'startWidth_' does not exist on type
    // '{ __proto__: HTMLDivElement; decorate(): void; resizeNextElement:
    // boolean; startDrag(clientX: number, isTouchEvent: boolean): void;
    // endDrag_(): void; getResizeTarget_(): Element; ... 9 more ...;
    // handleSplitterDragEnd(): void; }'.
    if (this.startWidth_ !== computedWidth) {
      // @ts-ignore: error TS2345: Argument of type '{ __proto__:
      // HTMLDivElement; decorate(): void; resizeNextElement: boolean;
      // startDrag(clientX: number, isTouchEvent: boolean): void; endDrag_():
      // void; getResizeTarget_(): Element; ... 9 more ...;
      // handleSplitterDragEnd(): void; }' is not assignable to parameter of
      // type 'EventTarget'.
      dispatchSimpleEvent(this, 'resize');
    }

    // @ts-ignore: error TS2339: Property 'classList' does not exist on type '{
    // __proto__: HTMLDivElement; decorate(): void; resizeNextElement: boolean;
    // startDrag(clientX: number, isTouchEvent: boolean): void; endDrag_():
    // void; getResizeTarget_(): Element; ... 9 more ...;
    // handleSplitterDragEnd(): void; }'.
    this.classList.remove('splitter-active');
  },
};
