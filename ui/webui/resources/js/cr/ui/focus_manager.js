// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui', function() {
  /**
   * Constructor for FocusManager singleton. Checks focus of elements to ensure
   * that elements in "background" pages (i.e., those in a dialog that is not
   * the topmost overlay) do not receive focus.
   * @constructor
   */
  function FocusManager() {}

  FocusManager.prototype = {
    /**
     * Whether focus is being transferred backward or forward through the DOM.
     * @type {boolean}
     * @private
     */
    focusDirBackwards_: false,

    /**
     * Determines whether the |child| is a descendant of |parent| in the page's
     * DOM.
     * @param {Node} parent The parent element to test.
     * @param {Node} child The child element to test.
     * @return {boolean} True if |child| is a descendant of |parent|.
     * @private
     */
    isDescendantOf_(parent, child) {
      return !!parent && !(parent === child) && parent.contains(child);
    },

    /**
     * Returns the parent element containing all elements which should be
     * allowed to receive focus.
     * @return {Element} The element containing focusable elements.
     */
    getFocusParent() {
      return document.body;
    },

    /**
     * Returns the elements on the page capable of receiving focus.
     * @return {Array<Element>} The focusable elements.
     */
    getFocusableElements_() {
      const focusableDiv = this.getFocusParent();

      // Create a TreeWalker object to traverse the DOM from |focusableDiv|.
      const treeWalker = document.createTreeWalker(
          focusableDiv, NodeFilter.SHOW_ELEMENT,
          /** @type {NodeFilter} */
          ({
            acceptNode(node) {
              const style = window.getComputedStyle(node);
              // Reject all hidden nodes. FILTER_REJECT also rejects these
              // nodes' children, so non-hidden elements that are descendants of
              // hidden <div>s will correctly be rejected.
              if (node.hidden || style.display === 'none' ||
                  style.visibility === 'hidden') {
                return NodeFilter.FILTER_REJECT;
              }

              // Skip nodes that cannot receive focus. FILTER_SKIP does not
              // cause this node's children also to be skipped.
              if (node.disabled || node.tabIndex < 0) {
                return NodeFilter.FILTER_SKIP;
              }

              // Accept nodes that are non-hidden and focusable.
              return NodeFilter.FILTER_ACCEPT;
            }
          }),
          false);

      const focusable = [];
      while (treeWalker.nextNode()) {
        focusable.push(treeWalker.currentNode);
      }

      return focusable;
    },

    /**
     * Dispatches an 'elementFocused' event to notify an element that it has
     * received focus. When focus wraps around within the a page, only the
     * element that has focus after the wrapping receives an 'elementFocused'
     * event. This differs from the native 'focus' event which is received by
     * an element outside the page first, followed by a 'focus' on an element
     * within the page after the FocusManager has intervened.
     * @param {EventTarget} element The element that has received focus.
     * @private
     */
    dispatchFocusEvent_(element) {
      cr.dispatchSimpleEvent(element, 'elementFocused', true, false);
    },

    /**
     * Attempts to focus the appropriate element in the current dialog.
     * @private
     */
    setFocus_() {
      const element = this.selectFocusableElement_();
      if (element) {
        element.focus();
        this.dispatchFocusEvent_(element);
      }
    },

    /**
     * Selects first appropriate focusable element according to the
     * current focus direction and element type.  If it is a radio button,
     * checked one is selected from the group.
     * @private
     */
    selectFocusableElement_() {
      // If |this.focusDirBackwards_| is true, the user has pressed "Shift+Tab"
      // and has caused the focus to be transferred backward, outside of the
      // current dialog. In this case, loop around and try to focus the last
      // element of the dialog; otherwise, try to focus the first element of the
      // dialog.
      const focusableElements = this.getFocusableElements_();
      let element = this.focusDirBackwards_ ? focusableElements.pop() :
                                              focusableElements.shift();
      if (!element) {
        return null;
      }
      if (element.tagName !== 'INPUT' || element.type !== 'radio' ||
          element.name === '') {
        return element;
      }
      if (!element.checked) {
        for (let i = 0; i < focusableElements.length; i++) {
          const e = focusableElements[i];
          if (e && e.tagName === 'INPUT' && e.type === 'radio' &&
              e.name === element.name && e.checked) {
            element = e;
            break;
          }
        }
      }
      return element;
    },

    /**
     * Handler for focus events on the page.
     * @param {Event} event The focus event.
     * @private
     */
    onDocumentFocus_(event) {
      // If the element being focused is a descendant of the currently visible
      // page, focus is valid.
      const targetNode = /** @type {Node} */ (event.target);
      if (this.isDescendantOf_(this.getFocusParent(), targetNode)) {
        this.dispatchFocusEvent_(event.target);
        return;
      }

      // Focus event handlers for descendant elements might dispatch another
      // focus event.
      event.stopPropagation();

      // The target of the focus event is not in the topmost visible page and
      // should not be focused.
      event.target.blur();

      // Attempt to wrap around focus within the current page.
      this.setFocus_();
    },

    /**
     * Handler for keydown events on the page.
     * @param {Event} event The keydown event.
     * @private
     */
    onDocumentKeyDown_(event) {
      /** @const */ const tabKeyCode = 9;

      if (event.keyCode === tabKeyCode) {
        // If the "Shift" key is held, focus is being transferred backward in
        // the page.
        this.focusDirBackwards_ = event.shiftKey ? true : false;
      }
    },

    /**
     * Initializes the FocusManager by listening for events in the document.
     */
    initialize() {
      document.addEventListener(
          'focus', this.onDocumentFocus_.bind(this), true);
      document.addEventListener(
          'keydown', this.onDocumentKeyDown_.bind(this), true);
    },
  };

  return {
    FocusManager: FocusManager,
  };
});
