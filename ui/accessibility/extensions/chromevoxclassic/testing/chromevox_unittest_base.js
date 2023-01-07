// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE([
    '//chrome/browser/resources/chromeos/accessibility/common/testing/' +
        'assert_additions.js'
]);
GEN_INCLUDE([
  '//chrome/browser/resources/chromeos/accessibility/chromevox/testing/' +
      'common.js',
  '//chrome/browser/resources/chromeos/accessibility/common/testing/' +
      'callback_helper.js',
  '//chrome/browser/resources/chromeos/accessibility/common/testing/' +
      'common.js'
]);

/**
 * Base test fixture for ChromeVox unit tests.
 *
 * Note that while conceptually these are unit tests, these tests need
 * to run in a full web page, so they're actually run as WebUI browser
 * tests.
 *
 * @constructor
 * @extends {testing.Test}
 */
function ChromeVoxUnitTestBase() {
  if (this.isAsync) {
    this.callbackHelper_ = new CallbackHelper(this);
  }
}

ChromeVoxUnitTestBase.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  testGenCppIncludes: function() {
    GEN(`
  #include "content/public/test/browser_test.h"
      `);
  },

  /** @override */
  closureModuleDeps: [
    'cvox.ChromeVoxTester',
    'cvox.ChromeVoxUserCommands',
    'cvox.SpokenListBuilder',
  ],

  /** @override */
  browsePreload: DUMMY_URL,

  /**
   * Loads some inlined html into the body of the current document, replacing
   * whatever was there previously.
   * @param {string} html The html to load as a string.
   */
  loadHtml: function(html) {
    while (document.head.firstChild) {
      document.head.removeChild(document.head.firstChild);
    }
    while (document.body.firstChild) {
      document.body.removeChild(document.body.firstChild);
    }
    this.appendHtml(html);
  },

  /**
   * Loads some inlined html into the current document, replacing
   * whatever was there previously. This version takes the html
   * encoded as a comment inside a function, so you can use it like this:
   *
   * this.loadDoc(function() {/*!
   *     <p>Html goes here</p>
   * * /});
   *
   * @param {Function} commentEncodedHtml The html to load, embedded as a
   *     comment inside an anonymous function - see example, above.
   */
  loadDoc: function(commentEncodedHtml) {
    var html =
        TestUtils.extractHtmlFromCommentEncodedString(commentEncodedHtml);
    this.loadHtml(html);
  },

  /**
   * Appends some inlined html into the current document, at the end of
   * the body element. Takes the html encoded as a comment inside a function,
   * so you can use it like this:
   *
   * this.appendDoc(function() {/*!
   *     <p>Html goes here</p>
   * * /});
   *
   * @param {Function} commentEncodedHtml The html to load, embedded as a
   *     comment inside an anonymous function - see example, above.
   */
  appendDoc: function(commentEncodedHtml) {
    var html =
        TestUtils.extractHtmlFromCommentEncodedString(commentEncodedHtml);
    this.appendHtml(html);
  },

  /**
   * Appends some inlined html into the current document, at the end of
   * the body element.
   * @param {string} html The html to load as a string.
   */
  appendHtml: function(html) {
    var div = document.createElement('div');
    div.innerHTML = html;
    var fragment = document.createDocumentFragment();
    while (div.firstChild) {
      fragment.appendChild(div.firstChild);
    }
    document.body.appendChild(fragment);
  },

  /**
   * Waits for the queued events in ChromeVoxEventWatcher to be
   * handled, then calls a callback function with provided arguments
   * in the test case scope. Very useful for asserting the results of events.
   *
   * @param {function()} func A function to call when ChromeVox is ready.
   * @param {*} var_args Additional arguments to pass through to the function.
   * @return {ChromeVoxUnitTestBase} this.
   */
  waitForCalm: function(func, var_args) {
    var calmArguments = Array.prototype.slice.call(arguments);
    calmArguments.shift();
    cvox.ChromeVoxEventWatcher.addReadyCallback(this.newCallback(function() {
      func.apply(this, calmArguments);
    }));
    return this;  // for chaining.
  },

  /**
   * Asserts the TTS engine spoke a certain string. Clears the TTS buffer.
   * @param {string} expectedText The expected text.
   * @return {ChromeVoxUnitTestBase} this.
   */
  assertSpoken: function(expectedText) {
    assertEquals(
        expectedText, cvox.ChromeVoxTester.testTts().getUtterancesAsString());
    cvox.ChromeVoxTester.clearUtterances();
    return this;  // for chaining.
  },

  /**
   * Asserts a list of utterances are in the correct queue mode.
   * @param {cvox.SpokenListBuilder|Array} expectedList A list
   *     of [text, queueMode] tuples OR a SpokenListBuilder with the expected
   *     utterances.
   * @return {ChromeVoxUnitTestBase} this.
   */
  assertSpokenList: function(expectedList) {
    if (expectedList instanceof cvox.SpokenListBuilder) {
      expectedList = expectedList.build();
    }

    var ulist = cvox.ChromeVoxTester.testTts().getUtteranceInfoList();
    for (var i = 0; i < expectedList.length; i++) {
      var text = expectedList[i][0];
      var queueMode = expectedList[i][1];
      this.assertSingleUtterance_(
          text, queueMode, ulist[i].text, ulist[i].queueMode);
    }
    cvox.ChromeVoxTester.clearUtterances();
    return this;  // for chaining.
  },

  assertSingleUtterance_: function(
      expectedText, expectedQueueMode, text, queueMode) {
    assertEquals(expectedQueueMode, queueMode);
    assertEquals(expectedText, text);
  },

  /**
   * Focuses an element.
   * @param {string} id The id of the element to focus.
   * @return {ChromeVoxUnitTestBase} this.
   */
  setFocus: function(id) {
    $(id).focus();
    return this;  // for chaining.
  },

  /**
   * Executes a ChromeVox user command.
   * @param {string} command The name of the command to run.
   * @return {ChromeVoxUnitTestBase} this.
   */
  userCommand: function(command) {
    cvox.ChromeVoxUserCommands.commands[command]();
    return this;  // for chaining.
  },

  /**
   * @return {cvox.SpokenListBuilder} A new builder.
   */
  spokenList: function() {
    return new cvox.SpokenListBuilder();
  },

  /**
   * @type {CallbackHelper}
   * @private
   */
  callbackHelper_: null,

  /**
   * Creates a callback that optionally calls {@code opt_callback} when
   * called.  If this method is called one or more times, then
   * {@code testDone()} will be called when all callbacks have been called.
   * @param {Function=} opt_callback Wrapped callback that will have its this
   *        reference bound to the test fixture.
   * @return {Function}
   */
  newCallback: function(opt_callback) {
    assertNotEquals(null, this.callbackHelper_);
    return this.callbackHelper_.wrap(opt_callback);
  }
};
