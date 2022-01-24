/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.formatTest');
goog.setTestOnly();

const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const format = goog.require('goog.format');
const googString = goog.require('goog.string');
const testSuite = goog.require('goog.testing.testSuite');

const propertyReplacer = new PropertyReplacer();

testSuite({
  tearDown() {
    // set wordBreakHtml back to the original value (some tests edit this
    // member).
    propertyReplacer.reset();
  },

  testFormatFileSize() {
    const fileSize = format.fileSize;

    assertEquals('45', fileSize(45));
    assertEquals('45', fileSize(45, 0));
    assertEquals('45', fileSize(45, 1));
    assertEquals('45', fileSize(45, 3));
    assertEquals('454', fileSize(454));
    assertEquals('600', fileSize(600));

    assertEquals('1K', fileSize(1024));
    assertEquals('2K', fileSize(2 * 1024));
    assertEquals('5K', fileSize(5 * 1024));
    assertEquals('5.123K', fileSize(5.12345 * 1024, 3));
    assertEquals('5.68K', fileSize(5.678 * 1024, 2));

    assertEquals('1M', fileSize(1024 * 1024));
    assertEquals('1.5M', fileSize(1.5 * 1024 * 1024));
    assertEquals('2M', fileSize(1.5 * 1024 * 1024, 0));
    assertEquals('1.5M', fileSize(1.51 * 1024 * 1024, 1));
    assertEquals('1.56M', fileSize(1.56 * 1024 * 1024, 2));

    assertEquals('1G', fileSize(1024 * 1024 * 1024));
    assertEquals('6G', fileSize(6 * 1024 * 1024 * 1024));
    assertEquals('12.06T', fileSize(12345.6789 * 1024 * 1024 * 1024));
  },

  testIsConvertableScaledNumber() {
    const isConvertableScaledNumber = format.isConvertableScaledNumber;

    assertTrue(isConvertableScaledNumber('0'));
    assertTrue(isConvertableScaledNumber('45'));
    assertTrue(isConvertableScaledNumber('45K'));
    assertTrue(isConvertableScaledNumber('45MB'));
    assertTrue(isConvertableScaledNumber('45GB'));
    assertTrue(isConvertableScaledNumber('45T'));
    assertTrue(isConvertableScaledNumber('2.33P'));
    assertTrue(isConvertableScaledNumber('3.45E'));
    assertTrue(isConvertableScaledNumber('5.33Z'));
    assertTrue(isConvertableScaledNumber('7.22Y'));
    assertTrue(isConvertableScaledNumber('45m'));
    assertTrue(isConvertableScaledNumber('45u'));
    assertTrue(isConvertableScaledNumber('-5.0n'));

    assertFalse(isConvertableScaledNumber('45x'));
    assertFalse(isConvertableScaledNumber('ux'));
    assertFalse(isConvertableScaledNumber('K'));
  },

  testNumericValueToString() {
    const numericValueToString = format.numericValueToString;

    assertEquals('Infinity', numericValueToString(Infinity));
    assertEquals('Infinity', numericValueToString(1.8e+309));
    assertEquals('-Infinity', numericValueToString(-Infinity));
    assertEquals('-Infinity', numericValueToString(-1.8e309));

    assertEquals('0', numericValueToString(0.0));
    assertEquals('45', numericValueToString(45));
    assertEquals('454', numericValueToString(454));
    assertEquals('600', numericValueToString(600));

    assertEquals('1.02K', numericValueToString(1024));
    assertEquals('2.05K', numericValueToString(2 * 1024));
    assertEquals('5.12K', numericValueToString(5 * 1024));
    assertEquals('5.246K', numericValueToString(5.12345 * 1024, 3));
    assertEquals('5.81K', numericValueToString(5.678 * 1024, 2));

    assertEquals('1.05M', numericValueToString(1024 * 1024));
    assertEquals('1.57M', numericValueToString(1.5 * 1024 * 1024));
    assertEquals('2M', numericValueToString(1.5 * 1024 * 1024, 0));
    assertEquals('1.6M', numericValueToString(1.51 * 1024 * 1024, 1));
    assertEquals('1.64M', numericValueToString(1.56 * 1024 * 1024, 2));

    assertEquals('1.07G', numericValueToString(1024 * 1024 * 1024));
    assertEquals('6.44G', numericValueToString(6 * 1024 * 1024 * 1024));
    assertEquals(
        '13.26T', numericValueToString(12345.6789 * 1024 * 1024 * 1024));
    assertEquals('50.67P', numericValueToString(45 * Math.pow(1024, 5)));
    assertEquals('57.65E', numericValueToString(50 * Math.pow(1024, 6)));
    assertEquals('77.33Z', numericValueToString(65.5 * Math.pow(1024, 7)));
    assertEquals('97.56Y', numericValueToString(80.7 * Math.pow(1024, 8)));

    assertEquals('23.4m', numericValueToString(0.0234));
    assertEquals('1.23u', numericValueToString(0.00000123));
    assertEquals('15.78n', numericValueToString(0.000000015784));
    assertEquals('0.58u', numericValueToString(0.0000005784));
    assertEquals('0.5', numericValueToString(0.5));

    assertEquals('-45', numericValueToString(-45.3, 0));
    assertEquals('-45', numericValueToString(-45.5, 0));
    assertEquals('-46', numericValueToString(-45.51, 0));

    assertEquals('300K', numericValueToString(3e5));
    assertEquals('700K', numericValueToString(7E+5));
    assertEquals('30u', numericValueToString(3e-5));
  },

  testFormatNumBytes() {
    const numBytesToString = format.numBytesToString;

    assertEquals('45', numBytesToString(45));
    assertEquals('454', numBytesToString(454));

    assertEquals('5KB', numBytesToString(5 * 1024));
    assertEquals('1MB', numBytesToString(1024 * 1024));
    assertEquals('6GB', numBytesToString(6 * 1024 * 1024 * 1024));
    assertEquals('12.06TB', numBytesToString(12345.6789 * 1024 * 1024 * 1024));
    assertEquals('45PB', numBytesToString(45 * Math.pow(1024, 5)));
    assertEquals('50EB', numBytesToString(50 * Math.pow(1024, 6)));
    assertEquals('65.5ZB', numBytesToString(65.5 * Math.pow(1024, 7)));
    assertEquals('80.7YB', numBytesToString(80.7 * Math.pow(1024, 8)));

    assertEquals('454', numBytesToString(454, 2, true, true));
    assertEquals('5 KB', numBytesToString(5 * 1024, 2, true, true));
  },

  testStringToNumeric() {
    const stringToNumericValue = format.stringToNumericValue;
    const epsilon = Math.pow(10, -10);

    assertNaN(stringToNumericValue('foo'));
    assertNaN(stringToNumericValue('3E5E6'));

    assertEquals(Infinity, stringToNumericValue('Infinity'));
    assertEquals(Infinity, stringToNumericValue('2E+1000'));
    assertEquals(-Infinity, stringToNumericValue('-Infinity'));
    assertEquals(-Infinity, stringToNumericValue('-4E+1000'));

    assertEquals(45, stringToNumericValue('45'));
    assertEquals(-45, stringToNumericValue('-45'));
    assertEquals(-45, stringToNumericValue('-45'));
    assertEquals(454, stringToNumericValue('454'));

    assertEquals(5 * 1024, stringToNumericValue('5KB'));
    assertEquals(1024 * 1024, stringToNumericValue('1MB'));
    assertEquals(6 * 1024 * 1024 * 1024, stringToNumericValue('6GB'));
    assertEquals(13260110230978.56, stringToNumericValue('12.06TB'));

    assertEquals(5010, stringToNumericValue('5.01K'));
    assertEquals(5100000, stringToNumericValue('5.1M'));
    assertTrue(Math.abs(0.051 - stringToNumericValue('51.0m')) < epsilon);
    assertTrue(Math.abs(0.000051 - stringToNumericValue('51.0u')) < epsilon);

    assertEquals(0.00003, stringToNumericValue('3e-5'));
    assertEquals(300000, stringToNumericValue('3e5'));
    assertEquals(700000, stringToNumericValue('7E+5'));
  },

  testStringToNumBytes() {
    const stringToNumBytes = format.stringToNumBytes;
    const epsilon = 0.1;

    assertEquals(45, stringToNumBytes('45'));
    assertEquals(454, stringToNumBytes('454'));

    assertEquals(5 * 1024, stringToNumBytes('5K'));
    assertEquals(1024 * 1024, stringToNumBytes('1M'));
    assertEquals(6 * 1024 * 1024 * 1024, stringToNumBytes('6G'));
    assertEquals(13260110230978.56, stringToNumBytes('12.06T'));
    assertTrue(
        Math.abs(3.191564163782621e+24 - stringToNumBytes('2.64Y')) < epsilon);
  },

  testInsertWordBreaks() {
    // HTML that gets inserted is browser dependent, ensure for the test it is
    // a constant - browser dependent HTML is for display purposes only.
    propertyReplacer.set(format, 'WORD_BREAK_HTML', '<wbr>');

    const insertWordBreaks = format.insertWordBreaks;

    assertEquals('abcdef', insertWordBreaks('abcdef', 10));
    assertEquals('ab<wbr>cd<wbr>ef', insertWordBreaks('abcdef', 2));
    assertEquals(
        'a<wbr>b<wbr>c<wbr>d<wbr>e<wbr>f', insertWordBreaks('abcdef', 1));

    assertEquals(
        'a&amp;b=<wbr>=fal<wbr>se', insertWordBreaks('a&amp;b==false', 4));
    assertEquals(
        '&lt;&amp;&gt;&raquo;<wbr>&laquo;',
        insertWordBreaks('&lt;&amp;&gt;&raquo;&laquo;', 4));

    assertEquals('a<wbr>b<wbr>c d<wbr>e<wbr>f', insertWordBreaks('abc def', 1));
    assertEquals('ab<wbr>c de<wbr>f', insertWordBreaks('abc def', 2));
    assertEquals('abc def', insertWordBreaks('abc def', 3));
    assertEquals('abc def', insertWordBreaks('abc def', 4));

    assertEquals('a<b>cd</b>e<wbr>f', insertWordBreaks('a<b>cd</b>ef', 4));
    assertEquals(
        'Thi<wbr>s is a <a href="">lin<wbr>k</a>.',
        insertWordBreaks('This is a <a href="">link</a>.', 3));
    assertEquals(
        '<abc a="&amp;&amp;&amp;&amp;&amp;">a<wbr>b',
        insertWordBreaks('<abc a="&amp;&amp;&amp;&amp;&amp;">ab', 1));

    assertEquals('ab\u0300<wbr>cd', insertWordBreaks('ab\u0300cd', 2));
    assertEquals('ab\u036F<wbr>cd', insertWordBreaks('ab\u036Fcd', 2));
    assertEquals('ab<wbr>\u0370c<wbr>d', insertWordBreaks('ab\u0370cd', 2));
    assertEquals('ab<wbr>\uFE1Fc<wbr>d', insertWordBreaks('ab\uFE1Fcd', 2));
    assertEquals(
        'ab\u0300<wbr>c\u0301<wbr>de<wbr>f',
        insertWordBreaks('ab\u0300c\u0301def', 2));
  },

  testInsertWordBreaksWithFormattingCharacters() {
    // HTML that gets inserted is browser dependent, ensure for the test it is
    // a constant - browser dependent HTML is for display purposes only.
    propertyReplacer.set(format, 'WORD_BREAK_HTML', '<wbr>');
    const insertWordBreaks = format.insertWordBreaks;

    // A date in Arabic-Indic digits with Right-to-Left Marks (U+200F).
    // The date is "11<RLM>/01<RLM>/2012".
    const textWithRLMs = 'This is a date - ' +
        '\u0661\u0661\u200f/\u0660\u0661\u200f/\u0662\u0660\u0661\u0662';
    // A string of 10 Xs with invisible formatting characters in between.
    // These characters are in the ranges U+200C to U+200F and U+202A to
    // U+202E, inclusive. See: http://unicode.org/charts/PDF/U2000.pdf
    const stringWithInvisibleFormatting =
        'X\u200cX\u200dX\u200eX\u200fX\u202a' +
        'X\u202bX\u202cX\u202dX\u202eX';
    // A string formed by concatenating copies of the previous string
    // alternating with characters which behave like breaking spaces. Besides
    // the space character itself, the other characters are in the range U+2000
    // to U+200B inclusive, except for the exclusion of U+2007 and inclusion of
    // U+2029. See: http://unicode.org/charts/PDF/U2000.pdf
    const stringWithInvisibleFormattingAndSpacelikeCharacters =
        `${stringWithInvisibleFormatting} ${stringWithInvisibleFormatting}` +
        '\u2000' + stringWithInvisibleFormatting + '\u2001' +
        stringWithInvisibleFormatting + '\u2002' +
        stringWithInvisibleFormatting + '\u2003' +
        stringWithInvisibleFormatting + '\u2005' +
        stringWithInvisibleFormatting + '\u2006' +
        stringWithInvisibleFormatting + '\u2008' +
        stringWithInvisibleFormatting + '\u2009' +
        stringWithInvisibleFormatting + '\u200A' +
        stringWithInvisibleFormatting + '\u200B' +
        stringWithInvisibleFormatting + '\u2029' +
        stringWithInvisibleFormatting;

    // Test that the word break algorithm does not count RLMs towards word
    // length, and therefore does not insert word breaks into a typical date
    // written in Arabic-Indic digits with RTMs (b/5853915).
    assertEquals(textWithRLMs, insertWordBreaks(textWithRLMs, 10));

    // Test that invisible formatting characters are not counted towards word
    // length, and that characters which are treated as breaking spaces behave
    // as breaking spaces.
    assertEquals(
        stringWithInvisibleFormattingAndSpacelikeCharacters,
        insertWordBreaks(
            stringWithInvisibleFormattingAndSpacelikeCharacters, 10));
  },

  testInsertWordBreaksBasic() {
    // HTML that gets inserted is browser dependent, ensure for the test it is
    // a constant - browser dependent HTML is for display purposes only.
    propertyReplacer.set(format, 'WORD_BREAK_HTML', '<wbr>');
    const insertWordBreaksBasic = format.insertWordBreaksBasic;

    assertEquals('abcdef', insertWordBreaksBasic('abcdef', 10));
    assertEquals('ab<wbr>cd<wbr>ef', insertWordBreaksBasic('abcdef', 2));
    assertEquals(
        'a<wbr>b<wbr>c<wbr>d<wbr>e<wbr>f', insertWordBreaksBasic('abcdef', 1));
    assertEquals(
        'ab\u0300<wbr>c\u0301<wbr>de<wbr>f',
        insertWordBreaksBasic('ab\u0300c\u0301def', 2));

    assertEquals(
        'Inserting word breaks into the word "Russia" should work fine.',
        '\u0420\u043E<wbr>\u0441\u0441<wbr>\u0438\u044F',
        insertWordBreaksBasic('\u0420\u043E\u0441\u0441\u0438\u044F', 2));

    // The word 'Internet' in Hindi.
    const hindiInternet = '\u0907\u0902\u091F\u0930\u0928\u0947\u091F';
    assertEquals(
        'The basic algorithm is not good enough to insert word ' +
            'breaks into Hindi.',
        hindiInternet, insertWordBreaksBasic(hindiInternet, 2));
    // The word 'Internet' in Hindi broken into slashes.
    assertEquals(
        'Hindi can have word breaks inserted between slashes',
        `${hindiInternet}<wbr>/${hindiInternet}<wbr>.${hindiInternet}`,
        insertWordBreaksBasic(
            `${hindiInternet}/${hindiInternet}.${hindiInternet}`, 2));
  },

  testWordBreaksWorking() {
    const text = googString.repeat('test', 20);
    const textWbr = googString.repeat('test' + format.WORD_BREAK_HTML, 20);

    const overflowEl = dom.createDom(
        TagName.DIV, {'style': 'width: 100px; overflow: hidden; margin 5px'});
    const wbrEl = dom.createDom(
        TagName.DIV,
        {'style': 'width: 100px; overflow: hidden; margin-top: 15px'});
    dom.appendChild(globalThis.document.body, overflowEl);
    dom.appendChild(globalThis.document.body, wbrEl);

    overflowEl.innerHTML = text;
    wbrEl.innerHTML = textWbr;

    assertTrue('Text should overflow', overflowEl.scrollWidth > 100);
    assertTrue('Text should not overflow', wbrEl.scrollWidth <= 100);
  },

  testWordBreaksRemovedFromTextContent() {
    const expectedText = googString.repeat('test', 20);
    const textWbr = googString.repeat('test' + format.WORD_BREAK_HTML, 20);

    const wbrEl = dom.createDom(TagName.DIV, null);
    wbrEl.innerHTML = textWbr;

    assertEquals(
        'text content should have wbr character removed', expectedText,
        dom.getTextContent(wbrEl));
  },
});
