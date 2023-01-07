/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.plugins.HeaderFormatterTest');
goog.setTestOnly();

const BasicTextFormatter = goog.require('goog.editor.plugins.BasicTextFormatter');
const BrowserEvent = goog.require('goog.events.BrowserEvent');
const Command = goog.require('goog.editor.Command');
const FieldMock = goog.require('goog.testing.editor.FieldMock');
const HeaderFormatter = goog.require('goog.editor.plugins.HeaderFormatter');
const LooseMock = goog.require('goog.testing.LooseMock');
const TestHelper = goog.require('goog.testing.editor.TestHelper');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let field;
let editableField;
let headerFormatter;
let btf;
let testHelper;

testSuite({
  setUpPage() {
    field = dom.getElement('field');
    testHelper = new TestHelper(field);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  setUp() {
    testHelper.setUpEditableElement();
    editableField = new FieldMock();
    headerFormatter = new HeaderFormatter();
    headerFormatter.registerFieldObject(editableField);
    btf = new BasicTextFormatter();
    btf.registerFieldObject(editableField);
  },

  tearDown() {
    editableField = null;
    headerFormatter.dispose();
    testHelper.tearDownEditableElement();
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testHeaderShortcuts() {
    dom.setTextContent(field, 'myText');

    const textNode = field.firstChild;
    testHelper.select(textNode, 0, textNode, textNode.length);

    editableField.getElement();
    editableField.$anyTimes();
    editableField.$returns(field);

    editableField.getPluginByClassId('Bidi');
    editableField.$anyTimes();
    editableField.$returns(null);

    editableField.execCommand(
        Command.FORMAT_BLOCK, HeaderFormatter.HEADER_COMMAND.H1);
    // Bypass EditableField's execCommand and directly call
    // basicTextFormatter's.  Future version of headerformatter will include
    // that code in its own execCommand.
    editableField.$does(/**
                           @suppress {visibility} suppression added to enable
                           type checking
                         */
                        () => {
                          btf.execCommandInternal(
                              BasicTextFormatter.COMMAND.FORMAT_BLOCK,
                              HeaderFormatter.HEADER_COMMAND.H1);
                        });

    const event = new LooseMock(BrowserEvent);
    if (userAgent.GECKO) {
      event.stopPropagation();
    }

    editableField.$replay();
    event.$replay();

    assertTrue(
        'Event handled',
        headerFormatter.handleKeyboardShortcut(event, '1', true));
    assertEquals('Field contains a header', 'H1', field.firstChild.nodeName);

    editableField.$verify();
    event.$verify();
  },
});
