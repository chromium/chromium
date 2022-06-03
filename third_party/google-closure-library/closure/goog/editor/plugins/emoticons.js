/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Plugin for generating emoticons.
 */

goog.provide('goog.editor.plugins.Emoticons');

goog.require('goog.dom.TagName');
goog.require('goog.editor.Plugin');
goog.require('goog.editor.range');
goog.require('goog.functions');
goog.require('goog.ui.emoji.Emoji');
goog.require('goog.userAgent');



/**
 * Plugin for generating emoticons.
 *
 * @constructor
 * @extends {goog.editor.Plugin}
 * @final
 */
goog.editor.plugins.Emoticons = function() {
  'use strict';
  goog.editor.plugins.Emoticons.base(this, 'constructor');
};
goog.inherits(goog.editor.plugins.Emoticons, goog.editor.Plugin);


/** The emoticon command. */
goog.editor.plugins.Emoticons.COMMAND = '+emoticon';


/** @override */
goog.editor.plugins.Emoticons.prototype.getTrogClassId =
    goog.functions.constant(goog.editor.plugins.Emoticons.COMMAND);


/** @override */
goog.editor.plugins.Emoticons.prototype.isSupportedCommand = function(command) {
  'use strict';
  return command == goog.editor.plugins.Emoticons.COMMAND;
};


/**
 * Inserts an emoticon into the editor at the cursor location. Places the
 * cursor to the right of the inserted emoticon.
 * @param {string} command Command to execute.
 * @param {*=} opt_arg Emoji to insert.
 * @return {!Object|undefined} The result of the command.
 * @override
 */
goog.editor.plugins.Emoticons.prototype.execCommandInternal = function(
    command, opt_arg) {
  'use strict';
  var emoji = /** @type {goog.ui.emoji.Emoji} */ (opt_arg);

  var styleProperties = 'margin:0 0.2ex;vertical-align:middle;';
  var emojiHeight = emoji.getHeight();
  styleProperties += emojiHeight ? 'height:' + emojiHeight + 'px;' : '';
  var emojiWidth = emoji.getWidth();
  styleProperties += emojiWidth ? 'width:' + emojiWidth + 'px;' : '';

  var dom = this.getFieldDomHelper();
  var imgAttributes = {'src': emoji.getUrl(), 'style': styleProperties};
  if (emoji.getAltText()) {
    imgAttributes['alt'] = emoji.getAltText();
  }
  var img = dom.createDom(goog.dom.TagName.IMG, imgAttributes);

  img.setAttribute(goog.ui.emoji.Emoji.ATTRIBUTE, emoji.getId());
  img.setAttribute(goog.ui.emoji.Emoji.DATA_ATTRIBUTE, emoji.getId());

  this.getFieldObject().getRange().replaceContentsWithNode(img);

  // IE8 does the right thing with the cursor, and has a js error when we try
  // to place the cursor manually.
  // IE9 loses the cursor when the window is focused, so focus first.
  if (!goog.userAgent.IE || goog.userAgent.isDocumentModeOrHigher(9)) {
    this.getFieldObject().focus();
    goog.editor.range.placeCursorNextTo(img, false);
  }
};
