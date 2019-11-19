// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Responsible for tagging nodes used by ChromeVox.
 */

goog.provide('cvox.NodeBreadcrumb');

goog.require('cvox.ChromeVox');



/**
 * Responsible for tagging nodes and tracking those nodes.
 * @constructor
 */
cvox.NodeBreadcrumb = function() {
  /**
   * Counter to be incremented each time HistoryEvent tries to tag a previously
   * untagged node.
   * @type {number}
   */
  this.cvTagCounter_ = 0;
};
goog.addSingletonGetter(cvox.NodeBreadcrumb);

/**
 * The attribute to mark nodes that have been touched, and in what order.
 * @type {string}
 * @const
 * NOTE: not private because tester is using this
 */
cvox.NodeBreadcrumb.TOUCHED_TAG = 'chromevoxtag';

/**
 * The attribute to mark nodes needed to replicate results with.
 * @type {string}
 * @const
 * @private
 */
cvox.NodeBreadcrumb.NEEDED_TAG_ = 'chromevoxneeded';


/**
 * Tags the current node.
 * @return {number} The tag number.
 */
cvox.NodeBreadcrumb.prototype.tagCurrentNode = function() {
  var cvTag;
  var currentNode = cvox.ChromeVox.navigationManager.getCurrentNode();
  while (currentNode && !currentNode.hasAttribute) {
      currentNode = currentNode.parentNode;
  }
  if (!currentNode) {
    cvTag = -1;
  } else if (currentNode.hasAttribute(cvox.NodeBreadcrumb.TOUCHED_TAG)) {
    cvTag = currentNode.getAttribute(cvox.NodeBreadcrumb.TOUCHED_TAG);
  } else {
    cvTag = this.cvTagCounter_;
    currentNode.setAttribute(cvox.NodeBreadcrumb.TOUCHED_TAG, cvTag);
    this.cvTagCounter_++;
  }
  return cvTag;
};


/**
 * Marks all elements that need to be in the test case, starting at the
 * elements that have been tagged.
 * @param {Node} node Root of the subtree which to mark.
 * @private
 */
cvox.NodeBreadcrumb.prototype.smartStart_ = function(node) {
  for (var i = 0; i < node.children.length; ++i) {
    var child = node.children[i];
    this.smartStart_(child);
    if (child.getAttribute &&
        !goog.isNull(child.getAttribute(cvox.NodeBreadcrumb.TOUCHED_TAG))) {
      this.setNeeded_(child);
    }
  }
};


/**
 * Recursively marks all elements that need to be in the test case.
 * Note: modifies the node passed in.
 * @param {Node} node The node to mark.
 * @private
 */
cvox.NodeBreadcrumb.prototype.setNeeded_ = function(node) {
  if (!node) {
    return;
  }

  if (node.getAttribute &&
      goog.isNull(node.getAttribute(cvox.NodeBreadcrumb.NEEDED_TAG_))) {
    node.setAttribute(cvox.NodeBreadcrumb.NEEDED_TAG_, true);

    // only the parent needs to be added
    // if the siblings are needed, then some ancestor
    // would have had chromevoxtag set, in which case
    // we copy the whole subtree of that ancestor anyways
    if (node.nodeName !== 'body') {
      this.setNeeded_(node.parentElement);
    }
  }
};


/**
 * Clones the part of the dom that is needed to recreate the test case.
 * The nodes must have been marked first by calling smartStart_.
 * @param {Node|Text} node The root of the subtree to clone.
 * @return {Node|Text} The cloned subtree.
 * @private
 */
cvox.NodeBreadcrumb.prototype.smartClone_ = function(node) {
  var skipattrs = {};
  skipattrs[cvox.NodeBreadcrumb.TOUCHED_TAG] = true;
  skipattrs[cvox.NodeBreadcrumb.NEEDED_TAG_] = true;

  if (node.getAttribute && node.getAttribute(cvox.NodeBreadcrumb.TOUCHED_TAG)) {
    return cvox.DomUtil.deepClone(node, skipattrs);
  }

  var ret = cvox.DomUtil.shallowChildlessClone(node, skipattrs);

  for (var i = 0; i < node.childNodes.length; ++i) {
    var child = node.childNodes[i];
    if (child.getAttribute &&
        !goog.isNull(child.getAttribute(cvox.NodeBreadcrumb.NEEDED_TAG_))) {
      ret.appendChild(this.smartClone_(child));
    }
  }
  return ret;
};


/**
 * Returns a sting containing the html needed to replicate the test.
 * @return {Node} The subset of the dom that was walked.
 */
cvox.NodeBreadcrumb.prototype.dumpWalkedDom = function() {
  this.smartStart_(document.body);
  return this.smartClone_(document.body);
};


/**
 * Retrieves the ChromeVox tag for the current node.
 *
 * @return {number} The ChromeVox tag or -1 if there is an error.
 */
cvox.NodeBreadcrumb.getCurrentNodeTag = function() {
  var currentNode = cvox.ChromeVox.navigationManager.getCurrentNode();
  while (currentNode && !currentNode.hasAttribute) {
      currentNode = currentNode.parentNode;
  }
  if (currentNode && currentNode.hasAttribute(cvox.NodeBreadcrumb.TOUCHED_TAG)) {
    return currentNode.getAttribute(cvox.NodeBreadcrumb.TOUCHED_TAG);
  } else {
    return -1;
  }
};
