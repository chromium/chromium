/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.RichTextSpellCheckerTest');
goog.setTestOnly();

const KeyCodes = goog.require('goog.events.KeyCodes');
const MockClock = goog.require('goog.testing.MockClock');
const Range = goog.require('goog.dom.Range');
const RichTextSpellChecker = goog.require('goog.ui.RichTextSpellChecker');
const SpellCheck = goog.require('goog.spell.SpellCheck');
const TagName = goog.require('goog.dom.TagName');
const classlist = goog.require('goog.dom.classlist');
const events = goog.require('goog.testing.events');
const googObject = goog.require('goog.object');
const testSuite = goog.require('goog.testing.testSuite');

const VOCABULARY = ['test', 'words', 'a', 'few'];
const SUGGESTIONS = ['foo', 'bar'];
const EXCLUDED_DATA = ['DIV.goog-quote', 'goog-comment', 'SPAN.goog-note'];

/**
 * Delay in ms needed for the spell check word lookup to finish. Finishing the
 * lookup also finishes the spell checking.
 * @see SpellCheck.LOOKUP_DELAY_
 */
const SPELL_CHECK_LOOKUP_DELAY = 100;

const TEST_TEXT1 = 'this test is longer than a few words now';
const TEST_TEXT2 = 'test another simple text with misspelled words';
const TEST_TEXT3 = 'test another simple text with misspelled words' +
    '<b class="goog-quote">test another simple text with misspelled words<u> ' +
    'test another simple text with misspelled words<del class="goog-quote"> ' +
    'test another simple text with misspelled words<i>this test is longer ' +
    'than a few words now</i>test another simple text with misspelled words ' +
    '<i>this test is longer than a few words now</i></del>test another ' +
    'simple text with misspelled words<del class="goog-quote">test another ' +
    'simple text with misspelled words<i>this test is longer than a few ' +
    'words now</i>test another simple text with misspelled words<i>this test ' +
    'is longer than a few words now</i></del></u>test another simple text ' +
    'with misspelled words<u>test another simple text with misspelled words' +
    '<del class="goog-quote">test another simple text with misspelled words' +
    '<i> thistest is longer than a few words now</i>test another simple text ' +
    'with misspelled words<i>this test is longer than a few words ' +
    'now</i></del>test another simple text with misspelled words' +
    '<del class="goog-quote">test another simple text with misspelled words' +
    '<i>this test is longer than a few words now</i>test another simple text ' +
    'with misspelled words<i>this test is longer than a few words ' +
    'now</i></del></u></b>';

let spellChecker;
let handler;
let mockClock;

function waitForSpellCheckToFinish() {
  mockClock.tick(SPELL_CHECK_LOOKUP_DELAY);
}

/**
 * Function to use for word lookup by the spell check handler. This function is
 * supplied as a constructor parameter for the spell check handler.
 * @param {!Array<string>} words Unknown words that need to be looked up.
 * @param {!SpellCheck} spellChecker The spell check handler.
 * @param {function(!Array)} callback The lookup callback function.
 */
function localSpellCheckingFunction(words, spellChecker, callback) {
  const len = words.length;
  const results = [];
  for (let i = 0; i < len; i++) {
    const word = words[i];
    let found = false;
    for (let j = 0; j < VOCABULARY.length; ++j) {
      if (VOCABULARY[j] == word) {
        found = true;
        break;
      }
    }
    if (found) {
      results.push([word, SpellCheck.WordStatus.VALID]);
    } else {
      results.push([word, SpellCheck.WordStatus.INVALID, SUGGESTIONS]);
    }
  }
  callback.call(spellChecker, results);
}

function assertCursorAtElement(expectedId) {
  const range = Range.createFromWindow();

  let focusedElementId;
  if (isCaret(range)) {
    if (isMisspelledWordElement(range.getStartNode())) {
      /**
       * @suppress {strictMissingProperties} suppression added to enable type
       * checking
       */
      focusedElementId = range.getStartNode().id;
    }

    // In Chrome a cursor at the start of a misspelled word will appear to be at
    // the end of the text node preceding it.
    if (isCursorAtEndOfStartNode(range) &&
        range.getStartNode().nextSibling != null &&
        isMisspelledWordElement(range.getStartNode().nextSibling)) {
      /**
       * @suppress {strictMissingProperties} suppression added to enable type
       * checking
       */
      focusedElementId = range.getStartNode().nextSibling.id;
    }
  }

  assertEquals(
      'The cursor is not at the expected misspelled word.', expectedId,
      focusedElementId);
}

function isCaret(range) {
  return range.getStartNode() == range.getEndNode();
}

function isMisspelledWordElement(element) {
  return classlist.contains(element, 'goog-spellcheck-word');
}

function isCursorAtEndOfStartNode(range) {
  return range.getStartNode().length == range.getStartOffset();
}
testSuite({
  setUp() {
    mockClock = new MockClock(true /* install */);
    handler = new SpellCheck(localSpellCheckingFunction);
    spellChecker = new RichTextSpellChecker(handler);
  },

  tearDown() {
    spellChecker.dispose();
    handler.dispose();
    mockClock.dispose();
  },

  testDocumentIntegrity() {
    const el = document.getElementById('test1');
    spellChecker.decorate(el);
    el.appendChild(document.createTextNode(TEST_TEXT3));
    const el2 = el.cloneNode(true);

    spellChecker.setExcludeMarker('goog-quote');
    spellChecker.check();
    waitForSpellCheckToFinish();
    spellChecker.ignoreWord('iggnore');
    waitForSpellCheckToFinish();
    spellChecker.check();
    waitForSpellCheckToFinish();
    spellChecker.resume();
    waitForSpellCheckToFinish();

    assertEquals(
        'Spell checker run should not change the underlying element.',
        el2.innerHTML, el.innerHTML);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testExcludeMarkers() {
    const el = document.getElementById('test1');
    spellChecker.decorate(el);
    spellChecker.setExcludeMarker(
        ['DIV.goog-quote', 'goog-comment', 'SPAN.goog-note']);
    assertArrayEquals(
        ['goog-quote', 'goog-comment', 'goog-note'],
        spellChecker.excludeMarker);
    assertArrayEquals(
        [String(TagName.DIV), undefined, String(TagName.SPAN)],
        spellChecker.excludeTags);
    el.innerHTML = '<div class="goog-quote">misspelling</div>' +
        '<div class="goog-yes">misspelling</div>' +
        '<div class="goog-note">misspelling</div>' +
        '<div class="goog-comment">misspelling</div>' +
        '<span>misspelling<span>';

    spellChecker.check();
    waitForSpellCheckToFinish();
    assertEquals(3, spellChecker.getLastIndex());
  },

  testBiggerDocument() {
    const el = document.getElementById('test2');
    spellChecker.decorate(el);
    el.appendChild(document.createTextNode(TEST_TEXT3));
    const el2 = el.cloneNode(true);

    spellChecker.check();
    waitForSpellCheckToFinish();
    spellChecker.resume();
    waitForSpellCheckToFinish();

    assertEquals(
        'Spell checker run should not change the underlying element.',
        el2.innerHTML, el.innerHTML);
  },

  testElementOverflow() {
    const el = document.getElementById('test3');
    spellChecker.decorate(el);
    el.appendChild(document.createTextNode(TEST_TEXT3));

    const el2 = el.cloneNode(true);

    spellChecker.check();
    waitForSpellCheckToFinish();
    spellChecker.check();
    waitForSpellCheckToFinish();
    spellChecker.resume();
    waitForSpellCheckToFinish();

    assertEquals(
        'Spell checker run should not change the underlying element.',
        el2.innerHTML, el.innerHTML);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testKeyboardNavigateNext() {
    const el = document.getElementById('test4');
    spellChecker.decorate(el);
    const text = 'a unit test for keyboard test';
    el.appendChild(document.createTextNode(text));
    const keyEventProperties =
        googObject.create('ctrlKey', true, 'shiftKey', false);

    spellChecker.check();
    waitForSpellCheckToFinish();

    // First call just moves focus to first misspelled word.
    events.fireKeySequence(el, KeyCodes.RIGHT, keyEventProperties);

    // Test moving from first to second misspelled word.
    const defaultExecuted =
        events.fireKeySequence(el, KeyCodes.RIGHT, keyEventProperties);

    assertFalse(
        'The default action should be prevented for the key event',
        defaultExecuted);
    assertCursorAtElement(spellChecker.makeElementId(2));

    spellChecker.resume();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testKeyboardNavigateNextOnLastWord() {
    const el = document.getElementById('test5');
    spellChecker.decorate(el);
    const text = 'a unit test for keyboard test';
    el.appendChild(document.createTextNode(text));
    const keyEventProperties =
        googObject.create('ctrlKey', true, 'shiftKey', false);

    spellChecker.check();
    waitForSpellCheckToFinish();

    // Move to the last invalid word.
    events.fireKeySequence(el, KeyCodes.RIGHT, keyEventProperties);
    events.fireKeySequence(el, KeyCodes.RIGHT, keyEventProperties);
    events.fireKeySequence(el, KeyCodes.RIGHT, keyEventProperties);

    // Test moving to the next invalid word. Should have no effect.
    const defaultExecuted =
        events.fireKeySequence(el, KeyCodes.RIGHT, keyEventProperties);

    assertFalse(
        'The default action should be prevented for the key event',
        defaultExecuted);
    assertCursorAtElement(spellChecker.makeElementId(3));

    spellChecker.resume();
  },

  testKeyboardNavigateOpenSuggestions() {
    const el = document.getElementById('test6');
    spellChecker.decorate(el);
    const text = 'unit';
    el.appendChild(document.createTextNode(text));
    const keyEventProperties =
        googObject.create('ctrlKey', true, 'shiftKey', false);

    spellChecker.check();
    waitForSpellCheckToFinish();

    /** @suppress {visibility} suppression added to enable type checking */
    const suggestionMenu = spellChecker.getMenu();

    events.fireKeySequence(el, KeyCodes.RIGHT, keyEventProperties);

    assertFalse(
        'The suggestion menu should not be visible yet.',
        suggestionMenu.isVisible());

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    keyEventProperties.ctrlKey = false;
    const defaultExecuted =
        events.fireKeySequence(el, KeyCodes.DOWN, keyEventProperties);

    assertFalse(
        'The default action should be prevented for the key event',
        defaultExecuted);
    assertTrue(
        'The suggestion menu should be visible after the key event.',
        suggestionMenu.isVisible());

    spellChecker.resume();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testKeyboardNavigatePrevious() {
    const el = document.getElementById('test7');
    spellChecker.decorate(el);
    const text = 'a unit test for keyboard test';
    el.appendChild(document.createTextNode(text));
    const keyEventProperties =
        googObject.create('ctrlKey', true, 'shiftKey', false);

    spellChecker.check();
    waitForSpellCheckToFinish();

    // Move to the third element, so we can test the move back to the second.
    events.fireKeySequence(el, KeyCodes.RIGHT, keyEventProperties);
    events.fireKeySequence(el, KeyCodes.RIGHT, keyEventProperties);
    events.fireKeySequence(el, KeyCodes.RIGHT, keyEventProperties);

    const defaultExecuted =
        events.fireKeySequence(el, KeyCodes.LEFT, keyEventProperties);

    assertFalse(
        'The default action should be prevented for the key event',
        defaultExecuted);
    assertCursorAtElement(spellChecker.makeElementId(2));

    spellChecker.resume();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testKeyboardNavigatePreviousOnLastWord() {
    const el = document.getElementById('test8');
    spellChecker.decorate(el);
    const text = 'a unit test for keyboard test';
    el.appendChild(document.createTextNode(text));
    const keyEventProperties =
        googObject.create('ctrlKey', true, 'shiftKey', false);

    spellChecker.check();
    waitForSpellCheckToFinish();

    // Move to the first invalid word.
    events.fireKeySequence(el, KeyCodes.RIGHT, keyEventProperties);

    // Test moving to the previous invalid word. Should have no effect.
    const defaultExecuted =
        events.fireKeySequence(el, KeyCodes.LEFT, keyEventProperties);

    assertFalse(
        'The default action should be prevented for the key event',
        defaultExecuted);
    assertCursorAtElement(spellChecker.makeElementId(1));

    spellChecker.resume();
  },
});
