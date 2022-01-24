/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A simple plugin that inserts 'Hello World!' on command. This
 * plugin is intended to be an example of a very simple plugin for plugin
 * developers.
 *
 * @see helloworld.html
 */

goog.provide('goog.demos.editor.HelloWorld');

goog.require('goog.dom');
goog.require('goog.dom.TagName');
goog.require('goog.editor.Plugin');



/**
 * Plugin to insert 'Hello World!' into an editable field.
 * @final
 * @unrestricted
 */
goog.demos.editor.HelloWorld = class extends goog.editor.Plugin {
  constructor() {
    super();
  }

  /** @override */
  getTrogClassId() {
    return 'HelloWorld';
  }

  /** @override */
  isSupportedCommand(command) {
    return command == goog.demos.editor.HelloWorld.COMMAND.HELLO_WORLD;
  }

  /**
   * Executes a command. Does not fire any BEFORECHANGE, CHANGE, or
   * SELECTIONCHANGE events (these are handled by the super class implementation
   * of `execCommand`.
   * @param {string} command Command to execute.
   * @override
   * @protected
   */
  execCommandInternal(command) {
    const domHelper = this.getFieldObject().getEditableDomHelper();
    const range = this.getFieldObject().getRange();
    range.removeContents();
    const newNode =
        domHelper.createDom(goog.dom.TagName.SPAN, null, 'Hello World!');
    range.insertNode(newNode, false);
  }
};



/**
 * Commands implemented by this plugin.
 * @enum {string}
 */
goog.demos.editor.HelloWorld.COMMAND = {
  HELLO_WORLD: '+helloWorld'
};
