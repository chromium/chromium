/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Editor plugin to handle tab keys in lists to indent and
 * outdent.
 */

goog.provide('goog.editor.plugins.ListTabHandler');

goog.require('goog.dom');
goog.require('goog.dom.TagName');
goog.require('goog.editor.Command');
goog.require('goog.editor.plugins.AbstractTabHandler');
goog.require('goog.iter');



/**
 * Plugin to handle tab keys in lists to indent and outdent.
 * @constructor
 * @extends {goog.editor.plugins.AbstractTabHandler}
 * @final
 */
goog.editor.plugins.ListTabHandler = function() {
  'use strict';
  goog.editor.plugins.AbstractTabHandler.call(this);
};
goog.inherits(
    goog.editor.plugins.ListTabHandler, goog.editor.plugins.AbstractTabHandler);


/** @override */
goog.editor.plugins.ListTabHandler.prototype.getTrogClassId = function() {
  'use strict';
  return 'ListTabHandler';
};


/**
 * @override
 * @suppress {strictMissingProperties} Added to tighten compiler checks
 */
goog.editor.plugins.ListTabHandler.prototype.handleTabKey = function(e) {
  'use strict';
  var range = this.getFieldObject().getRange();
  if (goog.dom.getAncestorByTagNameAndClass(
          range.getContainerElement(), goog.dom.TagName.LI) ||
      goog.iter.some(range, function(node) {
        'use strict';
        return node.tagName == goog.dom.TagName.LI;
      })) {
    this.getFieldObject().execCommand(
        e.shiftKey ? goog.editor.Command.OUTDENT : goog.editor.Command.INDENT);
    e.preventDefault();
    return true;
  }

  return false;
};
