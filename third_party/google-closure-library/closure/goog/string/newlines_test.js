/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for goog.string. */

goog.module('goog.string.newlinesTest');
goog.setTestOnly();

const newlines = goog.require('goog.string.newlines');
const testSuite = goog.require('goog.testing.testSuite');

// test for goog.string.splitLines

testSuite({
  testSplitLines() {
    /**
     * @param {!Array<string>} expected
     * @param {string} string
     * @param {boolean=} opt_keepNewlines
     */
    function assertSplitLines(expected, string, opt_keepNewlines) {
      const keepNewlines = opt_keepNewlines || false;
      const lines = newlines.splitLines(string, keepNewlines);
      assertElementsEquals(expected, lines);
    }

    // Test values borrowed from Python's splitlines. http://goo.gl/iwawx
    assertSplitLines(['abc', 'def', '', 'ghi'], 'abc\ndef\n\rghi');
    assertSplitLines(['abc', 'def', '', 'ghi'], 'abc\ndef\n\r\nghi');
    assertSplitLines(['abc', 'def', 'ghi'], 'abc\ndef\r\nghi');
    assertSplitLines(['abc', 'def', 'ghi'], 'abc\ndef\r\nghi\n');
    assertSplitLines(['abc', 'def', 'ghi', ''], 'abc\ndef\r\nghi\n\r');
    assertSplitLines(['', 'abc', 'def', 'ghi', ''], '\nabc\ndef\r\nghi\n\r');
    assertSplitLines(['', 'abc', 'def', 'ghi', ''], '\nabc\ndef\r\nghi\n\r');
    assertSplitLines(
        ['\n', 'abc\n', 'def\r\n', 'ghi\n', '\r'], '\nabc\ndef\r\nghi\n\r',
        true);
    assertSplitLines(['', 'abc', 'def', 'ghi', ''], '\nabc\ndef\r\nghi\n\r');
    assertSplitLines(
        ['\n', 'abc\n', 'def\r\n', 'ghi\n', '\r'], '\nabc\ndef\r\nghi\n\r',
        true);
  },

  testGetLines() {
    const lines = newlines.getLines('abc\ndef\n\rghi');

    assertEquals(4, lines.length);

    assertEquals(0, lines[0].startLineIndex);
    assertEquals(3, lines[0].endContentIndex);
    assertEquals(4, lines[0].endLineIndex);
    assertEquals('abc', lines[0].getContent());
    assertEquals('abc\n', lines[0].getFullLine());
    assertEquals('\n', lines[0].getNewline());

    assertEquals(4, lines[1].startLineIndex);
    assertEquals(7, lines[1].endContentIndex);
    assertEquals(8, lines[1].endLineIndex);
    assertEquals('def', lines[1].getContent());
    assertEquals('def\n', lines[1].getFullLine());
    assertEquals('\n', lines[1].getNewline());

    assertEquals(8, lines[2].startLineIndex);
    assertEquals(8, lines[2].endContentIndex);
    assertEquals(9, lines[2].endLineIndex);
    assertEquals('', lines[2].getContent());
    assertEquals('\r', lines[2].getFullLine());
    assertEquals('\r', lines[2].getNewline());

    assertEquals(9, lines[3].startLineIndex);
    assertEquals(12, lines[3].endContentIndex);
    assertEquals(12, lines[3].endLineIndex);
    assertEquals('ghi', lines[3].getContent());
    assertEquals('ghi', lines[3].getFullLine());
    assertEquals('', lines[3].getNewline());
  },
});
