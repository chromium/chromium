/* Copyright (c) 2014 The Chromium Authors. All rights reserved.
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

/**
 * Return whether a node is focusable. This includes nodes whose tabindex
 * attribute is set to "-1" explicitly - these nodes are not in the tab
 * order, but they should still be focused if the user navigates to them
 * using linear or smart DOM navigation.
 *
 * Note that when the tabIndex property of an Element is -1, that doesn't
 * tell us whether the tabIndex attribute is missing or set to "-1" explicitly,
 * so we have to check the attribute.
 *
 * @param {Object} targetNode The node to check if it's focusable.
 * @return {boolean} True if the node is focusable.
 */
function isFocusable(targetNode) {
  if (!targetNode || typeof(targetNode.tabIndex) != 'number') {
    return false;
  }

  if (targetNode.tabIndex >= 0) {
    return true;
  }

  if (targetNode.hasAttribute &&
      targetNode.hasAttribute('tabindex') &&
      targetNode.getAttribute('tabindex') == '-1') {
    return true;
  }

  return false;
}

/**
 * Determines whether or not a node is or is the descendant of another node.
 *
 * @param {Object} node The node to be checked.
 * @param {Object} ancestor The node to see if it's a descendant of.
 * @return {boolean} True if the node is ancestor or is a descendant of it.
 */
function isDescendantOfNode(node, ancestor) {
  while (node && ancestor) {
    if (node.isSameNode(ancestor)) {
      return true;
    }
    node = node.parentNode;
  }
  return false;
}



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
var CaretBrowsing = function() {};

/**
 * Is caret browsing enabled?
 * @type {boolean}
 */
CaretBrowsing.isEnabled = false;

/**
 * Keep it enabled even when flipped off (for the options page)?
 * @type {boolean}
 */
CaretBrowsing.forceEnabled = false;

/**
 * What to do when the caret appears?
 * @type {string}
 */
CaretBrowsing.onEnable;

/**
 * What to do when the caret jumps?
 * @type {string}
 */
CaretBrowsing.onJump;

/**
 * Is this window / iframe focused? We won't show the caret if not,
 * especially so that carets aren't shown in two iframes of the same
 * tab.
 * @type {boolean}
 */
CaretBrowsing.isWindowFocused = false;

/**
 * Is the caret actually visible? This is true only if isEnabled and
 * isWindowFocused are both true.
 * @type {boolean}
 */
CaretBrowsing.isCaretVisible = false;

/**
 * The actual caret element, an absolute-positioned flashing line.
 * @type {Element}
 */
CaretBrowsing.caretElement;

/**
 * The x-position of the caret, in absolute pixels.
 * @type {number}
 */
CaretBrowsing.caretX = 0;

/**
 * The y-position of the caret, in absolute pixels.
 * @type {number}
 */
CaretBrowsing.caretY = 0;

/**
 * The width of the caret in pixels.
 * @type {number}
 */
CaretBrowsing.caretWidth = 0;

/**
 * The height of the caret in pixels.
 * @type {number}
 */
CaretBrowsing.caretHeight = 0;

/**
 * The foregroundc color.
 * @type {string}
 */
CaretBrowsing.caretForeground = '#000';

/**
 * The backgroundc color.
 * @type {string}
 */
CaretBrowsing.caretBackground = '#fff';

/**
 * Is the selection collapsed, i.e. are the start and end locations
 * the same? If so, our blinking caret image is shown; otherwise
 * the Chrome selection is shown.
 * @type {boolean}
 */
CaretBrowsing.isSelectionCollapsed = false;

/**
 * The id returned by window.setInterval for our blink function, so
 * we can cancel it when caret browsing is disabled.
 * @type {number?}
 */
CaretBrowsing.blinkFunctionId = null;

/**
 * The desired x-coordinate to match when moving the caret up and down.
 * To match the behavior as documented in Mozilla's caret browsing spec
 * (http://www.mozilla.org/access/keyboard/proposal), we keep track of the
 * initial x position when the user starts moving the caret up and down,
 * so that the x position doesn't drift as you move throughout lines, but
 * stays as close as possible to the initial position. This is reset when
 * moving left or right or clicking.
 * @type {number?}
 */
CaretBrowsing.targetX = null;

/**
 * A flag that flips on or off as the caret blinks.
 * @type {boolean}
 */
CaretBrowsing.blinkFlag = true;

/**
 * Whether or not we're on a Mac - affects modifier keys.
 * @type {boolean}
 */
CaretBrowsing.isMac = (navigator.appVersion.indexOf("Mac") != -1);

/**
 * Check if a node is a control that normally allows the user to interact
 * with it using arrow keys. We won't override the arrow keys when such a
 * control has focus, the user must press Escape to do caret browsing outside
 * that control.
 * @param {Node} node A node to check.
 * @return {boolean} True if this node is a control that the user can
 *     interact with using arrow keys.
 */
CaretBrowsing.isControlThatNeedsArrowKeys = function(node) {
  if (!node) {
    return false;
  }

  if (node == document.body || node != document.activeElement) {
    return false;
  }

  if (node.constructor == HTMLSelectElement) {
    return true;
  }

  if (node.constructor == HTMLInputElement) {
    switch (node.type) {
      case 'email':
      case 'number':
      case 'password':
      case 'search':
      case 'text':
      case 'tel':
      case 'url':
      case '':
        return true;  // All of these are text boxes.
      case 'datetime':
      case 'datetime-local':
      case 'date':
      case 'month':
      case 'radio':
      case 'range':
      case 'week':
        return true;  // These are other input elements that use arrows.
    }
  }

  // Handle focusable ARIA controls.
  if (node.getAttribute && isFocusable(node)) {
    var role = node.getAttribute('role');
    switch (role) {
      case 'combobox':
      case 'grid':
      case 'gridcell':
      case 'listbox':
      case 'menu':
      case 'menubar':
      case 'menuitem':
      case 'menuitemcheckbox':
      case 'menuitemradio':
      case 'option':
      case 'radiogroup':
      case 'scrollbar':
      case 'slider':
      case 'spinbutton':
      case 'tab':
      case 'tablist':
      case 'textbox':
      case 'tree':
      case 'treegrid':
      case 'treeitem':
        return true;
    }
  }

  return false;
};

/**
 * If there's no initial selection, set the cursor just before the
 * first text character in the document.
 */
CaretBrowsing.setInitialCursor = function() {
  var sel = window.getSelection();
  if (sel.rangeCount > 0) {
    return;
  }

  var start = new Cursor(document.body, 0, '');
  var end = new Cursor(document.body, 0, '');
  var nodesCrossed = [];
  var result = TraverseUtil.getNextChar(start, end, nodesCrossed, true);
  if (result == null) {
    return;
  }
  CaretBrowsing.setAndValidateSelection(start, start);
};

/**
 * Set focus to a node if it's focusable. If it's an input element,
 * select the text, otherwise it doesn't appear focused to the user.
 * Every other control behaves normally if you just call focus() on it.
 * @param {Node} node The node to focus.
 * @return {boolean} True if the node was focused.
 */
CaretBrowsing.setFocusToNode = function(node) {
  while (node && node != document.body) {
    if (isFocusable(node) && node.constructor != HTMLIFrameElement) {
      node.focus();
      if (node.constructor == HTMLInputElement && node.select) {
        node.select();
      }
      return true;
    }
    node = node.parentNode;
  }

  return false;
};

/**
 * Set focus to the first focusable node in the given list.
 * select the text, otherwise it doesn't appear focused to the user.
 * Every other control behaves normally if you just call focus() on it.
 * @param {Array<Node>} nodeList An array of nodes to focus.
 * @return {boolean} True if the node was focused.
 */
CaretBrowsing.setFocusToFirstFocusable = function(nodeList) {
  for (var i = 0; i < nodeList.length; i++) {
    if (CaretBrowsing.setFocusToNode(nodeList[i])) {
      return true;
    }
  }
  return false;
};

/**
 * Set the caret element's normal style, i.e. not when animating.
 */
CaretBrowsing.setCaretElementNormalStyle = function() {
  var element = CaretBrowsing.caretElement;
  element.className = 'CaretBrowsing_Caret';
  element.style.opacity = CaretBrowsing.isSelectionCollapsed ? '1.0' : '0.0';
  element.style.left = CaretBrowsing.caretX + 'px';
  element.style.top = CaretBrowsing.caretY + 'px';
  element.style.width = CaretBrowsing.caretWidth + 'px';
  element.style.height = CaretBrowsing.caretHeight + 'px';
  element.style.color = CaretBrowsing.caretForeground;
};

/**
 * Animate the caret element into the normal style.
 */
CaretBrowsing.animateCaretElement = function() {
  var element = CaretBrowsing.caretElement;
  element.style.left = (CaretBrowsing.caretX - 50) + 'px';
  element.style.top = (CaretBrowsing.caretY - 100) + 'px';
  element.style.width = (CaretBrowsing.caretWidth + 100) + 'px';
  element.style.height = (CaretBrowsing.caretHeight + 200) + 'px';
  element.className = 'CaretBrowsing_AnimateCaret';

  // Start the animation. The setTimeout is so that the old values will get
  // applied first, so we can animate to the new values.
  window.setTimeout(function() {
    if (!CaretBrowsing.caretElement) {
      return;
    }
    CaretBrowsing.setCaretElementNormalStyle();
    element.style['transition'] = 'all 0.8s ease-in';
    function listener() {
      element.removeEventListener(
          'transitionend', listener, false);
      element.style['transition'] = 'none';
    }
    element.addEventListener(
        'transitionend', listener, false);
  }, 0);
};

/**
 * Quick flash and then show the normal caret style.
 */
CaretBrowsing.flashCaretElement = function() {
  var x = CaretBrowsing.caretX;
  var y = CaretBrowsing.caretY;
  var height = CaretBrowsing.caretHeight;

  var vert = document.createElement('div');
  vert.className = 'CaretBrowsing_FlashVert';
  vert.style.left = (x - 6) + 'px';
  vert.style.top = (y - 100) + 'px';
  vert.style.width = '11px';
  vert.style.height = (200) + 'px';
  document.body.appendChild(vert);

  window.setTimeout(function() {
    document.body.removeChild(vert);
    if (CaretBrowsing.caretElement) {
      CaretBrowsing.setCaretElementNormalStyle();
    }
  }, 250);
};

/**
 * Create the caret element. This assumes that caretX, caretY,
 * caretWidth, and caretHeight have all been set. The caret is
 * animated in so the user can find it when it first appears.
 */
CaretBrowsing.createCaretElement = function() {
  var element = document.createElement('div');
  element.className = 'CaretBrowsing_Caret';
  document.body.appendChild(element);
  CaretBrowsing.caretElement = element;

  if (CaretBrowsing.onEnable == 'anim') {
    CaretBrowsing.animateCaretElement();
  } else if (CaretBrowsing.onEnable == 'flash') {
    CaretBrowsing.flashCaretElement();
  } else {
    CaretBrowsing.setCaretElementNormalStyle();
  }
};

/**
 * Recreate the caret element, triggering any intro animation.
 */
CaretBrowsing.recreateCaretElement = function() {
  if (CaretBrowsing.caretElement) {
    window.clearInterval(CaretBrowsing.blinkFunctionId);
    CaretBrowsing.caretElement.parentElement.removeChild(
        CaretBrowsing.caretElement);
    CaretBrowsing.caretElement = null;
    CaretBrowsing.updateIsCaretVisible();
  }
};

/**
 * Get the rectangle for a cursor position. This is tricky because
 * you can't get the bounding rectangle of an empty range, so this function
 * computes the rect by trying a range including one character earlier or
 * later than the cursor position.
 * @param {Cursor} cursor A single cursor position.
 * @return {{left: number, top: number, width: number, height: number}}
 *     The bounding rectangle of the cursor.
 */
CaretBrowsing.getCursorRect = function(cursor) {
  var node = cursor.node;
  var index = cursor.index;
  var rect = {
    left: 0,
    top: 0,
    width: 1,
    height: 0
  };
  if (node.constructor == Text) {
    var left = index;
    var right = index;
    var max = node.data.length;
    var newRange = document.createRange();
    while (left > 0 || right < max) {
      if (left > 0) {
        left--;
        newRange.setStart(node, left);
        newRange.setEnd(node, index);
        var rangeRect = newRange.getBoundingClientRect();
        if (rangeRect && rangeRect.width && rangeRect.height) {
          rect.left = rangeRect.right;
          rect.top = rangeRect.top;
          rect.height = rangeRect.height;
          break;
        }
      }
      if (right < max) {
        right++;
        newRange.setStart(node, index);
        newRange.setEnd(node, right);
        var rangeRect = newRange.getBoundingClientRect();
        if (rangeRect && rangeRect.width && rangeRect.height) {
          rect.left = rangeRect.left;
          rect.top = rangeRect.top;
          rect.height = rangeRect.height;
          break;
        }
      }
    }
  } else {
    rect.height = node.offsetHeight;
    while (node !== null) {
      rect.left += node.offsetLeft;
      rect.top += node.offsetTop;
      node = node.offsetParent;
    }
  }
  rect.left += window.pageXOffset;
  rect.top += window.pageYOffset;
  return rect;
};

/**
 * Compute the new location of the caret or selection and update
 * the element as needed.
 * @param {boolean} scrollToSelection If true, will also scroll the page
 *     to the caret / selection location.
 */
CaretBrowsing.updateCaretOrSelection = function(scrollToSelection) {
  var previousX = CaretBrowsing.caretX;
  var previousY = CaretBrowsing.caretY;

  var sel = window.getSelection();
  if (sel.rangeCount == 0) {
    if (CaretBrowsing.caretElement) {
      CaretBrowsing.isSelectionCollapsed = false;
      CaretBrowsing.caretElement.style.opacity = '0.0';
    }
    return;
  }

  var range = sel.getRangeAt(0);
  if (!range) {
    if (CaretBrowsing.caretElement) {
      CaretBrowsing.isSelectionCollapsed = false;
      CaretBrowsing.caretElement.style.opacity = '0.0';
    }
    return;
  }

  if (CaretBrowsing.isControlThatNeedsArrowKeys(document.activeElement)) {
    var node = document.activeElement;
    CaretBrowsing.caretWidth = node.offsetWidth;
    CaretBrowsing.caretHeight = node.offsetHeight;
    CaretBrowsing.caretX = 0;
    CaretBrowsing.caretY = 0;
    while (node.offsetParent) {
      CaretBrowsing.caretX += node.offsetLeft;
      CaretBrowsing.caretY += node.offsetTop;
      node = node.offsetParent;
    }
    CaretBrowsing.isSelectionCollapsed = false;
  } else if (range.startOffset != range.endOffset ||
             range.startContainer != range.endContainer) {
    var rect = range.getBoundingClientRect();
    if (!rect) {
      return;
    }
    CaretBrowsing.caretX = rect.left + window.pageXOffset;
    CaretBrowsing.caretY = rect.top + window.pageYOffset;
    CaretBrowsing.caretWidth = rect.width;
    CaretBrowsing.caretHeight = rect.height;
    CaretBrowsing.isSelectionCollapsed = false;
  } else {
    var rect = CaretBrowsing.getCursorRect(
        new Cursor(range.startContainer,
                   range.startOffset,
                   TraverseUtil.getNodeText(range.startContainer)));
    CaretBrowsing.caretX = rect.left;
    CaretBrowsing.caretY = rect.top;
    CaretBrowsing.caretWidth = rect.width;
    CaretBrowsing.caretHeight = rect.height;
    CaretBrowsing.isSelectionCollapsed = true;
  }

  if (!CaretBrowsing.caretElement) {
    CaretBrowsing.createCaretElement();
  } else {
    var element = CaretBrowsing.caretElement;
    if (CaretBrowsing.isSelectionCollapsed) {
      element.style.opacity = '1.0';
      element.style.left = CaretBrowsing.caretX + 'px';
      element.style.top = CaretBrowsing.caretY + 'px';
      element.style.width = CaretBrowsing.caretWidth + 'px';
      element.style.height = CaretBrowsing.caretHeight + 'px';
    } else {
      element.style.opacity = '0.0';
    }
  }

  var elem = range.startContainer;
  if (elem.constructor == Text)
    elem = elem.parentElement;
  var style = window.getComputedStyle(elem);
  var bg = axs.utils.getBgColor(style, elem);
  var fg = axs.utils.getFgColor(style, elem, bg);
  CaretBrowsing.caretBackground = axs.color.colorToString(bg);
  CaretBrowsing.caretForeground = axs.color.colorToString(fg);

  if (scrollToSelection) {
    // Scroll just to the "focus" position of the selection,
    // the part the user is manipulating.
    var rect = CaretBrowsing.getCursorRect(
        new Cursor(sel.focusNode, sel.focusOffset,
                   TraverseUtil.getNodeText(sel.focusNode)));

    var yscroll = window.pageYOffset;
    var pageHeight = window.innerHeight;
    var caretY = rect.top;
    var caretHeight = Math.min(rect.height, 30);
    if (yscroll + pageHeight < caretY + caretHeight) {
      window.scroll(0, (caretY + caretHeight - pageHeight + 100));
    } else if (caretY < yscroll) {
      window.scroll(0, (caretY - 100));
    }
  }

  if (Math.abs(previousX - CaretBrowsing.caretX) > 500 ||
      Math.abs(previousY - CaretBrowsing.caretY) > 100) {
    if (CaretBrowsing.onJump == 'anim') {
      CaretBrowsing.animateCaretElement();
    } else if (CaretBrowsing.onJump == 'flash') {
      CaretBrowsing.flashCaretElement();
    }
  }
};

/**
 * Return true if the selection directionality is ambiguous, which happens
 * if, for example, the user double-clicks in the middle of a word to select
 * it. In that case, the selection should extend by the right edge if the
 * user presses right, and by the left edge if the user presses left.
 * @param {Selection} sel The selection.
 * @return {boolean} True if the selection directionality is ambiguous.
 */
CaretBrowsing.isAmbiguous = function(sel) {
  return (sel.anchorNode != sel.baseNode ||
          sel.anchorOffset != sel.baseOffset ||
          sel.focusNode != sel.extentNode ||
          sel.focusOffset != sel.extentOffset);
};

/**
 * Create a Cursor from the anchor position of the selection, the
 * part that doesn't normally move.
 * @param {Selection} sel The selection.
 * @return {Cursor} A cursor pointing to the selection's anchor location.
 */
CaretBrowsing.makeAnchorCursor = function(sel) {
  return new Cursor(sel.anchorNode, sel.anchorOffset,
                    TraverseUtil.getNodeText(sel.anchorNode));
};

/**
 * Create a Cursor from the focus position of the selection.
 * @param {Selection} sel The selection.
 * @return {Cursor} A cursor pointing to the selection's focus location.
 */
CaretBrowsing.makeFocusCursor = function(sel) {
  return new Cursor(sel.focusNode, sel.focusOffset,
                    TraverseUtil.getNodeText(sel.focusNode));
};

/**
 * Create a Cursor from the left boundary of the selection - the boundary
 * closer to the start of the document.
 * @param {Selection} sel The selection.
 * @return {Cursor} A cursor pointing to the selection's left boundary.
 */
CaretBrowsing.makeLeftCursor = function(sel) {
  var range = sel.rangeCount == 1 ? sel.getRangeAt(0) : null;
  if (range &&
      range.endContainer == sel.anchorNode &&
      range.endOffset == sel.anchorOffset) {
    return CaretBrowsing.makeFocusCursor(sel);
  } else {
    return CaretBrowsing.makeAnchorCursor(sel);
  }
};

/**
 * Create a Cursor from the right boundary of the selection - the boundary
 * closer to the end of the document.
 * @param {Selection} sel The selection.
 * @return {Cursor} A cursor pointing to the selection's right boundary.
 */
CaretBrowsing.makeRightCursor = function(sel) {
  var range = sel.rangeCount == 1 ? sel.getRangeAt(0) : null;
  if (range &&
      range.endContainer == sel.anchorNode &&
      range.endOffset == sel.anchorOffset) {
    return CaretBrowsing.makeAnchorCursor(sel);
  } else {
    return CaretBrowsing.makeFocusCursor(sel);
  }
};

/**
 * Try to set the window's selection to be between the given start and end
 * cursors, and return whether or not it was successful.
 * @param {Cursor} start The start position.
 * @param {Cursor} end The end position.
 * @return {boolean} True if the selection was successfully set.
 */
CaretBrowsing.setAndValidateSelection = function(start, end) {
  var sel = window.getSelection();
  sel.setBaseAndExtent(start.node, start.index, end.node, end.index);

  if (sel.rangeCount != 1) {
    return false;
  }

  return (sel.anchorNode == start.node &&
          sel.anchorOffset == start.index &&
          sel.focusNode == end.node &&
          sel.focusOffset == end.index);
};

/**
 * Note: the built-in function by the same name is unreliable.
 * @param {Selection} sel The selection.
 * @return {boolean} True if the start and end positions are the same.
 */
CaretBrowsing.isCollapsed = function(sel) {
  return (sel.anchorOffset == sel.focusOffset &&
          sel.anchorNode == sel.focusNode);
};

/**
 * Determines if the modifier key is held down that should cause
 * the cursor to move by word rather than by character.
 * @param {Event} evt A keyboard event.
 * @return {boolean} True if the cursor should move by word.
 */
CaretBrowsing.isMoveByWordEvent = function(evt) {
  if (CaretBrowsing.isMac) {
    return evt.altKey;
  } else {
    return evt.ctrlKey;
  }
};

/**
 * Moves the cursor forwards to the next valid position.
 * @param {Cursor} cursor The current cursor location.
 *     On exit, the cursor will be at the next position.
 * @param {Array<Node>} nodesCrossed Any HTML nodes crossed between the
 *     initial and final cursor position will be pushed onto this array.
 * @return {?string} The character reached, or null if the bottom of the
 *     document has been reached.
 */
CaretBrowsing.forwards = function(cursor, nodesCrossed) {
  var previousCursor = cursor.clone();
  var result = TraverseUtil.forwardsChar(cursor, nodesCrossed);

  // Work around the fact that TraverseUtil.forwardsChar returns once per
  // char in a block of text, rather than once per possible selection
  // position in a block of text.
  if (result && cursor.node != previousCursor.node && cursor.index > 0) {
    cursor.index = 0;
  }

  return result;
};

/**
 * Moves the cursor backwards to the previous valid position.
 * @param {Cursor} cursor The current cursor location.
 *     On exit, the cursor will be at the previous position.
 * @param {Array<Node>} nodesCrossed Any HTML nodes crossed between the
 *     initial and final cursor position will be pushed onto this array.
 * @return {?string} The character reached, or null if the top of the
 *     document has been reached.
 */
CaretBrowsing.backwards = function(cursor, nodesCrossed) {
  var previousCursor = cursor.clone();
  var result = TraverseUtil.backwardsChar(cursor, nodesCrossed);

  // Work around the fact that TraverseUtil.backwardsChar returns once per
  // char in a block of text, rather than once per possible selection
  // position in a block of text.
  if (result &&
      cursor.node != previousCursor.node &&
      cursor.index < cursor.text.length) {
    cursor.index = cursor.text.length;
  }

  return result;
};

/**
 * Called when the user presses the right arrow. If there's a selection,
 * moves the cursor to the end of the selection range. If it's a cursor,
 * moves past one character.
 * @param {Event} evt The DOM event.
 * @return {boolean} True if the default action should be performed.
 */
CaretBrowsing.moveRight = function(evt) {
  CaretBrowsing.targetX = null;

  var sel = window.getSelection();
  if (!evt.shiftKey && !CaretBrowsing.isCollapsed(sel)) {
    var right = CaretBrowsing.makeRightCursor(sel);
    CaretBrowsing.setAndValidateSelection(right, right);
    return false;
  }

  var start = CaretBrowsing.isAmbiguous(sel) ?
              CaretBrowsing.makeLeftCursor(sel) :
              CaretBrowsing.makeAnchorCursor(sel);
  var end = CaretBrowsing.isAmbiguous(sel) ?
            CaretBrowsing.makeRightCursor(sel) :
            CaretBrowsing.makeFocusCursor(sel);
  var previousEnd = end.clone();
  var nodesCrossed = [];
  while (true) {
    var result;
    if (CaretBrowsing.isMoveByWordEvent(evt)) {
      result = TraverseUtil.getNextWord(previousEnd, end, nodesCrossed);
    } else {
      previousEnd = end.clone();
      result = CaretBrowsing.forwards(end, nodesCrossed);
    }

    if (result === null) {
      return CaretBrowsing.moveLeft(evt);
    }

    if (CaretBrowsing.setAndValidateSelection(
            evt.shiftKey ? start : end, end)) {
      break;
    }
  }

  if (!evt.shiftKey) {
    nodesCrossed.push(end.node);
    CaretBrowsing.setFocusToFirstFocusable(nodesCrossed);
  }

  return false;
};

/**
 * Called when the user presses the left arrow. If there's a selection,
 * moves the cursor to the start of the selection range. If it's a cursor,
 * moves backwards past one character.
 * @param {Event} evt The DOM event.
 * @return {boolean} True if the default action should be performed.
 */
CaretBrowsing.moveLeft = function(evt) {
  CaretBrowsing.targetX = null;

  var sel = window.getSelection();
  if (!evt.shiftKey && !CaretBrowsing.isCollapsed(sel)) {
    var left = CaretBrowsing.makeLeftCursor(sel);
    CaretBrowsing.setAndValidateSelection(left, left);
    return false;
  }

  var start = CaretBrowsing.isAmbiguous(sel) ?
              CaretBrowsing.makeLeftCursor(sel) :
              CaretBrowsing.makeFocusCursor(sel);
  var end = CaretBrowsing.isAmbiguous(sel) ?
            CaretBrowsing.makeRightCursor(sel) :
            CaretBrowsing.makeAnchorCursor(sel);
  var previousStart = start.clone();
  var nodesCrossed = [];
  while (true) {
    var result;
    if (CaretBrowsing.isMoveByWordEvent(evt)) {
      result = TraverseUtil.getPreviousWord(
          start, previousStart, nodesCrossed);
    } else {
      previousStart = start.clone();
      result = CaretBrowsing.backwards(start, nodesCrossed);
    }

    if (result === null) {
      break;
    }

    if (CaretBrowsing.setAndValidateSelection(
            evt.shiftKey ? end : start, start)) {
      break;
    }
  }

  if (!evt.shiftKey) {
    nodesCrossed.push(start.node);
    CaretBrowsing.setFocusToFirstFocusable(nodesCrossed);
  }

  return false;
};


/**
 * Called when the user presses the down arrow. If there's a selection,
 * moves the cursor to the end of the selection range. If it's a cursor,
 * attempts to move to the equivalent horizontal pixel position in the
 * subsequent line of text. If this is impossible, go to the first character
 * of the next line.
 * @param {Event} evt The DOM event.
 * @return {boolean} True if the default action should be performed.
 */
CaretBrowsing.moveDown = function(evt) {
  var sel = window.getSelection();
  if (!evt.shiftKey && !CaretBrowsing.isCollapsed(sel)) {
    var right = CaretBrowsing.makeRightCursor(sel);
    CaretBrowsing.setAndValidateSelection(right, right);
    return false;
  }

  var start = CaretBrowsing.isAmbiguous(sel) ?
              CaretBrowsing.makeLeftCursor(sel) :
              CaretBrowsing.makeAnchorCursor(sel);
  var end = CaretBrowsing.isAmbiguous(sel) ?
            CaretBrowsing.makeRightCursor(sel) :
            CaretBrowsing.makeFocusCursor(sel);
  var endRect = CaretBrowsing.getCursorRect(end);
  if (CaretBrowsing.targetX === null) {
    CaretBrowsing.targetX = endRect.left;
  }
  var previousEnd = end.clone();
  var leftPos = end.clone();
  var rightPos = end.clone();
  var bestPos = null;
  var bestY = null;
  var bestDelta = null;
  var bestHeight = null;
  var nodesCrossed = [];
  var y = -1;
  while (true) {
    if (null === CaretBrowsing.forwards(rightPos, nodesCrossed)) {
      if (CaretBrowsing.setAndValidateSelection(
            evt.shiftKey ? start : leftPos, leftPos)) {
        break;
      } else {
        return CaretBrowsing.moveLeft(evt);
      }
      break;
    }
    var range = document.createRange();
    range.setStart(leftPos.node, leftPos.index);
    range.setEnd(rightPos.node, rightPos.index);
    var rect = range.getBoundingClientRect();
    if (rect && rect.width < rect.height) {
      y = rect.top + window.pageYOffset;

      // Return the best match so far if we get half a line past the best.
      if (bestY != null && y > bestY + bestHeight / 2) {
        if (CaretBrowsing.setAndValidateSelection(
                evt.shiftKey ? start : bestPos, bestPos)) {
          break;
        } else {
          bestY = null;
        }
      }

      // Stop here if we're an entire line the wrong direction
      // (for example, we reached the top of the next column).
      if (y < endRect.top - endRect.height) {
        if (CaretBrowsing.setAndValidateSelection(
                evt.shiftKey ? start : leftPos, leftPos)) {
          break;
        }
      }

      // Otherwise look to see if this current position is on the
      // next line and better than the previous best match, if any.
      if (y >= endRect.top + endRect.height) {
        var deltaLeft = Math.abs(CaretBrowsing.targetX - rect.left);
        if ((bestDelta == null || deltaLeft < bestDelta) &&
            (leftPos.node != end.node || leftPos.index != end.index)) {
          bestPos = leftPos.clone();
          bestY = y;
          bestDelta = deltaLeft;
          bestHeight = rect.height;
        }
        var deltaRight = Math.abs(CaretBrowsing.targetX - rect.right);
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
          if (CaretBrowsing.setAndValidateSelection(
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
    CaretBrowsing.setFocusToNode(leftPos.node);
  }

  return false;
};

/**
 * Called when the user presses the up arrow. If there's a selection,
 * moves the cursor to the start of the selection range. If it's a cursor,
 * attempts to move to the equivalent horizontal pixel position in the
 * previous line of text. If this is impossible, go to the last character
 * of the previous line.
 * @param {Event} evt The DOM event.
 * @return {boolean} True if the default action should be performed.
 */
CaretBrowsing.moveUp = function(evt) {
  var sel = window.getSelection();
  if (!evt.shiftKey && !CaretBrowsing.isCollapsed(sel)) {
    var left = CaretBrowsing.makeLeftCursor(sel);
    CaretBrowsing.setAndValidateSelection(left, left);
    return false;
  }

  var start = CaretBrowsing.isAmbiguous(sel) ?
              CaretBrowsing.makeLeftCursor(sel) :
              CaretBrowsing.makeFocusCursor(sel);
  var end = CaretBrowsing.isAmbiguous(sel) ?
            CaretBrowsing.makeRightCursor(sel) :
            CaretBrowsing.makeAnchorCursor(sel);
  var startRect = CaretBrowsing.getCursorRect(start);
  if (CaretBrowsing.targetX === null) {
    CaretBrowsing.targetX = startRect.left;
  }
  var previousStart = start.clone();
  var leftPos = start.clone();
  var rightPos = start.clone();
  var bestPos = null;
  var bestY = null;
  var bestDelta = null;
  var bestHeight = null;
  var nodesCrossed = [];
  var y = 999999;
  while (true) {
    if (null === CaretBrowsing.backwards(leftPos, nodesCrossed)) {
      CaretBrowsing.setAndValidateSelection(
          evt.shiftKey ? end : rightPos, rightPos);
      break;
    }
    var range = document.createRange();
    range.setStart(leftPos.node, leftPos.index);
    range.setEnd(rightPos.node, rightPos.index);
    var rect = range.getBoundingClientRect();
    if (rect && rect.width < rect.height) {
      y = rect.top + window.pageYOffset;

      // Return the best match so far if we get half a line past the best.
      if (bestY != null && y < bestY - bestHeight / 2) {
        if (CaretBrowsing.setAndValidateSelection(
                evt.shiftKey ? end : bestPos, bestPos)) {
          break;
        } else {
          bestY = null;
        }
      }

      // Exit if we're an entire line the wrong direction
      // (for example, we reached the bottom of the previous column.)
      if (y > startRect.top + startRect.height) {
        if (CaretBrowsing.setAndValidateSelection(
                evt.shiftKey ? end : rightPos, rightPos)) {
          break;
        }
      }

      // Otherwise look to see if this current position is on the
      // next line and better than the previous best match, if any.
      if (y <= startRect.top - startRect.height) {
        var deltaLeft = Math.abs(CaretBrowsing.targetX - rect.left);
        if (bestDelta == null || deltaLeft < bestDelta) {
          bestPos = leftPos.clone();
          bestY = y;
          bestDelta = deltaLeft;
          bestHeight = rect.height;
        }
        var deltaRight = Math.abs(CaretBrowsing.targetX - rect.right);
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
          if (CaretBrowsing.setAndValidateSelection(
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
    CaretBrowsing.setFocusToNode(rightPos.node);
  }

  return false;
};

/**
 * Set the document's selection to surround a control, so that the next
 * arrow key they press will allow them to explore the content before
 * or after a given control.
 * @param {Node} control The control to escape from.
 */
CaretBrowsing.escapeFromControl = function(control) {
  control.blur();

  var start = new Cursor(control, 0, '');
  var previousStart = start.clone();
  var end = new Cursor(control, 0, '');
  var previousEnd = end.clone();

  var nodesCrossed = [];
  while (true) {
    if (null === CaretBrowsing.backwards(start, nodesCrossed)) {
      break;
    }

    var r = document.createRange();
    r.setStart(start.node, start.index);
    r.setEnd(previousStart.node, previousStart.index);
    if (r.getBoundingClientRect()) {
      break;
    }
    previousStart = start.clone();
  }
  while (true) {
    if (null === CaretBrowsing.forwards(end, nodesCrossed)) {
      break;
    }
    if (isDescendantOfNode(end.node, control)) {
      previousEnd = end.clone();
      continue;
    }

    var r = document.createRange();
    r.setStart(previousEnd.node, previousEnd.index);
    r.setEnd(end.node, end.index);
    if (r.getBoundingClientRect()) {
      break;
    }
  }

  if (!isDescendantOfNode(previousStart.node, control)) {
    start = previousStart.clone();
  }

  if (!isDescendantOfNode(previousEnd.node, control)) {
    end = previousEnd.clone();
  }

  CaretBrowsing.setAndValidateSelection(start, end);

  window.setTimeout(function() {
    CaretBrowsing.updateCaretOrSelection(true);
  }, 0);
};

/**
 * Toggle whether caret browsing is enabled or not.
 */
CaretBrowsing.toggle = function() {
  if (CaretBrowsing.forceEnabled) {
    CaretBrowsing.recreateCaretElement();
    return;
  }

  CaretBrowsing.isEnabled = !CaretBrowsing.isEnabled;
  var obj = {};
  obj['enabled'] = CaretBrowsing.isEnabled;
  chrome.storage.sync.set(obj);
  CaretBrowsing.updateIsCaretVisible();
};

/**
 * Event handler, called when a key is pressed.
 * @param {Event} evt The DOM event.
 * @return {boolean} True if the default action should be performed.
 */
CaretBrowsing.onKeyDown = function(evt) {
  if (evt.defaultPrevented) {
    return;
  }

  if (evt.keyCode == 118) {  // F7
    CaretBrowsing.toggle();
  }

  if (!CaretBrowsing.isEnabled) {
    return true;
  }

  if (evt.target && CaretBrowsing.isControlThatNeedsArrowKeys(
      /** @type (Node) */(evt.target))) {
    if (evt.keyCode == 27) {
      CaretBrowsing.escapeFromControl(/** @type {Node} */(evt.target));
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
  var sel = window.getSelection();
  if (sel.rangeCount == 0) {
    if (document.activeElement) {
      CaretBrowsing.escapeFromControl(document.activeElement);
      sel = window.getSelection();
    }

    if (sel.rangeCount == 0) {
      return true;
    }
  }

  if (CaretBrowsing.caretElement) {
    CaretBrowsing.caretElement.style.visibility = 'visible';
    CaretBrowsing.blinkFlag = true;
  }

  var result = true;
  switch (evt.keyCode) {
    case 37:
      result = CaretBrowsing.moveLeft(evt);
      break;
    case 38:
      result = CaretBrowsing.moveUp(evt);
      break;
    case 39:
      result = CaretBrowsing.moveRight(evt);
      break;
    case 40:
      result = CaretBrowsing.moveDown(evt);
      break;
  }

  if (result == false) {
    evt.preventDefault();
    evt.stopPropagation();
  }

  window.setTimeout(function() {
    CaretBrowsing.updateCaretOrSelection(result == false);
  }, 0);

  return result;
};

/**
 * Event handler, called when the mouse is clicked. Chrome already
 * sets the selection when the mouse is clicked, all we need to do is
 * update our cursor.
 * @param {Event} evt The DOM event.
 * @return {boolean} True if the default action should be performed.
 */
CaretBrowsing.onClick = function(evt) {
  if (!CaretBrowsing.isEnabled) {
    return true;
  }
  window.setTimeout(function() {
    CaretBrowsing.targetX = null;
    CaretBrowsing.updateCaretOrSelection(false);
  }, 0);
  return true;
};

/**
 * Called at a regular interval. Blink the cursor by changing its visibility.
 */
CaretBrowsing.caretBlinkFunction = function() {
  if (CaretBrowsing.caretElement) {
    if (CaretBrowsing.blinkFlag) {
      CaretBrowsing.caretElement.style.backgroundColor =
          CaretBrowsing.caretForeground;
      CaretBrowsing.blinkFlag = false;
    } else {
      CaretBrowsing.caretElement.style.backgroundColor =
          CaretBrowsing.caretBackground;
      CaretBrowsing.blinkFlag = true;
    }
  }
};

/**
 * Update whether or not the caret is visible, based on whether caret browsing
 * is enabled and whether this window / iframe has focus.
 */
CaretBrowsing.updateIsCaretVisible = function() {
  CaretBrowsing.isCaretVisible =
      (CaretBrowsing.isEnabled && CaretBrowsing.isWindowFocused);
  if (CaretBrowsing.isCaretVisible && !CaretBrowsing.caretElement) {
    CaretBrowsing.setInitialCursor();
    CaretBrowsing.updateCaretOrSelection(true);
    if (CaretBrowsing.caretElement) {
      CaretBrowsing.blinkFunctionId = window.setInterval(
          CaretBrowsing.caretBlinkFunction, 500);
    }
  } else if (!CaretBrowsing.isCaretVisible &&
             CaretBrowsing.caretElement) {
    window.clearInterval(CaretBrowsing.blinkFunctionId);
    if (CaretBrowsing.caretElement) {
      CaretBrowsing.isSelectionCollapsed = false;
      CaretBrowsing.caretElement.parentElement.removeChild(
          CaretBrowsing.caretElement);
      CaretBrowsing.caretElement = null;
    }
  }
};

/**
 * Called when the prefs get updated.
 */
CaretBrowsing.onPrefsUpdated = function() {
  chrome.storage.sync.get(null, function(result) {
    if (!CaretBrowsing.forceEnabled) {
      CaretBrowsing.isEnabled = result['enabled'];
    }
    CaretBrowsing.onEnable = result['onenable'];
    CaretBrowsing.onJump = result['onjump'];
    CaretBrowsing.recreateCaretElement();
  });
};

/**
 * Called when this window / iframe gains focus.
 */
CaretBrowsing.onWindowFocus = function() {
  CaretBrowsing.isWindowFocused = true;
  CaretBrowsing.updateIsCaretVisible();
};

/**
 * Called when this window / iframe loses focus.
 */
CaretBrowsing.onWindowBlur = function() {
  CaretBrowsing.isWindowFocused = false;
  CaretBrowsing.updateIsCaretVisible();
};

/**
 * Initializes caret browsing by adding event listeners and extension
 * message listeners.
 */
CaretBrowsing.init = function() {
  CaretBrowsing.isWindowFocused = document.hasFocus();

  document.addEventListener('keydown', CaretBrowsing.onKeyDown, false);
  document.addEventListener('click', CaretBrowsing.onClick, false);
  window.addEventListener('focus', CaretBrowsing.onWindowFocus, false);
  window.addEventListener('blur', CaretBrowsing.onWindowBlur, false);
};

window.setTimeout(function() {

  // Make sure the script only loads once.
  if (!window['caretBrowsingLoaded']) {
    window['caretBrowsingLoaded'] = true;
    CaretBrowsing.init();

    if (document.body.getAttribute('caretbrowsing') == 'on') {
      CaretBrowsing.forceEnabled = true;
      CaretBrowsing.isEnabled = true;
      CaretBrowsing.updateIsCaretVisible();
    }

    chrome.storage.onChanged.addListener(function() {
      CaretBrowsing.onPrefsUpdated();
    });
    CaretBrowsing.onPrefsUpdated();
  }

}, 0);
