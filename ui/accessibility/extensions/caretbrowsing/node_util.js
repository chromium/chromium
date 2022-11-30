// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** A collection of functions for dealing with DOM nodes. */
class NodeUtil {
  /**
   * Return whether a node is focusable. This includes nodes whose tabindex
   * attribute is set to "-1" explicitly - these nodes are not in the tab
   * order, but they should still be focused if the user navigates to them
   * using linear or smart DOM navigation.
   *
   * Note that when the tabIndex property of an Element is -1, that doesn't
   * tell us whether the tabIndex attribute is missing or set to "-1"
   * explicitly, so we have to check the attribute.
   *
   * @param {Object} targetNode The node to check if it's focusable.
   * @return {boolean} True if the node is focusable.
   */
  static isFocusable(targetNode) {
    if (!targetNode || typeof (targetNode.tabIndex) != 'number') {
      return false;
    }

    if (targetNode.tabIndex >= 0) {
      return true;
    }

    if (targetNode.hasAttribute && targetNode.hasAttribute('tabindex') &&
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
  static isDescendantOfNode(node, ancestor) {
    while (node && ancestor) {
      if (node.isSameNode(ancestor)) {
        return true;
      }
      node = node.parentNode;
    }
    return false;
  }

  /**
   * Check if a node is a control that normally allows the user to interact
   * with it using arrow keys. We won't override the arrow keys when such a
   * control has focus, the user must press Escape to do caret browsing outside
   * that control.
   * @param {Node} node A node to check.
   * @return {boolean} True if this node is a control that the user can
   *     interact with using arrow keys.
   */
  static isControlThatNeedsArrowKeys(node) {
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
    if (node.getAttribute && NodeUtil.isFocusable(node)) {
      const role = node.getAttribute('role');
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
  }

  /**
   * Set focus to a node if it's focusable. If it's an input element,
   * select the text, otherwise it doesn't appear focused to the user.
   * Every other control behaves normally if you just call focus() on it.
   * @param {Node} node The node to focus.
   * @return {boolean} True if the node was focused.
   */
  static setFocusToNode(node) {
    while (node && node != document.body) {
      if (NodeUtil.isFocusable(node) && node.constructor != HTMLIFrameElement) {
        node.focus();
        if (node.constructor == HTMLInputElement && node.select) {
          node.select();
        }
        return true;
      }
      node = node.parentNode;
    }

    return false;
  }

  /**
   * Set focus to the first focusable node in the given list.
   * @param {!Array<!Node>} nodeList An array of nodes to focus.
   * @return {boolean} True if the node was focused.
   */
  static setFocusToFirstFocusable(nodeList) {
    for (let i = 0; i < nodeList.length; i++) {
      if (NodeUtil.setFocusToNode(nodeList[i])) {
        return true;
      }
    }
    return false;
  }
}
