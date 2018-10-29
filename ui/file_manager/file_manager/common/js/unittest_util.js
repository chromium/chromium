// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Asserts that two lists contain the same set of Entries.  Entries are deemed
 * to be the same if they point to the same full path.
 *
 * @param {!Array<!FileEntry>} expected
 * @param {!Array<!FileEntry>} actual
 */
function assertFileEntryListEquals(expected, actual) {
  var entryToPath = function(entry) {
    assertTrue(entry.isFile);
    return entry.fullPath;
  };

  assertFileEntryPathsEqual(expected.map(entryToPath), actual);
}

/**
 * Asserts that a list of FileEntry instances point to the expected paths.
 *
 * @param {!Array<string>} expectedPaths
 * @param {!Array<!FileEntry>} fileEntries
 */
function assertFileEntryPathsEqual(expectedPaths, fileEntries) {
  assertEquals(expectedPaths.length, fileEntries.length);

  var entryToPath = function(entry) {
    assertTrue(entry.isFile);
    return entry.fullPath;
  };

  var actualPaths = fileEntries.map(entryToPath);
  actualPaths.sort();
  expectedPaths = expectedPaths.slice();
  expectedPaths.sort();

  assertEquals(JSON.stringify(expectedPaths), JSON.stringify(actualPaths));
}

/**
 * A class that captures calls to a function so that values can be validated.
 * For use in tests only.
 *
 * <p>Example:
 * <pre>
 *   var recorder = new TestCallRecorder();
 *   someClass.addListener(recorder.callback);
 *   // do stuff ...
 *   recorder.assertCallCount(1);
 *   assertEquals(recorder.getListCall()[0], 'hammy');
 * </pre>
 * @constructor
 */
function TestCallRecorder() {
  /** @private {!Array<!Arguments>} */
  this.calls_ = [];

  /**
   * The recording function. Bound in our constructor to ensure we always
   * return the same object. This is necessary as some clients may make use
   * of object equality.
   *
   * @type {function(*)}
   */
  this.callback = this.recordArguments_.bind(this);
}

/**
 * Records the magic {@code arguments} value for later inspection.
 * @private
 */
TestCallRecorder.prototype.recordArguments_ = function() {
  this.calls_.push(arguments);
};

/**
 * Asserts that the recorder was called {@code expected} times.
 * @param {number} expected The expected number of calls.
 */
TestCallRecorder.prototype.assertCallCount = function(expected) {
  var actual = this.calls_.length;
  assertEquals(
      expected, actual,
      'Expected ' + expected + ' call(s), but was ' + actual + '.');
};

/**
 * @return {?Arguments} Returns the {@code Arguments} for the last call,
 *    or null if the recorder hasn't been called.
 */
TestCallRecorder.prototype.getLastArguments = function() {
  return (this.calls_.length === 0) ? null :
                                      this.calls_[this.calls_.length - 1];
};

/**
 * @param {number} index Index of which args to return.
 * @return {?Arguments} Returns the {@code Arguments} for the call specified
 *    by indexd.
 */
TestCallRecorder.prototype.getArguments = function(index) {
  return (index < this.calls_.length) ? this.calls_[index] : null;
};

/**
 * Stubs the chrome.storage API.
 * @constructor
 * @struct
 */
function MockChromeStorageAPI() {
  /** @type {Object<?>} */
  this.state = {};

  window.chrome = window.chrome || {};
  /** @suppress {const} */
  window.chrome.runtime = window.chrome.runtime || {};  // For lastError.
  /** @suppress {checkTypes} */
  window.chrome.storage = {
    local: {
      get: this.get_.bind(this),
      set: this.set_.bind(this),
    }
  };
}

/**
 * @param {Array<string>|string} keys
 * @param {function(Object<?>)} callback
 * @private
 */
MockChromeStorageAPI.prototype.get_ = function(keys, callback) {
  var keys = keys instanceof Array ? keys : [keys];
  var result = {};
  keys.forEach((key) => {
    if (key in this.state)
      result[key] = this.state[key];
  });
  callback(result);
};

/**
 * @param {Object<?>} values
 * @param {function()=} opt_callback
 * @private
 */
MockChromeStorageAPI.prototype.set_ = function(values, opt_callback) {
  for (var key in values) {
    this.state[key] = values[key];
  }
  if (opt_callback)
    opt_callback();
};
