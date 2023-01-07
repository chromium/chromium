/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Class that allows for simple text editing tests.
 */

goog.setTestOnly('goog.testing.editor.TestHelper');
goog.provide('goog.testing.editor.TestHelper');

goog.require('goog.Disposable');
goog.require('goog.dom');
goog.require('goog.dom.Range');
goog.require('goog.editor.BrowserFeature');
goog.require('goog.editor.node');
goog.require('goog.editor.plugins.AbstractBubblePlugin');
goog.require('goog.testing.dom');
goog.requireType('goog.dom.AbstractRange');



/**
 * Create a new test controller.
 * @param {Element} root The root editable element.
 * @constructor
 * @extends {goog.Disposable}
 * @final
 */
goog.testing.editor.TestHelper = function(root) {
  'use strict';
  if (!root) {
    throw new Error('Null root');
  }
  goog.Disposable.call(this);

  /**
   * Convenience variable for root DOM element.
   * @type {!Element}
   * @private
   */
  this.root_ = root;

  /**
   * The starting HTML of the editable element.
   * @type {string}
   * @private
   */
  this.savedHtml_ = '';
};
goog.inherits(goog.testing.editor.TestHelper, goog.Disposable);


/**
 * Selects a new root element.
 * @param {Element} root The root editable element.
 */
goog.testing.editor.TestHelper.prototype.setRoot = function(root) {
  'use strict';
  if (!root) {
    throw new Error('Null root');
  }
  this.root_ = root;
};


/**
 * Make the root element editable.  Also saves its HTML to be restored
 * in tearDown.
 */
goog.testing.editor.TestHelper.prototype.setUpEditableElement = function() {
  'use strict';
  this.savedHtml_ = this.root_.innerHTML;
  if (goog.editor.BrowserFeature.HAS_CONTENT_EDITABLE) {
    this.root_.contentEditable = true;
  } else {
    this.root_.ownerDocument.designMode = 'on';
  }
  this.root_.setAttribute('g_editable', 'true');
};


/**
 * Reset the element previously initialized, restoring its HTML and making it
 * non editable.
 * @suppress {accessControls} Private state of
 *     {@link goog.editor.plugins.AbstractBubblePlugin} is accessed for test
 *     purposes.
 */
goog.testing.editor.TestHelper.prototype.tearDownEditableElement = function() {
  'use strict';
  if (goog.editor.BrowserFeature.HAS_CONTENT_EDITABLE) {
    this.root_.contentEditable = false;
  } else {
    this.root_.ownerDocument.designMode = 'off';
  }
  goog.dom.removeChildren(this.root_);
  this.root_.innerHTML = this.savedHtml_;
  this.root_.removeAttribute('g_editable');

  if (goog.editor.plugins && goog.editor.plugins.AbstractBubblePlugin) {
    // Remove old bubbles.
    for (let key in goog.editor.plugins.AbstractBubblePlugin.bubbleMap_) {
      goog.editor.plugins.AbstractBubblePlugin.bubbleMap_[key].dispose();
    }
    // Ensure we get a new bubble for each test.
    goog.editor.plugins.AbstractBubblePlugin.bubbleMap_ = {};
  }
};


/**
 * Assert that the html in 'root' is substantially similar to htmlPattern.
 * This method tests for the same set of styles, and for the same order of
 * nodes.  Breaking whitespace nodes are ignored.  Elements can be annotated
 * with classnames corresponding to keys in goog.userAgent and will be
 * expected to show up in that user agent and expected not to show up in
 * others.
 * @param {string} htmlPattern The pattern to match.
 */
goog.testing.editor.TestHelper.prototype.assertHtmlMatches = function(
    htmlPattern) {
  'use strict';
  goog.testing.dom.assertHtmlContentsMatch(htmlPattern, this.root_);
};


/**
 * Finds the first text node descendant of root with the given content.
 * @param {string|RegExp} textOrRegexp The text to find, or a regular
 *     expression to find a match of.
 * @return {Node} The first text node that matches, or null if none is found.
 */
goog.testing.editor.TestHelper.prototype.findTextNode = function(textOrRegexp) {
  'use strict';
  return goog.testing.dom.findTextNode(textOrRegexp, this.root_);
};


/**
 * Select from the given `fromOffset` in the given `from` node to
 * the given `toOffset` in the optionally given `to` node. If nodes
 * are passed in, uses them, otherwise uses findTextNode to find the nodes to
 * select. Selects a caret if opt_to and opt_toOffset are not given.
 * @param {Node|string} from Node or text of the node to start the selection at.
 * @param {number} fromOffset Offset within the above node to start the
 *     selection at.
 * @param {Node|string=} opt_to Node or text of the node to end the selection
 *     at.
 * @param {number=} opt_toOffset Offset within the above node to end the
 *     selection at.
 * @return {!goog.dom.AbstractRange}
 */
goog.testing.editor.TestHelper.prototype.select = function(
    from, fromOffset, opt_to, opt_toOffset) {
  'use strict';
  let end;
  const start = end =
      (typeof from === 'string') ? this.findTextNode(from) : from;
  let endOffset;
  const startOffset = endOffset = fromOffset;

  if (opt_to && typeof opt_toOffset === 'number') {
    end = (typeof opt_to === 'string') ? this.findTextNode(opt_to) : opt_to;
    endOffset = opt_toOffset;
  }

  const range =
      goog.dom.Range.createFromNodes(start, startOffset, end, endOffset);
  range.select();
  return range;
};


/** @override */
goog.testing.editor.TestHelper.prototype.disposeInternal = function() {
  'use strict';
  if (goog.editor.node.isEditableContainer(this.root_)) {
    this.tearDownEditableElement();
  }
  delete this.root_;
  goog.testing.editor.TestHelper.base(this, 'disposeInternal');
};
