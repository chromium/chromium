/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.labs.format.csvTest');
goog.setTestOnly();

const ParseError = goog.require('goog.labs.format.csv.ParseError');
const asserts = goog.require('goog.testing.asserts');
const csv = goog.require('goog.labs.format.csv');
const googObject = goog.require('goog.object');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testGoldenPath() {
    assertObjectEquals(
        [['a', 'b', 'c'], ['d', 'e', 'f'], ['g', 'h', 'i']],
        csv.parse('a,b,c\nd,e,f\ng,h,i\n'));
    assertObjectEquals(
        [['a', 'b', 'c'], ['d', 'e', 'f'], ['g', 'h', 'i']],
        csv.parse('a,b,c\r\nd,e,f\r\ng,h,i\r\n'));
  },

  testGoldenPathWithSpaceAsCustomDelimiter() {
    assertObjectEquals(
        [['a', 'b', 'c'], ['d', 'e', 'f'], ['g', 'h', 'i']],
        csv.parse('a b c\nd e f\ng h i\n', undefined, ' '));
    assertObjectEquals(
        [['a', 'b', 'c'], ['d', 'e', 'f'], ['g', 'h', 'i']],
        csv.parse('a b c\r\nd e f\r\ng h i\r\n', undefined, ' '));
  },

  testGoldenPathWithEmptyStringAsCustomDelimiterUsesComma() {
    assertObjectEquals(
        [['a', 'b', 'c'], ['d', 'e', 'f'], ['g', 'h', 'i']],
        csv.parse('a,b,c\nd,e,f\ng,h,i\n', undefined, ''));
    assertObjectEquals(
        [['a', 'b', 'c'], ['d', 'e', 'f'], ['g', 'h', 'i']],
        csv.parse('a,b,c\r\nd,e,f\r\ng,h,i\r\n', undefined, ''));
  },

  testBadDelimitersNewLine() {
    const e = assertThrows(() => {
      csv.parse('a,b,c\r\nd,e,f\r\ng,h,i\r\n', undefined, '\n');
    });
    assertEquals(
        'Assertion failed: Cannot use newline or carriage return as delimiter.',
        e.message);
  },

  testBadDelimitersCarriageReturn() {
    const e = assertThrows(() => {
      csv.parse('a,b,c\r\nd,e,f\r\ng,h,i\r\n', undefined, '\r');
    });
    assertEquals(
        'Assertion failed: Cannot use newline or carriage return as delimiter.',
        e.message);
  },

  testBadDelimitersTwoCharacter() {
    const e = assertThrows(() => {
      csv.parse('a,b,c\r\nd,e,f\r\ng,h,i\r\n', undefined, 'aa');
    });
    assertEquals(
        'Assertion failed: Delimiter must be a single character.', e.message);
  },

  testNoCrlfAtEnd() {
    assertObjectEquals(
        [['a', 'b', 'c'], ['d', 'e', 'f'], ['g', 'h', 'i']],
        csv.parse('a,b,c\nd,e,f\ng,h,i'));
  },

  testQuotes() {
    assertObjectEquals(
        [['a', 'b', 'c'], ['d', 'e', 'f'], ['g', 'h', 'i']],
        csv.parse('a,"b",c\n"d","e","f"\ng,h,"i"'));
    assertObjectEquals(
        [['a', 'b, as in boy', 'c'], ['d', 'e', 'f']],
        csv.parse('a,"b, as in boy",c\n"d","e","f"\n'));
    // Make sure empty elements after quoted elements are parsed.
    assertObjectEquals(
        [['a', 'b, as in boy', ''], ['d', 'e', 'f']],
        csv.parse('a,"b, as in boy",\nd,e,f\n'));
  },

  testEmbeddedCrlfs() {
    assertObjectEquals(
        [['a', 'b\nball', 'c'], ['d\nd', 'e', 'f'], ['g', 'h', 'i']],
        csv.parse('a,"b\nball",c\n"d\nd","e","f"\ng,h,"i"\n'));
  },

  testEmbeddedQuotes() {
    assertObjectEquals(
        [['a', '"b"', 'Jonathan "Smokey" Feinberg'], ['d', 'e', 'f']],
        csv.parse('a,"""b""","Jonathan ""Smokey"" Feinberg"\nd,e,f\r\n'));
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testUnclosedQuote() {
    const e = assertThrows(() => {
      csv.parse('a,"b,c\nd,e,f');
    });

    assertTrue(e instanceof ParseError);
    assertEquals(2, e.position.line);
    assertEquals(5, e.position.column);
    assertEquals(
        'Unexpected end of text after open quote at line 2 column 5\n' +
            'd,e,f\n' +
            '    ^',
        e.message);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testQuotesInUnquotedField() {
    const e = assertThrows(() => {
      csv.parse('a,b "and" b,c\nd,e,f');
    });

    assertTrue(e instanceof ParseError);

    assertEquals(1, e.position.line);
    assertEquals(5, e.position.column);

    assertEquals(
        'Unexpected quote mark at line 1 column 5\n' +
            'a,b "and" b,c\n' +
            '    ^',
        e.message);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testGarbageOutsideQuotes() {
    const e = assertThrows(() => {
      csv.parse('a,"b",c\nd,"e"oops,f');
    });

    assertTrue(e instanceof ParseError);
    assertEquals(2, e.position.line);
    assertEquals(6, e.position.column);
    assertEquals(
        'Unexpected character "o" after quote mark at line 2 column 6\n' +
            'd,"e"oops,f\n' +
            '     ^',
        e.message);
  },

  testEmptyRecords() {
    assertObjectEquals(
        [['a', '', 'c'], ['d', 'e', ''], ['', '', '']],
        csv.parse('a,,c\r\nd,e,\n,,'));
  },

  testEmptyRecordsWithColonCustomDelimiter() {
    assertObjectEquals(
        [['a', '', 'c'], ['d', 'e', ''], ['', '', '']],
        csv.parse('a::c\r\nd:e:\n::', undefined, ':'));
  },

  testIgnoringErrors() {
    // The results of these tests are not defined by the RFC. They
    // generally strive to be "reasonable" while keeping the code simple.

    // Quotes inside field
    assertObjectEquals(
        [['Hello "World"!', 'b'], ['c', 'd']],
        csv.parse('Hello "World"!,b\nc,d', true));

    // Missing closing quote
    assertObjectEquals([['Hello', 'World!']], csv.parse('Hello,"World!', true));

    // Broken use of quotes in quoted field
    assertObjectEquals(
        [['a', '"Hello"World!"']], csv.parse('a,"Hello"World!"', true));

    // All of the above. A real mess.
    assertObjectEquals(
        [['This" is', '"very\n\tvery"broken"', ' indeed!']],
        csv.parse('This" is,"very\n\tvery"broken"," indeed!', true));
  },

  testIgnoringErrorsTrailingTabs() {
    assertObjectEquals(
        [['"a\tb"\t'], ['c,d']], csv.parse('"a\tb"\t\n"c,d"', true));
  },

  testFindLineInfo() {
    const testString = 'abc\ndef\rghi';
    /** @suppress {visibility} suppression added to enable type checking */
    const info = ParseError.findLineInfo_(testString, 4);

    assertEquals(4, info.line.startLineIndex);
    assertEquals(7, info.line.endContentIndex);
    assertEquals(8, info.line.endLineIndex);

    assertEquals(1, info.lineIndex);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetLineDebugString() {
    const str = 'abcdefghijklmnop';
    const index = str.indexOf('j');
    const column = index + 1;
    assertEquals(
        ParseError.getLineDebugString_(str, column),
        'abcdefghijklmnop\n' +
            '         ^');
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testIsCharacterString() {
    assertTrue(csv.isCharacterString_('a'));
    assertTrue(csv.isCharacterString_('\n'));
    assertTrue(csv.isCharacterString_(' '));

    assertFalse(csv.isCharacterString_(null));
    assertFalse(csv.isCharacterString_('  '));
    assertFalse(csv.isCharacterString_(''));
    assertFalse(csv.isCharacterString_('aa'));
  },

  /**
     @suppress {visibility,missingProperties} suppression added to enable type
     checking
   */
  testAssertToken() {
    csv.assertToken_('a');

    googObject.forEach(
        csv.SENTINELS_, /**
                           @suppress {visibility} suppression added to enable
                           type checking
                         */
        (value) => {
          csv.assertToken_(value);
        });

    assertThrows(/**
                    @suppress {visibility} suppression added to enable type
                    checking
                  */
                 () => {
                   csv.assertToken_('aa');
                 });

    assertThrows(/**
                    @suppress {visibility} suppression added to enable type
                    checking
                  */
                 () => {
                   csv.assertToken_('');
                 });

    assertThrows(/**
                    @suppress {visibility} suppression added to enable type
                    checking
                  */
                 () => {
                   csv.assertToken_({});
                 });
  },
});
