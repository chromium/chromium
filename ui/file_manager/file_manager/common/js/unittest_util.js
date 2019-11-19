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
  const entryToPath = entry => {
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

  const entryToPath = entry => {
    assertTrue(entry.isFile);
    return entry.fullPath;
  };

  const actualPaths = fileEntries.map(entryToPath);
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
 */
class TestCallRecorder {
  constructor() {
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
  recordArguments_() {
    this.calls_.push(arguments);
  }

  /**
   * Asserts that the recorder was called {@code expected} times.
   * @param {number} expected The expected number of calls.
   */
  assertCallCount(expected) {
    const actual = this.calls_.length;
    assertEquals(
        expected, actual,
        'Expected ' + expected + ' call(s), but was ' + actual + '.');
  }

  /**
   * @return {?Arguments} Returns the {@code Arguments} for the last call,
   *    or null if the recorder hasn't been called.
   */
  getLastArguments() {
    return (this.calls_.length === 0) ? null :
                                        this.calls_[this.calls_.length - 1];
  }

  /**
   * @param {number} index Index of which args to return.
   * @return {?Arguments} Returns the {@code Arguments} for the call specified
   *    by indexd.
   */
  getArguments(index) {
    return (index < this.calls_.length) ? this.calls_[index] : null;
  }
}
