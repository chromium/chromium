/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.PlainTextSpellCheckerTest');
goog.setTestOnly();

const AbstractSpellChecker = goog.require('goog.ui.AbstractSpellChecker');
const KeyCodes = goog.require('goog.events.KeyCodes');
const PlainTextSpellChecker = goog.require('goog.ui.PlainTextSpellChecker');
const SpellCheck = goog.require('goog.spell.SpellCheck');
const Timer = goog.require('goog.Timer');
const dom = goog.require('goog.dom');
const events = goog.require('goog.testing.events');
const testSuite = goog.require('goog.testing.testSuite');

const missspelling = 'missspelling';
const iggnore = 'iggnore';
const vocabulary = ['test', 'words', 'a', 'few', missspelling, iggnore];

// We don't use Math.random() to make test predictable. Math.random is not
// repeatable, so a success on the dev machine != success in the lab (or on
// other dev machines). This is the same pseudorandom logic that CRT rand()
// uses.
let rseed = 1;
function random(range) {
  rseed = (rseed * 1103515245 + 12345) & 0xffffffff;
  return ((rseed >> 16) & 0x7fff) % range;
}

function localSpellCheckingFunction(words, spellChecker, callback) {
  const len = words.length;
  const results = [];
  for (let i = 0; i < len; i++) {
    const word = words[i];
    let found = false;
    // Last two words are considered misspellings
    for (let j = 0; j < vocabulary.length - 2; ++j) {
      if (vocabulary[j] == word) {
        found = true;
        break;
      }
    }
    if (found) {
      results.push([word, SpellCheck.WordStatus.VALID]);
    } else {
      results.push([word, SpellCheck.WordStatus.INVALID, ['foo', 'bar']]);
    }
  }
  callback.call(spellChecker, results);
}

function generateRandomSpace() {
  let string = '';
  const nSpace = 1 + random(4);
  for (let i = 0; i < nSpace; ++i) {
    string += ' ';
  }
  return string;
}

function generateRandomString(maxWords, doQuotes) {
  const x = random(10);
  let string = '';
  if (doQuotes) {
    if (x == 0) {
      string = 'On xxxxx yyyy wrote:\n> ';
    } else if (x < 3) {
      string = '> ';
    }
  }

  const nWords = 1 + random(maxWords);
  for (let i = 0; i < nWords; ++i) {
    string += vocabulary[random(vocabulary.length)];
    string += generateRandomSpace();
  }
  return string;
}

const timerQueue = [];
function processTimerQueue() {
  while (timerQueue.length > 0) {
    const fn = timerQueue.shift();
    fn();
  }
}

function localTimer(fn, delay, obj) {
  if (obj) {
    fn = goog.bind(fn, obj);
  }
  timerQueue.push(fn);
  return timerQueue.length;
}

testSuite({
  testPlainTextSpellCheckerNoQuotes() {
    const handler = new SpellCheck(localSpellCheckingFunction);
    const s = new PlainTextSpellChecker(handler);
    /** @suppress {visibility} suppression added to enable type checking */
    s.asyncWordsPerBatch_ = 100;
    const el = document.getElementById('test1');
    s.decorate(el);
    let text = '';
    for (let i = 0; i < 10; ++i) {
      text += generateRandomString(10, false) + '\n';
    }
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    el.value = text;
    // Yes this looks bizarre. This is for '\n' processing.
    // They get converted to CRLF as part of the above statement.
    text = el.value;

    const timerSav = Timer.callOnce;
    /** @suppress {checkTypes} suppression added to enable type checking */
    Timer.callOnce = localTimer;

    s.check();
    processTimerQueue();
    s.ignoreWord(iggnore);
    processTimerQueue();
    s.check();
    processTimerQueue();
    s.resume();
    processTimerQueue();

    Timer.callOnce = timerSav;

    assertEquals(
        'Spell checker run should not change the underlying element.', text,
        el.value);
    s.dispose();
  },

  testPlainTextSpellCheckerWithQuotes() {
    const handler = new SpellCheck(localSpellCheckingFunction);
    const s = new PlainTextSpellChecker(handler);
    /** @suppress {visibility} suppression added to enable type checking */
    s.asyncWordsPerBatch_ = 100;
    const el = document.getElementById('test2');
    s.decorate(el);
    let text = '';
    for (let i = 0; i < 10; ++i) {
      text += generateRandomString(10, true) + '\n';
    }
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    el.value = text;
    // Yes this looks bizarre. This is for '\n' processing.
    // They get converted to CRLF as part of the above statement.
    text = el.value;

    const timerSav = Timer.callOnce;
    /** @suppress {checkTypes} suppression added to enable type checking */
    Timer.callOnce = localTimer;

    s.setExcludeMarker(new RegExp('\nOn .* wrote:\n(> .*\n)+|\n(> .*\n)', 'g'));
    s.check();
    processTimerQueue();
    s.ignoreWord(iggnore);
    processTimerQueue();
    s.check();
    processTimerQueue();
    s.resume();
    processTimerQueue();

    Timer.callOnce = timerSav;

    assertEquals(
        'Spell checker run should not change the underlying element.', text,
        el.value);
    s.dispose();
  },

  /**
     @suppress {checkTypes,strictMissingProperties,visibility} suppression
     added to enable type checking
   */
  testPlainTextSpellCheckerWordReplacement() {
    const handler = new SpellCheck(localSpellCheckingFunction);
    const s = new PlainTextSpellChecker(handler);
    /** @suppress {visibility} suppression added to enable type checking */
    s.asyncWordsPerBatch_ = 100;
    const el = document.getElementById('test3');
    s.decorate(el);
    let text = '';
    for (let i = 0; i < 10; ++i) {
      text += generateRandomString(10, false) + '\n';
    }
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    el.value = text;

    const timerSav = Timer.callOnce;
    /** @suppress {checkTypes} suppression added to enable type checking */
    Timer.callOnce = localTimer;

    s.check();
    processTimerQueue();

    /** @suppress {visibility} suppression added to enable type checking */
    const container = s.overlay_;
    let wordEl = container.firstChild;
    while (wordEl) {
      if (dom.getTextContent(wordEl) == missspelling) {
        break;
      }
      wordEl = wordEl.nextSibling;
    }

    if (!wordEl) {
      assertTrue(
          'Cannot find the world that should have been here.' +
              'Please revise the test',
          false);
      return;
    }

    /** @suppress {visibility} suppression added to enable type checking */
    s.activeWord_ = missspelling;
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    s.activeElement_ = wordEl;
    /** @suppress {visibility} suppression added to enable type checking */
    const suggestions = s.getSuggestions_();
    s.replaceWord(wordEl, missspelling, 'foo');
    assertEquals(
        'Should have set the original word attribute!',
        wordEl.getAttribute(AbstractSpellChecker.ORIGINAL_), missspelling);

    /** @suppress {visibility} suppression added to enable type checking */
    s.activeWord_ = dom.getTextContent(wordEl);
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    s.activeElement_ = wordEl;
    /** @suppress {visibility} suppression added to enable type checking */
    const newSuggestions = s.getSuggestions_();
    assertEquals(
        'Suggestion list should still be present even if the word ' +
            'is now correct!',
        suggestions, newSuggestions);

    s.resume();
    processTimerQueue();

    Timer.callOnce = timerSav;
    s.dispose();
  },

  testPlainTextSpellCheckerKeyboardNavigateNext() {
    const handler = new SpellCheck(localSpellCheckingFunction);
    const s = new PlainTextSpellChecker(handler);
    const el = document.getElementById('test4');
    s.decorate(el);
    const text = 'a unit test for keyboard test';
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    el.value = text;
    const keyEventProperties = {};
    keyEventProperties.ctrlKey = true;
    keyEventProperties.shiftKey = false;

    const timerSav = Timer.callOnce;
    /** @suppress {checkTypes} suppression added to enable type checking */
    Timer.callOnce = localTimer;

    s.check();
    processTimerQueue();

    /** @suppress {visibility} suppression added to enable type checking */
    const container = s.overlay_;

    // First call just moves focus to first misspelled word.
    events.fireKeySequence(container, KeyCodes.RIGHT, keyEventProperties);

    // Test moving from first to second misspelled word.
    const defaultExecuted =
        events.fireKeySequence(container, KeyCodes.RIGHT, keyEventProperties);

    assertFalse(
        'The default action should be prevented for the key event',
        defaultExecuted);
    assertEquals(
        'The second misspelled word should have focus.', document.activeElement,
        container.children[1]);

    s.resume();
    processTimerQueue();

    Timer.callOnce = timerSav;
    s.dispose();
  },

  testPlainTextSpellCheckerKeyboardNavigateNextOnLastWord() {
    const handler = new SpellCheck(localSpellCheckingFunction);
    const s = new PlainTextSpellChecker(handler);
    const el = document.getElementById('test5');
    s.decorate(el);
    const text = 'a unit test for keyboard test';
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    el.value = text;
    const keyEventProperties = {};
    keyEventProperties.ctrlKey = true;
    keyEventProperties.shiftKey = false;

    const timerSav = Timer.callOnce;
    /** @suppress {checkTypes} suppression added to enable type checking */
    Timer.callOnce = localTimer;

    s.check();
    processTimerQueue();

    /** @suppress {visibility} suppression added to enable type checking */
    const container = s.overlay_;

    // First call just moves focus to first misspelled word.
    events.fireKeySequence(container, KeyCodes.RIGHT, keyEventProperties);
    events.fireKeySequence(container, KeyCodes.RIGHT, keyEventProperties);
    events.fireKeySequence(container, KeyCodes.RIGHT, keyEventProperties);

    // Test moving to the next invalid word.
    const defaultExecuted =
        events.fireKeySequence(container, KeyCodes.RIGHT, keyEventProperties);

    assertFalse(
        'The default action should be prevented for the key event',
        defaultExecuted);
    assertEquals(
        'The third/last misspelled word should have focus.',
        document.activeElement, container.children[2]);

    s.resume();
    processTimerQueue();

    Timer.callOnce = timerSav;
    s.dispose();
  },

  testPlainTextSpellCheckerKeyboardNavigateOpenSuggestions() {
    const handler = new SpellCheck(localSpellCheckingFunction);
    const s = new PlainTextSpellChecker(handler);
    const el = document.getElementById('test6');
    s.decorate(el);
    const text = 'unit';
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    el.value = text;
    const keyEventProperties = {};
    keyEventProperties.ctrlKey = true;
    keyEventProperties.shiftKey = false;

    const timerSav = Timer.callOnce;
    /** @suppress {checkTypes} suppression added to enable type checking */
    Timer.callOnce = localTimer;

    s.check();
    processTimerQueue();

    /** @suppress {visibility} suppression added to enable type checking */
    const container = s.overlay_;
    /** @suppress {visibility} suppression added to enable type checking */
    const suggestionMenu = s.getMenu();

    events.fireKeySequence(container, KeyCodes.RIGHT, keyEventProperties);

    assertFalse(
        'The suggestion menu should not be visible yet.',
        suggestionMenu.isVisible());

    keyEventProperties.ctrlKey = false;
    const defaultExecuted =
        events.fireKeySequence(container, KeyCodes.DOWN, keyEventProperties);

    assertFalse(
        'The default action should be prevented for the key event',
        defaultExecuted);
    assertTrue(
        'The suggestion menu should be visible after the key event.',
        suggestionMenu.isVisible());

    s.resume();
    processTimerQueue();

    Timer.callOnce = timerSav;
    s.dispose();
  },

  testPlainTextSpellCheckerKeyboardNavigatePrevious() {
    const handler = new SpellCheck(localSpellCheckingFunction);
    const s = new PlainTextSpellChecker(handler);
    const el = document.getElementById('test7');
    s.decorate(el);
    const text = 'a unit test for keyboard test';
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    el.value = text;
    const keyEventProperties = {};
    keyEventProperties.ctrlKey = true;
    keyEventProperties.shiftKey = false;

    const timerSav = Timer.callOnce;
    /** @suppress {checkTypes} suppression added to enable type checking */
    Timer.callOnce = localTimer;

    s.check();
    processTimerQueue();

    /** @suppress {visibility} suppression added to enable type checking */
    const container = s.overlay_;

    // Move to the third element, so we can test the move back to the second.
    events.fireKeySequence(container, KeyCodes.RIGHT, keyEventProperties);
    events.fireKeySequence(container, KeyCodes.RIGHT, keyEventProperties);
    events.fireKeySequence(container, KeyCodes.RIGHT, keyEventProperties);

    // Test moving from third to second misspelled word.
    const defaultExecuted =
        events.fireKeySequence(container, KeyCodes.LEFT, keyEventProperties);

    assertFalse(
        'The default action should be prevented for the key event',
        defaultExecuted);
    assertEquals(
        'The second misspelled word should have focus.', document.activeElement,
        container.children[1]);

    s.resume();
    processTimerQueue();

    Timer.callOnce = timerSav;
    s.dispose();
  },

  testPlainTextSpellCheckerKeyboardNavigatePreviousOnFirstWord() {
    const handler = new SpellCheck(localSpellCheckingFunction);
    const s = new PlainTextSpellChecker(handler);
    const el = document.getElementById('test8');
    s.decorate(el);
    const text = 'a unit test for keyboard test';
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    el.value = text;
    const keyEventProperties = {};
    keyEventProperties.ctrlKey = true;
    keyEventProperties.shiftKey = false;

    const timerSav = Timer.callOnce;
    /** @suppress {checkTypes} suppression added to enable type checking */
    Timer.callOnce = localTimer;

    s.check();
    processTimerQueue();

    /** @suppress {visibility} suppression added to enable type checking */
    const container = s.overlay_;

    // Move to the first invalid word.
    events.fireKeySequence(container, KeyCodes.RIGHT, keyEventProperties);

    // Test moving to the previous invalid word.
    const defaultExecuted =
        events.fireKeySequence(container, KeyCodes.LEFT, keyEventProperties);

    assertFalse(
        'The default action should be prevented for the key event',
        defaultExecuted);
    assertEquals(
        'The first misspelled word should have focus.', document.activeElement,
        container.children[0]);

    s.resume();
    processTimerQueue();

    Timer.callOnce = timerSav;
    s.dispose();
  },
});
