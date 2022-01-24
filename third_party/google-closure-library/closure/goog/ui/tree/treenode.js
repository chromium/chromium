/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Definition of the goog.ui.tree.TreeNode class.
 *
 *
 * This is a based on the webfx tree control. See file comment in
 * treecontrol.js.
 */

goog.provide('goog.ui.tree.TreeNode');

goog.forwardDeclare('goog.ui.tree.TreeControl');
goog.require('goog.ui.tree.BaseNode');
goog.requireType('goog.dom.DomHelper');
goog.requireType('goog.html.SafeHtml');  // circular



/**
 * A single node in the tree.
 * @param {string|!goog.html.SafeHtml} content The content of the node label.
 *     Strings are treated as plain-text and will be HTML escaped.
 * @param {Object=} opt_config The configuration for the tree. See
 *    goog.ui.tree.TreeControl.defaultConfig. If not specified, a default config
 *    will be used.
 * @param {goog.dom.DomHelper=} opt_domHelper Optional DOM helper.
 * @constructor
 * @extends {goog.ui.tree.BaseNode}
 */
goog.ui.tree.TreeNode = function(content, opt_config, opt_domHelper) {
  'use strict';
  goog.ui.tree.BaseNode.call(this, content, opt_config, opt_domHelper);
};
goog.inherits(goog.ui.tree.TreeNode, goog.ui.tree.BaseNode);


/**
 * Returns the tree.
 * @return {?goog.ui.tree.TreeControl} The tree.
 * @override
 */
goog.ui.tree.TreeNode.prototype.getTree = function() {
  'use strict';
  if (this.tree) {
    return this.tree;
  }
  const parent = this.getParent();
  if (parent) {
    const tree = parent.getTree();
    if (tree) {
      this.setTreeInternal(tree);
      return tree;
    }
  }
  return null;
};


/**
 * Returns the source for the icon.
 * @return {string} Src for the icon.
 * @override
 */
goog.ui.tree.TreeNode.prototype.getCalculatedIconClass = function() {
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
  if (this.hasChildren()) {
    if (expanded && config.cssExpandedFolderIcon) {
      return config.cssTreeIcon + ' ' + config.cssExpandedFolderIcon;
    } else if (!expanded && config.cssCollapsedFolderIcon) {
      return config.cssTreeIcon + ' ' + config.cssCollapsedFolderIcon;
    }
  } else {
    if (config.cssFileIcon) {
      return config.cssTreeIcon + ' ' + config.cssFileIcon;
    }
  }
  return '';
};
