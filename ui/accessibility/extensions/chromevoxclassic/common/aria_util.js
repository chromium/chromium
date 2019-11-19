// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A collection of JavaScript utilities used to simplify working
 * with ARIA (http://www.w3.org/TR/wai-aria).
 */


goog.provide('cvox.AriaUtil');
goog.require('cvox.AbstractEarcons');
goog.require('cvox.ChromeVox');
goog.require('cvox.NodeState');
goog.require('cvox.NodeStateUtil');


/**
 * Create the namespace
 * @constructor
 */
cvox.AriaUtil = function() {
};


/**
 * A mapping from ARIA role names to their message ids.
 * Note: If you are adding a new mapping, the new message identifier needs a
 * corresponding braille message. For example, a message id 'tag_button'
 * requires another message 'tag_button_brl' within messages.js.
 * @type {Object<string>}
 */
cvox.AriaUtil.WIDGET_ROLE_TO_NAME = {
  'alert' : 'role_alert',
  'alertdialog' : 'role_alertdialog',
  'button' : 'role_button',
  'checkbox' : 'role_checkbox',
  'columnheader' : 'role_columnheader',
  'combobox' : 'role_combobox',
  'dialog' : 'role_dialog',
  'grid' : 'role_grid',
  'gridcell' : 'role_gridcell',
  'link' : 'role_link',
  'listbox' : 'role_listbox',
  'log' : 'role_log',
  'marquee' : 'role_marquee',
  'menu' : 'role_menu',
  'menubar' : 'role_menubar',
  'menuitem' : 'role_menuitem',
  'menuitemcheckbox' : 'role_menuitemcheckbox',
  'menuitemradio' : 'role_menuitemradio',
  'option' : 'role_option',
  'progressbar' : 'role_progressbar',
  'radio' : 'role_radio',
  'radiogroup' : 'role_radiogroup',
  'rowheader' : 'role_rowheader',
  'scrollbar' : 'role_scrollbar',
  'slider' : 'role_slider',
  'spinbutton' : 'role_spinbutton',
  'status' : 'role_status',
  'tab' : 'role_tab',
  'tablist' : 'role_tablist',
  'tabpanel' : 'role_tabpanel',
  'textbox' : 'role_textbox',
  'timer' : 'role_timer',
  'toolbar' : 'role_toolbar',
  'tooltip' : 'role_tooltip',
  'treeitem' : 'role_treeitem'
};


/**
 * Note: If you are adding a new mapping, the new message identifier needs a
 * corresponding braille message. For example, a message id 'tag_button'
 * requires another message 'tag_button_brl' within messages.js.
 * @type {Object<string>}
 */
cvox.AriaUtil.STRUCTURE_ROLE_TO_NAME = {
  'article' : 'role_article',
  'application' : 'role_application',
  'banner' : 'role_banner',
  'columnheader' : 'role_columnheader',
  'complementary' : 'role_complementary',
  'contentinfo' : 'role_contentinfo',
  'definition' : 'role_definition',
  'directory' : 'role_directory',
  'document' : 'role_document',
  'form' : 'role_form',
  'group' : 'role_group',
  'heading' : 'role_heading',
  'img' : 'role_img',
  'list' : 'role_list',
  'listitem' : 'role_listitem',
  'main' : 'role_main',
  'math' : 'role_math',
  'navigation' : 'role_navigation',
  'note' : 'role_note',
  'region' : 'role_region',
  'rowheader' : 'role_rowheader',
  'search' : 'role_search',
  'separator' : 'role_separator'
};


/**
 * @type {Array<Object>}
 */
cvox.AriaUtil.ATTRIBUTE_VALUE_TO_STATUS = [
  { name: 'aria-autocomplete', values:
      {'inline' : 'aria_autocomplete_inline',
       'list' : 'aria_autocomplete_list',
       'both' : 'aria_autocomplete_both'} },
  { name: 'aria-checked', values:
      {'true' : 'aria_checked_true',
       'false' : 'aria_checked_false',
       'mixed' : 'aria_checked_mixed'} },
  { name: 'aria-disabled', values:
      {'true' : 'aria_disabled_true'} },
  { name: 'aria-expanded', values:
      {'true' : 'aria_expanded_true',
       'false' : 'aria_expanded_false'} },
  { name: 'aria-invalid', values:
      {'true' : 'aria_invalid_true',
       'grammar' : 'aria_invalid_grammar',
       'spelling' : 'aria_invalid_spelling'} },
  { name: 'aria-multiline', values:
      {'true' : 'aria_multiline_true'} },
  { name: 'aria-multiselectable', values:
      {'true' : 'aria_multiselectable_true'} },
  { name: 'aria-pressed', values:
      {'true' : 'aria_pressed_true',
       'false' : 'aria_pressed_false',
       'mixed' : 'aria_pressed_mixed'} },
  { name: 'aria-readonly', values:
      {'true' : 'aria_readonly_true'} },
  { name: 'aria-required', values:
      {'true' : 'aria_required_true'} },
  { name: 'aria-selected', values:
      {'true' : 'aria_selected_true',
       'false' : 'aria_selected_false'} }
];


/**
 * Checks if a node should be treated as a hidden node because of its ARIA
 * markup.
 *
 * @param {Node} targetNode The node to check.
 * @return {boolean} True if the targetNode should be treated as hidden.
 */
cvox.AriaUtil.isHiddenRecursive = function(targetNode) {
  if (cvox.AriaUtil.isHidden(targetNode)) {
    return true;
  }
  var parent = targetNode.parentElement;
  while (parent) {
    if ((parent.getAttribute('aria-hidden') == 'true') &&
        (parent.getAttribute('chromevoxignoreariahidden') != 'true')) {
      return true;
    }
    parent = parent.parentElement;
  }
  return false;
};


/**
 * Checks if a node should be treated as a hidden node because of its ARIA
 * markup. Does not check parents, so if you need to know if this is a
 * descendant of a hidden node, call isHiddenRecursive.
 *
 * @param {Node} targetNode The node to check.
 * @return {boolean} True if the targetNode should be treated as hidden.
 */
cvox.AriaUtil.isHidden = function(targetNode) {
  if (!targetNode) {
    return true;
  }
  if (targetNode.getAttribute) {
    if ((targetNode.getAttribute('aria-hidden') == 'true') &&
        (targetNode.getAttribute('chromevoxignoreariahidden') != 'true')) {
      return true;
    }
  }
  return false;
};


/**
 * Checks if a node should be treated as a visible node because of its ARIA
 * markup, regardless of whatever other styling/attributes it may have.
 * It is possible to force a node to be visible by setting aria-hidden to
 * false.
 *
 * @param {Node} targetNode The node to check.
 * @return {boolean} True if the targetNode should be treated as visible.
 */
cvox.AriaUtil.isForcedVisibleRecursive = function(targetNode) {
  var node = targetNode;
  while (node) {
    if (node.getAttribute) {
      // Stop and return the result based on the closest node that has
      // aria-hidden set.
      if (node.hasAttribute('aria-hidden') &&
          (node.getAttribute('chromevoxignoreariahidden') != 'true')) {
        return node.getAttribute('aria-hidden') == 'false';
      }
    }
    node = node.parentElement;
  }
  return false;
};


/**
 * Checks if a node should be treated as a leaf node because of its ARIA
 * markup. Does not check recursively, and does not check isControlWidget.
 *
 * @param {Element} targetElement The node to check.
 * @return {boolean} True if the targetNode should be treated as a leaf node.
 */
cvox.AriaUtil.isLeafElement = function(targetElement) {
  var role = targetElement.getAttribute('role');
  return role == 'img' || role == 'progressbar';
};


/**
 * Determines whether or not a node is or is the descendant of a node
 * with a particular role.
 *
 * @param {Node} node The node to be checked.
 * @param {string} roleName The role to check for.
 * @return {boolean} True if the node or one of its ancestor has the specified
 * role.
 */
cvox.AriaUtil.isDescendantOfRole = function(node, roleName) {
  while (node) {
    if (roleName && node && (node.getAttribute('role') == roleName)) {
      return true;
    }
    node = node.parentNode;
  }
  return false;
};


/**
 * Helper function to return the role name message identifier for a role.
 * @param {string} role The role.
 * @return {?string} The role name message identifier.
 * @private
 */
cvox.AriaUtil.getRoleNameMsgForRole_ = function(role) {
  var msgId = cvox.AriaUtil.WIDGET_ROLE_TO_NAME[role];
  if (!msgId) {
    return null;
  }
  return msgId;
};

/**
 * Returns true is the node is any kind of button.
 *
 * @param {Node} node The node to check.
 * @return {boolean} True if the node is a button.
 */
cvox.AriaUtil.isButton = function(node) {
  var role = cvox.AriaUtil.getRoleAttribute(node);
  if (role == 'button') {
    return true;
  }
  if (node.tagName == 'BUTTON') {
    return true;
  }
  if (node.tagName == 'INPUT') {
    return (node.type == 'submit' ||
            node.type == 'reset' ||
            node.type == 'button');
  }
  return false;
};

/**
 * Returns a role message identifier for a node.
 * For a localized string, see cvox.AriaUtil.getRoleName.
 * @param {Node} targetNode The node to get the role name for.
 * @return {string} The role name message identifier      for the targetNode.
 */
cvox.AriaUtil.getRoleNameMsg = function(targetNode) {
  var roleName;
  if (targetNode && targetNode.getAttribute) {
    var role = cvox.AriaUtil.getRoleAttribute(targetNode);

    // Special case for pop-up buttons.
    if (targetNode.getAttribute('aria-haspopup') == 'true' &&
        cvox.AriaUtil.isButton(targetNode)) {
      return 'role_popup_button';
    }

    if (role) {
      roleName = cvox.AriaUtil.getRoleNameMsgForRole_(role);
      if (!roleName) {
        roleName = cvox.AriaUtil.STRUCTURE_ROLE_TO_NAME[role];
      }
    }

    // To a user, a menu item within a menu bar is called a "menu";
    // any other menu item is called a "menu item".
    //
    // TODO(deboer): This block feels like a hack. dmazzoni suggests
    // using css-like syntax for names.  Investigate further if
    // we need more of these hacks.
    if (role == 'menuitem') {
      var container = targetNode.parentElement;
      while (container) {
        if (container.getAttribute &&
            (cvox.AriaUtil.getRoleAttribute(container) == 'menu' ||
             cvox.AriaUtil.getRoleAttribute(container) == 'menubar')) {
          break;
        }
        container = container.parentElement;
      }
      if (container && cvox.AriaUtil.getRoleAttribute(container) == 'menubar') {
        roleName = cvox.AriaUtil.getRoleNameMsgForRole_('menu');
      }  // else roleName is already 'Menu item', no need to change it.
    }
  }
  if (!roleName) {
    roleName = '';
  }
  return roleName;
};

/**
 * Returns a string to be presented to the user that identifies what the
 * targetNode's role is.
 *
 * @param {Node} targetNode The node to get the role name for.
 * @return {string} The role name for the targetNode.
 */
cvox.AriaUtil.getRoleName = function(targetNode) {
  var roleMsg = cvox.AriaUtil.getRoleNameMsg(targetNode);
  var roleName = Msgs.getMsg(roleMsg);
  var role = cvox.AriaUtil.getRoleAttribute(targetNode);
  if ((role == 'heading') && (targetNode.hasAttribute('aria-level'))) {
    roleName += ' ' + targetNode.getAttribute('aria-level');
  }
  return roleName ? roleName : '';
};

/**
 * Returns a string that gives information about the state of the targetNode.
 *
 * @param {Node} targetNode The node to get the state information for.
 * @param {boolean} primary Whether this is the primary node we're
 *     interested in, where we might want extra information - as
 *     opposed to an ancestor, where we might be more brief.
 * @return {cvox.NodeState} The status information about the node.
 */
cvox.AriaUtil.getStateMsgs = function(targetNode, primary) {
  var state = [];
  if (!targetNode || !targetNode.getAttribute) {
    return state;
  }

  for (var i = 0, attr; attr = cvox.AriaUtil.ATTRIBUTE_VALUE_TO_STATUS[i];
      i++) {
    var value = targetNode.getAttribute(attr.name);
    var msgId = attr.values[value];
    if (msgId) {
      state.push([msgId]);
    }
  }
  if (targetNode.getAttribute('role') == 'grid') {
      return cvox.AriaUtil.getGridState_(targetNode, targetNode);
  }

  var role = cvox.AriaUtil.getRoleAttribute(targetNode);
  if (targetNode.getAttribute('aria-haspopup') == 'true') {
    if (role == 'menuitem') {
      state.push(['has_submenu']);
    } else if (cvox.AriaUtil.isButton(targetNode)) {
      // Do nothing - the role name will be 'pop-up button'.
    } else {
      state.push(['has_popup']);
    }
  }

  var valueText = targetNode.getAttribute('aria-valuetext');
  if (valueText) {
    // If there is a valueText, that always wins.
    state.push(['aria_value_text', valueText]);
    return state;
  }

  var valueNow = targetNode.getAttribute('aria-valuenow');
  var valueMin = targetNode.getAttribute('aria-valuemin');
  var valueMax = targetNode.getAttribute('aria-valuemax');

  // Scrollbar and progressbar should speak the percentage.
  // http://www.w3.org/TR/wai-aria/roles#scrollbar
  // http://www.w3.org/TR/wai-aria/roles#progressbar
  if ((valueNow != null) && (valueMin != null) && (valueMax != null)) {
    if ((role == 'scrollbar') || (role == 'progressbar')) {
      var percent = Math.round((valueNow / (valueMax - valueMin)) * 100);
      state.push(['state_percent', percent]);
      return state;
    }
  }

  // Return as many of the value attributes as possible.
  if (valueNow != null) {
    state.push(['aria_value_now', valueNow]);
  }
  if (valueMin != null) {
    state.push(['aria_value_min', valueMin]);
  }
  if (valueMax != null) {
    state.push(['aria_value_max', valueMax]);
  }

  // If this is a composite control or an item within a composite control,
  // get the index and count of the current descendant or active
  // descendant.
  var parentControl = targetNode;
  var currentDescendant = null;

  if (cvox.AriaUtil.isCompositeControl(parentControl) && primary) {
    currentDescendant = cvox.AriaUtil.getActiveDescendant(parentControl);
  } else {
    role = cvox.AriaUtil.getRoleAttribute(targetNode);
    if (role == 'option' ||
        role == 'menuitem' ||
        role == 'menuitemcheckbox' ||
        role == 'menuitemradio' ||
        role == 'radio' ||
        role == 'tab' ||
        role == 'treeitem') {
      currentDescendant = targetNode;
      parentControl = targetNode.parentElement;
      while (parentControl &&
             !cvox.AriaUtil.isCompositeControl(parentControl)) {
        parentControl = parentControl.parentElement;
        if (parentControl &&
            cvox.AriaUtil.getRoleAttribute(parentControl) == 'treeitem') {
          break;
        }
      }
    }
  }

  if (parentControl &&
      (cvox.AriaUtil.isCompositeControl(parentControl) ||
          cvox.AriaUtil.getRoleAttribute(parentControl) == 'treeitem') &&
      currentDescendant) {
    var parentRole = cvox.AriaUtil.getRoleAttribute(parentControl);
    var descendantRoleList;
    switch (parentRole) {
      case 'combobox':
      case 'listbox':
        descendantRoleList = ['option'];
        break;
      case 'menu':
        descendantRoleList = ['menuitem',
                             'menuitemcheckbox',
                             'menuitemradio'];
        break;
      case 'radiogroup':
        descendantRoleList = ['radio'];
        break;
      case 'tablist':
        descendantRoleList = ['tab'];
        break;
      case 'tree':
      case 'treegrid':
      case 'treeitem':
        descendantRoleList = ['treeitem'];
        break;
    }

    if (descendantRoleList) {
      var listLength;
      var currentIndex;

      var ariaLength =
          parseInt(currentDescendant.getAttribute('aria-setsize'), 10);
      if (!isNaN(ariaLength)) {
        listLength = ariaLength;
      }
      var ariaIndex =
          parseInt(currentDescendant.getAttribute('aria-posinset'), 10);
      if (!isNaN(ariaIndex)) {
        currentIndex = ariaIndex;
      }

      if (listLength == undefined || currentIndex == undefined) {
        var descendants = cvox.AriaUtil.getNextLevel(parentControl,
            descendantRoleList);
        if (listLength == undefined) {
          listLength = descendants.length;
        }
        if (currentIndex == undefined) {
          for (var j = 0; j < descendants.length; j++) {
            if (descendants[j] == currentDescendant) {
              currentIndex = j + 1;
            }
          }
        }
      }
      if (currentIndex && listLength) {
        state.push(['list_position', currentIndex, listLength]);
      }
    }
  }
  return state;
};


/**
 * Returns a string that gives information about the state of the grid node.
 *
 * @param {Node} targetNode The node to get the state information for.
 * @param {Node} parentControl The parent composite control.
 * @return {cvox.NodeState} The status information about the node.
 * @private
 */
cvox.AriaUtil.getGridState_ = function(targetNode, parentControl) {
  var activeDescendant = cvox.AriaUtil.getActiveDescendant(parentControl);

  if (activeDescendant) {
    var descendantSelector = '*[role~="row"]';
    var rows = parentControl.querySelectorAll(descendantSelector);
    var currentIndex = null;
    for (var j = 0; j < rows.length; j++) {
      var gridcells = rows[j].querySelectorAll('*[role~="gridcell"]');
      for (var k = 0; k < gridcells.length; k++) {
        if (gridcells[k] == activeDescendant) {
          return /** @type {cvox.NodeState} */ (
                  [['role_gridcell_pos', j + 1, k + 1]]);
        }
      }
    }
  }
  return [];
};


/**
 * Returns the id of a node's active descendant
 * @param {Node} targetNode The node.
 * @return {?string} The id of the active descendant.
 * @private
 */
cvox.AriaUtil.getActiveDescendantId_ = function(targetNode) {
  if (!targetNode.getAttribute) {
    return null;
  }

  var activeId = targetNode.getAttribute('aria-activedescendant');
  if (!activeId) {
    return null;
  }
  return activeId;
};


/**
 * Returns the list of elements that are one aria-level below.
 *
 * @param {Node} parentControl The node whose descendants should be analyzed.
 * @param {Array<string>} role The role(s) of descendant we are looking for.
 * @return {Array<Node>} The array of matching nodes.
 */
cvox.AriaUtil.getNextLevel = function(parentControl, role) {
  var result = [];
  var children = parentControl.childNodes;
  var length = children.length;
  for (var i = 0; i < children.length; i++) {
    if (cvox.AriaUtil.isHidden(children[i]) ||
        !cvox.DomUtil.isVisible(children[i])) {
      continue;
    }
    var nextLevel = cvox.AriaUtil.getNextLevelItems(children[i], role);
    if (nextLevel.length > 0) {
      result = result.concat(nextLevel);
    }
  }
  return result;
};


/**
 * Recursively finds the first node(s) that match the role.
 *
 * @param {Node} current The node to start looking at.
 * @param {Array<string>} role The role(s) to match.
 * @return {Array<Element>} The array of matching nodes.
 */
cvox.AriaUtil.getNextLevelItems = function(current, role) {
  if (current.nodeType != 1) { // If reached a node that is not an element.
    return [];
  }
  if (role.indexOf(cvox.AriaUtil.getRoleAttribute(current)) != -1) {
    return [current];
  } else {
    var children = current.childNodes;
    var length = children.length;
    if (length == 0) {
      return [];
    } else {
      var resultArray = [];
      for (var i = 0; i < length; i++) {
        var result = cvox.AriaUtil.getNextLevelItems(children[i], role);
        if (result.length > 0) {
          resultArray = resultArray.concat(result);
        }
      }
      return resultArray;
    }
  }
};


/**
 * If the node is an object with an active descendant, returns the
 * descendant node.
 *
 * This function will fully resolve an active descendant chain. If a circular
 * chain is detected, it will return null.
 *
 * @param {Node} targetNode The node to get descendant information for.
 * @return {Node} The descendant node or null if no node exists.
 */
cvox.AriaUtil.getActiveDescendant = function(targetNode) {
  var seenIds = {};
  var node = targetNode;

  while (node) {
    var activeId = cvox.AriaUtil.getActiveDescendantId_(node);
    if (!activeId) {
      break;
    }
    if (activeId in seenIds) {
      // A circlar activeDescendant is an error, so return null.
      return null;
    }
    seenIds[activeId] = true;
    node = document.getElementById(activeId);
  }

  if (node == targetNode) {
    return null;
  }
  return node;
};


/**
 * Given a node, returns true if it's an ARIA control widget. Control widgets
 * are treated as leaf nodes.
 *
 * @param {Node} targetNode The node to be checked.
 * @return {boolean} Whether the targetNode is an ARIA control widget.
 */
cvox.AriaUtil.isControlWidget = function(targetNode) {
  if (targetNode && targetNode.getAttribute) {
    var role = cvox.AriaUtil.getRoleAttribute(targetNode);
    switch (role) {
      case 'button':
      case 'checkbox':
      case 'combobox':
      case 'listbox':
      case 'menu':
      case 'menuitemcheckbox':
      case 'menuitemradio':
      case 'radio':
      case 'slider':
      case 'progressbar':
      case 'scrollbar':
      case 'spinbutton':
      case 'tab':
      case 'tablist':
      case 'textbox':
        return true;
    }
  }
  return false;
};


/**
 * Given a node, returns true if it's an ARIA composite control.
 *
 * @param {Node} targetNode The node to be checked.
 * @return {boolean} Whether the targetNode is an ARIA composite control.
 */
cvox.AriaUtil.isCompositeControl = function(targetNode) {
  if (targetNode && targetNode.getAttribute) {
    var role = cvox.AriaUtil.getRoleAttribute(targetNode);
    switch (role) {
      case 'combobox':
      case 'grid':
      case 'listbox':
      case 'menu':
      case 'menubar':
      case 'radiogroup':
      case 'tablist':
      case 'tree':
      case 'treegrid':
        return true;
    }
  }
  return false;
};


/**
 * Given a node, returns its 'aria-live' value if it's a live region, or
 * null otherwise.
 *
 * @param {Node} node The node to be checked.
 * @return {?string} The live region value, like 'polite' or
 *     'assertive', or null if 'off' or none.
 */
cvox.AriaUtil.getAriaLive = function(node) {
  if (!node.hasAttribute)
    return null;
  var value = node.getAttribute('aria-live');
  if (value == 'off') {
    return null;
  } else if (value) {
    return value;
  }
  var role = cvox.AriaUtil.getRoleAttribute(node);
  switch (role) {
    case 'alert':
      return 'assertive';
    case 'log':
    case 'status':
      return 'polite';
    default:
      return null;
  }
};


/**
 * Given a node, returns its 'aria-atomic' value.
 *
 * @param {Node} node The node to be checked.
 * @return {boolean} The aria-atomic live region value, either true or false.
 */
cvox.AriaUtil.getAriaAtomic = function(node) {
  if (!node.hasAttribute)
    return false;
  var value = node.getAttribute('aria-atomic');
  if (value) {
    return (value === 'true');
  }
  var role = cvox.AriaUtil.getRoleAttribute(node);
  if (role == 'alert') {
    return true;
  }
  return false;
};


/**
 * Given a node, returns its 'aria-busy' value.
 *
 * @param {Node} node The node to be checked.
 * @return {boolean} The aria-busy live region value, either true or false.
 */
cvox.AriaUtil.getAriaBusy = function(node) {
  if (!node.hasAttribute)
    return false;
  var value = node.getAttribute('aria-busy');
  if (value) {
    return (value === 'true');
  }
  return false;
};


/**
 * Given a node, checks its aria-relevant attribute (with proper inheritance)
 * and determines whether the given change (additions, removals, text, all)
 * is relevant and should be announced.
 *
 * @param {Node} node The node to be checked.
 * @param {string} change The name of the change to check - one of
 *     'additions', 'removals', 'text', 'all'.
 * @return {boolean} True if that change is relevant to that node as part of
 *     a live region.
 */
cvox.AriaUtil.getAriaRelevant = function(node, change) {
  if (!node.hasAttribute)
    return false;
  var value;
  if (node.hasAttribute('aria-relevant')) {
    value = node.getAttribute('aria-relevant');
  } else {
    value = 'additions text';
  }
  if (value == 'all') {
    value = 'additions removals text';
  }

  var tokens = value.replace(/\s+/g, ' ').replace(/^\s+|\s+$/g, '').split(' ');

  if (change == 'all') {
    return (tokens.indexOf('additions') >= 0 &&
            tokens.indexOf('text') >= 0 &&
            tokens.indexOf('removals') >= 0);
  } else {
    return (tokens.indexOf(change) >= 0);
  }
};


/**
 * Given a node, return all live regions that are either rooted at this
 * node or contain this node.
 *
 * @param {Node} node The node to be checked.
 * @return {Array<Element>} All live regions affected by this node changing.
 */
cvox.AriaUtil.getLiveRegions = function(node) {
  var result = [];
  if (node.querySelectorAll) {
    var nodes = node.querySelectorAll(
        '[role="alert"], [role="log"],  [role="marquee"], ' +
        '[role="status"], [role="timer"],  [aria-live]');
    if (nodes) {
      for (var i = 0; i < nodes.length; i++) {
        result.push(nodes[i]);
      }
    }
  }

  while (node) {
    if (cvox.AriaUtil.getAriaLive(node)) {
      result.push(node);
      return result;
    }
    node = node.parentElement;
  }

  return result;
};


/**
 * Checks to see whether or not a node is an ARIA landmark.
 *
 * @param {Node} node The node to be checked.
 * @return {boolean} Whether or not the node is an ARIA landmark.
 */
cvox.AriaUtil.isLandmark = function(node) {
    if (!node || !node.getAttribute) {
      return false;
    }
    var role = cvox.AriaUtil.getRoleAttribute(node);
    switch (role) {
      case 'application':
      case 'banner':
      case 'complementary':
      case 'contentinfo':
      case 'form':
      case 'main':
      case 'navigation':
      case 'search':
        return true;
    }
    return false;
};


/**
 * Checks to see whether or not a node is an ARIA grid.
 *
 * @param {Node} node The node to be checked.
 * @return {boolean} Whether or not the node is an ARIA grid.
 */
cvox.AriaUtil.isGrid = function(node) {
    if (!node || !node.getAttribute) {
      return false;
    }
    var role = cvox.AriaUtil.getRoleAttribute(node);
    switch (role) {
      case 'grid':
      case 'treegrid':
        return true;
    }
    return false;
};


/**
 * Returns the id of an earcon to play along with the description for a node.
 *
 * @param {Node} node The node to get the earcon for.
 * @return {cvox.Earcon?} The earcon id, or null if none applies.
 */
cvox.AriaUtil.getEarcon = function(node) {
  if (!node || !node.getAttribute) {
    return null;
  }
  var role = cvox.AriaUtil.getRoleAttribute(node);
  switch (role) {
    case 'button':
      return cvox.Earcon.BUTTON;
    case 'checkbox':
    case 'radio':
    case 'menuitemcheckbox':
    case 'menuitemradio':
      var checked = node.getAttribute('aria-checked');
      if (checked == 'true') {
        return cvox.Earcon.CHECK_ON;
      } else {
        return cvox.Earcon.CHECK_OFF;
      }
    case 'combobox':
    case 'listbox':
      return cvox.Earcon.LISTBOX;
    case 'textbox':
      return cvox.Earcon.EDITABLE_TEXT;
    case 'listitem':
      return cvox.Earcon.LIST_ITEM;
    case 'link':
      return cvox.Earcon.LINK;
  }

  return null;
};


/**
 * Returns the role of the node.
 *
 * This is equivalent to targetNode.getAttribute('role')
 * except it also takes into account cases where ChromeVox
 * itself has changed the role (ie, adding role="application"
 * to BODY elements for better screen reader compatibility.
 *
 * @param {Node} targetNode The node to get the role for.
 * @return {string} role of the targetNode.
 */
cvox.AriaUtil.getRoleAttribute = function(targetNode) {
  if (!targetNode.getAttribute) {
    return '';
  }
  var role = targetNode.getAttribute('role');
  if (targetNode.hasAttribute('chromevoxoriginalrole')) {
    role = targetNode.getAttribute('chromevoxoriginalrole');
  }
  return role;
};


/**
 * Checks to see whether or not a node is an ARIA math node.
 *
 * @param {Node} node The node to be checked.
 * @return {boolean} Whether or not the node is an ARIA math node.
 */
cvox.AriaUtil.isMath = function(node) {
  if (!node || !node.getAttribute) {
    return false;
  }
  var role = cvox.AriaUtil.getRoleAttribute(node);
  return role == 'math';
};
