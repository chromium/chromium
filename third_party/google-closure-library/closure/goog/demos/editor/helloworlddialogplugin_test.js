/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.demos.editor.HelloWorldDialogPluginTest');
goog.setTestOnly('goog.demos.editor.HelloWorldDialogPluginTest');

const ArgumentMatcher = goog.require('goog.testing.mockmatchers.ArgumentMatcher');
const Command = goog.require('goog.demos.editor.HelloWorldDialogPlugin.Command');
const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const Field = goog.require('goog.editor.Field');
const FieldMock = goog.require('goog.testing.editor.FieldMock');
const HelloWorldDialog = goog.require('goog.demos.editor.HelloWorldDialog');
const HelloWorldDialogPlugin = goog.require('goog.demos.editor.HelloWorldDialogPlugin');
const MockControl = goog.require('goog.testing.MockControl');
const MockRange = goog.require('goog.testing.MockRange');
const NodeType = goog.require('goog.dom.NodeType');
const OkEvent = goog.require('goog.demos.editor.HelloWorldDialog.OkEvent');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const SafeHtml = goog.require('goog.html.SafeHtml');
const TagName = goog.require('goog.dom.TagName');
const TestHelper = goog.require('goog.testing.editor.TestHelper');
const googDom = goog.require('goog.dom');
const googEditorRange = goog.require('goog.editor.range');
const googTestingEditorDom = goog.require('goog.testing.editor.dom');
const googTestingEvents = goog.require('goog.testing.events');
const googUserAgent = goog.require('goog.userAgent');
const testSuite = goog.require('goog.testing.testSuite');

let plugin;
let mockCtrl;
let mockField;
let mockRange;
let mockPlaceCursorNextTo;
const stubs = new PropertyReplacer();

let fieldObj;

const CUSTOM_MESSAGE = 'Hello, cruel world...';

let expectedFailures;

testSuite({
  setUpPage() {
    expectedFailures = new ExpectedFailures();
  },

  setUp() {
    mockCtrl = new MockControl();

    mockRange = new MockRange();
    mockCtrl.addMock(mockRange);

    /** @suppress {checkTypes} suppression added to enable type checking */
    mockField = new FieldMock(undefined, undefined, mockRange);
    mockCtrl.addMock(mockField);

    mockPlaceCursorNextTo = mockCtrl.createFunctionMock('placeCursorNextTo');
  },

  tearDown() {
    plugin.dispose();
    tearDownRealEditableField();
    expectedFailures.handleTearDown();
    stubs.reset();
    googDom.removeChildren(googDom.getElement('myField'));
  },

  /**
   * Tests that the plugin's dialog is properly created.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testCreateDialog() {
    mockField.$replay();

    plugin = new HelloWorldDialogPlugin();
    plugin.registerFieldObject(mockField);

    /** @suppress {visibility} suppression added to enable type checking */
    const dialog = plugin.createDialog(googDom.getDomHelper());
    assertTrue(
        'Dialog should be of type goog.demos.editor.HelloWorldDialog',
        dialog instanceof HelloWorldDialog);

    mockField.$verify();
  },

  /**
   * Tests that when the OK event fires the editable field is properly updated.
   * @suppress {missingProperties,checkTypes} suppression added to enable type
   * checking
   */
  testOk() {
    mockField.focus();
    mockField.dispatchBeforeChange();
    mockRange.removeContents();
    // Tests that an argument is a span with the custom message.
    const createdNodeMatcher = new ArgumentMatcher(function(arg) {
      return arg.nodeType == NodeType.ELEMENT && arg.tagName == TagName.SPAN &&
          googDom.getRawTextContent(arg) == CUSTOM_MESSAGE;
    });
    mockRange.insertNode(createdNodeMatcher, false);
    mockRange.$does(function(node, before) {
      return node;
    });
    mockPlaceCursorNextTo(createdNodeMatcher, false);
    stubs.set(googEditorRange, 'placeCursorNextTo', mockPlaceCursorNextTo);
    mockField.dispatchSelectionChangeEvent();
    mockField.dispatchChange();
    mockCtrl.$replayAll();

    plugin = new HelloWorldDialogPlugin();
    plugin.registerFieldObject(mockField);
    /** @suppress {visibility} suppression added to enable type checking */
    const dialog = plugin.createDialog(googDom.getDomHelper());

    // Mock of execCommand + clicking OK without actually opening the dialog.
    dialog.dispatchEvent(new OkEvent(CUSTOM_MESSAGE));

    mockCtrl.$verifyAll();
  },

  /**
   * Tests that the selection is cleared when the dialog opens and is
   * correctly restored after ok is clicked.
   * @suppress {visibility} suppression added to enable type checking
   */
  testRestoreSelectionOnOk() {
    setUpRealEditableField();

    fieldObj.setSafeHtml(false, SafeHtml.htmlEscape('12345'));

    const elem = fieldObj.getElement();
    const helper = new TestHelper(elem);
    helper.select('12345', 1, '12345', 4);  // Selects '234'.

    assertEquals(
        'Incorrect text selected before dialog is opened', '234',
        fieldObj.getRange().getText());
    plugin.execCommand(Command.HELLO_WORLD_DIALOG);

    // TODO(user): IE returns some bogus range when field doesn't have
    // selection. Remove the expectedFailure when robbyw fixes the issue.
    // NOTE(user): You can't remove the selection from a field in Opera without
    // blurring it.
    elem.parentNode.blur();
    expectedFailures.expectFailureFor(googUserAgent.IE);
    try {
      assertNull(
          'There should be no selection while dialog is open',
          fieldObj.getRange());
    } catch (e) {
      expectedFailures.handleException(e);
    }

    googTestingEvents.fireClickSequence(plugin.dialog_.getOkButtonElement());
    assertEquals(
        'No text should be selected after clicking ok', '',
        fieldObj.getRange().getText());

    // Test that the caret is placed after the custom message.
    googTestingEditorDom.assertRangeBetweenText(
        'Hello, world!', '5', fieldObj.getRange());
  },
});

/**
 * Setup a real editable field (instead of a mock) and register the plugin to
 * it.
 */
function setUpRealEditableField() {
  fieldObj = new Field('myField', document);
  fieldObj.makeEditable();
  // Register the plugin to that field.
  plugin = new HelloWorldDialogPlugin();
  fieldObj.registerPlugin(plugin);
}

/**
 * Tear down the real editable field.
 */
function tearDownRealEditableField() {
  if (fieldObj) {
    fieldObj.makeUneditable();
    fieldObj.dispose();
  }
}
