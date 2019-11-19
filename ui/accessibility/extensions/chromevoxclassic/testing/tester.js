// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('cvox.ChromeVoxTester');

goog.require('TestMsgs');
goog.require('cvox.AbstractEarcons');
goog.require('cvox.BrailleInterface');
goog.require('cvox.ChromeVoxEventWatcher');
goog.require('cvox.ChromeVoxUserCommands');
goog.require('cvox.LiveRegions');
goog.require('cvox.NavigationManager');
goog.require('cvox.NavigationShifter');
goog.require('cvox.QueueMode');
goog.require('cvox.TestHost');
goog.require('cvox.TestTts');


/**
 * @fileoverview Testing framework for ChromeVox.
 *
 */


/**
 * Initializes cvox.ChromeVoxTester and sets up the mock ChromeVox
 * environment.
 * @param {!Document} doc The DOM document to add event listeners to.
 */
cvox.ChromeVoxTester.setUp = function(doc) {

  cvox.ChromeVox.navigationManager = new cvox.NavigationManager();
  cvox.ChromeVoxTester.testTts_ = new cvox.TestTts();
  cvox.ChromeVox.tts = cvox.ChromeVoxTester.testTts_;

  // TODO(deboer): Factor this out as 'TestEarcons'
  cvox.ChromeVox.earcons = new cvox.AbstractEarcons();
  cvox.ChromeVox.earcons.playEarcon = function(earcon) {};

  cvox.ChromeVox.braille = new cvox.BrailleInterface();
  cvox.ChromeVox.braille.write = function(params) {};

  Msgs = TestMsgs;

  cvox.ChromeVox.host = new cvox.TestHost();

  // Init LiveRegions with a date of 0 so that the initial delay before
  // things is spoken is skipped.
  cvox.LiveRegions.init(new Date(0), cvox.QueueMode.QUEUE, false);

  cvox.ChromeVoxEventWatcher.init(doc);
  window.console.log('done setup');
};

/**
 * Tears down cvox.ChromeVoxTester.
 * @param {!Document} doc The DOM document where event listeners were added.
 */
cvox.ChromeVoxTester.tearDown = function(doc) {
  cvox.ChromeVoxEventWatcher.cleanup(doc);
};


/**
 * Returns the cvox.TestTts created by the tester.
 * @return {cvox.TestTts} The TestTts.
 */
cvox.ChromeVoxTester.testTts = function() {
  return cvox.ChromeVoxTester.testTts_;
};


/**
 * All calls to tts.speak are saved in an array of utterances.
 * Clear any utterances that were saved up to this poing.
 */
cvox.ChromeVoxTester.clearUtterances = function() {
  cvox.ChromeVoxTester.testTts_.clearUtterances();
};


/**
 * Return a list of strings of what was spoken by tts.speak().
 * @return {Array<string>} A list of all utterances spoken since
 *     initialization or the last call to clearUtterances.
 */
cvox.ChromeVoxTester.getUtteranceList = function() {
  return cvox.ChromeVoxTester.testTts_.getUtteranceList();
};

/**
 * @type {Object<number>} Map from a navigation strategy name to the Navigation
 *     Manager strategy enum.
 */
cvox.ChromeVoxTester.STRATEGY_MAP = {
  'lineardom': cvox.NavigationShifter.GRANULARITIES.OBJECT,
  'smart': cvox.NavigationShifter.GRANULARITIES.GROUP,
  'selection': cvox.NavigationShifter.GRANULARITIES.SENTENCE
};

/**
 * Switches to a different navigation strategy.
 * @param {string} strategy The desired navigation strategy.
 */
cvox.ChromeVoxTester.setStrategy = function(strategy) {
  cvox.ChromeVox.navigationManager.ensureNotSubnavigating();
  cvox.ChromeVox.navigationManager.setGranularity(
      cvox.ChromeVoxTester.STRATEGY_MAP[strategy]);
};

/**
 * Starts reading the page from the current node.
 */
cvox.ChromeVoxTester.readFromHere = function() {
  cvox.ChromeVox.navigationManager.startReading(cvox.QueueMode.FLUSH);
};

/**
 * Syncs to the given node in the test HTML
 * @param {Node} node The node to sync to.
 */
cvox.ChromeVoxTester.syncToNode = function(node) {
  cvox.ChromeVox.navigationManager.updateSel(
      cvox.CursorSelection.fromNode(node));
  cvox.ChromeVox.navigationManager.sync();
};

/**
 * Syncs to the first node in the test.
 */
cvox.ChromeVoxTester.syncToFirstNode = function() {
  cvox.ChromeVox.navigationManager.updateSel(cvox.CursorSelection.fromBody());
  cvox.ChromeVox.navigationManager.sync();
};
