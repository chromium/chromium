/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/**
 * @fileoverview Caret browsing content script, runs in each frame.
 *
 * The behavior is based on Mozilla's spec whenever possible:
 *   http://www.mozilla.org/access/keyboard/proposal
 *
 * The one exception is that Esc is used to escape out of a form control,
 * rather than their proposed key (which doesn't seem to work in the
 * latest Firefox anyway).
 *
 * Some details about how Chrome selection works, which will help in
 * understanding the code:
 *
 * The Selection object (window.getSelection()) has four components that
 * completely describe the state of the caret or selection:
 *
 * base and anchor: this is the start of the selection, the fixed point.
 * extent and focus: this is the end of the selection, the part that
 *     moves when you hold down shift and press the left or right arrows.
 *
 * When the selection is a cursor, the base, anchor, extent, and focus are
 * all the same.
 *
 * There's only one time when the base and anchor are not the same, or the
 * extent and focus are not the same, and that's when the selection is in
 * an ambiguous state - i.e. it's not clear which edge is the focus and which
 * is the anchor. As an example, if you double-click to select a word, then
 * the behavior is dependent on your next action. If you press Shift+Right,
 * the right edge becomes the focus. But if you press Shift+Left, the left
 * edge becomes the focus.
 *
 * When the selection is in an ambiguous state, the base and extent are set
 * to the position where the mouse clicked, and the anchor and focus are set
 * to the boundaries of the selection.
 *
 * The only way to set the selection and give it direction is to use
 * the non-standard Selection.setBaseAndExtent method. If you try to use
 * Selection.addRange(), the anchor will always be on the left and the focus
 * will always be on the right, making it impossible to manipulate
 * selections that move from right to left.
 *
 * Finally, Chrome will throw an exception if you try to set an invalid
 * selection - a selection where the left and right edges are not the same,
 * but it doesn't span any visible characters. A common example is that
 * there are often many whitespace characters in the DOM that are not
 * visible on the page; trying to select them will fail. Another example is
 * any node that's invisible or not displayed.
 *
 * While there are probably many possible methods to determine what is
 * selectable, this code uses the method of determining if there's a valid
 * bounding box for the range or not - keep moving the cursor forwards until
 * the range from the previous position and candidate next position has a
 * valid bounding box.
 */

Storage.initialize();

/**
 * The class handling the Caret Browsing implementation in the page.
 * Installs a keydown listener that always responds to the F7 key,
 * sets up communication with the background page, and then when caret
 * browsing is enabled, response to various key events to move the caret
 * or selection within the text content of the document. Uses the native
 * Chrome selection wherever possible, but displays its own flashing
 * caret using a DIV because there's no native caret available.
 * @constructor
 */
class CaretBrowsing {
  constructor() {
    /**
     * Tracks whether to keep caret browsing enabled on this page even when it's
     * flipped off. This is used on the options page.
     * @type {boolean}
     */
    this.forceEnabled = false;

    /**
     * Tracks whether this window / iframe is focused. The caret isn't shown on
     * pages that are not focused, which is especially important so that carets
     * aren't shown in two iframes of the same tab.
     * @type {boolean}
     */
    this.isWindowFocused = false;

    /**
     * Tracks whether the caret is actually visible. This is true only if
     * Storage.enabled and this.isWindowFocused are both true.
     * @type {boolean}
     */
    this.isCaretVisible = false;

    /**
     * The actual caret HTML element, an absolute-positioned flashing line.
     * @type {Element}
     */
    this.caretElement;

    /**
     * The x-position of the caret, in absolute pixels.
     * @type {number}
     */
    this.caretX = 0;

    /**
     * The y-position of the caret, in absolute pixels.
     * @type {number}
     */
    this.caretY = 0;

    /**
     * The width of the caret in pixels.
     * @type {number}
     */
    this.caretWidth = 0;

    /**
     * The height of the caret in pixels.
     * @type {number}
     */
    this.caretHeight = 0;

    /**
     * The caret's foreground color.
     * @type {string}
     */
    this.caretForeground = '#000';

    /**
     * The caret's background color.
     * @type {string}
     */
    this.caretBackground = '#fff';

    /**
     * Tracks whether the selection is collapsed, i.e. are the start and end
     * locations the same? If so, our blinking caret image is shown; otherwise
     * the Chrome selection is shown.
     * @type {boolean}
     */
    this.isSelectionCollapsed = false;

    /**
     * The id returned by window.setInterval for our blink function, so
     * we can cancel it when caret browsing is disabled.
     * @type {?number}
     */
    this.blinkFunctionId = null;

    /**
     * The desired x-coordinate to match when moving the caret up and down.
     * To match the behavior as documented in Mozilla's caret browsing spec
     * (http://www.mozilla.org/access/keyboard/proposal), we keep track of the
     * initial x position when the user starts moving the caret up and down,
     * so that the x position doesn't drift as you move throughout lines, but
     * stays as close as possible to the initial position. This is reset when
     * moving left or right or clicking.
     * @type {?number}
     */
    this.targetX = null;

    /**
     * A flag that flips on or off as the caret blinks.
     * @type {boolean}
     */
    this.blinkFlag = true;

    /**
     * Whether or not we're on a Mac - which affects modifier keys.
     * @type {boolean}
     */
    this.isMac = (navigator.appVersion.indexOf("Mac") != -1);

    this.init();
  }

  /**
   * If there's no initial selection, set the cursor just before the
   * first text character in the document.
   */
  setInitialCursor() {
    const sel = window.getSelection();
    if (sel.rangeCount > 0) {
      return;
    }

    const start = new Cursor(document.body, 0, '');
    const end = new Cursor(document.body, 0, '');
    const nodesCrossed = [];
    const result = TraverseUtil.getNextChar(start, end, nodesCrossed, true);
    if (result == null) {
      return;
    }
    SelectionUtil.setAndValidateSelection(start, start);
  }

  /**
   * Set the caret element's normal style, i.e. not when animating.
   */
  setCaretElementNormalStyle() {
    const element = this.caretElement;
    element.className = 'CaretBrowsing_Caret';
    element.style.opacity = this.isSelectionCollapsed ? '1.0' : '0.0';
    element.style.left = this.caretX + 'px';
    element.style.top = this.caretY + 'px';
    element.style.width = this.caretWidth + 'px';
    element.style.height = this.caretHeight + 'px';
    element.style.color = this.caretForeground;
  }

  /**
   * Animate the caret element into the normal style.
   */
  animateCaretElement() {
    const element = this.caretElement;
    element.style.left = (this.caretX - 50) + 'px';
    element.style.top = (this.caretY - 100) + 'px';
    element.style.width = (this.caretWidth + 100) + 'px';
    element.style.height = (this.caretHeight + 200) + 'px';
    element.className = 'CaretBrowsing_AnimateCaret';

    // Start the animation. The setTimeout is so that the old values will get
    // applied first, so we can animate to the new values.
    window.setTimeout(() => {
      if (!this.caretElement) {
        return;
      }
      this.setCaretElementNormalStyle();
      element.style['transition'] = 'all 0.8s ease-in';
      const listener = () => {
        element.removeEventListener(
            'transitionend', listener, false);
        element.style['transition'] = 'none';
      }
      element.addEventListener(
          'transitionend', listener, false);
    }, 0);
  }

  /**
   * Quick flash and then show the normal caret style.
   */
  flashCaretElement() {
    const x = this.caretX;
    const y = this.caretY;
    const height = this.caretHeight;

    const vert = document.createElement('div');
    vert.className = 'CaretBrowsing_FlashVert';
    vert.style.left = (x - 6) + 'px';
    vert.style.top = (y - 100) + 'px';
    vert.style.width = '11px';
    vert.style.height = (200) + 'px';
    document.body.appendChild(vert);

    window.setTimeout(() => {
      document.body.removeChild(vert);
      if (this.caretElement) {
        this.setCaretElementNormalStyle();
      }
    }, 250);
  }

  /**
   * Create the caret element. This assumes that caretX, caretY,
   * caretWidth, and caretHeight have all been set. The caret is
   * animated in so the user can find it when it first appears.
   */
  createCaretElement() {
    const element = document.createElement('div');
    element.className = 'CaretBrowsing_Caret';
    document.body.appendChild(element);
    this.caretElement = element;

    if (Storage.onEnable === FlourishType.ANIMATE) {
      this.animateCaretElement();
    } else if (Storage.onEnable === FlourishType.FLASH) {
      this.flashCaretElement();
    } else {
      this.setCaretElementNormalStyle();
    }
  }

  /**
   * Recreate the caret element, triggering any intro animation.
   */
  recreateCaretElement() {
    if (this.caretElement) {
      window.clearInterval(this.blinkFunctionId);
      this.caretElement.parentElement.removeChild(
          this.caretElement);
      this.caretElement = null;
      this.updateIsCaretVisible();
    }
  }

  /**
   * Compute the new location of the caret or selection and update
   * the element as needed.
   * @param {boolean} scrollToSelection If true, will also scroll the page
   *     to the caret / selection location.
   */
  updateCaretOrSelection(scrollToSelection) {
    const previousX = this.caretX;
    const previousY = this.caretY;

    const sel = window.getSelection();
    if (sel.rangeCount == 0) {
      if (this.caretElement) {
        this.isSelectionCollapsed = false;
        this.caretElement.style.opacity = '0.0';
      }
      return;
    }

    const range = sel.getRangeAt(0);
    if (!range) {
      if (this.caretElement) {
        this.isSelectionCollapsed = false;
        this.caretElement.style.opacity = '0.0';
      }
      return;
    }

    if (NodeUtil.isControlThatNeedsArrowKeys(document.activeElement)) {
      let node = document.activeElement;
      this.caretWidth = node.offsetWidth;
      this.caretHeight = node.offsetHeight;
      this.caretX = 0;
      this.caretY = 0;
      while (node.offsetParent) {
        this.caretX += node.offsetLeft;
        this.caretY += node.offsetTop;
        node = node.offsetParent;
      }
      this.isSelectionCollapsed = false;
    } else if (
        range.startOffset != range.endOffset ||
        range.startContainer != range.endContainer) {
      const rect = range.getBoundingClientRect();
      if (!rect) {
        return;
      }
      this.caretX = rect.left + window.pageXOffset;
      this.caretY = rect.top + window.pageYOffset;
      this.caretWidth = rect.width;
      this.caretHeight = rect.height;
      this.isSelectionCollapsed = false;
    } else {
      const rect = SelectionUtil.getCursorRect(new Cursor(
          range.startContainer, range.startOffset,
          TraverseUtil.getNodeText(range.startContainer)));
      this.caretX = rect.left;
      this.caretY = rect.top;
      this.caretWidth = rect.width;
      this.caretHeight = rect.height;
      this.isSelectionCollapsed = true;
    }

    if (!this.caretElement) {
      this.createCaretElement();
    } else {
      const element = this.caretElement;
      if (this.isSelectionCollapsed) {
        element.style.opacity = '1.0';
        element.style.left = this.caretX + 'px';
        element.style.top = this.caretY + 'px';
        element.style.width = this.caretWidth + 'px';
        element.style.height = this.caretHeight + 'px';
      } else {
        element.style.opacity = '0.0';
      }
    }

    let elem = range.startContainer;
    if (elem.constructor == Text)
      elem = elem.parentElement;
    const style = window.getComputedStyle(elem);
    const bg = axs.utils.getBgColor(style, elem);
    const fg = axs.utils.getFgColor(style, elem, bg);
    this.caretBackground = axs.color.colorToString(bg);
    this.caretForeground = axs.color.colorToString(fg);

    if (scrollToSelection) {
      // Scroll just to the "focus" position of the selection,
      // the part the user is manipulating.
      const rect = SelectionUtil.getCursorRect(new Cursor(
          sel.focusNode, sel.focusOffset,
          TraverseUtil.getNodeText(sel.focusNode)));

      const yscroll = window.pageYOffset;
      const pageHeight = window.innerHeight;
      const caretY = rect.top;
      const caretHeight = Math.min(rect.height, 30);
      if (yscroll + pageHeight < caretY + caretHeight) {
        window.scroll(0, (caretY + caretHeight - pageHeight + 100));
      } else if (caretY < yscroll) {
        window.scroll(0, (caretY - 100));
      }
    }

    if (Math.abs(previousX - this.caretX) > 500 ||
        Math.abs(previousY - this.caretY) > 100) {
      if (Storage.onJump === FlourishType.ANIMATE) {
        this.animateCaretElement();
      } else if (Storage.onJump === FlourishType.FLASH) {
        this.flashCaretElement();
      }
    }
  }

  /**
   * Determines if the modifier key is held down that should cause
   * the cursor to move by word rather than by character.
   * @param {Event} evt A keyboard event.
   * @return {boolean} True if the cursor should move by word.
   */
  isMoveByWordEvent(evt) {
    if (this.isMac) {
      return evt.altKey;
    } else {
      return evt.ctrlKey;
    }
  }

  /**
   * Moves the cursor forwards to the next valid position.
   * @param {Cursor} cursor The current cursor location.
   *     On exit, the cursor will be at the next position.
   * @param {Array<Node>} nodesCrossed Any HTML nodes crossed between the
   *     initial and final cursor position will be pushed onto this array.
   * @return {?string} The character reached, or null if the bottom of the
   *     document has been reached.
   */
  forwards(cursor, nodesCrossed) {
    const previousCursor = cursor.clone();
    const result = TraverseUtil.forwardsChar(cursor, nodesCrossed);

    // Work around the fact that TraverseUtil.forwardsChar returns once per
    // char in a block of text, rather than once per possible selection
    // position in a block of text.
    if (result && cursor.node != previousCursor.node && cursor.index > 0) {
      cursor.index = 0;
    }

    return result;
  }

  /**
   * Moves the cursor backwards to the previous valid position.
   * @param {Cursor} cursor The current cursor location.
   *     On exit, the cursor will be at the previous position.
   * @param {Array<Node>} nodesCrossed Any HTML nodes crossed between the
   *     initial and final cursor position will be pushed onto this array.
   * @return {?string} The character reached, or null if the top of the
   *     document has been reached.
   */
  backwards(cursor, nodesCrossed) {
    const previousCursor = cursor.clone();
    const result = TraverseUtil.backwardsChar(cursor, nodesCrossed);

    // Work around the fact that TraverseUtil.backwardsChar returns once per
    // char in a block of text, rather than once per possible selection
    // position in a block of text.
    if (result &&
        cursor.node != previousCursor.node &&
        cursor.index < cursor.text.length) {
      cursor.index = cursor.text.length;
    }

    return result;
  }

  /**
   * Called when the user presses the right arrow. If there's a selection,
   * moves the cursor to the end of the selection range. If it's a cursor,
   * moves past one character.
   * @param {Event} evt The DOM event.
   * @return {boolean} True if the default action should be performed.
   */
  moveRight(evt) {
    this.targetX = null;

    const sel = window.getSelection();
    if (!evt.shiftKey && !SelectionUtil.isCollapsed(sel)) {
      const right = SelectionUtil.makeRightCursor(sel);
      SelectionUtil.setAndValidateSelection(right, right);
      return false;
    }

    const start = SelectionUtil.isAmbiguous(sel) ?
        SelectionUtil.makeLeftCursor(sel) :
        SelectionUtil.makeAnchorCursor(sel);
    const end = SelectionUtil.isAmbiguous(sel) ?
        SelectionUtil.makeRightCursor(sel) :
        SelectionUtil.makeFocusCursor(sel);
    let previousEnd = end.clone();
    const nodesCrossed = [];
    while (true) {
      let result;
      if (this.isMoveByWordEvent(evt)) {
        result = TraverseUtil.getNextWord(previousEnd, end, nodesCrossed);
      } else {
        previousEnd = end.clone();
        result = this.forwards(end, nodesCrossed);
      }

      if (result === null) {
        return this.moveLeft(evt);
      }

      if (SelectionUtil.setAndValidateSelection(
              evt.shiftKey ? start : end, end)) {
        break;
      }
    }

    if (!evt.shiftKey) {
      nodesCrossed.push(end.node);
      NodeUtil.setFocusToFirstFocusable(nodesCrossed);
    }

    return false;
  }

  /**
   * Called when the user presses the left arrow. If there's a selection,
   * moves the cursor to the start of the selection range. If it's a cursor,
   * moves backwards past one character.
   * @param {Event} evt The DOM event.
   * @return {boolean} True if the default action should be performed.
   */
  moveLeft(evt) {
    this.targetX = null;

    const sel = window.getSelection();
    if (!evt.shiftKey && !SelectionUtil.isCollapsed(sel)) {
      const left = SelectionUtil.makeLeftCursor(sel);
      SelectionUtil.setAndValidateSelection(left, left);
      return false;
    }

    const start = SelectionUtil.isAmbiguous(sel) ?
        SelectionUtil.makeLeftCursor(sel) :
        SelectionUtil.makeFocusCursor(sel);
    const end = SelectionUtil.isAmbiguous(sel) ?
        SelectionUtil.makeRightCursor(sel) :
        SelectionUtil.makeAnchorCursor(sel);
    let previousStart = start.clone();
    const nodesCrossed = [];
    while (true) {
      let result;
      if (this.isMoveByWordEvent(evt)) {
        result = TraverseUtil.getPreviousWord(
            start, previousStart, nodesCrossed);
      } else {
        previousStart = start.clone();
        result = this.backwards(start, nodesCrossed);
      }

      if (result === null) {
        break;
      }

      if (SelectionUtil.setAndValidateSelection(
              evt.shiftKey ? end : start, start)) {
        break;
      }
    }

    if (!evt.shiftKey) {
      nodesCrossed.push(start.node);
      NodeUtil.setFocusToFirstFocusable(nodesCrossed);
    }

    return false;
  }


  /**
   * Called when the user presses the down arrow. If there's a selection,
   * moves the cursor to the end of the selection range. If it's a cursor,
   * attempts to move to the equivalent horizontal pixel position in the
   * subsequent line of text. If this is impossible, go to the first character
   * of the next line.
   * @param {Event} evt The DOM event.
   * @return {boolean} True if the default action should be performed.
   */
  moveDown(evt) {
    const sel = window.getSelection();
    if (!evt.shiftKey && !SelectionUtil.isCollapsed(sel)) {
      const right = SelectionUtil.makeRightCursor(sel);
      SelectionUtil.setAndValidateSelection(right, right);
      return false;
    }

    const start = SelectionUtil.isAmbiguous(sel) ?
        SelectionUtil.makeLeftCursor(sel) :
        SelectionUtil.makeAnchorCursor(sel);
    const end = SelectionUtil.isAmbiguous(sel) ?
        SelectionUtil.makeRightCursor(sel) :
        SelectionUtil.makeFocusCursor(sel);
    const endRect = SelectionUtil.getCursorRect(end);
    if (this.targetX === null) {
      this.targetX = endRect.left;
    }
    const previousEnd = end.clone();
    let leftPos = end.clone();
    const rightPos = end.clone();
    let bestPos = null;
    let bestY = null;
    let bestDelta = null;
    let bestHeight = null;
    const nodesCrossed = [];
    let y = -1;
    while (true) {
      if (null === this.forwards(rightPos, nodesCrossed)) {
        if (SelectionUtil.setAndValidateSelection(
                evt.shiftKey ? start : leftPos, leftPos)) {
          break;
        } else {
          return this.moveLeft(evt);
        }
        break;
      }
      const range = document.createRange();
      range.setStart(leftPos.node, leftPos.index);
      range.setEnd(rightPos.node, rightPos.index);
      const rect = range.getBoundingClientRect();
      if (rect && rect.width < rect.height) {
        y = rect.top + window.pageYOffset;

        // Return the best match so far if we get half a line past the best.
        if (bestY != null && y > bestY + bestHeight / 2) {
          if (SelectionUtil.setAndValidateSelection(
                  evt.shiftKey ? start : bestPos, bestPos)) {
            break;
          } else {
            bestY = null;
          }
        }

        // Stop here if we're an entire line the wrong direction
        // (for example, we reached the top of the next column).
        if (y < endRect.top - endRect.height) {
          if (SelectionUtil.setAndValidateSelection(
                  evt.shiftKey ? start : leftPos, leftPos)) {
            break;
          }
        }

        // Otherwise look to see if this current position is on the
        // next line and better than the previous best match, if any.
        if (y >= endRect.top + endRect.height) {
          const deltaLeft = Math.abs(this.targetX - rect.left);
          if ((bestDelta == null || deltaLeft < bestDelta) &&
              (leftPos.node != end.node || leftPos.index != end.index)) {
            bestPos = leftPos.clone();
            bestY = y;
            bestDelta = deltaLeft;
            bestHeight = rect.height;
          }
          const deltaRight = Math.abs(this.targetX - rect.right);
          if (bestDelta == null || deltaRight < bestDelta) {
            bestPos = rightPos.clone();
            bestY = y;
            bestDelta = deltaRight;
            bestHeight = rect.height;
          }

          // Return the best match so far if the deltas are getting worse,
          // not better.
          if (bestDelta != null &&
              deltaLeft > bestDelta &&
              deltaRight > bestDelta) {
            if (SelectionUtil.setAndValidateSelection(
                    evt.shiftKey ? start : bestPos, bestPos)) {
              break;
            } else {
              bestY = null;
            }
          }
        }
      }
      leftPos = rightPos.clone();
    }

    if (!evt.shiftKey) {
      NodeUtil.setFocusToNode(leftPos.node);
    }

    return false;
  }

  /**
   * Called when the user presses the up arrow. If there's a selection,
   * moves the cursor to the start of the selection range. If it's a cursor,
   * attempts to move to the equivalent horizontal pixel position in the
   * previous line of text. If this is impossible, go to the last character
   * of the previous line.
   * @param {Event} evt The DOM event.
   * @return {boolean} True if the default action should be performed.
   */
  moveUp(evt) {
    const sel = window.getSelection();
    if (!evt.shiftKey && !SelectionUtil.isCollapsed(sel)) {
      const left = SelectionUtil.makeLeftCursor(sel);
      SelectionUtil.setAndValidateSelection(left, left);
      return false;
    }

    const start = SelectionUtil.isAmbiguous(sel) ?
        SelectionUtil.makeLeftCursor(sel) :
        SelectionUtil.makeFocusCursor(sel);
    const end = SelectionUtil.isAmbiguous(sel) ?
        SelectionUtil.makeRightCursor(sel) :
        SelectionUtil.makeAnchorCursor(sel);
    const startRect = SelectionUtil.getCursorRect(start);
    if (this.targetX === null) {
      this.targetX = startRect.left;
    }
    const previousStart = start.clone();
    const leftPos = start.clone();
    let rightPos = start.clone();
    let bestPos = null;
    let bestY = null;
    let bestDelta = null;
    let bestHeight = null;
    const nodesCrossed = [];
    let y = 999999;
    while (true) {
      if (null === this.backwards(leftPos, nodesCrossed)) {
        SelectionUtil.setAndValidateSelection(
            evt.shiftKey ? end : rightPos, rightPos);
        break;
      }
      const range = document.createRange();
      range.setStart(leftPos.node, leftPos.index);
      range.setEnd(rightPos.node, rightPos.index);
      const rect = range.getBoundingClientRect();
      if (rect && rect.width < rect.height) {
        y = rect.top + window.pageYOffset;

        // Return the best match so far if we get half a line past the best.
        if (bestY != null && y < bestY - bestHeight / 2) {
          if (SelectionUtil.setAndValidateSelection(
                  evt.shiftKey ? end : bestPos, bestPos)) {
            break;
          } else {
            bestY = null;
          }
        }

        // Exit if we're an entire line the wrong direction
        // (for example, we reached the bottom of the previous column.)
        if (y > startRect.top + startRect.height) {
          if (SelectionUtil.setAndValidateSelection(
                  evt.shiftKey ? end : rightPos, rightPos)) {
            break;
          }
        }

        // Otherwise look to see if this current position is on the
        // next line and better than the previous best match, if any.
        if (y <= startRect.top - startRect.height) {
          const deltaLeft = Math.abs(this.targetX - rect.left);
          if (bestDelta == null || deltaLeft < bestDelta) {
            bestPos = leftPos.clone();
            bestY = y;
            bestDelta = deltaLeft;
            bestHeight = rect.height;
          }
          const deltaRight = Math.abs(this.targetX - rect.right);
          if ((bestDelta == null || deltaRight < bestDelta) &&
              (rightPos.node != start.node || rightPos.index != start.index)) {
            bestPos = rightPos.clone();
            bestY = y;
            bestDelta = deltaRight;
            bestHeight = rect.height;
          }

          // Return the best match so far if the deltas are getting worse,
          // not better.
          if (bestDelta != null &&
              deltaLeft > bestDelta &&
              deltaRight > bestDelta) {
            if (SelectionUtil.setAndValidateSelection(
                    evt.shiftKey ? end : bestPos, bestPos)) {
              break;
            } else {
              bestY = null;
            }
          }
        }
      }
      rightPos = leftPos.clone();
    }

    if (!evt.shiftKey) {
      NodeUtil.setFocusToNode(rightPos.node);
    }

    return false;
  }

  /**
   * Set the document's selection to surround a control, so that the next
   * arrow key they press will allow them to explore the content before
   * or after a given control.
   * @param {Node} control The control to escape from.
   */
  escapeFromControl(control) {
    control.blur();

    let start = new Cursor(control, 0, '');
    let previousStart = start.clone();
    let end = new Cursor(control, 0, '');
    let previousEnd = end.clone();

    const nodesCrossed = [];
    while (true) {
      if (null === this.backwards(start, nodesCrossed)) {
        break;
      }

      const r = document.createRange();
      r.setStart(start.node, start.index);
      r.setEnd(previousStart.node, previousStart.index);
      if (r.getBoundingClientRect()) {
        break;
      }
      previousStart = start.clone();
    }
    while (true) {
      if (null === this.forwards(end, nodesCrossed)) {
        break;
      }
      if (NodeUtil.isDescendantOfNode(end.node, control)) {
        previousEnd = end.clone();
        continue;
      }

      const r = document.createRange();
      r.setStart(previousEnd.node, previousEnd.index);
      r.setEnd(end.node, end.index);
      if (r.getBoundingClientRect()) {
        break;
      }
    }

    if (!NodeUtil.isDescendantOfNode(previousStart.node, control)) {
      start = previousStart.clone();
    }

    if (!NodeUtil.isDescendantOfNode(previousEnd.node, control)) {
      end = previousEnd.clone();
    }

    SelectionUtil.setAndValidateSelection(start, end);

    window.setTimeout(() => {
      this.updateCaretOrSelection(true);
    }, 0);
  }

  /**
   * Toggle whether caret browsing is enabled or not.
   */
  toggle() {
    if (this.forceEnabled) {
      this.recreateCaretElement();
      return;
    }

    Storage.enabled = !Storage.enabled;
    this.updateIsCaretVisible();
  }

  /**
   * Event handler, called when a key is pressed.
   * @param {Event} evt The DOM event.
   * @return {boolean} True if the default action should be performed.
   */
  onKeyDown(evt) {
    if (evt.defaultPrevented) {
      return;
    }

    if (evt.keyCode == 118) {  // F7
      this.toggle();
    }

    if (!Storage.enabled) {
      return true;
    }

    if (evt.target &&
        NodeUtil.isControlThatNeedsArrowKeys(
            /** @type (Node) */ (evt.target))) {
      if (evt.keyCode == 27) {
        this.escapeFromControl(/** @type {Node} */(evt.target));
        evt.preventDefault();
        evt.stopPropagation();
        return false;
      } else {
        return true;
      }
    }

    // If the current selection doesn't have a range, try to escape out of
    // the current control. If that fails, return so we don't fail whe
    // trying to move the cursor or selection.
    let sel = window.getSelection();
    if (sel.rangeCount == 0) {
      if (document.activeElement) {
        this.escapeFromControl(document.activeElement);
        sel = window.getSelection();
      }

      if (sel.rangeCount == 0) {
        return true;
      }
    }

    if (this.caretElement) {
      this.caretElement.style.visibility = 'visible';
      this.blinkFlag = true;
    }

    let result = true;
    switch (evt.keyCode) {
      case 37:
        result = this.moveLeft(evt);
        break;
      case 38:
        result = this.moveUp(evt);
        break;
      case 39:
        result = this.moveRight(evt);
        break;
      case 40:
        result = this.moveDown(evt);
        break;
    }

    if (result == false) {
      evt.preventDefault();
      evt.stopPropagation();
    }

    window.setTimeout(() => {
      this.updateCaretOrSelection(result == false);
    }, 0);

    return result;
  }

  /**
   * Event handler, called when the mouse is clicked. Chrome already
   * sets the selection when the mouse is clicked, all we need to do is
   * update our cursor.
   * @param {Event} evt The DOM event.
   * @return {boolean} True if the default action should be performed.
   */
  onClick(evt) {
    if (!Storage.enabled) {
      return true;
    }
    window.setTimeout(() => {
      this.targetX = null;
      this.updateCaretOrSelection(false);
    }, 0);
    return true;
  }

  /**
   * Called at a regular interval. Blink the cursor by changing its visibility.
   */
  caretBlinkFunction() {
    if (this.caretElement) {
      if (this.blinkFlag) {
        this.caretElement.style.backgroundColor =
            this.caretForeground;
        this.blinkFlag = false;
      } else {
        this.caretElement.style.backgroundColor =
            this.caretBackground;
        this.blinkFlag = true;
      }
    }
  }

  /**
   * Update whether or not the caret is visible, based on whether caret browsing
   * is enabled and whether this window / iframe has focus.
   */
  updateIsCaretVisible() {
    this.isCaretVisible =
        (Storage.enabled && this.isWindowFocused);
    if (this.isCaretVisible && !this.caretElement) {
      this.setInitialCursor();
      this.updateCaretOrSelection(true);
      if (this.caretElement) {
        this.blinkFunctionId = window.setInterval(
            this.caretBlinkFunction, 500);
      }
    } else if (!this.isCaretVisible &&
               this.caretElement) {
      window.clearInterval(this.blinkFunctionId);
      if (this.caretElement) {
        this.isSelectionCollapsed = false;
        this.caretElement.parentElement.removeChild(
            this.caretElement);
        this.caretElement = null;
      }
    }
  }

  /**
   * Called when the prefs get updated.
   */
  onPrefsUpdated() {
    this.recreateCaretElement();
  }

  /**
   * Called when this window / iframe gains focus.
   */
  onWindowFocus() {
    this.isWindowFocused = true;
    this.updateIsCaretVisible();
  }

  /**
   * Called when this window / iframe loses focus.
   */
  onWindowBlur() {
    this.isWindowFocused = false;
    this.updateIsCaretVisible();
  }

  /**
   * Initializes caret browsing by adding event listeners and extension
   * message listeners.
   */
  init() {
    this.isWindowFocused = document.hasFocus();

    document.addEventListener('keydown', this.onKeyDown.bind(this), false);
    document.addEventListener('click', this.onClick.bind(this), false);
    window.addEventListener('focus', this.onWindowFocus.bind(this), false);
    window.addEventListener('blur', this.onWindowBlur.bind(this), false);
  }
}

window.setTimeout(() => {

  // Make sure the script only loads once.
  if (!window['caretBrowsingLoaded']) {
    window['caretBrowsingLoaded'] = true;
    window.caretBrowsing = new CaretBrowsing();

    if (document.body.getAttribute('caretbrowsing') == 'on') {
      caretBrowsing.forceEnabled = true;
      Storage.enabled = true;
      caretBrowsing.updateIsCaretVisible();
    }

    Storage.ENABLED.listeners.push(() => caretBrowsing.onPrefsUpdated());
    Storage.ON_ENABLE.listeners.push(() => caretBrowsing.onPrefsUpdated());
    Storage.ON_JUMP.listeners.push(() => caretBrowsing.onPrefsUpdated());

    caretBrowsing.onPrefsUpdated();
  }

}, 0);
