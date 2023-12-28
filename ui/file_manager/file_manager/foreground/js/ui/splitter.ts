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

interface SplitterHandlers {
  touchmove?: (event: TouchEvent) => void;
  touchstart?: (event: TouchEvent) => void;
  touchend?: (event: TouchEvent) => void;
  touchcancel?: (event: TouchEvent) => void;

  mousemove?: (event: MouseEvent) => void;
  mouseup?: (event: MouseEvent) => void;
}

/**
 * Creates a new splitter element.
 */
export class Splitter extends HTMLDivElement {
  private resizeNextElement_: boolean = false;
  private handlers_: SplitterHandlers|null = null;
  private startX_: number = 0;
  private startWidth_: number = 0;

  /**
   * Initializes the element.
   */
  initialize(..._args: any[]) {
    this.addEventListener('mousedown', this.handleMouseDown_.bind(this), true);
    this.addEventListener(
        'touchstart', this.handleTouchStart_.bind(this), true);
    this.resizeNextElement_ = false;
  }

  /**
   * @param resizeNext True if resize the next element. By default, splitter
   *     resizes previous (left) element.
   */
  set resizeNextElement(resizeNext: boolean) {
    this.resizeNextElement_ = resizeNext;
  }

  /**
   * Starts the dragging of the splitter. Adds listeners for mouse or touch
   * events and calls splitter drag start handler.
   * @param clientX X position of the mouse or touch event that
   *                         started the drag.
   * @param isTouchEvent True if the drag started by touch event.
   */
  startDrag(clientX: number, isTouchEvent: boolean) {
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
    for (const [eventType, handler] of Object.entries(this.handlers_)) {
      doc.addEventListener(eventType, handler, true);
    }

    this.startX_ = clientX;
    this.handleSplitterDragStart();
  }

  /**
   * Ends the dragging of the splitter. Removes listeners set in startDrag
   * and calls splitter drag end handler.
   */
  private endDrag_() {
    const doc = this.ownerDocument;
    if (this.handlers_) {
      for (const [eventType, handler] of Object.entries(this.handlers_)) {
        doc.removeEventListener(eventType, handler, true);
      }
    }
    this.handlers_ = null;
    this.handleSplitterDragEnd();
  }

  private getResizeTarget_(): HTMLElement {
    return (this.resizeNextElement_ ? this.nextElementSibling! :
                                      this.previousElementSibling!) as
        HTMLElement;
  }

  /**
   * Calculate width to resize target element.
   * @param deltaX horizontal drag amount
   */
  private calcDeltaX_(deltaX: number): number {
    return this.resizeNextElement_ ? -deltaX : deltaX;
  }

  /**
   * Handles the mousedown event which starts the dragging of the splitter.
   */
  private handleMouseDown_(e: MouseEvent) {
    if (e.button) {
      return;
    }
    this.startDrag(e.clientX, false);
    // Default action is to start selection and to move focus.
    e.preventDefault();
  }

  /**
   * Handles the touchstart event which starts the dragging of the splitter.
   */
  private handleTouchStart_(e: TouchEvent) {
    if (e.touches.length === 1) {
      this.startDrag(e.touches[0]!.clientX, true);
      if (e.cancelable) {
        e.preventDefault();
      }
    }
  }

  /**
   * Handles the mousemove event which moves the splitter as the user moves
   * the mouse.
   */
  private handleMouseMove_(e: MouseEvent) {
    this.handleMove_(e.clientX);
  }

  /**
   * Handles the touch move event.
   * @param e The touch event.
   */
  private handleTouchMove_(e: TouchEvent) {
    if (e.touches.length === 1) {
      this.handleMove_(e.touches[0]!.clientX);
    }
  }

  /**
   * Common part of handling mousemove and touchmove. Calls splitter drag
   * move handler.
   * @param clientX X position of the mouse or touch event.
   */
  private handleMove_(clientX: number) {
    const rtl =
        this.ownerDocument.defaultView!.getComputedStyle(this).direction ===
        'rtl';
    const dirMultiplier = rtl ? -1 : 1;
    const deltaX = dirMultiplier * (clientX - this.startX_);
    this.handleSplitterDragMove(deltaX);
  }

  /**
   * Handles the mouse up event which ends the dragging of the splitter.
   */
  private handleMouseUp_(_e: MouseEvent) {
    this.endDrag_();
  }

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
        parseFloat(doc.defaultView!.getComputedStyle(targetElement).width) +
        targetElement.offsetWidth - targetElement.clientWidth;

    this.classList.add('splitter-active');
  }

  /**
   * Handles splitter moves. Updates width of the element being resized.
   * @param deltaX The change of splitter horizontal position.
   */
  handleSplitterDragMove(deltaX: number) {
    const targetElement = this.getResizeTarget_();
    const newWidth = this.startWidth_ + this.calcDeltaX_(deltaX);
    targetElement.style.width = newWidth + 'px';
    dispatchSimpleEvent(this, 'dragmove');
  }

  /**
   * Handles end of the splitter dragging. This fires a 'resize' event if
   * the size changed.
   */
  handleSplitterDragEnd() {
    // Check if the size changed.
    const targetElement = this.getResizeTarget_();
    const doc = targetElement.ownerDocument;
    const computedWidth =
        parseFloat(doc.defaultView!.getComputedStyle(targetElement).width);
    if (this.startWidth_ !== computedWidth) {
      dispatchSimpleEvent(this, 'resize');
    }

    this.classList.remove('splitter-active');
  }
}
