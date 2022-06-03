/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Handles applying header styles to text.
 */

goog.provide('goog.editor.plugins.HeaderFormatter');

goog.require('goog.editor.Command');
goog.require('goog.editor.Plugin');
goog.require('goog.userAgent');



/**
 * Applies header styles to text.
 * @constructor
 * @extends {goog.editor.Plugin}
 * @final
 */
goog.editor.plugins.HeaderFormatter = function() {
  'use strict';
  goog.editor.Plugin.call(this);
};
goog.inherits(goog.editor.plugins.HeaderFormatter, goog.editor.Plugin);


/** @override */
goog.editor.plugins.HeaderFormatter.prototype.getTrogClassId = function() {
  'use strict';
  return 'HeaderFormatter';
};

// TODO(user):  Move execCommand functionality from basictextformatter into
// here for headers.  I'm not doing this now because it depends on the
// switch statements in basictextformatter and we'll need to abstract that out
// in order to separate out any of the functions from basictextformatter.


/**
 * Commands that can be passed as the optional argument to execCommand.
 * @enum {string}
 */
goog.editor.plugins.HeaderFormatter.HEADER_COMMAND = {
  H1: 'H1',
  H2: 'H2',
  H3: 'H3',
  H4: 'H4'
};


/**
 * @override
 */
goog.editor.plugins.HeaderFormatter.prototype.handleKeyboardShortcut = function(
    e, key, isModifierPressed) {
  'use strict';
  if (!isModifierPressed) {
    return false;
  }
  var command = null;
  switch (key) {
    case '1':
      command = goog.editor.plugins.HeaderFormatter.HEADER_COMMAND.H1;
      break;
    case '2':
      command = goog.editor.plugins.HeaderFormatter.HEADER_COMMAND.H2;
      break;
    case '3':
      command = goog.editor.plugins.HeaderFormatter.HEADER_COMMAND.H3;
      break;
    case '4':
      command = goog.editor.plugins.HeaderFormatter.HEADER_COMMAND.H4;
      break;
  }
  if (command) {
    this.getFieldObject().execCommand(
        goog.editor.Command.FORMAT_BLOCK, command);
    // Prevent default isn't enough to cancel tab navigation in FF.
    if (goog.userAgent.GECKO) {
      e.stopPropagation();
    }
    return true;
  }
  return false;
};
