/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Definition of the goog.ui.tree.TreeControl class, which
 * provides a way to view a hierarchical set of data.
 *
 *
 * This is a based on the webfx tree control. It since been updated to add
 * typeahead support, as well as accessibility support using ARIA framework.
 *
 * @see ../../demos/tree/demo.html
 */

goog.provide('goog.ui.tree.TreeControl');

goog.require('goog.a11y.aria');
goog.require('goog.asserts');
goog.require('goog.dom.classlist');
goog.require('goog.events.EventType');
goog.require('goog.events.FocusHandler');
goog.require('goog.events.KeyHandler');
goog.require('goog.html.SafeHtml');
goog.require('goog.log');
goog.require('goog.ui.tree.BaseNode');
goog.require('goog.ui.tree.TreeNode');
goog.require('goog.ui.tree.TypeAhead');
goog.require('goog.userAgent');
goog.requireType('goog.dom.DomHelper');
goog.requireType('goog.events.BrowserEvent');



/**
 * This creates a TreeControl object. A tree control provides a way to
 * view a hierarchical set of data.
 * @param {string|!goog.html.SafeHtml} content The content of the node label.
 *     Strings are treated as plain-text and will be HTML escaped.
 * @param {Object=} opt_config The configuration for the tree. See
 *    goog.ui.tree.TreeControl.defaultConfig. If not specified, a default config
 *    will be used.
 * @param {goog.dom.DomHelper=} opt_domHelper Optional DOM helper.
 * @constructor
 * @extends {goog.ui.tree.BaseNode}
 */
goog.ui.tree.TreeControl = function(content, opt_config, opt_domHelper) {
  'use strict';
  goog.ui.tree.BaseNode.call(this, content, opt_config, opt_domHelper);

  // The root is open and selected by default.
  this.setExpandedInternal(true);
  this.setSelectedInternal(true);

  this.selectedItem_ = this;

  /**
   * Used for typeahead support.
   * @private {!goog.ui.tree.TypeAhead}
   */
  this.typeAhead_ = new goog.ui.tree.TypeAhead();

  /**
   * The object handling keyboard events.
   * @private {?goog.events.KeyHandler}
   */
  this.keyHandler_ = null;

  /**
   * The object handling focus events.
   * @private {?goog.events.FocusHandler}
   */
  this.focusHandler_ = null;

  /**
   * Logger
   * @private {?goog.log.Logger}
   */
  this.logger_ = goog.log.getLogger('this');

  /**
   * Whether the tree is focused.
   * @private {boolean}
   */
  this.focused_ = false;

  /**
   * Child node that currently has focus.
   * @private {?goog.ui.tree.BaseNode}
   */
  this.focusedNode_ = null;

  /**
   * Whether to show lines.
   * @private {boolean}
   */
  this.showLines_ = true;

  /**
   * Whether to show expanded lines.
   * @private {boolean}
   */
  this.showExpandIcons_ = true;

  /**
   * Whether to show the root node.
   * @private {boolean}
   */
  this.showRootNode_ = true;

  /**
   * Whether to show the root lines.
   * @private {boolean}
   */
  this.showRootLines_ = true;

  if (goog.userAgent.IE) {

    try {
      // works since IE6SP1
      document.execCommand('BackgroundImageCache', false, true);
    } catch (e) {
      goog.log.warning(this.logger_, 'Failed to enable background image cache');
    }
  }
};
goog.inherits(goog.ui.tree.TreeControl, goog.ui.tree.BaseNode);


/** @override */
goog.ui.tree.TreeControl.prototype.getTree = function() {
  'use strict';
  return this;
};


/** @override */
goog.ui.tree.TreeControl.prototype.getDepth = function() {
  'use strict';
  return 0;
};


/**
 * Expands the parent chain of this node so that it is visible.
 * @override
 */
goog.ui.tree.TreeControl.prototype.reveal = function() {
  // always expanded by default
  // needs to be overriden so that we don't try to reveal our parent
  // which is a generic component
};


/**
 * Handles focus on the tree.
 * @param {!goog.events.BrowserEvent} e The browser event.
 * @private
 */
goog.ui.tree.TreeControl.prototype.handleFocus_ = function(e) {
  'use strict';
  this.focused_ = true;
  goog.dom.classlist.add(
      goog.asserts.assert(this.getElement()), goog.getCssName('focused'));

  if (this.selectedItem_) {
    this.selectedItem_.select();
  }
};


/**
 * Handles blur on the tree.
 * @param {!goog.events.BrowserEvent} e The browser event.
 * @private
 */
goog.ui.tree.TreeControl.prototype.handleBlur_ = function(e) {
  'use strict';
  this.focused_ = false;
  goog.dom.classlist.remove(
      goog.asserts.assert(this.getElement()), goog.getCssName('focused'));
};


/**
 * @return {boolean} Whether the tree has keyboard focus.
 */
goog.ui.tree.TreeControl.prototype.hasFocus = function() {
  'use strict';
  return this.focused_;
};


/** @override */
goog.ui.tree.TreeControl.prototype.getExpanded = function() {
  'use strict';
  return !this.showRootNode_ ||
      goog.ui.tree.TreeControl.superClass_.getExpanded.call(this);
};


/** @override */
goog.ui.tree.TreeControl.prototype.setExpanded = function(expanded) {
  'use strict';
  if (!this.showRootNode_) {
    this.setExpandedInternal(expanded);
  } else {
    goog.ui.tree.TreeControl.superClass_.setExpanded.call(this, expanded);
  }
};


/** @override */
goog.ui.tree.TreeControl.prototype.getExpandIconSafeHtml = function() {
  'use strict';
  // no expand icon for root element
  return goog.html.SafeHtml.EMPTY;
};


/** @override */
goog.ui.tree.TreeControl.prototype.getIconElement = function() {
  'use strict';
  const el = this.getRowElement();
  return el ? /** @type {Element} */ (el.firstChild) : null;
};


/** @override */
goog.ui.tree.TreeControl.prototype.getExpandIconElement = function() {
  'use strict';
  // no expand icon for root element
  return null;
};


/** @override */
goog.ui.tree.TreeControl.prototype.updateExpandIcon = function() {
  // no expand icon
};


/**
 * @override
 * @suppress {strictMissingProperties}
 */
goog.ui.tree.TreeControl.prototype.getRowClassName = function() {
  'use strict';
  return goog.ui.tree.TreeControl.superClass_.getRowClassName.call(this) +
      (this.showRootNode_ ? '' : ' ' + this.getConfig().cssHideRoot);
};


/**
 * Returns the source for the icon.
 * @return {string} Src for the icon.
 * @override
 * @suppress {strictMissingProperties}
 */
goog.ui.tree.TreeControl.prototype.getCalculatedIconClass = function() {
  'use strict';
  const expanded = this.getExpanded();
  const expandedIconClass = this.getExpandedIconClass();
  if (expanded && expandedIconClass) {
    return expandedIconClass;
  }
  const iconClass = this.getIconClass();
  if (!expanded && iconClass) {
    return iconClass;
  }

  // fall back on default icons
  const config = this.getConfig();
  if (expanded && config.cssExpandedRootIcon) {
    return config.cssTreeIcon + ' ' + config.cssExpandedRootIcon;
  } else if (!expanded && config.cssCollapsedRootIcon) {
    return config.cssTreeIcon + ' ' + config.cssCollapsedRootIcon;
  }
  return '';
};


/**
 * Sets the selected item.
 * @param {goog.ui.tree.BaseNode} node The item to select.
 */
goog.ui.tree.TreeControl.prototype.setSelectedItem = function(node) {
  'use strict';
  if (this.selectedItem_ == node) {
    return;
  }

  let hadFocus = false;
  if (this.selectedItem_) {
    hadFocus = this.selectedItem_ == this.focusedNode_;
    this.selectedItem_.setSelectedInternal(false);
  }

  this.selectedItem_ = node;

  if (node) {
    node.setSelectedInternal(true);
    if (hadFocus) {
      node.select();
    }
  }

  this.dispatchEvent(goog.events.EventType.CHANGE);
};


/**
 * Returns the selected item.
 * @return {goog.ui.tree.BaseNode} The currently selected item.
 */
goog.ui.tree.TreeControl.prototype.getSelectedItem = function() {
  'use strict';
  return this.selectedItem_;
};


/**
 * Sets whether to show lines.
 * @param {boolean} b Whether to show lines.
 */
goog.ui.tree.TreeControl.prototype.setShowLines = function(b) {
  'use strict';
  if (this.showLines_ != b) {
    this.showLines_ = b;
    if (this.isInDocument()) {
      this.updateLinesAndExpandIcons_();
    }
  }
};


/**
 * @return {boolean} Whether to show lines.
 */
goog.ui.tree.TreeControl.prototype.getShowLines = function() {
  'use strict';
  return this.showLines_;
};


/**
 * Updates the lines after the tree has been drawn.
 * @private
 */
goog.ui.tree.TreeControl.prototype.updateLinesAndExpandIcons_ = function() {
  'use strict';
  const tree = this;
  const showLines = tree.getShowLines();
  const showRootLines = tree.getShowRootLines();

  /**
   * Recursively walk through all nodes and update the class names of the
   * expand icon and the children element.
   * @param {!goog.ui.tree.BaseNode} node
   */
  function updateShowLines(node) {
    const childrenEl = node.getChildrenElement();
    if (childrenEl) {
      const hideLines =
          !showLines || tree == node.getParent() && !showRootLines;
      /** @suppress {strictMissingProperties} */
      const childClass = hideLines ? node.getConfig().cssChildrenNoLines :
                                     node.getConfig().cssChildren;
      childrenEl.className = childClass;

      const expandIconEl = node.getExpandIconElement();
      if (expandIconEl) {
        expandIconEl.className = node.getExpandIconClass();
      }
    }
    node.forEachChild(updateShowLines);
  }
  updateShowLines(this);
};


/**
 * Sets whether to show root lines.
 * @param {boolean} b Whether to show root lines.
 */
goog.ui.tree.TreeControl.prototype.setShowRootLines = function(b) {
  'use strict';
  if (this.showRootLines_ != b) {
    this.showRootLines_ = b;
    if (this.isInDocument()) {
      this.updateLinesAndExpandIcons_();
    }
  }
};


/**
 * @return {boolean} Whether to show root lines.
 */
goog.ui.tree.TreeControl.prototype.getShowRootLines = function() {
  'use strict';
  return this.showRootLines_;
};


/**
 * Sets whether to show expand icons.
 * @param {boolean} b Whether to show expand icons.
 */
goog.ui.tree.TreeControl.prototype.setShowExpandIcons = function(b) {
  'use strict';
  if (this.showExpandIcons_ != b) {
    this.showExpandIcons_ = b;
    if (this.isInDocument()) {
      this.updateLinesAndExpandIcons_();
    }
  }
};


/**
 * @return {boolean} Whether to show expand icons.
 */
goog.ui.tree.TreeControl.prototype.getShowExpandIcons = function() {
  'use strict';
  return this.showExpandIcons_;
};


/**
 * Sets whether to show the root node.
 * @param {boolean} b Whether to show the root node.
 */
goog.ui.tree.TreeControl.prototype.setShowRootNode = function(b) {
  'use strict';
  if (this.showRootNode_ != b) {
    this.showRootNode_ = b;
    if (this.isInDocument()) {
      const el = this.getRowElement();
      if (el) {
        el.className = this.getRowClassName();
      }
    }
    // Ensure that we do not hide the selected item.
    if (!b && this.getSelectedItem() == this && this.getFirstChild()) {
      this.setSelectedItem(this.getFirstChild());
    }
  }
};


/**
 * @return {boolean} Whether to show the root node.
 */
goog.ui.tree.TreeControl.prototype.getShowRootNode = function() {
  'use strict';
  return this.showRootNode_;
};


/**
 * Add roles and states.
 * @protected
 * @override
 */
goog.ui.tree.TreeControl.prototype.initAccessibility = function() {
  'use strict';
  goog.ui.tree.TreeControl.superClass_.initAccessibility.call(this);

  const elt = this.getElement();
  goog.asserts.assert(elt, 'The DOM element for the tree cannot be null.');
  goog.a11y.aria.setRole(elt, 'tree');
  goog.a11y.aria.setState(elt, 'labelledby', this.getLabelElement().id);
};


/** @override */
goog.ui.tree.TreeControl.prototype.enterDocument = function() {
  'use strict';
  goog.ui.tree.TreeControl.superClass_.enterDocument.call(this);
  const el = this.getElement();
  /** @suppress {strictMissingProperties} */
  el.className = this.getConfig().cssRoot;
  el.setAttribute('hideFocus', 'true');
  this.attachEvents_();
  this.initAccessibility();
};


/** @override */
goog.ui.tree.TreeControl.prototype.exitDocument = function() {
  'use strict';
  goog.ui.tree.TreeControl.superClass_.exitDocument.call(this);
  this.detachEvents_();
};


/**
 * Adds the event listeners to the tree.
 * @private
 * @suppress {strictMissingProperties}
 */
goog.ui.tree.TreeControl.prototype.attachEvents_ = function() {
  'use strict';
  const el = this.getElement();
  el.tabIndex = 0;

  const kh = this.keyHandler_ = new goog.events.KeyHandler(el);
  const fh = this.focusHandler_ = new goog.events.FocusHandler(el);

  this.getHandler()
      .listen(fh, goog.events.FocusHandler.EventType.FOCUSOUT, this.handleBlur_)
      .listen(fh, goog.events.FocusHandler.EventType.FOCUSIN, this.handleFocus_)
      .listen(kh, goog.events.KeyHandler.EventType.KEY, this.handleKeyEvent)
      .listen(el, goog.events.EventType.MOUSEDOWN, this.handleMouseEvent_)
      .listen(el, goog.events.EventType.CLICK, this.handleMouseEvent_)
      .listen(el, goog.events.EventType.DBLCLICK, this.handleMouseEvent_);
};


/**
 * Removes the event listeners from the tree.
 * @private
 */
goog.ui.tree.TreeControl.prototype.detachEvents_ = function() {
  'use strict';
  this.keyHandler_.dispose();
  this.keyHandler_ = null;
  this.focusHandler_.dispose();
  this.focusHandler_ = null;
};


/**
 * Handles mouse events.
 * @param {!goog.events.BrowserEvent} e The browser event.
 * @private
 */
goog.ui.tree.TreeControl.prototype.handleMouseEvent_ = function(e) {
  'use strict';
  goog.log.fine(this.logger_, 'Received event ' + e.type);
  const node = this.getNodeFromEvent_(e);
  if (node) {
    switch (e.type) {
      case goog.events.EventType.MOUSEDOWN:
        node.onMouseDown(e);
        break;
      case goog.events.EventType.CLICK:
        node.onClick_(e);
        break;
      case goog.events.EventType.DBLCLICK:
        node.onDoubleClick_(e);
        break;
    }
  }
};


/**
 * Handles key down on the tree.
 * @param {!goog.events.BrowserEvent} e The browser event.
 * @return {boolean} The handled value.
 */
goog.ui.tree.TreeControl.prototype.handleKeyEvent = function(e) {
  'use strict';
  let handled = false;

  // Handle typeahead and navigation keystrokes.
  handled = this.typeAhead_.handleNavigation(e) ||
      (this.selectedItem_ && this.selectedItem_.onKeyDown(e)) ||
      this.typeAhead_.handleTypeAheadChar(e);

  if (handled) {
    e.preventDefault();
  }

  return handled;
};


/**
 * Finds the containing node given an event.
 * @param {!goog.events.BrowserEvent} e The browser event.
 * @return {goog.ui.tree.BaseNode} The containing node or null if no node is
 *     found.
 * @private
 * @suppress {strictMissingProperties}
 */
goog.ui.tree.TreeControl.prototype.getNodeFromEvent_ = function(e) {
  'use strict';
  // find the right node
  let node = null;
  let target = e.target;
  while (target != null) {
    const id = target.id;
    node = goog.ui.tree.BaseNode.allNodes[id];
    if (node) {
      return node;
    }
    if (target == this.getElement()) {
      break;
    }
    target = target.parentNode;
  }
  return null;
};


/**
 * Creates a new tree node using the same config as the root.
 * @param {string=} opt_content The content of the node label. Strings are
 *     treated as plain-text and will be HTML escaped. To set SafeHtml content,
 *     omit opt_content and call setSafeHtml on the resulting node.
 * @return {!goog.ui.tree.TreeNode} The new item.
 */
goog.ui.tree.TreeControl.prototype.createNode = function(opt_content) {
  'use strict';
  return new goog.ui.tree.TreeNode(
      opt_content || goog.html.SafeHtml.EMPTY, this.getConfig(),
      this.getDomHelper());
};


/**
 * Allows the caller to notify that the given node has been added or just had
 * been updated in the tree.
 * @param {goog.ui.tree.BaseNode} node New node being added or existing node
 *    that just had been updated.
 */
goog.ui.tree.TreeControl.prototype.setNode = function(node) {
  'use strict';
  this.typeAhead_.setNodeInMap(node);
};


/**
 * Allows the caller to notify that the given node is being removed from the
 * tree.
 * @param {goog.ui.tree.BaseNode} node Node being removed.
 */
goog.ui.tree.TreeControl.prototype.removeNode = function(node) {
  'use strict';
  this.typeAhead_.removeNodeFromMap(node);
};


/**
 * Clear the typeahead buffer.
 */
goog.ui.tree.TreeControl.prototype.clearTypeAhead = function() {
  'use strict';
  this.typeAhead_.clear();
};


/**
 * A default configuration for the tree.
 */
goog.ui.tree.TreeControl.defaultConfig = goog.ui.tree.BaseNode.defaultConfig;
