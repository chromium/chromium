/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.a11y.ariaTest');
goog.setTestOnly();

const Role = goog.require('goog.a11y.aria.Role');
const State = goog.require('goog.a11y.aria.State');
const TagName = goog.require('goog.dom.TagName');
const aria = goog.require('goog.a11y.aria');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

let sandbox;
let someDiv;
let someSpan;
let htmlButton;

testSuite({
  setUp() {
    sandbox = dom.getElement('sandbox');
    someDiv = dom.createDom(TagName.DIV, {id: 'someDiv'}, 'DIV');
    someSpan = dom.createDom(TagName.SPAN, {id: 'someSpan'}, 'SPAN');
    htmlButton = dom.createDom(TagName.BUTTON, {id: 'someButton'}, 'BUTTON');
    dom.appendChild(sandbox, someDiv);
    dom.appendChild(someDiv, someSpan);
  },

  tearDown() {
    dom.removeChildren(sandbox);
    someDiv = null;
    someSpan = null;
    htmlButton = null;
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetSetRole() {
    assertNull('someDiv\'s role should be null', aria.getRole(someDiv));
    assertNull('someSpan\'s role should be null', aria.getRole(someSpan));

    aria.setRole(someDiv, Role.MENU);
    aria.setRole(someSpan, Role.MENU_ITEM);

    assertEquals(
        'someDiv\'s role should be MENU', Role.MENU, aria.getRole(someDiv));
    assertEquals(
        'someSpan\'s role should be MENU_ITEM', Role.MENU_ITEM,
        aria.getRole(someSpan));

    const div = dom.createElement(TagName.DIV);
    dom.appendChild(sandbox, div);
    dom.appendChild(
        div,
        dom.createDom(TagName.SPAN, {id: 'anotherSpan', role: Role.CHECKBOX}));
    assertEquals(
        'anotherSpan\'s role should be CHECKBOX', Role.CHECKBOX,
        aria.getRole(dom.getElement('anotherSpan')));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetSetToggleState() {
    assertThrows(
        'Should throw because no state is specified.',
        /**
         * @suppress {checkTypes} suppression added to enable type checking
         */
        () => {
          aria.getState(someDiv);
        });
    assertThrows(
        'Should throw because no state is specified.',
        /**
         * @suppress {checkTypes} suppression added to enable type checking
         */
        () => {
          aria.getState(someDiv);
        });
    aria.setState(someDiv, State.LABELLEDBY, 'someSpan');

    assertEquals(
        'someDiv\'s labelledby state should be "someSpan"', 'someSpan',
        aria.getState(someDiv, State.LABELLEDBY));

    // Test setting for aria-activedescendant with empty value.
    assertFalse(
        someDiv.hasAttribute ? someDiv.hasAttribute('aria-activedescendant') :
                               !!someDiv.getAttribute('aria-activedescendant'));
    aria.setState(someDiv, State.ACTIVEDESCENDANT, 'someSpan');
    assertEquals('someSpan', aria.getState(someDiv, State.ACTIVEDESCENDANT));
    aria.setState(someDiv, State.ACTIVEDESCENDANT, '');
    assertFalse(
        someDiv.hasAttribute ? someDiv.hasAttribute('aria-activedescendant') :
                               !!someDiv.getAttribute('aria-activedescendant'));

    // Test setting state that has a default value to empty value.
    assertFalse(
        someDiv.hasAttribute ? someDiv.hasAttribute('aria-relevant') :
                               !!someDiv.getAttribute('aria-relevant'));
    aria.setState(someDiv, State.RELEVANT, aria.RelevantValues.TEXT);
    assertEquals(
        aria.RelevantValues.TEXT, aria.getState(someDiv, State.RELEVANT));
    aria.setState(someDiv, State.RELEVANT, '');
    assertEquals(
        aria.RelevantValues.ADDITIONS + ' ' + aria.RelevantValues.TEXT,
        aria.getState(someDiv, State.RELEVANT));

    // Test toggling an attribute that has a true/false value.
    aria.setState(someDiv, State.EXPANDED, false);
    assertEquals('false', aria.getState(someDiv, State.EXPANDED));
    aria.toggleState(someDiv, State.EXPANDED);
    assertEquals('true', aria.getState(someDiv, State.EXPANDED));
    aria.setState(someDiv, State.EXPANDED, true);
    assertEquals('true', aria.getState(someDiv, State.EXPANDED));
    aria.toggleState(someDiv, State.EXPANDED);
    assertEquals('false', aria.getState(someDiv, State.EXPANDED));

    // Test toggling an attribute that does not have a true/false value.
    aria.setState(someDiv, State.RELEVANT, aria.RelevantValues.TEXT);
    assertEquals(
        aria.RelevantValues.TEXT, aria.getState(someDiv, State.RELEVANT));
    aria.toggleState(someDiv, State.RELEVANT);
    assertEquals('', aria.getState(someDiv, State.RELEVANT));
    aria.removeState(someDiv, State.RELEVANT);
    assertEquals('', aria.getState(someDiv, State.RELEVANT));
    // This is not a valid value, but this is what happens if toggle is misused.
    aria.toggleState(someDiv, State.RELEVANT);
    assertEquals('true', aria.getState(someDiv, State.RELEVANT));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetStateString() {
    aria.setState(someDiv, State.LABEL, 'test_label');
    aria.setState(
        someSpan, State.LABEL, aria.getStateString(someDiv, State.LABEL));
    assertEquals(
        aria.getState(someDiv, State.LABEL),
        aria.getState(someSpan, State.LABEL));
    assertEquals(
        'The someDiv\'s enum value should be "test_label".', 'test_label',
        aria.getState(someDiv, State.LABEL));
    assertEquals(
        'The someSpan\'s enum value should be "copy move".', 'test_label',
        aria.getStateString(someSpan, State.LABEL));
    someDiv.setAttribute('aria-label', '');
    assertEquals(null, aria.getStateString(someDiv, State.LABEL));
    aria.setState(someDiv, State.MULTILINE, true);
    let thrown = false;
    try {
      aria.getStateString(someDiv, State.MULTILINE);
    } catch (e) {
      thrown = true;
    }
    assertTrue('invalid use of getStateString on boolean.', thrown);
    aria.setState(someDiv, State.LIVE, aria.LivePriority.ASSERTIVE);
    thrown = false;
    aria.setState(someDiv, State.LEVEL, 1);
    try {
      aria.getStateString(someDiv, State.LEVEL);
    } catch (e) {
      thrown = true;
    }
    assertTrue('invalid use of getStateString on numbers.', thrown);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetStateStringArray() {
    aria.setState(someDiv, State.LABELLEDBY, ['1', '2']);
    aria.setState(
        someSpan, State.LABELLEDBY,
        aria.getStringArrayStateInternalUtil(someDiv, State.LABELLEDBY));
    assertEquals(
        aria.getState(someDiv, State.LABELLEDBY),
        aria.getState(someSpan, State.LABELLEDBY));

    assertEquals(
        'The someDiv\'s enum value should be "1 2".', '1 2',
        aria.getState(someDiv, State.LABELLEDBY));
    assertEquals(
        'The someSpan\'s enum value should be "1 2".', '1 2',
        aria.getState(someSpan, State.LABELLEDBY));

    assertSameElements(
        'The someDiv\'s enum value should be "1 2".', ['1', '2'],
        aria.getStringArrayStateInternalUtil(someDiv, State.LABELLEDBY));
    assertSameElements(
        'The someSpan\'s enum value should be "1 2".', ['1', '2'],
        aria.getStringArrayStateInternalUtil(someSpan, State.LABELLEDBY));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetStateNumber() {
    aria.setState(someDiv, State.LEVEL, 1);
    aria.setState(
        someSpan, State.LEVEL, aria.getStateNumber(someDiv, State.LEVEL));
    assertEquals(
        aria.getState(someDiv, State.LEVEL),
        aria.getState(someSpan, State.LEVEL));
    assertEquals(
        'The someDiv\'s enum value should be "1".', '1',
        aria.getState(someDiv, State.LEVEL));
    assertEquals(
        'The someSpan\'s enum value should be "1".', '1',
        aria.getState(someSpan, State.LEVEL));
    assertEquals(
        'The someDiv\'s enum value should be "1".', 1,
        aria.getStateNumber(someDiv, State.LEVEL));
    assertEquals(
        'The someSpan\'s enum value should be "1".', 1,
        aria.getStateNumber(someSpan, State.LEVEL));
    aria.setState(someDiv, State.MULTILINE, true);
    let thrown = false;
    try {
      aria.getStateNumber(someDiv, State.MULTILINE);
    } catch (e) {
      thrown = true;
    }
    assertTrue('invalid use of getStateNumber on boolean.', thrown);
    aria.setState(someDiv, State.LIVE, aria.LivePriority.ASSERTIVE);
    thrown = false;
    try {
      aria.getStateBoolean(someDiv, State.LIVE);
    } catch (e) {
      thrown = true;
    }
    assertTrue('invalid use of getStateNumber on strings.', thrown);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetStateBoolean() {
    assertNull(aria.getStateBoolean(someDiv, State.MULTILINE));

    aria.setState(someDiv, State.MULTILINE, false);
    assertFalse(aria.getStateBoolean(someDiv, State.MULTILINE));

    aria.setState(someDiv, State.MULTILINE, true);
    aria.setState(
        someSpan, State.MULTILINE,
        aria.getStateBoolean(someDiv, State.MULTILINE));
    assertEquals(
        aria.getState(someDiv, State.MULTILINE),
        aria.getState(someSpan, State.MULTILINE));
    assertEquals(
        'The someDiv\'s enum value should be "true".', 'true',
        aria.getState(someDiv, State.MULTILINE));
    assertEquals(
        'The someSpan\'s enum value should be "true".', 'true',
        aria.getState(someSpan, State.MULTILINE));
    assertEquals(
        'The someDiv\'s enum value should be "true".', true,
        aria.getStateBoolean(someDiv, State.MULTILINE));
    assertEquals(
        'The someSpan\'s enum value should be "true".', true,
        aria.getStateBoolean(someSpan, State.MULTILINE));
    aria.setState(someDiv, State.LEVEL, 1);
    let thrown = false;
    try {
      aria.getStateBoolean(someDiv, State.LEVEL);
    } catch (e) {
      thrown = true;
    }
    assertTrue('invalid use of getStateBoolean on numbers.', thrown);
    aria.setState(someDiv, State.LIVE, aria.LivePriority.ASSERTIVE);
    thrown = false;
    try {
      aria.getStateBoolean(someDiv, State.LIVE);
    } catch (e) {
      thrown = true;
    }
    assertTrue('invalid use of getStateBoolean on strings.', thrown);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetSetActiveDescendant() {
    aria.setActiveDescendant(someDiv, null);
    assertNull(
        'someDiv\'s activedescendant should be null',
        aria.getActiveDescendant(someDiv));

    aria.setActiveDescendant(someDiv, someSpan);

    assertEquals(
        'someDiv\'s active descendant should be "someSpan"', someSpan,
        aria.getActiveDescendant(someDiv));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetSetLabel() {
    assertEquals('someDiv\'s label should be ""', '', aria.getLabel(someDiv));

    aria.setLabel(someDiv, 'somelabel');
    assertEquals(
        'someDiv\'s label should be "somelabel"', 'somelabel',
        aria.getLabel(someDiv));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testHasState() {
    aria.setState(someDiv, State.EXPANDED, false);
    assertTrue(aria.hasState(someDiv, State.EXPANDED));
    aria.removeState(someDiv, State.EXPANDED);
    assertFalse(aria.hasState(someDiv, State.EXPANDED));
  },
});
