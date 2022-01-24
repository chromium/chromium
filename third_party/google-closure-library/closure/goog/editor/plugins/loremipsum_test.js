/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.plugins.LoremIpsumTest');
goog.setTestOnly();

const Command = goog.require('goog.editor.Command');
const Field = goog.require('goog.editor.Field');
const LoremIpsum = goog.require('goog.editor.plugins.LoremIpsum');
const SafeHtml = goog.require('goog.html.SafeHtml');
const Unicode = goog.require('goog.string.Unicode');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

let FIELD;
let PLUGIN;
let HTML;
const UPPERCASE_CONTENTS = '<P>THE OWLS ARE NOT WHAT THEY SEEM.</P>';

function getNbsp() {
  return '&nbsp;';
}
testSuite({
  setUp() {
    HTML = dom.getElement('root').innerHTML;

    FIELD = new Field('field');

    PLUGIN = new LoremIpsum('The owls are not what they seem.');
    FIELD.registerPlugin(PLUGIN);
  },

  tearDown() {
    FIELD.dispose();
    dom.getElement('root').innerHTML = HTML;
  },

  testQueryUsingLorem() {
    FIELD.makeEditable();

    assertTrue(FIELD.queryCommandValue(Command.USING_LOREM));
    FIELD.setSafeHtml(true, SafeHtml.htmlEscape('fresh content'), false, true);
    assertFalse(FIELD.queryCommandValue(Command.USING_LOREM));
  },

  testUpdateLoremIpsum() {
    dom.setTextContent(dom.getElement('field'), 'stuff');

    const loremPlugin = FIELD.getPluginByClassId('LoremIpsum');
    FIELD.makeEditable();
    const content = SafeHtml.create('div', {}, 'foo');

    FIELD.setSafeHtml(
        false, SafeHtml.EMPTY, false, /* Don't update lorem */ false);
    assertFalse(
        'Field started with content, lorem must not be enabled.',
        FIELD.queryCommandValue(Command.USING_LOREM));
    FIELD.execCommand(Command.UPDATE_LOREM);
    assertTrue(
        'Field was set to empty, update must turn on lorem ipsum',
        FIELD.queryCommandValue(Command.USING_LOREM));

    FIELD.unregisterPlugin(loremPlugin);
    FIELD.setSafeHtml(
        false, content, false,
        /* Update (turn off) lorem */ true);
    FIELD.setSafeHtml(
        false, SafeHtml.EMPTY, false, /* Don't update lorem */ false);
    FIELD.execCommand(Command.UPDATE_LOREM);
    assertFalse(
        'Field with no lorem message must not use lorem ipsum',
        FIELD.queryCommandValue(Command.USING_LOREM));
    FIELD.registerPlugin(loremPlugin);

    FIELD.setSafeHtml(false, content, false, true);
    FIELD.setSafeHtml(false, SafeHtml.EMPTY, false, false);
    Field.setActiveFieldId(FIELD.id);
    FIELD.execCommand(Command.UPDATE_LOREM);
    assertFalse(
        'Active field must not use lorem ipsum',
        FIELD.queryCommandValue(Command.USING_LOREM));
    Field.setActiveFieldId(null);

    FIELD.setSafeHtml(false, content, false, true);
    FIELD.setSafeHtml(false, SafeHtml.EMPTY, false, false);
    FIELD.setModalMode(true);
    FIELD.execCommand(Command.UPDATE_LOREM);
    assertFalse(
        'Must not turn on lorem ipsum while a dialog is open.',
        FIELD.queryCommandValue(Command.USING_LOREM));
    FIELD.setModalMode(true);

    FIELD.dispose();
  },

  testLoremIpsumAndGetCleanContents() {
    dom.setTextContent(dom.getElement('field'), 'This is a field');
    FIELD.makeEditable();

    // test direct getCleanContents
    assertEquals(
        'field reported wrong contents', 'This is a field',
        FIELD.getCleanContents());

    // test indirect getCleanContents
    const contents = FIELD.getCleanContents();
    assertEquals('field reported wrong contents', 'This is a field', contents);

    // set field html, but explicitly forbid converting to lorem ipsum text
    FIELD.setSafeHtml(
        false, SafeHtml.htmlEscape(Unicode.NBSP), true, false /* no lorem */);
    assertEquals(
        'field contains unexpected contents', getNbsp(),
        FIELD.getElement().innerHTML);
    assertEquals(
        'field reported wrong contents', getNbsp(), FIELD.getCleanContents());

    // now set field html allowing lorem
    FIELD.setSafeHtml(
        false, SafeHtml.htmlEscape(Unicode.NBSP), true, true /* lorem */);
    assertEquals(
        'field reported wrong contents', Unicode.NBSP,
        FIELD.getCleanContents());
    assertEquals(
        'field contains unexpected contents', UPPERCASE_CONTENTS,
        FIELD.getElement().innerHTML.toUpperCase());
  },

  testLoremIpsumAndGetCleanContents2() {
    // make a field blank before we make it editable, and then check
    // that making it editable activates lorem.
    assert('field is editable', FIELD.isUneditable());
    dom.setTextContent(dom.getElement('field'), '   ');

    FIELD.makeEditable();
    assertEquals(
        'field contains unexpected contents', UPPERCASE_CONTENTS,
        FIELD.getElement().innerHTML.toUpperCase());

    FIELD.makeUneditable();
    assertEquals(
        'field contains unexpected contents', UPPERCASE_CONTENTS,
        dom.getElement('field').innerHTML.toUpperCase());
  },

  testLoremIpsumInClickToEditMode() {
    // in click-to-edit mode, trogedit manages the editable state of the editor,
    // so we must manage lorem ipsum in uneditable mode too.
    FIELD.makeEditable();

    assertEquals(
        'field contains unexpected contents', UPPERCASE_CONTENTS,
        FIELD.getElement().innerHTML.toUpperCase());

    FIELD.makeUneditable();
    assertEquals(
        'field contains unexpected contents', UPPERCASE_CONTENTS,
        dom.getElement('field').innerHTML.toUpperCase());
  },
});
