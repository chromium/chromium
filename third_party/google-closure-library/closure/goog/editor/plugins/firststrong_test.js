/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.plugins.FirstStrongTest');
goog.setTestOnly();

const Command = goog.require('goog.editor.Command');
const Field = goog.require('goog.editor.Field');
const FirstStrong = goog.require('goog.editor.plugins.FirstStrong');
const KeyCodes = goog.require('goog.events.KeyCodes');
const MockClock = goog.require('goog.testing.MockClock');
const Range = goog.require('goog.dom.Range');
const TestHelper = goog.require('goog.testing.editor.TestHelper');
const dispose = goog.require('goog.dispose');
const events = goog.require('goog.testing.events');
const range = goog.require('goog.editor.range');
const testSuite = goog.require('goog.testing.testSuite');
const testing = goog.require('goog.html.testing');
const userAgent = goog.require('goog.userAgent');

// The key code for the Hebrew א, a strongly RTL letter.
const ALEPH_KEYCODE = 1488;
let field;
let fieldElement;
let dom;
let helper;
let triggeredCommand = null;
let clock;

function assertRTL() {
  assertEquals(Command.DIR_RTL, triggeredCommand);
}

function assertLTR() {
  assertEquals(Command.DIR_LTR, triggeredCommand);
}

function assertNoCommand() {
  assertNull(triggeredCommand);
}

testSuite({
  setUp() {
    field = new Field('field');
    field.registerPlugin(new FirstStrong());
    field.makeEditable();

    fieldElement = field.getElement();

    helper = new TestHelper(fieldElement);

    dom = field.getEditableDomHelper();

    // Mock out execCommand to see if a direction change has been triggered.
    field.execCommand = (command) => {
      if (command == Command.DIR_LTR || command == Command.DIR_RTL)
        triggeredCommand = command;
    };
  },

  tearDown() {
    dispose(field);
    dispose(helper);
    triggeredCommand = null;
    dispose(clock);  // Make sure clock is disposed.
  },

  testFirstCharacter_RTL() {
    field.setSafeHtml(
        false, testing.newSafeHtmlForTest('<div id="text">&nbsp;</div>'));
    field.focusAndPlaceCursorAtStart();
    events.fireNonAsciiKeySequence(fieldElement, KeyCodes.T, ALEPH_KEYCODE);
    assertRTL();
  },

  testFirstCharacter_LTR() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest('<div dir="rtl" id="text">&nbsp;</div>'));
    field.focusAndPlaceCursorAtStart();
    events.fireKeySequence(fieldElement, KeyCodes.A);
    assertLTR();
  },

  testFirstStrongCharacter_RTL() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<div id="text">123.7 3121, <b><++{}></b> - $45</div>'));
    field.focusAndPlaceCursorAtStart();
    events.fireNonAsciiKeySequence(fieldElement, KeyCodes.T, ALEPH_KEYCODE);
    assertRTL();
  },

  testFirstStrongCharacter_LTR() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<div dir="rtl" id="text">123.7 3121, <b><++{}></b> - $45</div>'));
    field.focusAndPlaceCursorAtStart();
    events.fireKeySequence(fieldElement, KeyCodes.A);
    assertLTR();
  },

  testNotStrongCharacter_RTL() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest('<div id="text">123.7 3121, - $45</div>'));
    field.focusAndPlaceCursorAtStart();
    events.fireKeySequence(fieldElement, KeyCodes.NINE);
    assertNoCommand();
  },

  testNotStrongCharacter_LTR() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<div dir="rtl" id="text">123.7 3121 $45</div>'));
    field.focusAndPlaceCursorAtStart();
    events.fireKeySequence(fieldElement, KeyCodes.NINE);
    assertNoCommand();
  },

  testNotFirstStrongCharacter_RTL() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<div id="text">123.7 3121, <b>English</b> - $45</div>'));
    field.focusAndPlaceCursorAtStart();
    events.fireNonAsciiKeySequence(fieldElement, KeyCodes.T, ALEPH_KEYCODE);
    assertNoCommand();
  },

  testNotFirstStrongCharacter_LTR() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<div dir="rtl" id="text">123.7 3121, <b>עברית</b> - $45</div>'));
    field.focusAndPlaceCursorAtStart();
    events.fireKeySequence(fieldElement, KeyCodes.A);
    assertNoCommand();
  },

  testFirstStrongCharacterWithInnerDiv_RTL() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<div id="text">123.7 3121, <b id="b"><++{}></b>' +
            '<div id="inner">English</div>' +
            '</div>'));
    field.focusAndPlaceCursorAtStart();
    events.fireNonAsciiKeySequence(fieldElement, KeyCodes.T, ALEPH_KEYCODE);
    assertRTL();
  },

  testFirstStrongCharacterWithInnerDiv_LTR() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<div dir="rtl" id="text">123.7 3121, <b id="b"><++{}></b>' +
            '<div id="inner">English</div>' +
            '</div>'));
    field.focusAndPlaceCursorAtStart();
    events.fireKeySequence(fieldElement, KeyCodes.A);
    assertLTR();
  },

  /** Regression test for {@link http://b/7549696} */
  testFirstStrongCharacterInNewLine_RTL() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest('<div><b id="cur">English<br>1</b></div>'));
    Range.createCaret(dom.$('cur'), 2).select();

    events.fireNonAsciiKeySequence(fieldElement, KeyCodes.T, ALEPH_KEYCODE);
    // Only GECKO treats <br> as a new paragraph.
    if (userAgent.GECKO) {
      assertRTL();
    } else {
      assertNoCommand();
    }
  },

  testFirstStrongCharacterInParagraph_RTL() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<div id="text1">1&gt; English</div>' +
            '<div id="text2">2&gt;</div>' +
            '<div id="text3">3&gt;</div>'));
    Range.createCaret(dom.$('text2'), 0).select();

    events.fireNonAsciiKeySequence(fieldElement, KeyCodes.T, ALEPH_KEYCODE);
    assertRTL();
  },

  testFirstStrongCharacterInParagraph_LTR() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<div dir="rtl" id="text1">1&gt; עברית</div>' +
            '<div dir="rtl" id="text2">2&gt;</div>' +
            '<div dir="rtl" id="text3">3&gt;</div>'));
    Range.createCaret(dom.$('text2'), 0).select();

    events.fireKeySequence(fieldElement, KeyCodes.A);
    assertLTR();
  },

  testFirstStrongCharacterInList_RTL() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<div id="text1">1&gt; English</div>' +
            '<ul id="list">' +
            '<li>10&gt;</li>' +
            '<li id="li2"></li>' +
            '<li>30</li>' +
            '</ul>' +
            '<div id="text3">3&gt;</div>'));
    range.placeCursorNextTo(dom.$('li2'), true);

    events.fireNonAsciiKeySequence(fieldElement, KeyCodes.T, ALEPH_KEYCODE);
    assertRTL();
  },

  testFirstStrongCharacterInList_LTR() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<div dir="rtl" id="text1">1&gt; English</div>' +
            '<ul dir="rtl" id="list">' +
            '<li>10&gt;</li>' +
            '<li id="li2"></li>' +
            '<li>30</li>' +
            '</ul>' +
            '<div dir="rtl" id="text3">3&gt;</div>'));
    range.placeCursorNextTo(dom.$('li2'), true);

    events.fireKeySequence(fieldElement, KeyCodes.A);
    assertLTR();
  },

  testNotFirstStrongCharacterInList_RTL() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<div id="text1">1</div>' +
            '<ul id="list">' +
            '<li>10&gt;</li>' +
            '<li id="li2"></li>' +
            '<li>30<b>3<i>Hidden English</i>32</b></li>' +
            '</ul>' +
            '<div id="text3">3&gt;</div>'));
    range.placeCursorNextTo(dom.$('li2'), true);

    events.fireNonAsciiKeySequence(fieldElement, KeyCodes.T, ALEPH_KEYCODE);
    assertNoCommand();
  },

  testNotFirstStrongCharacterInList_LTR() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<div dir="rtl" id="text1">1&gt; English</div>' +
            '<ul dir="rtl" id="list">' +
            '<li>10&gt;</li>' +
            '<li id="li2"></li>' +
            '<li>30<b>3<i>עברית סמויה</i>32</b></li>' +
            '</ul>' +
            '<div dir="rtl" id="text3">3&gt;</div>'));
    range.placeCursorNextTo(dom.$('li2'), true);

    events.fireKeySequence(fieldElement, KeyCodes.A);
    assertNoCommand();
  },

  testFirstStrongCharacterWithBR_RTL() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<div id="container">' +
            '<div id="text1">ABC</div>' +
            '<div id="text2">' +
            '1<br>' +
            '2<b id="inner">3</b><i>4<u>5<br>' +
            '6</u>7</i>8</b>9<br>' +
            '10' +
            '</div>' +
            '<div id="text3">11</div>' +
            '</div>'));

    range.placeCursorNextTo(dom.$('inner'), true);
    events.fireNonAsciiKeySequence(fieldElement, KeyCodes.T, ALEPH_KEYCODE);
    assertRTL();
  },

  testFirstStrongCharacterWithBR_LTR() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<div dir="rtl" id="container">' +
            '<div dir="rtl" id="text1">אבג</div>' +
            '<div dir="rtl" id="text2">' +
            '1<br>' +
            '2<b id="inner">3</b><i>4<u>5<br>' +
            '6</u>7</i>8</b>9<br>' +
            '10' +
            '</div>' +
            '<div dir="rtl" id="text3">11</div>' +
            '</div>'));

    range.placeCursorNextTo(dom.$('inner'), true);
    events.fireKeySequence(fieldElement, KeyCodes.A);
    assertLTR();
  },

  testNotFirstStrongCharacterInBR_RTL() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<div id="container">' +
            '<div id="text1">ABC</div>' +
            '<div id="text2">' +
            '1<br>' +
            '2<b id="inner">3</b><i><em>4G</em><u>5<br>' +
            '6</u>7</i>8</b>9<br>' +
            '10' +
            '</div>' +
            '<div id="text3">11</div>' +
            '</div>'));

    range.placeCursorNextTo(dom.$('inner'), true);
    events.fireNonAsciiKeySequence(fieldElement, KeyCodes.T, ALEPH_KEYCODE);
    assertNoCommand();
  },

  testNotFirstStrongCharacterInBR_LTR() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<div dir="rtl" id="container">' +
            '<div dir="rtl" id="text1">ABC</div>' +
            '<div dir="rtl" id="text2">' +
            '1<br>' +
            '2<b id="inner">3</b><i><em>4G</em><u>5<br>' +
            '6</u>7</i>8</b>9<br>' +
            '10' +
            '</div>' +
            '<div dir="rtl" id="text3">11</div>' +
            '</div>'));

    range.placeCursorNextTo(dom.$('inner'), true);
    events.fireKeySequence(fieldElement, KeyCodes.A);
    assertNoCommand();
  },

  /** Regression test for {@link http://b/7530985} */
  testFirstStrongCharacterWithPreviousBlockSibling_RTL() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<div>Te<div>xt</div>1<b id="cur">2</b>3</div>'));
    range.placeCursorNextTo(dom.$('cur'), true);
    events.fireNonAsciiKeySequence(fieldElement, KeyCodes.T, ALEPH_KEYCODE);
    assertRTL();
  },

  testFirstStrongCharacterWithPreviousBlockSibling_LTR() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<div dir="rtl">טק<div>סט</div>1<b id="cur">2</b>3</div>'));
    range.placeCursorNextTo(dom.$('cur'), true);
    events.fireKeySequence(fieldElement, KeyCodes.A);
    assertLTR();
  },

  testFirstStrongCharacterWithFollowingBlockSibling_RTL1() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<div>1<b id="cur">2</b>3<div>Te</div>xt</div>'));
    range.placeCursorNextTo(dom.$('cur'), true);
    events.fireNonAsciiKeySequence(fieldElement, KeyCodes.T, ALEPH_KEYCODE);
    assertRTL();
  },

  testFirstStrongCharacterWithFollowingBlockSibling_RTL2() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<div dir="rtl">1<b id="cur">2</b>3<div>א</div>ב</div>'));
    range.placeCursorNextTo(dom.$('cur'), true);
    events.fireKeySequence(fieldElement, KeyCodes.A);
    assertLTR();
  },

  testFirstStrongCharacterFromIME_RTL() {
    field.setSafeHtml(
        false, testing.newSafeHtmlForTest('<div id="text">123.7 3121, </div>'));
    field.focusAndPlaceCursorAtStart();
    const attributes = {};
    attributes[FirstStrong.INPUT_ATTRIBUTE] = 'אבג';
    events.fireNonAsciiKeySequence(fieldElement, 0, 0, attributes);
    if (userAgent.IE || userAgent.GECKO) {
      // goog.testing.events.fireNonAsciiKeySequence doesn't send KEYPRESS event
      // so no command is expected.
      assertNoCommand();
    } else {
      assertRTL();
    }
  },

  testFirstCharacterFromIME_LTR() {
    field.setSafeHtml(
        false,
        testing.newSafeHtmlForTest('<div dir="rtl" id="text"> 1234 </div>'));
    field.focusAndPlaceCursorAtStart();
    const attributes = {};
    attributes[FirstStrong.INPUT_ATTRIBUTE] = 'ABC';
    events.fireNonAsciiKeySequence(fieldElement, 0, 0, attributes);
    if (userAgent.IE || userAgent.GECKO) {
      // goog.testing.events.fireNonAsciiKeySequence doesn't send KEYPRESS event
      // so no command is expected.
      assertNoCommand();
    } else {
      assertLTR();
    }
  },

  /** Regression test for {@link http://b/19297723} */
  testLTRShortlyAfterRTLAndEnter() {
    clock = new MockClock();
    field.focusAndPlaceCursorAtStart();
    events.fireNonAsciiKeySequence(fieldElement, KeyCodes.T, ALEPH_KEYCODE);
    assertRTL();
    clock.tick(1000);  // Make sure no pending selection change event.
    events.fireKeySequence(fieldElement, KeyCodes.ENTER);
    assertRTL();
    events.fireKeySequence(fieldElement, KeyCodes.A);
    assertLTR();
    // Verify no RTL for first keypress on already-striong paragraph after
    // delayed selection change event.
    clock.tick(1000);  // Let delayed selection change event fire.
    events.fireNonAsciiKeySequence(fieldElement, KeyCodes.T, ALEPH_KEYCODE);
    assertLTR();
  },

  testRTLShortlyAfterLTRAndEnter() {
    clock = new MockClock();
    field.focusAndPlaceCursorAtStart();
    events.fireKeySequence(fieldElement, KeyCodes.A);
    assertLTR();
    clock.tick(1000);  // Make sure no pending selection change event.
    events.fireKeySequence(fieldElement, KeyCodes.ENTER);
    assertLTR();
    events.fireNonAsciiKeySequence(fieldElement, KeyCodes.T, ALEPH_KEYCODE);
    assertRTL();
    // Verify no LTR for first keypress on already-strong paragraph after
    // delayed selection change event.
    clock.tick(1000);  // Let delayed selection change event fire.
    events.fireKeySequence(fieldElement, KeyCodes.A);
    assertRTL();
  },
});
