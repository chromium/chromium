/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Adds a keyboard shortcut for the link command.
 */

goog.provide('goog.editor.plugins.LinkShortcutPlugin');

goog.require('goog.editor.Command');
goog.require('goog.editor.Link');
goog.require('goog.editor.Plugin');



/**
 * Plugin to add a keyboard shortcut for the link command
 * @constructor
 * @extends {goog.editor.Plugin}
 * @final
 */
goog.editor.plugins.LinkShortcutPlugin = function() {
  'use strict';
  goog.editor.plugins.LinkShortcutPlugin.base(this, 'constructor');
};
goog.inherits(goog.editor.plugins.LinkShortcutPlugin, goog.editor.Plugin);


/** @override */
goog.editor.plugins.LinkShortcutPlugin.prototype.getTrogClassId = function() {
  'use strict';
  return 'LinkShortcutPlugin';
};


/**
 * @override
 */
goog.editor.plugins.LinkShortcutPlugin.prototype.handleKeyboardShortcut =
    function(e, key, isModifierPressed) {
  'use strict';
  if (isModifierPressed && key == 'k' && !e.shiftKey) {
    var link = /** @type {goog.editor.Link?} */ (
        this.getFieldObject().execCommand(goog.editor.Command.LINK));
    if (link) {
      link.finishLinkCreation(this.getFieldObject());
    }
    return true;
  }

  return false;
};
