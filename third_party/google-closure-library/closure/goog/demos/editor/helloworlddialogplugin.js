/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview An example of how to write a dialog plugin.
 */

goog.provide('goog.demos.editor.HelloWorldDialogPlugin');
goog.provide('goog.demos.editor.HelloWorldDialogPlugin.Command');

goog.require('goog.demos.editor.HelloWorldDialog');
goog.require('goog.dom.TagName');
goog.require('goog.editor.plugins.AbstractDialogPlugin');
goog.require('goog.editor.range');
goog.require('goog.functions');
goog.require('goog.ui.editor.AbstractDialog');
goog.requireType('goog.dom.DomHelper');


// *** Public interface ***************************************************** //



/**
 * A plugin that opens the hello world dialog.
 * @final
 * @unrestricted
 */
goog.demos.editor.HelloWorldDialogPlugin =
    class extends goog.editor.plugins.AbstractDialogPlugin {
  constructor() {
    super(goog.demos.editor.HelloWorldDialogPlugin.Command.HELLO_WORLD_DIALOG);
  }

  /**
   * Creates a new instance of the dialog and registers for the relevant events.
   * @param {goog.dom.DomHelper} dialogDomHelper The dom helper to be used to
   *     create the dialog.
   * @return {!goog.demos.editor.HelloWorldDialog} The dialog.
   * @override
   * @protected
   */
  createDialog(dialogDomHelper) {
    const dialog = new goog.demos.editor.HelloWorldDialog(dialogDomHelper);
    dialog.addEventListener(
        goog.ui.editor.AbstractDialog.EventType.OK, this.handleOk_, false,
        this);
    return dialog;
  }

  /**
   * Handles the OK event from the dialog by inserting the hello world message
   * into the field.
   * @param {goog.demos.editor.HelloWorldDialog.OkEvent} e OK event object.
   * @private
   */
  handleOk_(e) {
    // First restore the selection so we can manipulate the field's content
    // according to what was selected.
    this.restoreOriginalSelection();

    // Notify listeners that the field's contents are about to change.
    this.getFieldObject().dispatchBeforeChange();

    // Now we can clear out what was previously selected (if anything).
    const range = this.getFieldObject().getRange();
    range.removeContents();
    // And replace it with a span containing our hello world message.
    let createdNode = this.getFieldDomHelper().createDom(
        goog.dom.TagName.SPAN, null, e.message);
    createdNode = range.insertNode(createdNode, false);
    // Place the cursor at the end of the new text node (false == to the right).
    goog.editor.range.placeCursorNextTo(createdNode, false);

    // Notify listeners that the field's selection has changed.
    this.getFieldObject().dispatchSelectionChangeEvent();
    // Notify listeners that the field's contents have changed.
    this.getFieldObject().dispatchChange();
  }
};



/**
 * Commands implemented by this plugin.
 * @enum {string}
 */
goog.demos.editor.HelloWorldDialogPlugin.Command = {
  HELLO_WORLD_DIALOG: 'helloWorldDialog'
};


/** @override */
goog.demos.editor.HelloWorldDialogPlugin.prototype.getTrogClassId =
    goog.functions.constant('HelloWorldDialog');


// *** Protected interface ************************************************** //



// *** Private implementation *********************************************** //
