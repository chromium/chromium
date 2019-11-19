// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A collection of JavaScript utilities used to simplify working
 * with the DOM.
 */


goog.provide('cvox.DomUtil');

goog.require('cvox.AbstractTts');
goog.require('cvox.AriaUtil');
goog.require('cvox.ChromeVox');
goog.require('cvox.DomPredicates');
goog.require('cvox.Memoize');
goog.require('cvox.NodeState');
goog.require('cvox.XpathUtil');



/**
 * Create the namespace
 * @constructor
 */
cvox.DomUtil = function() {
};


/**
 * Note: If you are adding a new mapping, the new message identifier needs a
 * corresponding braille message. For example, a message id 'tag_button'
 * requires another message 'tag_button_brl' within messages.js.
 * @type {Object}
 */
cvox.DomUtil.INPUT_TYPE_TO_INFORMATION_TABLE_MSG = {
  'button' : 'role_button',
  'checkbox' : 'role_checkbox',
  'color' : 'input_type_color',
  'datetime' : 'input_type_datetime',
  'datetime-local' : 'input_type_datetime_local',
  'date' : 'input_type_date',
  'email' : 'input_type_email',
  'file' : 'input_type_file',
  'image' : 'role_button',
  'month' : 'input_type_month',
  'number' : 'input_type_number',
  'password' : 'input_type_password',
  'radio' : 'role_radio',
  'range' : 'role_slider',
  'reset' : 'input_type_reset',
  'search' : 'input_type_search',
  'submit' : 'role_button',
  'tel' : 'input_type_number',
  'text' : 'input_type_text',
  'url' : 'input_type_url',
  'week' : 'input_type_week'
};


/**
 * Note: If you are adding a new mapping, the new message identifier needs a
 * corresponding braille message. For example, a message id 'tag_button'
 * requires another message 'tag_button_brl' within messages.js.
 * @type {Object}
 */
cvox.DomUtil.TAG_TO_INFORMATION_TABLE_VERBOSE_MSG = {
  'A' : 'role_link',
  'ARTICLE' : 'tag_article',
  'ASIDE' : 'tag_aside',
  'AUDIO' : 'tag_audio',
  'BUTTON' : 'role_button',
  'FOOTER' : 'tag_footer',
  'H1' : 'tag_h1',
  'H2' : 'tag_h2',
  'H3' : 'tag_h3',
  'H4' : 'tag_h4',
  'H5' : 'tag_h5',
  'H6' : 'tag_h6',
  'HEADER' : 'tag_header',
  'HGROUP' : 'tag_hgroup',
  'LI' : 'tag_li',
  'MARK' : 'tag_mark',
  'NAV' : 'tag_nav',
  'OL' : 'tag_ol',
  'SECTION' : 'tag_section',
  'SELECT' : 'tag_select',
  'TABLE' : 'tag_table',
  'TEXTAREA' : 'tag_textarea',
  'TIME' : 'tag_time',
  'UL' : 'tag_ul',
  'VIDEO' : 'tag_video'
};

/**
 * ChromeVox does not speak the omitted tags.
 * @type {Object}
 */
cvox.DomUtil.TAG_TO_INFORMATION_TABLE_BRIEF_MSG = {
  'AUDIO' : 'tag_audio',
  'BUTTON' : 'role_button',
  'SELECT' : 'tag_select',
  'TABLE' : 'tag_table',
  'TEXTAREA' : 'tag_textarea',
  'VIDEO' : 'tag_video'
};

/**
 * These tags are treated as text formatters.
 * @type {Array<string>}
 */
cvox.DomUtil.FORMATTING_TAGS =
    ['B', 'BIG', 'CITE', 'CODE', 'DFN', 'EM', 'I', 'KBD', 'SAMP', 'SMALL',
     'SPAN', 'STRIKE', 'STRONG', 'SUB', 'SUP', 'U', 'VAR'];

/**
 * Determine if the given node is visible on the page. This does not check if
 * it is inside the document view-port as some sites try to communicate with
 * screen readers with such elements.
 * @param {Node} node The node to determine as visible or not.
 * @param {{checkAncestors: (boolean|undefined),
            checkDescendants: (boolean|undefined)}=} opt_options
 *     In certain cases, we already have information
 *     on the context of the node. To improve performance and avoid redundant
 *     operations, you may wish to turn certain visibility checks off by
 *     passing in an options object. The following properties are configurable:
 *   checkAncestors: {boolean=} True if we should check the ancestor chain
 *       for forced invisibility traits of descendants. True by default.
 *   checkDescendants: {boolean=} True if we should consider descendants of
 *       the  given node for visible elements. True by default.
 * @return {boolean} True if the node is visible.
 */
cvox.DomUtil.isVisible = function(node, opt_options) {
  var checkAncestors = true;
  var checkDescendants = true;
  if (opt_options) {
    if (opt_options.checkAncestors !== undefined) {
      checkAncestors = opt_options.checkAncestors;
    }
    if (opt_options.checkDescendants !== undefined) {
      checkDescendants = opt_options.checkDescendants;
    }
  }

  // Generate a unique function name based on the arguments, and
  // memoize the result of the internal visibility computation so that
  // within the same call stack, we don't need to recompute the visibility
  // of the same node.
  var fname = 'isVisible-' + checkAncestors + '-' + checkDescendants;
  return /** @type {boolean} */ (cvox.Memoize.memoize(
      cvox.DomUtil.computeIsVisible_.bind(
          this, node, checkAncestors, checkDescendants), fname, node));
};

/**
 * Implementation of |cvox.DomUtil.isVisible|.
 * @param {Node} node The node to determine as visible or not.
 * @param {boolean} checkAncestors True if we should check the ancestor chain
 *       for forced invisibility traits of descendants.
 * @param {boolean} checkDescendants True if we should consider descendants of
 *       the  given node for visible elements.
 * @return {boolean} True if the node is visible.
 * @private
 */
cvox.DomUtil.computeIsVisible_ = function(
    node, checkAncestors, checkDescendants) {
  // If the node is an iframe that we can never inject into, consider it hidden.
  if (node.tagName == 'IFRAME' && !node.src) {
    return false;
  }

  // If the node is being forced visible by ARIA, ARIA wins.
  if (cvox.AriaUtil.isForcedVisibleRecursive(node)) {
    return true;
  }

  // Confirm that no subtree containing node is invisible.
  if (checkAncestors &&
      cvox.DomUtil.hasInvisibleAncestor_(node)) {
    return false;
  }

  // If the node's subtree has a visible node, we declare it as visible.
  if (cvox.DomUtil.hasVisibleNodeSubtree_(node, checkDescendants)) {
    return true;
  }

  return false;
};


/**
 * Checks the ancestor chain for the given node for invisibility. If an
 * ancestor is invisible and this cannot be overriden by a descendant,
 * we return true. If the element is not a descendant of the document
 * element it will return true (invisible).
 * @param {Node} node The node to check the ancestor chain for.
 * @return {boolean} True if a descendant is invisible.
 * @private
 */
cvox.DomUtil.hasInvisibleAncestor_ = function(node) {
  var ancestor = node;
  while (ancestor = ancestor.parentElement) {
    var style = document.defaultView.getComputedStyle(ancestor, null);
    if (cvox.DomUtil.isInvisibleStyle(style, true)) {
      return true;
    }
    // Once we reach the document element and we haven't found anything
    // invisible yet, we're done. If we exit the while loop and never found
    // the document element, the element wasn't part of the DOM and thus it's
    // invisible.
    if (ancestor == document.documentElement) {
      return false;
    }
  }
  return true;
};


/**
 * Checks for a visible node in the subtree defined by root.
 * @param {Node} root The root of the subtree to check.
 * @param {boolean} recursive Whether or not to check beyond the root of the
 *     subtree for visible nodes. This option exists for performance tuning.
 *     Sometimes we already have information about the descendants, and we do
 *     not need to check them again.
 * @return {boolean} True if the subtree contains a visible node.
 * @private
 */
cvox.DomUtil.hasVisibleNodeSubtree_ = function(root, recursive) {
  if (!(root instanceof Element)) {
    if (!root.parentElement) {
      return false;
    }
    var parentStyle = document.defaultView
        .getComputedStyle(root.parentElement, null);
    var isVisibleParent = !cvox.DomUtil.isInvisibleStyle(parentStyle);
    return isVisibleParent;
  }

  var rootStyle = document.defaultView.getComputedStyle(root, null);
  var isRootVisible = !cvox.DomUtil.isInvisibleStyle(rootStyle);
  if (isRootVisible) {
    return true;
  }
  var isSubtreeInvisible = cvox.DomUtil.isInvisibleStyle(rootStyle, true);
  if (!recursive || isSubtreeInvisible) {
    return false;
  }

  // Carry on with a recursive check of the descendants.
  var children = root.childNodes;
  for (var i = 0; i < children.length; i++) {
    var child = children[i];
    if (cvox.DomUtil.hasVisibleNodeSubtree_(child, recursive)) {
      return true;
    }
  }
  return false;
};


/**
 * Determines whether or a node is not visible according to any CSS criteria
 * that can hide it.
 * @param {CSSStyleDeclaration} style The style of the node to determine as
 *     invsible or not.
 * @param {boolean=} opt_strict If set to true, we do not check the visibility
 *     style attribute. False by default.
 * CAUTION: Checking the visibility style attribute can result in returning
 *     true (invisible) even when an element has have visible descendants. This
 *     is because an element with visibility:hidden can have descendants that
 *     are visible.
 * @return {boolean} True if the node is invisible.
 */
cvox.DomUtil.isInvisibleStyle = function(style, opt_strict) {
  if (!style) {
    return false;
  }
  if (style.display == 'none') {
    return true;
  }
  // Opacity values range from 0.0 (transparent) to 1.0 (fully opaque).
  if (parseFloat(style.opacity) == 0) {
    return true;
  }
  // Visibility style tests for non-strict checking.
  if (!opt_strict &&
      (style.visibility == 'hidden' || style.visibility == 'collapse')) {
    return true;
  }
  return false;
};


/**
 * Determines whether a control should be announced as disabled.
 *
 * @param {Node} node The node to be examined.
 * @return {boolean} Whether or not the node is disabled.
 */
cvox.DomUtil.isDisabled = function(node) {
  if (node.disabled) {
    return true;
  }
  var ancestor = node;
  while (ancestor = ancestor.parentElement) {
    if (ancestor.tagName == 'FIELDSET' && ancestor.disabled) {
      return true;
    }
  }
  return false;
};


/**
 * Determines whether a node is an HTML5 semantic element
 *
 * @param {Node} node The node to be checked.
 * @return {boolean} True if the node is an HTML5 semantic element.
 */
cvox.DomUtil.isSemanticElt = function(node) {
  if (node.tagName) {
    var tag = node.tagName;
    if ((tag == 'SECTION') || (tag == 'NAV') || (tag == 'ARTICLE') ||
        (tag == 'ASIDE') || (tag == 'HGROUP') || (tag == 'HEADER') ||
        (tag == 'FOOTER') || (tag == 'TIME') || (tag == 'MARK')) {
      return true;
    }
  }
  return false;
};


/**
 * Determines whether or not a node is a leaf node.
 * TODO (adu): This function is doing a lot more than just checking for the
 *     presence of descendants. We should be more precise in the documentation
 *     about what we mean by leaf node.
 *
 * @param {Node} node The node to be checked.
 * @param {boolean=} opt_allowHidden Allows hidden nodes during descent.
 * @return {boolean} True if the node is a leaf node.
 */
cvox.DomUtil.isLeafNode = function(node, opt_allowHidden) {
  // If it's not an Element, then it's a leaf if it has no first child.
  if (!(node instanceof Element)) {
    return (node.firstChild == null);
  }

  // Now we know for sure it's an element.
  var element = /** @type {Element} */(node);
  if (!opt_allowHidden &&
      !cvox.DomUtil.isVisible(element, {checkAncestors: false})) {
    return true;
  }
  if (!opt_allowHidden && cvox.AriaUtil.isHidden(element)) {
    return true;
  }
  if (cvox.AriaUtil.isLeafElement(element)) {
    return true;
  }
  switch (element.tagName) {
    case 'OBJECT':
    case 'EMBED':
    case 'VIDEO':
    case 'AUDIO':
    case 'IFRAME':
    case 'FRAME':
      return true;
  }

  if (!!cvox.DomPredicates.linkPredicate([element])) {
    return !cvox.DomUtil.findNode(element, function(node) {
      return !!cvox.DomPredicates.headingPredicate([node]);
    });
  }
  if (cvox.DomUtil.isLeafLevelControl(element)) {
    return true;
  }
  if (!element.firstChild) {
    return true;
  }
  if (cvox.DomUtil.isMath(element)) {
    return true;
  }
  if (cvox.DomPredicates.headingPredicate([element])) {
    return !cvox.DomUtil.findNode(element, function(n) {
      return !!cvox.DomPredicates.controlPredicate([n]);
    });
  }
  return false;
};


/**
 * Determines whether or not a node is or is the descendant of a node
 * with a particular tag or class name.
 *
 * @param {Node} node The node to be checked.
 * @param {?string} tagName The tag to check for, or null if the tag
 * doesn't matter.
 * @param {?string=} className The class to check for, or null if the class
 * doesn't matter.
 * @return {boolean} True if the node or one of its ancestor has the specified
 * tag.
 */
cvox.DomUtil.isDescendantOf = function(node, tagName, className) {
  while (node) {

    if (tagName && className &&
        (node.tagName && (node.tagName == tagName)) &&
        (node.className && (node.className == className))) {
      return true;
    } else if (tagName && !className &&
               (node.tagName && (node.tagName == tagName))) {
      return true;
    } else if (!tagName && className &&
               (node.className && (node.className == className))) {
      return true;
    }
    node = node.parentNode;
  }
  return false;
};


/**
 * Determines whether or not a node is or is the descendant of another node.
 *
 * @param {Object} node The node to be checked.
 * @param {Object} ancestor The node to see if it's a descendant of.
 * @return {boolean} True if the node is ancestor or is a descendant of it.
 */
cvox.DomUtil.isDescendantOfNode = function(node, ancestor) {
  while (node && ancestor) {
    if (node.isSameNode(ancestor)) {
      return true;
    }
    node = node.parentNode;
  }
  return false;
};


/**
 * Remove all whitespace from the beginning and end, and collapse all
 * inner strings of whitespace to a single space.
 * @param {string} str The input string.
 * @return {string} The string with whitespace collapsed.
 */
cvox.DomUtil.collapseWhitespace = function(str) {
  return str.replace(/\s+/g, ' ').replace(/^\s+|\s+$/g, '');
};

/**
 * Gets the base label of a node. I don't know exactly what this is.
 *
 * @param {Node} node The node to get the label from.
 * @param {boolean=} recursive Whether or not the element's subtree
 *  should be used; true by default.
 * @param {boolean=} includeControls Whether or not controls in the subtree
 *  should be included; true by default.
 * @return {string} The base label of the node.
 * @private
 */
cvox.DomUtil.getBaseLabel_ = function(node, recursive, includeControls) {
  var label = '';
  if (node.hasAttribute) {
    if (node.hasAttribute('aria-labelledby')) {
      var labelNodeIds = node.getAttribute('aria-labelledby').split(' ');
      for (var labelNodeId, i = 0; labelNodeId = labelNodeIds[i]; i++) {
        var labelNode = document.getElementById(labelNodeId);
        if (labelNode) {
          label += ' ' + cvox.DomUtil.getName(
              labelNode, true, includeControls, true);
        }
      }
    } else if (node.hasAttribute('aria-label')) {
      label = node.getAttribute('aria-label');
    } else if (node.constructor == HTMLImageElement) {
      label = cvox.DomUtil.getImageTitle(node);
    } else if (node.tagName == 'FIELDSET') {
      // Other labels will trump fieldset legend with this implementation.
      // Depending on how this works out on the web, we may later switch this
      // to appending the fieldset legend to any existing label.
      var legends = node.getElementsByTagName('LEGEND');
      label = '';
      for (var legend, i = 0; legend = legends[i]; i++) {
        label += ' ' + cvox.DomUtil.getName(legend, true, includeControls);
      }
    }

    if (label.length == 0 && node && node.id) {
      var labelFor = document.querySelector('label[for="' + node.id + '"]');
      if (labelFor) {
        label = cvox.DomUtil.getName(labelFor, recursive, includeControls);
      }
    }
  }
  return cvox.DomUtil.collapseWhitespace(label);
};

/**
 * Gets the nearest label in the ancestor chain, if one exists.
 * @param {Node} node The node to start from.
 * @return {string} The label.
 * @private
 */
cvox.DomUtil.getNearestAncestorLabel_ = function(node) {
  var label = '';
  var enclosingLabel = node;
  while (enclosingLabel && enclosingLabel.tagName != 'LABEL') {
    enclosingLabel = enclosingLabel.parentElement;
  }
  if (enclosingLabel && !enclosingLabel.hasAttribute('for')) {
    // Get all text from the label but don't include any controls.
    label = cvox.DomUtil.getName(enclosingLabel, true, false);
  }
  return label;
};


/**
 * Gets the name for an input element.
 * @param {Node} node The node.
 * @return {string} The name.
 * @private
 */
cvox.DomUtil.getInputName_ = function(node) {
  var label = '';
  if (node.type == 'image') {
    label = cvox.DomUtil.getImageTitle(node);
  } else if (node.type == 'submit') {
    if (node.hasAttribute('value')) {
      label = node.getAttribute('value');
    } else {
      label = 'Submit';
    }
  } else if (node.type == 'reset') {
    if (node.hasAttribute('value')) {
      label = node.getAttribute('value');
    } else {
      label = 'Reset';
    }
  } else if (node.type == 'button') {
    if (node.hasAttribute('value')) {
      label = node.getAttribute('value');
    }
  }
  return label;
};

/**
 * Wraps getName_ with marking and unmarking nodes so that infinite loops
 * don't occur. This is the ugly way to solve this; getName should not ever
 * do a recursive call somewhere above it in the tree.
 * @param {Node} node See getName_.
 * @param {boolean=} recursive See getName_.
 * @param {boolean=} includeControls See getName_.
 * @param {boolean=} opt_allowHidden Allows hidden nodes in name computation.
 * @return {string} See getName_.
 */
cvox.DomUtil.getName = function(
    node, recursive, includeControls, opt_allowHidden) {
  if (!node || node.cvoxGetNameMarked == true) {
    return '';
  }
  node.cvoxGetNameMarked = true;
  var ret =
      cvox.DomUtil.getName_(node, recursive, includeControls, opt_allowHidden);
  node.cvoxGetNameMarked = false;
  var prefix = cvox.DomUtil.getPrefixText(node);
  return prefix + ret;
};

// TODO(dtseng): Seems like this list should be longer...
/**
 * Determines if a node has a name obtained from concatinating the names of its
 * children.
 * @param {!Node} node The node under consideration.
 * @param {boolean=} opt_allowHidden Allows hidden nodes in name computation.
 * @return {boolean} True if node has name based on children.
 * @private
 */
cvox.DomUtil.hasChildrenBasedName_ = function(node, opt_allowHidden) {
  if (!!cvox.DomPredicates.linkPredicate([node]) ||
      !!cvox.DomPredicates.headingPredicate([node]) ||
      node.tagName == 'BUTTON' ||
      cvox.AriaUtil.isControlWidget(node) ||
      !cvox.DomUtil.isLeafNode(node, opt_allowHidden)) {
    return true;
  } else {
    return false;
  }
};

/**
 * Get the name of a node: this includes all static text content and any
 * HTML-author-specified label, title, alt text, aria-label, etc. - but
 * does not include:
 * - the user-generated control value (use getValue)
 * - the current state (use getState)
 * - the role (use getRole)
 *
 * Order of precedence:
 *   Text content if it's a text node.
 *   aria-labelledby
 *   aria-label
 *   alt (for an image)
 *   title
 *   label (for a control)
 *   placeholder (for an input element)
 *   recursive calls to getName on all children
 *
 * @param {Node} node The node to get the name from.
 * @param {boolean=} recursive Whether or not the element's subtree should
 *     be used; true by default.
 * @param {boolean=} includeControls Whether or not controls in the subtree
 *     should be included; true by default.
 * @param {boolean=} opt_allowHidden Allows hidden nodes in name computation.
 * @return {string} The name of the node.
 * @private
 */
cvox.DomUtil.getName_ = function(
    node, recursive, includeControls, opt_allowHidden) {
  if (typeof(recursive) === 'undefined') {
    recursive = true;
  }
  if (typeof(includeControls) === 'undefined') {
    includeControls = true;
  }

  if (node.constructor == Text) {
    return node.data;
  }

  var label = cvox.DomUtil.getBaseLabel_(node, recursive, includeControls);

  if (label.length == 0 && cvox.DomUtil.isControl(node)) {
    label = cvox.DomUtil.getNearestAncestorLabel_(node);
  }

  if (label.length == 0 && node.constructor == HTMLInputElement) {
    label = cvox.DomUtil.getInputName_(node);
  }

  if (cvox.DomUtil.isInputTypeText(node) && node.hasAttribute('placeholder')) {
    var placeholder = node.getAttribute('placeholder');
    if (label.length > 0) {
      if (cvox.DomUtil.getValue(node).length > 0) {
        return label;
      } else {
        return label + ' with hint ' + placeholder;
      }
    } else {
      return placeholder;
    }
  }

  if (label.length > 0) {
    return label;
  }

  // Fall back to naming via title only if there is no text content.
  if (cvox.DomUtil.collapseWhitespace(node.textContent).length == 0 &&
      node.hasAttribute &&
      node.hasAttribute('title')) {
    return node.getAttribute('title');
  }

  if (!recursive) {
    return '';
  }

  if (cvox.AriaUtil.isCompositeControl(node)) {
    return '';
  }
  if (cvox.DomUtil.hasChildrenBasedName_(node, opt_allowHidden)) {
    return cvox.DomUtil.getNameFromChildren(
        node, includeControls, opt_allowHidden);
  }
  return '';
};


/**
 * Get the name from the children of a node, not including the node itself.
 *
 * @param {Node} node The node to get the name from.
 * @param {boolean=} includeControls Whether or not controls in the subtree
 *     should be included; true by default.
 * @param {boolean=} opt_allowHidden Allow hidden nodes in name computation.
 * @return {string} The concatenated text of all child nodes.
 */
cvox.DomUtil.getNameFromChildren = function(
    node, includeControls, opt_allowHidden) {
  if (includeControls == undefined) {
    includeControls = true;
  }
  var name = '';
  var delimiter = '';
  for (var i = 0; i < node.childNodes.length; i++) {
    var child = node.childNodes[i];
    var prevChild = node.childNodes[i - 1] || child;
    if (!includeControls && cvox.DomUtil.isControl(child)) {
      continue;
    }
    var isVisible = cvox.DomUtil.isVisible(child, {checkAncestors: false});
    if (opt_allowHidden || (isVisible && !cvox.AriaUtil.isHidden(child))) {
      delimiter = (prevChild.tagName == 'SPAN' ||
                   child.tagName == 'SPAN' ||
                   child.parentNode.tagName == 'SPAN') ?
          '' : ' ';
      name += delimiter + cvox.DomUtil.getName(child, true, includeControls);
    }
  }

  return name;
};

/**
 * Get any prefix text for the given node.
 * This includes list style text for the leftmost leaf node under a listitem.
 * @param {Node} node Compute prefix for this node.
 * @param {number=} opt_index Starting offset into the given node's text.
 * @return {string} Prefix text, if any.
 */
cvox.DomUtil.getPrefixText = function(node, opt_index) {
  opt_index = opt_index || 0;

  // Generate list style text.
  var ancestors = cvox.DomUtil.getAncestors(node);
  var prefix = '';
  var firstListitem = cvox.DomPredicates.listItemPredicate(ancestors);

  var leftmost = firstListitem;
  while (leftmost && leftmost.firstChild) {
    leftmost = leftmost.firstChild;
  }

  // Do nothing if we're not at the leftmost leaf.
  if (firstListitem &&
      firstListitem.parentNode &&
      opt_index == 0 &&
      firstListitem.parentNode.tagName == 'OL' &&
          node == leftmost &&
      document.defaultView.getComputedStyle(firstListitem.parentNode)
          .listStyleType != 'none') {
    var items = cvox.DomUtil.toArray(firstListitem.parentNode.children).filter(
        function(li) { return li.tagName == 'LI'; });
    var position = items.indexOf(firstListitem) + 1;
    // TODO(dtseng): Support all list style types.
    if (document.defaultView.getComputedStyle(
            firstListitem.parentNode).listStyleType.indexOf('latin') != -1) {
      position--;
      prefix = String.fromCharCode('A'.charCodeAt(0) + position % 26);
    } else {
      prefix = position;
    }
    prefix += '. ';
  }
  return prefix;
};


/**
 * Use heuristics to guess at the label of a control, to be used if one
 * is not explicitly set in the DOM. This is useful when a control
 * field gets focus, but probably not useful when browsing the page
 * element at a time.
 * @param {Node} node The node to get the label from.
 * @return {string} The name of the control, using heuristics.
 */
cvox.DomUtil.getControlLabelHeuristics = function(node) {
  // If the node explicitly has aria-label or title set to '',
  // treat it the same way as alt='' and do not guess - just assume
  // the web developer knew what they were doing and wanted
  // no title/label for that control.
  if (node.hasAttribute &&
      ((node.hasAttribute('aria-label') &&
      (node.getAttribute('aria-label') == '')) ||
      (node.hasAttribute('aria-title') &&
      (node.getAttribute('aria-title') == '')))) {
    return '';
  }

  // TODO (clchen, rshearer): Implement heuristics for getting the label
  // information from the table headers once the code for getting table
  // headers quickly is implemented.

  // If no description has been found yet and heuristics are enabled,
  // then try getting the content from the closest node.
  var prevNode = cvox.DomUtil.previousLeafNode(node);
  var prevTraversalCount = 0;
  while (prevNode && (!cvox.DomUtil.hasContent(prevNode) ||
      cvox.DomUtil.isControl(prevNode))) {
    prevNode = cvox.DomUtil.previousLeafNode(prevNode);
    prevTraversalCount++;
  }
  var nextNode = cvox.DomUtil.directedNextLeafNode(node);
  var nextTraversalCount = 0;
  while (nextNode && (!cvox.DomUtil.hasContent(nextNode) ||
      cvox.DomUtil.isControl(nextNode))) {
    nextNode = cvox.DomUtil.directedNextLeafNode(nextNode);
    nextTraversalCount++;
  }
  var guessedLabelNode;
  if (prevNode && nextNode) {
    var parentNode = node;
    // Count the number of parent nodes until there is a shared parent; the
    // label is most likely in the same branch of the DOM as the control.
    // TODO (chaitanyag): Try to generalize this algorithm and move it to
    // its own function in DOM Utils.
    var prevCount = 0;
    while (parentNode) {
      if (cvox.DomUtil.isDescendantOfNode(prevNode, parentNode)) {
        break;
      }
      parentNode = parentNode.parentNode;
      prevCount++;
    }
    parentNode = node;
    var nextCount = 0;
    while (parentNode) {
      if (cvox.DomUtil.isDescendantOfNode(nextNode, parentNode)) {
        break;
      }
      parentNode = parentNode.parentNode;
      nextCount++;
    }
    guessedLabelNode = nextCount < prevCount ? nextNode : prevNode;
  } else {
    guessedLabelNode = prevNode || nextNode;
  }
  if (guessedLabelNode) {
    return cvox.DomUtil.collapseWhitespace(
        cvox.DomUtil.getValue(guessedLabelNode) + ' ' +
        cvox.DomUtil.getName(guessedLabelNode));
  }

  return '';
};


/**
 * Get the text value of a node: the selected value of a select control or the
 * current text of a text control. Does not return the state of a checkbox
 * or radio button.
 *
 * Not recursive.
 *
 * @param {Node} node The node to get the value from.
 * @return {string} The value of the node.
 */
cvox.DomUtil.getValue = function(node) {
  var activeDescendant = cvox.AriaUtil.getActiveDescendant(node);
  if (activeDescendant) {
    return cvox.DomUtil.collapseWhitespace(
        cvox.DomUtil.getValue(activeDescendant) + ' ' +
        cvox.DomUtil.getName(activeDescendant));
  }

  if (node.constructor == HTMLSelectElement) {
    node = /** @type {HTMLSelectElement} */(node);
    var value = '';
    var start = node.selectedOptions ? node.selectedOptions[0] : null;
    var end = node.selectedOptions ?
        node.selectedOptions[node.selectedOptions.length - 1] : null;
    // TODO(dtseng): Keeping this stateless means we describe the start and end
    // of the selection only since we don't know which was added or
    // removed. Once we keep the previous selection, we can read the diff.
    if (start && end && start != end) {
      value = Msgs.getMsg(
        'selected_options_value', [start.text, end.text]);
    } else if (start) {
      value = start.text + '';
    }
    return value;
  }

  if (node.constructor == HTMLTextAreaElement) {
    return node.value;
  }

  if (node.constructor == HTMLInputElement) {
    switch (node.type) {
      // Returning '' for inputs that are covered by getName.
      case 'hidden':
      case 'image':
      case 'submit':
      case 'reset':
      case 'button':
      case 'checkbox':
      case 'radio':
        return '';
      case 'password':
        return node.value.replace(/./g, 'dot ');
      default:
        return node.value;
    }
  }

  if (node.isContentEditable) {
    return cvox.DomUtil.getNameFromChildren(node, true);
  }

  return '';
};


/**
 * Given an image node, return its title as a string. The preferred title
 * is always the alt text, and if that's not available, then the title
 * attribute. If neither of those are available, it attempts to construct
 * a title from the filename, and if all else fails returns the word Image.
 * @param {Node} node The image node.
 * @return {string} The title of the image.
 */
cvox.DomUtil.getImageTitle = function(node) {
  var text;
  if (node.hasAttribute('alt')) {
    text = node.alt;
  } else if (node.hasAttribute('title')) {
    text = node.title;
  } else {
    var url = node.src;
    if (url.substring(0, 4) != 'data') {
      var filename = url.substring(
          url.lastIndexOf('/') + 1, url.lastIndexOf('.'));

      // Hack to not speak the filename if it's ridiculously long.
      if (filename.length >= 1 && filename.length <= 16) {
        text = filename + ' Image';
      } else {
        text = 'Image';
      }
    } else {
      text = 'Image';
    }
  }
  return text;
};


/**
 * Search the whole page for any aria-labelledby attributes and collect
 * the complete set of ids they map to, so that we can skip elements that
 * just label other elements and not double-speak them. We cache this
 * result and then throw it away at the next event loop.
 * @return {Object<boolean>} Set of all ids that are mapped by aria-labelledby.
 */
cvox.DomUtil.getLabelledByTargets = function() {
  if (cvox.labelledByTargets) {
    return cvox.labelledByTargets;
  }

  // Start by getting all elements with
  // aria-labelledby on the page since that's probably a short list,
  // then see if any of those ids overlap with an id in this element's
  // ancestor chain.
  var labelledByElements = document.querySelectorAll('[aria-labelledby]');
  var labelledByTargets = {};
  for (var i = 0; i < labelledByElements.length; ++i) {
    var element = labelledByElements[i];
    var attrValue = element.getAttribute('aria-labelledby');
    var ids = attrValue.split(/ +/);
    for (var j = 0; j < ids.length; j++) {
      labelledByTargets[ids[j]] = true;
    }
  }
  cvox.labelledByTargets = labelledByTargets;

  window.setTimeout(function() {
    cvox.labelledByTargets = null;
  }, 0);

  return labelledByTargets;
};


/**
 * Determines whether or not a node has content.
 *
 * @param {Node} node The node to be checked.
 * @return {boolean} True if the node has content.
 */
cvox.DomUtil.hasContent = function(node) {
  // Memoize the result of the internal content computation so that
  // within the same call stack, we don't need to redo the computation
  // on the same node twice.
  return /** @type {boolean} */ (cvox.Memoize.memoize(
      cvox.DomUtil.computeHasContent_.bind(this), 'hasContent', node));
};

/**
 * Internal implementation of |cvox.DomUtil.hasContent|.
 *
 * @param {Node} node The node to be checked.
 * @return {boolean} True if the node has content.
 * @private
 */
cvox.DomUtil.computeHasContent_ = function(node) {
  // nodeType:8 == COMMENT_NODE
  if (node.nodeType == 8) {
    return false;
  }

  // Exclude anything in the head
  if (cvox.DomUtil.isDescendantOf(node, 'HEAD')) {
    return false;
  }

  // Exclude script nodes
  if (cvox.DomUtil.isDescendantOf(node, 'SCRIPT')) {
    return false;
  }

  // Exclude noscript nodes
  if (cvox.DomUtil.isDescendantOf(node, 'NOSCRIPT')) {
    return false;
  }

  // Exclude noembed nodes since NOEMBED is deprecated. We treat
  // noembed as having not content rather than try to get its content since
  // Chrome will return raw HTML content rather than a valid DOM subtree.
  if (cvox.DomUtil.isDescendantOf(node, 'NOEMBED')) {
    return false;
  }

  // Exclude style nodes that have been dumped into the body.
  if (cvox.DomUtil.isDescendantOf(node, 'STYLE')) {
    return false;
  }

  // Check the style to exclude undisplayed/hidden nodes.
  if (!cvox.DomUtil.isVisible(node)) {
    return false;
  }

  // Ignore anything that is hidden by ARIA.
  if (cvox.AriaUtil.isHidden(node)) {
    return false;
  }

  // We need to speak controls, including those with no value entered. We
  // therefore treat visible controls as if they had content, and return true
  // below.
  if (cvox.DomUtil.isControl(node)) {
    return true;
  }

  // Videos are always considered to have content so that we can navigate to
  // and use the controls of the video widget.
  if (cvox.DomUtil.isDescendantOf(node, 'VIDEO')) {
    return true;
  }
  // Audio elements are always considered to have content so that we can
  // navigate to and use the controls of the audio widget.
  if (cvox.DomUtil.isDescendantOf(node, 'AUDIO')) {
    return true;
  }

  // We want to try to jump into an iframe iff it has a src attribute.
  // For right now, we will avoid iframes without any content in their src since
  // ChromeVox is not being injected in those cases and will cause the user to
  // get stuck.
  // TODO (clchen, dmazzoni): Manually inject ChromeVox for iframes without src.
  if ((node.tagName == 'IFRAME') && (node.src) &&
      (node.src.indexOf('javascript:') != 0)) {
    return true;
  }

  var controlQuery = 'button,input,select,textarea';

  // Skip any non-control content inside of a label if the label is
  // correctly associated with a control, the label text will get spoken
  // when the control is reached.
  var enclosingLabel = node.parentElement;
  while (enclosingLabel && enclosingLabel.tagName != 'LABEL') {
    enclosingLabel = enclosingLabel.parentElement;
  }
  if (enclosingLabel) {
    var embeddedControl = enclosingLabel.querySelector(controlQuery);
    if (enclosingLabel.hasAttribute('for')) {
      var targetId = enclosingLabel.getAttribute('for');
      var targetNode = document.getElementById(targetId);
      if (targetNode &&
          cvox.DomUtil.isControl(targetNode) &&
          !embeddedControl) {
        return false;
      }
    } else if (embeddedControl) {
      return false;
    }
  }

  // Skip any non-control content inside of a legend if the legend is correctly
  // nested within a fieldset. The legend text will get spoken when the fieldset
  // is reached.
  var enclosingLegend = node.parentElement;
  while (enclosingLegend && enclosingLegend.tagName != 'LEGEND') {
    enclosingLegend = enclosingLegend.parentElement;
  }
  if (enclosingLegend) {
    var legendAncestor = enclosingLegend.parentElement;
    while (legendAncestor && legendAncestor.tagName != 'FIELDSET') {
      legendAncestor = legendAncestor.parentElement;
    }
    var embeddedControl =
        legendAncestor && legendAncestor.querySelector(controlQuery);
    if (legendAncestor && !embeddedControl) {
      return false;
    }
  }

  if (!!cvox.DomPredicates.linkPredicate([node])) {
    return true;
  }

  // At this point, any non-layout tables are considered to have content.
  // For layout tables, it is safe to consider them as without content since the
  // sync operation would select a descendant of a layout table if possible. The
  // only instance where |hasContent| gets called on a layout table is if no
  // descendants have content (see |AbstractNodeWalker.next|).
  if (node.tagName == 'TABLE' && !cvox.DomUtil.isLayoutTable(node)) {
    return true;
  }

  // Math is always considered to have content.
  if (cvox.DomUtil.isMath(node)) {
    return true;
  }

  if (cvox.DomPredicates.headingPredicate([node])) {
    return true;
  }

  if (cvox.DomUtil.isFocusable(node)) {
    return true;
  }

  // Skip anything referenced by another element on the page
  // via aria-labelledby.
  var labelledByTargets = cvox.DomUtil.getLabelledByTargets();
  var enclosingNodeWithId = node;
  while (enclosingNodeWithId) {
    if (enclosingNodeWithId.id &&
        labelledByTargets[enclosingNodeWithId.id]) {
      // If we got here, some element on this page has an aria-labelledby
      // attribute listing this node as its id. As long as that "some" element
      // is not this element, we should return false, indicating this element
      // should be skipped.
      var attrValue = enclosingNodeWithId.getAttribute('aria-labelledby');
      if (attrValue) {
        var ids = attrValue.split(/ +/);
        if (ids.indexOf(enclosingNodeWithId.id) == -1) {
          return false;
        }
      } else {
        return false;
      }
    }
    enclosingNodeWithId = enclosingNodeWithId.parentElement;
  }

  var text = cvox.DomUtil.getValue(node) + ' ' + cvox.DomUtil.getName(node);
  var state = cvox.DomUtil.getState(node, true);
  if (text.match(/^\s+$/) && state === '') {
    // Text only contains whitespace
    return false;
  }

  return true;
};


/**
 * Returns a list of all the ancestors of a given node. The last element
 * is the current node.
 *
 * @param {Node} targetNode The node to get ancestors for.
 * @return {Array<Node>} An array of ancestors for the targetNode.
 */
cvox.DomUtil.getAncestors = function(targetNode) {
  var ancestors = new Array();
  while (targetNode) {
    ancestors.push(targetNode);
    targetNode = targetNode.parentNode;
  }
  ancestors.reverse();
  while (ancestors.length && !ancestors[0].tagName && !ancestors[0].nodeValue) {
    ancestors.shift();
  }
  return ancestors;
};


/**
 * Compares Ancestors of A with Ancestors of B and returns
 * the index value in B at which B diverges from A.
 * If there is no divergence, the result will be -1.
 * Note that if B is the same as A except B has more nodes
 * even after A has ended, that is considered a divergence.
 * The first node that B has which A does not have will
 * be treated as the divergence point.
 *
 * @param {Object} ancestorsA The array of ancestors for Node A.
 * @param {Object} ancestorsB The array of ancestors for Node B.
 * @return {number} The index of the divergence point (the first node that B has
 * which A does not have in B's list of ancestors).
 */
cvox.DomUtil.compareAncestors = function(ancestorsA, ancestorsB) {
  var i = 0;
  while (ancestorsA[i] && ancestorsB[i] && (ancestorsA[i] == ancestorsB[i])) {
    i++;
  }
  if (!ancestorsA[i] && !ancestorsB[i]) {
    i = -1;
  }
  return i;
};


/**
 * Returns an array of ancestors that are unique for the currentNode when
 * compared to the previousNode. Having such an array is useful in generating
 * the node information (identifying when interesting node boundaries have been
 * crossed, etc.).
 *
 * @param {Node} previousNode The previous node.
 * @param {Node} currentNode The current node.
 * @param {boolean=} opt_fallback True returns node's ancestors in the case
 * where node's ancestors is a subset of previousNode's ancestors.
 * @return {Array<Node>} An array of unique ancestors for the current node
 * (inclusive).
 */
cvox.DomUtil.getUniqueAncestors = function(
    previousNode, currentNode, opt_fallback) {
  var prevAncestors = cvox.DomUtil.getAncestors(previousNode);
  var currentAncestors = cvox.DomUtil.getAncestors(currentNode);
  var divergence = cvox.DomUtil.compareAncestors(prevAncestors,
      currentAncestors);
  var diff = currentAncestors.slice(divergence);
  return (diff.length == 0 && opt_fallback) ? currentAncestors : diff;
};


/**
 * Returns a role message identifier for a node.
 * For a localized string, see cvox.DomUtil.getRole.
 * @param {Node} targetNode The node to get the role name for.
 * @param {number} verbosity The verbosity setting to use.
 * @return {string} The role message identifier for the targetNode.
 */
cvox.DomUtil.getRoleMsg = function(targetNode, verbosity) {
  var info;
  info = cvox.AriaUtil.getRoleNameMsg(targetNode);
  if (!info) {
    if (targetNode.tagName == 'INPUT') {
      info = cvox.DomUtil.INPUT_TYPE_TO_INFORMATION_TABLE_MSG[targetNode.type];
    } else if (targetNode.tagName == 'A' &&
        cvox.DomUtil.isInternalLink(targetNode)) {
      info = 'internal_link';
    } else if (targetNode.tagName == 'A' &&
        targetNode.getAttribute('href') &&
        cvox.ChromeVox.visitedUrls[targetNode.href]) {
      info = 'visited_link';
    } else if (targetNode.tagName == 'A' &&
        targetNode.getAttribute('name')) {
      info = ''; // Don't want to add any role to anchors.
    } else if (targetNode.isContentEditable) {
      info = 'input_type_text';
    } else if (cvox.DomUtil.isMath(targetNode)) {
      info = 'math_expr';
    } else if (targetNode.tagName == 'TABLE' &&
        cvox.DomUtil.isLayoutTable(targetNode)) {
      info = '';
    } else {
      if (verbosity == cvox.VERBOSITY_BRIEF) {
        info =
            cvox.DomUtil.TAG_TO_INFORMATION_TABLE_BRIEF_MSG[targetNode.tagName];
      } else {
        info = cvox.DomUtil.TAG_TO_INFORMATION_TABLE_VERBOSE_MSG[
          targetNode.tagName];

        if (cvox.DomUtil.hasLongDesc(targetNode)) {
          info = 'image_with_long_desc';
        }

        if (!info && targetNode.onclick) {
          info = 'clickable';
        }
      }
    }
  }

  return info;
};


/**
 * Returns a string to be presented to the user that identifies what the
 * targetNode's role is.
 * ARIA roles are given priority; if there is no ARIA role set, the role
 * will be determined by the HTML tag for the node.
 *
 * @param {Node} targetNode The node to get the role name for.
 * @param {number} verbosity The verbosity setting to use.
 * @return {string} The role name for the targetNode.
 */
cvox.DomUtil.getRole = function(targetNode, verbosity) {
  var roleMsg = cvox.DomUtil.getRoleMsg(targetNode, verbosity) || '';
  var role = roleMsg && roleMsg != ' ' ?
      Msgs.getMsg(roleMsg) : '';
  return role ? role : roleMsg;
};


/**
 * Count the number of items in a list node.
 *
 * @param {Node} targetNode The list node.
 * @return {number} The number of items in the list.
 */
cvox.DomUtil.getListLength = function(targetNode) {
  var count = 0;
  for (var node = targetNode.firstChild;
       node;
       node = node.nextSibling) {
    if (cvox.DomUtil.isVisible(node) &&
        (node.tagName == 'LI' ||
        (node.getAttribute && node.getAttribute('role') == 'listitem'))) {
      if (node.hasAttribute('aria-setsize')) {
        var ariaLength = parseInt(node.getAttribute('aria-setsize'), 10);
        if (!isNaN(ariaLength)) {
          return ariaLength;
        }
      }
      count++;
    }
  }
  return count;
};


/**
 * Returns a NodeState that gives information about the state of the targetNode.
 *
 * @param {Node} targetNode The node to get the state information for.
 * @param {boolean} primary Whether this is the primary node we're
 *     interested in, where we might want extra information - as
 *     opposed to an ancestor, where we might be more brief.
 * @return {cvox.NodeState} The status information about the node.
 */
cvox.DomUtil.getStateMsgs = function(targetNode, primary) {
  var activeDescendant = cvox.AriaUtil.getActiveDescendant(targetNode);
  if (activeDescendant) {
    return cvox.DomUtil.getStateMsgs(activeDescendant, primary);
  }
  var info = [];
  var role = targetNode.getAttribute ? targetNode.getAttribute('role') : '';
  info = cvox.AriaUtil.getStateMsgs(targetNode, primary);
  if (!info) {
    info = [];
  }

  if (targetNode.tagName == 'INPUT') {
    if (!targetNode.hasAttribute('aria-checked')) {
      var INPUT_MSGS = {
        'checkbox-true': 'checkbox_checked_state',
        'checkbox-false': 'checkbox_unchecked_state',
        'radio-true': 'radio_selected_state',
        'radio-false': 'radio_unselected_state' };
      var msgId = INPUT_MSGS[targetNode.type + '-' + !!targetNode.checked];
      if (msgId) {
        info.push([msgId]);
      }
    }
  } else if (targetNode.tagName == 'SELECT') {
    if (targetNode.selectedOptions && targetNode.selectedOptions.length <= 1) {
      info.push(['list_position',
                 Msgs.getNumber(targetNode.selectedIndex + 1),
                 Msgs.getNumber(targetNode.options.length)]);
    } else {
      info.push(['selected_options_state',
          Msgs.getNumber(targetNode.selectedOptions.length)]);
    }
  } else if (targetNode.tagName == 'UL' ||
             targetNode.tagName == 'OL' ||
             role == 'list') {
    info.push(['list_with_items_not_pluralized',
               Msgs.getNumber(
                   cvox.DomUtil.getListLength(targetNode))]);
  }

  if (cvox.DomUtil.isDisabled(targetNode)) {
    info.push(['aria_disabled_true']);
  }

  if (targetNode.accessKey) {
    info.push(['access_key', targetNode.accessKey]);
  }

  return info;
};


/**
 * Returns a string that gives information about the state of the targetNode.
 *
 * @param {Node} targetNode The node to get the state information for.
 * @param {boolean} primary Whether this is the primary node we're
 *     interested in, where we might want extra information - as
 *     opposed to an ancestor, where we might be more brief.
 * @return {string} The status information about the node.
 */
cvox.DomUtil.getState = function(targetNode, primary) {
  return cvox.NodeStateUtil.expand(
      cvox.DomUtil.getStateMsgs(targetNode, primary));
};


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
cvox.DomUtil.isFocusable = function(targetNode) {
  if (!targetNode || typeof(targetNode.tabIndex) != 'number') {
    return false;
  }

  // Workaround for http://code.google.com/p/chromium/issues/detail?id=153904
  if ((targetNode.tagName == 'A') && !targetNode.hasAttribute('href') &&
      !targetNode.hasAttribute('tabindex')) {
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
};


/**
 * Find a focusable descendant of a given node. This includes nodes whose
 * tabindex attribute is set to "-1" explicitly - these nodes are not in the
 * tab order, but they should still be focused if the user navigates to them
 * using linear or smart DOM navigation.
 *
 * @param {Node} targetNode The node whose descendants to check if focusable.
 * @return {Node} The focusable descendant node. Null if no descendant node
 * was found.
 */
cvox.DomUtil.findFocusableDescendant = function(targetNode) {
  // Search down the descendants chain until a focusable node is found
  if (targetNode) {
    var focusableNode =
        cvox.DomUtil.findNode(targetNode, cvox.DomUtil.isFocusable);
    if (focusableNode) {
      return focusableNode;
    }
  }
  return null;
};


/**
 * Returns the number of focusable nodes in root's subtree. The count does not
 * include root.
 *
 * @param {Node} targetNode The node whose descendants to check are focusable.
 * @return {number} The number of focusable descendants.
 */
cvox.DomUtil.countFocusableDescendants = function(targetNode) {
  return targetNode ?
      cvox.DomUtil.countNodes(targetNode, cvox.DomUtil.isFocusable) : 0;
};


/**
 * Checks if the targetNode is still attached to the document.
 * A node can become detached because of AJAX changes.
 *
 * @param {Object} targetNode The node to check.
 * @return {boolean} True if the targetNode is still attached.
 */
cvox.DomUtil.isAttachedToDocument = function(targetNode) {
  while (targetNode) {
    if (targetNode.tagName && (targetNode.tagName == 'HTML')) {
      return true;
    }
    targetNode = targetNode.parentNode;
  }
  return false;
};


/**
 * Dispatches a left click event on the element that is the targetNode.
 * Clicks go in the sequence of mousedown, mouseup, and click.
 * @param {Node} targetNode The target node of this operation.
 * @param {boolean} shiftKey Specifies if shift is held down.
 * @param {boolean} callOnClickDirectly Specifies whether or not to directly
 * invoke the onclick method if there is one.
 * @param {boolean=} opt_double True to issue a double click.
 * @param {boolean=} opt_handleOwnEvents Whether to handle the generated
 *     events through the normal event processing.
 */
cvox.DomUtil.clickElem = function(
    targetNode, shiftKey, callOnClickDirectly, opt_double,
    opt_handleOwnEvents) {
  // If there is an activeDescendant of the targetNode, then that is where the
  // click should actually be targeted.
  var activeDescendant = cvox.AriaUtil.getActiveDescendant(targetNode);
  if (activeDescendant) {
    targetNode = activeDescendant;
  }
  if (callOnClickDirectly) {
    var onClickFunction = null;
    if (targetNode.onclick) {
      onClickFunction = targetNode.onclick;
    }
    if (!onClickFunction && (targetNode.nodeType != 1) &&
        targetNode.parentNode && targetNode.parentNode.onclick) {
      onClickFunction = targetNode.parentNode.onclick;
    }
    var keepGoing = true;
    if (onClickFunction) {
      try {
        keepGoing = onClickFunction();
      } catch (exception) {
        // Something went very wrong with the onclick method; we'll ignore it
        // and just dispatch a click event normally.
      }
    }
    if (!keepGoing) {
      // The onclick method ran successfully and returned false, meaning the
      // event should not bubble up, so we will return here.
      return;
    }
  }

  // Send a mousedown (or simply a double click if requested).
  var evt = document.createEvent('MouseEvents');
  var evtType = opt_double ? 'dblclick' : 'mousedown';
  evt.initMouseEvent(evtType, true, true, document.defaultView,
                     1, 0, 0, 0, 0, false, false, shiftKey, false, 0, null);
  // Unless asked not to, Mark any events we generate so we don't try to
  // process our own events.
  evt.fromCvox = !opt_handleOwnEvents;
  try {
    targetNode.dispatchEvent(evt);
  } catch (e) {}
  //Send a mouse up
  evt = document.createEvent('MouseEvents');
  evt.initMouseEvent('mouseup', true, true, document.defaultView,
                     1, 0, 0, 0, 0, false, false, shiftKey, false, 0, null);
  evt.fromCvox = !opt_handleOwnEvents;
  try {
    targetNode.dispatchEvent(evt);
  } catch (e) {}
  //Send a click
  evt = document.createEvent('MouseEvents');
  evt.initMouseEvent('click', true, true, document.defaultView,
                     1, 0, 0, 0, 0, false, false, shiftKey, false, 0, null);
  evt.fromCvox = !opt_handleOwnEvents;
  try {
    targetNode.dispatchEvent(evt);
  } catch (e) {}

  if (cvox.DomUtil.isInternalLink(targetNode)) {
    cvox.DomUtil.syncInternalLink(targetNode);
  }
};


/**
 * Syncs to an internal link.
 * @param {Node} node A link whose href's target we want to sync.
 */
cvox.DomUtil.syncInternalLink = function(node) {
  var targetNode;
  var targetId = node.href.split('#')[1];
  targetNode = document.getElementById(targetId);
  if (!targetNode) {
    var nodes = document.getElementsByName(targetId);
    if (nodes.length > 0) {
      targetNode = nodes[0];
    }
  }
  if (targetNode) {
    // Insert a dummy node to adjust next Tab focus location.
    var parent = targetNode.parentNode;
    var dummyNode = document.createElement('div');
    dummyNode.setAttribute('tabindex', '-1');
    parent.insertBefore(dummyNode, targetNode);
    dummyNode.setAttribute('chromevoxignoreariahidden', 1);
    dummyNode.focus();
    cvox.ChromeVox.syncToNode(targetNode, false);
  }
};


/**
 * Given an HTMLInputElement, returns true if it's an editable text type.
 * This includes input type='text' and input type='password' and a few
 * others.
 *
 * @param {Node} node The node to check.
 * @return {boolean} True if the node is an INPUT with an editable text type.
 */
cvox.DomUtil.isInputTypeText = function(node) {
  if (!node || node.constructor != HTMLInputElement) {
    return false;
  }

  switch (node.type) {
    case 'email':
    case 'number':
    case 'password':
    case 'search':
    case 'text':
    case 'tel':
    case 'url':
    case '':
      return true;
    default:
      return false;
  }
};


/**
 * Given a node, returns true if it's a control. Controls are *not necessarily*
 * leaf-level given that some composite controls may have focusable children
 * if they are managing focus with tabindex:
 * ( http://www.w3.org/TR/2010/WD-wai-aria-practices-20100916/#visualfocus ).
 *
 * @param {Node} node The node to check.
 * @return {boolean} True if the node is a control.
 */
cvox.DomUtil.isControl = function(node) {
  if (cvox.AriaUtil.isControlWidget(node) &&
      cvox.DomUtil.isFocusable(node)) {
    return true;
  }
  if (node.tagName) {
    switch (node.tagName) {
      case 'BUTTON':
      case 'TEXTAREA':
      case 'SELECT':
        return true;
      case 'INPUT':
        return node.type != 'hidden';
    }
  }
  if (node.isContentEditable) {
    return true;
  }
  return false;
};


/**
 * Given a node, returns true if it's a leaf-level control. This includes
 * composite controls thare are managing focus for children with
 * activedescendant, but not composite controls with focusable children:
 * ( http://www.w3.org/TR/2010/WD-wai-aria-practices-20100916/#visualfocus ).
 *
 * @param {Node} node The node to check.
 * @return {boolean} True if the node is a leaf-level control.
 */
cvox.DomUtil.isLeafLevelControl = function(node) {
  if (cvox.DomUtil.isControl(node)) {
    return !(cvox.AriaUtil.isCompositeControl(node) &&
             cvox.DomUtil.findFocusableDescendant(node));
  }
  return false;
};


/**
 * Given a node that might be inside of a composite control like a listbox,
 * return the surrounding control.
 * @param {Node} node The node from which to start looking.
 * @return {Node} The surrounding composite control node, or null if none.
 */
cvox.DomUtil.getSurroundingControl = function(node) {
  var surroundingControl = null;
  if (!cvox.DomUtil.isControl(node) && node.hasAttribute &&
      node.hasAttribute('role')) {
    surroundingControl = node.parentElement;
    while (surroundingControl &&
        !cvox.AriaUtil.isCompositeControl(surroundingControl)) {
      surroundingControl = surroundingControl.parentElement;
    }
  }
  return surroundingControl;
};


/**
 * Given a node and a function for determining when to stop
 * descent, return the next leaf-like node.
 *
 * @param {!Node} node The node from which to start looking,
 * this node *must not* be above document.body.
 * @param {boolean} r True if reversed. False by default.
 * @param {function(!Node):boolean} isLeaf A function that
 *   returns true if we should stop descending.
 * @return {Node} The next leaf-like node or null if there is no next
 *   leaf-like node.  This function will always return a node below
 *   document.body and never document.body itself.
 */
cvox.DomUtil.directedNextLeafLikeNode = function(node, r, isLeaf) {
  if (node != document.body) {
    // if not at the top of the tree, we want to find the next possible
    // branch forward in the dom, so we climb up the parents until we find a
    // node that has a nextSibling
    while (!cvox.DomUtil.directedNextSibling(node, r)) {
      if (!node) {
        return null;
      }
      // since node is never above document.body, it always has a parent.
      // so node.parentNode will never be null.
      node = /** @type {!Node} */(node.parentNode);
      if (node == document.body) {
        // we've readed the end of the document.
        return null;
      }
    }
    if (cvox.DomUtil.directedNextSibling(node, r)) {
      // we just checked that next sibling is non-null.
      node = /** @type {!Node} */(cvox.DomUtil.directedNextSibling(node, r));
    }
  }
  // once we're at our next sibling, we want to descend down into it as
  // far as the child class will allow
  while (cvox.DomUtil.directedFirstChild(node, r) && !isLeaf(node)) {
    node = /** @type {!Node} */(cvox.DomUtil.directedFirstChild(node, r));
  }

  // after we've done all that, if we are still at document.body, this must
  // be an empty document.
  if (node == document.body) {
    return null;
  }
  return node;
};


/**
 * Given a node, returns the next leaf node.
 *
 * @param {!Node} node The node from which to start looking
 * for the next leaf node.
 * @param {boolean=} reverse True if reversed. False by default.
 * @return {Node} The next leaf node.
 * Null if there is no next leaf node.
 */
cvox.DomUtil.directedNextLeafNode = function(node, reverse) {
  reverse = !!reverse;
  return cvox.DomUtil.directedNextLeafLikeNode(
      node, reverse, cvox.DomUtil.isLeafNode);
};


/**
 * Given a node, returns the previous leaf node.
 *
 * @param {!Node} node The node from which to start looking
 * for the previous leaf node.
 * @return {Node} The previous leaf node.
 * Null if there is no previous leaf node.
 */
cvox.DomUtil.previousLeafNode = function(node) {
  return cvox.DomUtil.directedNextLeafNode(node, true);
};


/**
 * Computes the outer most leaf node of a given node, depending on value
 * of the reverse flag r.
 * @param {!Node} node in the DOM.
 * @param {boolean} r True if reversed. False by default.
 * @param {function(!Node):boolean} pred Predicate to decide
 * what we consider a leaf.
 * @return {Node} The outer most leaf node of that node.
 */
cvox.DomUtil.directedFindFirstNode = function(node, r, pred) {
  var child = cvox.DomUtil.directedFirstChild(node, r);
  while (child) {
    if (pred(child)) {
      return child;
    } else {
      var leaf = cvox.DomUtil.directedFindFirstNode(child, r, pred);
      if (leaf) {
        return leaf;
      }
    }
    child = cvox.DomUtil.directedNextSibling(child, r);
  }
  return null;
};


/**
 * Moves to the deepest node satisfying a given predicate under the given node.
 * @param {!Node} node in the DOM.
 * @param {boolean} r True if reversed. False by default.
 * @param {function(!Node):boolean} pred Predicate deciding what a leaf is.
 * @return {Node} The deepest node satisfying pred.
 */
cvox.DomUtil.directedFindDeepestNode = function(node, r, pred) {
  var next = cvox.DomUtil.directedFindFirstNode(node, r, pred);
  if (!next) {
    if (pred(node)) {
      return node;
    } else {
      return null;
    }
  } else {
    return cvox.DomUtil.directedFindDeepestNode(next, r, pred);
  }
};


/**
 * Computes the next node wrt. a predicate that is a descendant of ancestor.
 * @param {!Node} node in the DOM.
 * @param {!Node} ancestor of the given node.
 * @param {boolean} r True if reversed. False by default.
 * @param {function(!Node):boolean} pred Predicate to decide
 * what we consider a leaf.
 * @param {boolean=} above True if the next node can live in the subtree
 * directly above the start node. False by default.
 * @param {boolean=} deep True if we are looking for the next node that is
 * deepest in the tree. Otherwise the next shallow node is returned.
 * False by default.
 * @return {Node} The next node in the DOM that satisfies the predicate.
 */
cvox.DomUtil.directedFindNextNode = function(
    node, ancestor, r, pred, above, deep) {
  above = !!above;
  deep = !!deep;
  if (!cvox.DomUtil.isDescendantOfNode(node, ancestor) || node == ancestor) {
    return null;
  }
  var next = cvox.DomUtil.directedNextSibling(node, r);
  while (next) {
    if (!deep && pred(next)) {
      return next;
    }
    var leaf = (deep ?
                cvox.DomUtil.directedFindDeepestNode :
                cvox.DomUtil.directedFindFirstNode)(next, r, pred);
    if (leaf) {
      return leaf;
    }
    if (deep && pred(next)) {
      return next;
    }
    next = cvox.DomUtil.directedNextSibling(next, r);
  }
  var parent = /** @type {!Node} */(node.parentNode);
  if (above && pred(parent)) {
    return parent;
  }
  return cvox.DomUtil.directedFindNextNode(
      parent, ancestor, r, pred, above, deep);
};


/**
 * Get a string representing a control's value and state, i.e. the part
 *     that changes while interacting with the control
 * @param {Element} control A control.
 * @return {string} The value and state string.
 */
cvox.DomUtil.getControlValueAndStateString = function(control) {
  var parentControl = cvox.DomUtil.getSurroundingControl(control);
  if (parentControl) {
    return cvox.DomUtil.collapseWhitespace(
        cvox.DomUtil.getValue(control) + ' ' +
        cvox.DomUtil.getName(control) + ' ' +
        cvox.DomUtil.getState(control, true));
  } else {
    return cvox.DomUtil.collapseWhitespace(
        cvox.DomUtil.getValue(control) + ' ' +
        cvox.DomUtil.getState(control, true));
  }
};


/**
 * Determine whether the given node is an internal link.
 * @param {Node} node The node to be examined.
 * @return {boolean} True if the node is an internal link, false otherwise.
 */
cvox.DomUtil.isInternalLink = function(node) {
  if (node.nodeType == 1) { // Element nodes only.
    var href = node.getAttribute('href');
    if (href && href.indexOf('#') != -1) {
      var path = href.split('#')[0];
      return path == '' || path == window.location.pathname;
    }
  }
  return false;
};


/**
 * Get a string containing the currently selected link's URL.
 * @param {Node} node The link from which URL needs to be extracted.
 * @return {string} The value of the URL.
 */
cvox.DomUtil.getLinkURL = function(node) {
  if (node.tagName == 'A') {
    if (node.getAttribute('href')) {
      if (cvox.DomUtil.isInternalLink(node)) {
        return Msgs.getMsg('internal_link');
      } else {
        return node.getAttribute('href');
      }
    } else {
      return '';
    }
  } else if (cvox.AriaUtil.getRoleName(node) ==
             Msgs.getMsg('role_link')) {
    return Msgs.getMsg('unknown_link');
  }

  return '';
};


/**
 * Checks if a given node is inside a table and returns the table node if it is
 * @param {Node} node The node.
 * @param {{allowCaptions: (undefined|boolean)}=} kwargs Optional named args.
 *  allowCaptions: If true, will return true even if inside a caption. False
 *    by default.
 * @return {Node} If the node is inside a table, the table node. Null if it
 * is not.
 */
cvox.DomUtil.getContainingTable = function(node, kwargs) {
  var ancestors = cvox.DomUtil.getAncestors(node);
  return cvox.DomUtil.findTableNodeInList(ancestors, kwargs);
};


/**
 * Extracts a table node from a list of nodes.
 * @param {Array<Node>} nodes The list of nodes.
 * @param {{allowCaptions: (undefined|boolean)}=} kwargs Optional named args.
 *  allowCaptions: If true, will return true even if inside a caption. False
 *    by default.
 * @return {Node} The table node if the list of nodes contains a table node.
 * Null if it does not.
 */
cvox.DomUtil.findTableNodeInList = function(nodes, kwargs) {
  kwargs = kwargs || {allowCaptions: false};
  // Don't include the caption node because it is actually rendered outside
  // of the table.
  for (var i = nodes.length - 1, node; node = nodes[i]; i--) {
    if (node.constructor != Text) {
      if (!kwargs.allowCaptions && node.tagName == 'CAPTION') {
        return null;
      }
      if ((node.tagName == 'TABLE') || cvox.AriaUtil.isGrid(node)) {
        return node;
      }
    }
  }
  return null;
};


/**
 * Determines whether a given table is a data table or a layout table
 * @param {Node} tableNode The table node.
 * @return {boolean} If the table is a layout table, returns true. False
 * otherwise.
 */
cvox.DomUtil.isLayoutTable = function(tableNode) {
  // TODO(stoarca): Why are we returning based on this inaccurate heuristic
  // instead of first trying the better heuristics below?
  if (tableNode.rows && (tableNode.rows.length <= 1 ||
      (tableNode.rows[0].childElementCount == 1))) {
    // This table has either 0 or one rows, or only "one" column.
    // This is a quick check for column count and may not be accurate. See
    // TraverseTable.getW3CColCount_ for a more accurate
    // (but more complicated) way to determine column count.
    return true;
  }

  // These heuristics are adapted from the Firefox data and layout table.
  // heuristics: http://asurkov.blogspot.com/2011/10/data-vs-layout-table.html
  if (cvox.AriaUtil.isGrid(tableNode)) {
    // This table has an ARIA role identifying it as a grid.
    // Not a layout table.
    return false;
  }
  if (cvox.AriaUtil.isLandmark(tableNode)) {
    // This table has an ARIA landmark role - not a layout table.
    return false;
  }

  if (tableNode.caption || tableNode.summary) {
    // This table has a caption or a summary - not a layout table.
    return false;
  }

  if ((cvox.XpathUtil.evalXPath('tbody/tr/th', tableNode).length > 0) &&
      (cvox.XpathUtil.evalXPath('tbody/tr/td', tableNode).length > 0)) {
    // This table at least one column and at least one column header.
    // Not a layout table.
    return false;
  }

  if (cvox.XpathUtil.evalXPath('colgroup', tableNode).length > 0) {
    // This table specifies column groups - not a layout table.
    return false;
  }

  if ((cvox.XpathUtil.evalXPath('thead', tableNode).length > 0) ||
      (cvox.XpathUtil.evalXPath('tfoot', tableNode).length > 0)) {
    // This table has header or footer rows - not a layout table.
    return false;
  }

  if ((cvox.XpathUtil.evalXPath('tbody/tr/td/embed', tableNode).length > 0) ||
      (cvox.XpathUtil.evalXPath('tbody/tr/td/object', tableNode).length > 0) ||
      (cvox.XpathUtil.evalXPath('tbody/tr/td/iframe', tableNode).length > 0) ||
      (cvox.XpathUtil.evalXPath('tbody/tr/td/applet', tableNode).length > 0)) {
    // This table contains embed, object, applet, or iframe elements. It is
    // a layout table.
    return true;
  }

  // These heuristics are loosely based on Okada and Miura's "Detection of
  // Layout-Purpose TABLE Tags Based on Machine Learning" (2007).
  // http://books.google.com/books?id=kUbmdqasONwC&lpg=PA116&ots=Lb3HJ7dISZ&lr&pg=PA116

  // Increase the points for each heuristic. If there are 3 or more points,
  // this is probably a layout table.
  var points = 0;

  if (! cvox.DomUtil.hasBorder(tableNode)) {
    // This table has no border.
    points++;
  }

  if (tableNode.rows.length <= 6) {
    // This table has a limited number of rows.
    points++;
  }

  if (cvox.DomUtil.countPreviousTags(tableNode) <= 12) {
    // This table has a limited number of previous tags.
    points++;
  }

 if (cvox.XpathUtil.evalXPath('tbody/tr/td/table', tableNode).length > 0) {
   // This table has nested tables.
   points++;
 }
  return (points >= 3);
};


/**
 * Count previous tags, which we dfine as the number of HTML tags that
 * appear before the given node.
 * @param {Node} node The given node.
 * @return {number} The number of previous tags.
 */
cvox.DomUtil.countPreviousTags = function(node) {
  var ancestors = cvox.DomUtil.getAncestors(node);
  return ancestors.length + cvox.DomUtil.countPreviousSiblings(node);
};


/**
 * Counts previous siblings, not including text nodes.
 * @param {Node} node The given node.
 * @return {number} The number of previous siblings.
 */
cvox.DomUtil.countPreviousSiblings = function(node) {
  var count = 0;
  var prev = node.previousSibling;
  while (prev != null) {
    if (prev.constructor != Text) {
      count++;
    }
    prev = prev.previousSibling;
  }
  return count;
};


/**
 * Whether a given table has a border or not.
 * @param {Node} tableNode The table node.
 * @return {boolean} If the table has a border, return true. False otherwise.
 */
cvox.DomUtil.hasBorder = function(tableNode) {
  // If .frame contains "void" there is no border.
  if (tableNode.frame) {
    return (tableNode.frame.indexOf('void') == -1);
  }

  // If .border is defined and  == "0" then there is no border.
  if (tableNode.border) {
    if (tableNode.border.length == 1) {
      return (tableNode.border != '0');
    } else {
      return (tableNode.border.slice(0, -2) != 0);
    }
  }

  // If .style.border-style is 'none' there is no border.
  if (tableNode.style.borderStyle && tableNode.style.borderStyle == 'none') {
    return false;
  }

  // If .style.border-width is specified in units of length
  // ( https://developer.mozilla.org/en/CSS/border-width ) then we need
  // to check if .style.border-width starts with 0[px,em,etc]
  if (tableNode.style.borderWidth) {
    return (tableNode.style.borderWidth.slice(0, -2) != 0);
  }

  // If .style.border-color is defined, then there is a border
  if (tableNode.style.borderColor) {
    return true;
  }
  return false;
};


/**
 * Return the first leaf node, starting at the top of the document.
 * @return {Node?} The first leaf node in the document, if found.
 */
cvox.DomUtil.getFirstLeafNode = function() {
  var node = document.body;
  while (node && node.firstChild) {
    node = node.firstChild;
  }
  while (node && !cvox.DomUtil.hasContent(node)) {
    node = cvox.DomUtil.directedNextLeafNode(node);
  }
  return node;
};


/**
 * Finds the first descendant node that matches the filter function, using
 * a depth first search. This function offers the most general purpose way
 * of finding a matching element. You may also wish to consider
 * {@code goog.dom.query} which can express many matching criteria using
 * CSS selector expressions. These expressions often result in a more
 * compact representation of the desired result.
 * This is the findNode function from goog.dom:
 * http://code.google.com/p/closure-library/source/browse/trunk/closure/goog/dom/dom.js
 *
 * @param {Node} root The root of the tree to search.
 * @param {function(Node) : boolean} p The filter function.
 * @return {Node|undefined} The found node or undefined if none is found.
 */
cvox.DomUtil.findNode = function(root, p) {
  var rv = [];
  var found = cvox.DomUtil.findNodes_(root, p, rv, true, 10000);
  return found ? rv[0] : undefined;
};


/**
 * Finds the number of nodes matching the filter.
 * @param {Node} root The root of the tree to search.
 * @param {function(Node) : boolean} p The filter function.
 * @return {number} The number of nodes selected by filter.
 */
cvox.DomUtil.countNodes = function(root, p) {
  var rv = [];
  cvox.DomUtil.findNodes_(root, p, rv, false, 10000);
  return rv.length;
};


/**
 * Finds the first or all the descendant nodes that match the filter function,
 * using a depth first search.
 * @param {Node} root The root of the tree to search.
 * @param {function(Node) : boolean} p The filter function.
 * @param {Array<Node>} rv The found nodes are added to this array.
 * @param {boolean} findOne If true we exit after the first found node.
 * @param {number} maxChildCount The max child count. This is used as a kill
 * switch - if there are more nodes than this, terminate the search.
 * @return {boolean} Whether the search is complete or not. True in case
 * findOne is true and the node is found. False otherwise. This is the
 * findNodes_ function from goog.dom:
 * http://code.google.com/p/closure-library/source/browse/trunk/closure/goog/dom/dom.js.
 * @private
 */
cvox.DomUtil.findNodes_ = function(root, p, rv, findOne, maxChildCount) {
  if ((root != null) || (maxChildCount == 0)) {
    var child = root.firstChild;
    while (child) {
      if (p(child)) {
        rv.push(child);
        if (findOne) {
          return true;
        }
      }
      maxChildCount = maxChildCount - 1;
      if (cvox.DomUtil.findNodes_(child, p, rv, findOne, maxChildCount)) {
        return true;
      }
      child = child.nextSibling;
    }
  }
  return false;
};


/**
 * Converts a NodeList into an array
 * @param {NodeList} nodeList The nodeList.
 * @return {Array} The array of nodes in the nodeList.
 */
cvox.DomUtil.toArray = function(nodeList) {
  var nodeArray = [];
  for (var i = 0; i < nodeList.length; i++) {
    nodeArray.push(nodeList[i]);
  }
  return nodeArray;
};


/**
 * Creates a new element with the same attributes and no children.
 * @param {Node|Text} node A node to clone.
 * @param {Object<boolean>} skipattrs Set the attribute to true to skip it
 *     during cloning.
 * @return {Node|Text} The cloned node.
 */
cvox.DomUtil.shallowChildlessClone = function(node, skipattrs) {
  if (node.nodeName == '#text') {
    return document.createTextNode(node.nodeValue);
  }

  if (node.nodeName == '#comment') {
    return document.createComment(node.nodeValue);
  }

  var ret = document.createElement(node.nodeName);
  for (var i = 0; i < node.attributes.length; ++i) {
    var attr = node.attributes[i];
    if (skipattrs && skipattrs[attr.nodeName]) {
      continue;
    }
    ret.setAttribute(attr.nodeName, attr.nodeValue);
  }
  return ret;
};


/**
 * Creates a new element with the same attributes and clones of children.
 * @param {Node|Text} node A node to clone.
 * @param {Object<boolean>} skipattrs Set the attribute to true to skip it
 *     during cloning.
 * @return {Node|Text} The cloned node.
 */
cvox.DomUtil.deepClone = function(node, skipattrs) {
  var ret = cvox.DomUtil.shallowChildlessClone(node, skipattrs);
  for (var i = 0; i < node.childNodes.length; ++i) {
    ret.appendChild(cvox.DomUtil.deepClone(node.childNodes[i], skipattrs));
  }
  return ret;
};


/**
 * Returns either node.firstChild or node.lastChild, depending on direction.
 * @param {Node|Text} node The node.
 * @param {boolean} reverse If reversed.
 * @return {Node|Text} The directed first child or null if the node has
 *   no children.
 */
cvox.DomUtil.directedFirstChild = function(node, reverse) {
  if (reverse) {
    return node.lastChild;
  }
  return node.firstChild;
};

/**
 * Returns either node.nextSibling or node.previousSibling, depending on
 * direction.
 * @param {Node|Text} node The node.
 * @param {boolean=} reverse If reversed.
 * @return {Node|Text} The directed next sibling or null if there are
 *   no more siblings in that direction.
 */
cvox.DomUtil.directedNextSibling = function(node, reverse) {
  if (!node) {
    return null;
  }
  if (reverse) {
    return node.previousSibling;
  }
  return node.nextSibling;
};

/**
 * Creates a function that sends a click. This is because loop closures
 * are dangerous.
 * See: http://joust.kano.net/weblog/archive/2005/08/08/
 * a-huge-gotcha-with-javascript-closures/
 * @param {Node} targetNode The target node to click on.
 * @return {function()} A function that will click on the given targetNode.
 */
cvox.DomUtil.createSimpleClickFunction = function(targetNode) {
  var target = targetNode.cloneNode(true);
  return function() { cvox.DomUtil.clickElem(target, false, false); };
};

/**
 * Adds a node to document.head if that node has not already been added.
 * If document.head does not exist, this will add the node to the body.
 * @param {Node} node The node to add.
 * @param {string=} opt_id The id of the node to ensure the node is only
 *     added once.
 */
cvox.DomUtil.addNodeToHead = function(node, opt_id) {
  if (opt_id && document.getElementById(opt_id)) {
      return;
  }
  var p = document.head || document.body;
  p.appendChild(node);
};


/**
 * Checks if a given node is inside a math expressions and
 * returns the math node if one exists.
 * @param {Node} node The node.
 * @return {Node} The math node, if the node is inside a math expression.
 * Null if it is not.
 */
cvox.DomUtil.getContainingMath = function(node) {
  var ancestors = cvox.DomUtil.getAncestors(node);
  return cvox.DomUtil.findMathNodeInList(ancestors);
};


/**
 * Extracts a math node from a list of nodes.
 * @param {Array<Node>} nodes The list of nodes.
 * @return {Node} The math node if the list of nodes contains a math node.
 * Null if it does not.
 */
cvox.DomUtil.findMathNodeInList = function(nodes) {
  for (var i = 0, node; node = nodes[i]; i++) {
    if (cvox.DomUtil.isMath(node)) {
      return node;
    }
  }
  return null;
};


/**
 * Checks to see wether a node is a math node.
 * @param {Node} node The node to be tested.
 * @return {boolean} Whether or not a node is a math node.
 */
cvox.DomUtil.isMath = function(node) {
  return cvox.DomUtil.isMathml(node) ||
      cvox.DomUtil.isMathJax(node) ||
          cvox.DomUtil.isMathImg(node) ||
              cvox.AriaUtil.isMath(node);
};


/**
 * Specifies node classes in which we expect maths expressions a alt text.
 * @type {{tex: Array<string>,
 *         asciimath: Array<string>}}
 */
// These are the classes for which we assume they contain Maths in the ALT or
// TITLE attribute.
// tex: Wikipedia;
// latex: Wordpress;
// numberedequation, inlineformula, displayformula: MathWorld;
cvox.DomUtil.ALT_MATH_CLASSES = {
  tex: ['tex', 'latex'],
  asciimath: ['numberedequation', 'inlineformula', 'displayformula']
};


/**
 * Composes a query selector string for image nodes with alt math content by
 * type of content.
 * @param {string} contentType The content type, e.g., tex, asciimath.
 * @return {!string} The query elector string.
 */
cvox.DomUtil.altMathQuerySelector = function(contentType) {
  var classes = cvox.DomUtil.ALT_MATH_CLASSES[contentType];
  if (classes) {
    return classes.map(function(x) {return 'img.' + x;}).join(', ');
  }
  return '';
};


/**
 * Check if a given node is potentially a math image with alternative text in
 * LaTeX.
 * @param {Node} node The node to be tested.
 * @return {boolean} Whether or not a node has an image with class TeX or LaTeX.
 */
cvox.DomUtil.isMathImg = function(node) {
  if (!node || !node.tagName || !node.className) {
    return false;
  }
  if (node.tagName != 'IMG') {
    return false;
  }
  for (var i = 0, className; className = node.classList.item(i); i++) {
    className = className.toLowerCase();
    if (cvox.DomUtil.ALT_MATH_CLASSES.tex.indexOf(className) != -1 ||
        cvox.DomUtil.ALT_MATH_CLASSES.asciimath.indexOf(className) != -1) {
      return true;
    }
  }
  return false;
};


/**
 * Checks to see whether a node is a MathML node.
 * !! This is necessary as Chrome currently does not upperCase Math tags !!
 * @param {Node} node The node to be tested.
 * @return {boolean} Whether or not a node is a MathML node.
 */
cvox.DomUtil.isMathml = function(node) {
  if (!node || !node.tagName) {
    return false;
  }
  return node.tagName.toLowerCase() == 'math';
};


/**
 * Checks to see wether a node is a MathJax node.
 * @param {Node} node The node to be tested.
 * @return {boolean} Whether or not a node is a MathJax node.
 */
cvox.DomUtil.isMathJax = function(node) {
  if (!node || !node.tagName || !node.className) {
    return false;
  }

  function isSpanWithClass(n, cl) {
    return (n.tagName == 'SPAN' &&
            n.className.split(' ').some(function(x) {
                                          return x.toLowerCase() == cl;}));
  };
  if (isSpanWithClass(node, 'math')) {
    var ancestors = cvox.DomUtil.getAncestors(node);
    return ancestors.some(function(x) {return isSpanWithClass(x, 'mathjax');});
  }
  return false;
};


/**
 * Computes the id of the math span in a MathJax DOM element.
 * @param {string} jaxId The id of the MathJax node.
 * @return {string} The id of the span node.
 */
cvox.DomUtil.getMathSpanId = function(jaxId) {
  var node = document.getElementById(jaxId + '-Frame');
  if (node) {
    var span = node.querySelector('span.math');
    if (span) {
      return span.id;
    }
  }
  return '';
};


/**
 * Returns true if the node has a longDesc.
 * @param {Node} node The node to be tested.
 * @return {boolean} Whether or not a node has a longDesc.
 */
cvox.DomUtil.hasLongDesc = function(node) {
  if (node && node.longDesc) {
    return true;
  }
  return false;
};


/**
 * Returns tag name of a node if it has one.
 * @param {Node} node A node.
 * @return {string} A the tag name of the node.
 */
cvox.DomUtil.getNodeTagName = function(node) {
  if (node.nodeType == Node.ELEMENT_NODE) {
    return node.tagName;
  }
  return '';
};


/**
 * Cleaning up a list of nodes to remove empty text nodes.
 * @param {NodeList} nodes The nodes list.
 * @return {!Array<Node|string|null>} The cleaned up list of nodes.
 */
cvox.DomUtil.purgeNodes = function(nodes) {
  return cvox.DomUtil.toArray(nodes).
      filter(function(node) {
               return node.nodeType != Node.TEXT_NODE ||
                   !node.textContent.match(/^\s+$/);});
};


/**
 * Calculates a hit point for a given node.
 * @return {{x:(number), y:(number)}} The position.
 */
cvox.DomUtil.elementToPoint = function(node) {
  if (!node) {
    return {x: 0, y: 0};
  }
  if (node.constructor == Text) {
    node = node.parentNode;
  }
  var r = node.getBoundingClientRect();
  return {
    x: r.left + (r.width / 2),
    y: r.top + (r.height / 2)
  };
};


/**
 * Checks if an input node supports HTML5 selection.
 * If the node is not an input element, returns false.
 * @param {Node} node The node to check.
 * @return {boolean} True if HTML5 selection supported.
 */
cvox.DomUtil.doesInputSupportSelection = function(node) {
  return goog.isDef(node) &&
      node.tagName == 'INPUT' &&
      node.type != 'email' &&
      node.type != 'number';
};


/**
 * Gets the hint text for a given element.
 * @param {Node} node The target node.
 * @return {string} The hint text.
 */
cvox.DomUtil.getHint = function(node) {
  var desc = '';
  if (node.hasAttribute) {
    if (node.hasAttribute('aria-describedby')) {
      var describedByIds = node.getAttribute('aria-describedby').split(' ');
      for (var describedById, i = 0; describedById = describedByIds[i]; i++) {
        var describedNode = document.getElementById(describedById);
        if (describedNode) {
          desc += ' ' + cvox.DomUtil.getName(
              describedNode, true, true, true);
        }
      }
    }
  }
  return desc;
};
