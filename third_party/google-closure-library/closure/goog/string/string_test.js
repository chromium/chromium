/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Unit tests for googString.
 */

/** @suppress {extraProvide} */
goog.module('goog.stringTest');
goog.setTestOnly();

const MockControl = goog.require('goog.testing.MockControl');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TagName = goog.require('goog.dom.TagName');
const Unicode = goog.require('goog.string.Unicode');
const dom = goog.require('goog.dom');
const functions = goog.require('goog.functions');
const googObject = goog.require('goog.object');
const googString = goog.require('goog.string');
const testSuite = goog.require('goog.testing.testSuite');

let stubs;
let mockControl;

//=== tests for goog.string.collapseWhitespace ===

//=== tests for goog.string.isAlpha ===

//=== tests for goog.string.isNumeric ===

//=== tests for tests for goog.string.isAlphaNumeric ===

//== tests for goog.string.isBreakingWhitespace ===

//=== tests for goog.string.isSpace ===

// === tests for goog.string.stripNewlines ===

// === tests for goog.string.canonicalizeNewlines ===

// === tests for goog.string.normalizeWhitespace ===

// === tests for goog.string.normalizeSpaces ===

/// === tests for goog.string.trim ===

/// === tests for goog.string.trimLeft ===

/// === tests for goog.string.trimRight ===

// === tests for goog.string.startsWith ===

// === tests for goog.string.caseInsensitiveStartsWith ===

// === tests for goog.string.caseInsensitiveEndsWith ===

// === tests for goog.string.caseInsensitiveEquals ===

// === tests for goog.string.subs ===

// === tests for goog.string.caseInsensitiveCompare ===

/**
 * Test cases for googString.floatAwareCompare and googString.intAwareCompare.
 * Each comparison in this list is tested to assure that terms[0] < terms[1],
 * terms[1] > terms[0], and identity tests terms[0] == terms[0] and
 * terms[1] == terms[1].
 * @const {!Array<!Array<string>>}
 */
const NUMERIC_COMPARISON_TEST_CASES = [
  ['', '0'],
  ['2', '10'],
  ['05', '9'],
  ['sub', 'substring'],
  ['photo 7', 'Photo 8'],  // Case insensitive for most sorts.
  ['Mango', 'mango'],      // Case sensitive if strings are otherwise identical.
  ['album 2 photo 20', 'album 10 photo 20'],
  ['album 7 photo 20', 'album 7 photo 100'],
];

// === tests for goog.string.urlEncode && .urlDecode ===
// NOTE: When test was written it was simply an alias for the built in
// 'encodeURICompoent', therefore this test is simply used to make sure that in
// the future it doesn't get broken.

// === tests for goog.string.newLineToBr ===

// === tests for goog.string.htmlEscape and .unescapeEntities ===

const globalXssVar = 0;

// === tests for goog.string.whitespaceEscape ===

// === tests for goog.string.preserveSpaces ===

// === tests for goog.string.stripQuotes ===

// === tests for goog.string.truncate ===

// === tests for goog.string.truncateMiddle ===

// === goog.string.quote ===

function allChars(start = undefined, end = undefined) {
  start = start || 0;
  end = end || 256;
  let rv = '';
  for (let i = start; i < end; i++) {
    rv += String.fromCharCode(i);
  }
  return rv;
}

function assertHashcodeEquals(expectedHashCode, str) {
  assertEquals(
      'wrong hashCode for ' + str.substring(0, 32), expectedHashCode,
      googString.hashCode(str));
}

testSuite({
  setUp() {
    stubs = new PropertyReplacer();
    mockControl = new MockControl();
  },

  tearDown() {
    stubs.reset();
    mockControl.$tearDown();
  },

  testCollapseWhiteSpace() {
    const f = googString.collapseWhitespace;

    assertEquals('Leading spaces not stripped', f('  abc'), 'abc');
    assertEquals('Trailing spaces not stripped', f('abc  '), 'abc');
    assertEquals('Wrapping spaces not stripped', f('  abc  '), 'abc');

    assertEquals(
        'All white space chars not stripped', f('\xa0\n\t abc\xa0\n\t '),
        'abc');

    assertEquals('Spaces not collapsed', f('a   b    c'), 'a b c');

    assertEquals('Tabs not collapsed', f('a\t\t\tb\tc'), 'a b c');

    assertEquals(
        'All check failed', f(' \ta \t \t\tb\t\n\xa0  c  \t\n'), 'a b c');
  },

  testIsEmpty() {
    assertTrue(googString.isEmpty(''));
    assertTrue(googString.isEmpty(' '));
    assertTrue(googString.isEmpty('    '));
    assertTrue(googString.isEmpty(' \t\t\n\xa0   '));

    assertFalse(googString.isEmpty(' abc \t\xa0'));
    assertFalse(googString.isEmpty(' a b c \t'));
    assertFalse(googString.isEmpty(';'));

    assertFalse(googString.isEmpty(/** @type {?} */ (undefined)));
    assertFalse(googString.isEmpty(/** @type {?} */ (null)));
    assertFalse(googString.isEmpty(/** @type {?} */ ({a: 1, b: 2})));
  },

  testIsEmptyOrWhitespace() {
    assertTrue(googString.isEmptyOrWhitespace(''));
    assertTrue(googString.isEmptyOrWhitespace(' '));
    assertTrue(googString.isEmptyOrWhitespace('    '));
    assertTrue(googString.isEmptyOrWhitespace(' \t\t\n\xa0   '));

    assertFalse(googString.isEmptyOrWhitespace(' abc \t\xa0'));
    assertFalse(googString.isEmptyOrWhitespace(' a b c \t'));
    assertFalse(googString.isEmptyOrWhitespace(';'));

    assertFalse(googString.isEmptyOrWhitespace(/** @type {?} */ (undefined)));
    assertFalse(googString.isEmptyOrWhitespace(/** @type {?} */ (null)));
    assertFalse(
        googString.isEmptyOrWhitespace(/** @type {?} */ ({a: 1, b: 2})));
  },

  testIsEmptyString() {
    assertTrue(googString.isEmptyString(''));

    assertFalse(googString.isEmptyString(' '));
    assertFalse(googString.isEmptyString('    '));
    assertFalse(googString.isEmptyString(' \t\t\n\xa0   '));
    assertFalse(googString.isEmptyString(' abc \t\xa0'));
    assertFalse(googString.isEmptyString(' a b c \t'));
    assertFalse(googString.isEmptyString(';'));

    assertFalse(googString.isEmptyString(/** @type {?} */ ({a: 1, b: 2})));
  },

  testIsEmptySafe() {
    assertTrue(googString.isEmptySafe(''));
    assertTrue(googString.isEmptySafe(' '));
    assertTrue(googString.isEmptySafe('    '));
    assertTrue(googString.isEmptySafe(' \t\t\n\xa0   '));

    assertFalse(googString.isEmptySafe(' abc \t\xa0'));
    assertFalse(googString.isEmptySafe(' a b c \t'));
    assertFalse(googString.isEmptySafe(';'));

    assertTrue(googString.isEmptySafe(undefined));
    assertTrue(googString.isEmptySafe(null));
    assertFalse(googString.isEmptySafe({a: 1, b: 2}));
  },

  testIsEmptyOrWhitespaceSafe() {
    assertTrue(googString.isEmptyOrWhitespaceSafe(''));
    assertTrue(googString.isEmptyOrWhitespaceSafe(' '));
    assertTrue(googString.isEmptyOrWhitespaceSafe('    '));
    assertTrue(googString.isEmptyOrWhitespaceSafe(' \t\t\n\xa0   '));

    assertFalse(googString.isEmptyOrWhitespaceSafe(' abc \t\xa0'));
    assertFalse(googString.isEmptyOrWhitespaceSafe(' a b c \t'));
    assertFalse(googString.isEmptyOrWhitespaceSafe(';'));

    assertTrue(googString.isEmptyOrWhitespaceSafe(undefined));
    assertTrue(googString.isEmptyOrWhitespaceSafe(null));
    assertFalse(googString.isEmptyOrWhitespaceSafe({a: 1, b: 2}));
  },

  testIsAlpha() {
    assertTrue('"a" should be alpha', googString.isAlpha('a'));
    assertTrue('"n" should be alpha', googString.isAlpha('n'));
    assertTrue('"z" should be alpha', googString.isAlpha('z'));
    assertTrue('"A" should be alpha', googString.isAlpha('A'));
    assertTrue('"N" should be alpha', googString.isAlpha('N'));
    assertTrue('"Z" should be alpha', googString.isAlpha('Z'));
    assertTrue('"aa" should be alpha', googString.isAlpha('aa'));
    assertTrue('null is alpha', googString.isAlpha(/** @type {?} */ (null)));
    assertTrue(
        'undefined is alpha', googString.isAlpha(/** @type {?} */ (undefined)));

    assertFalse('"aa!" is not alpha', googString.isAlpha('aa!s'));
    assertFalse('"!" is not alpha', googString.isAlpha('!'));
    assertFalse('"0" is not alpha', googString.isAlpha('0'));
    assertFalse('"5" is not alpha', googString.isAlpha('5'));
  },

  testIsNumeric() {
    assertTrue('"8" is a numeric string', googString.isNumeric('8'));
    assertTrue('"5" is a numeric string', googString.isNumeric('5'));
    assertTrue('"34" is a numeric string', googString.isNumeric('34'));
    assertTrue('34 is a number', googString.isNumeric(34));

    assertFalse('"3.14" has a period', googString.isNumeric('3.14'));
    assertFalse('"A" is a letter', googString.isNumeric('A'));
    assertFalse('"!" is punctuation', googString.isNumeric('!'));
    assertFalse('null is not numeric', googString.isNumeric(null));
    assertFalse('undefined is not numeric', googString.isNumeric(undefined));
  },

  testIsAlphaNumeric() {
    assertTrue(
        '"ABCabc" should be alphanumeric', googString.isAlphaNumeric('ABCabc'));
    assertTrue(
        '"123" should be alphanumeric', googString.isAlphaNumeric('123'));
    assertTrue(
        '"ABCabc123" should be alphanumeric',
        googString.isAlphaNumeric('ABCabc123'));
    assertTrue(
        'null is alphanumeric',
        googString.isAlphaNumeric(/** @type {?} */ (null)));
    assertTrue(
        'undefined is alphanumeric',
        googString.isAlphaNumeric(/** @type {?} */ (undefined)));

    assertFalse(
        '"123!" should not be alphanumeric', googString.isAlphaNumeric('123!'));
    assertFalse(
        '"  " should not be alphanumeric', googString.isAlphaNumeric('  '));
  },

  testIsBreakingWhitespace() {
    assertTrue('" " is breaking', googString.isBreakingWhitespace(' '));
    assertTrue('"\\n" is breaking', googString.isBreakingWhitespace('\n'));
    assertTrue('"\\t" is breaking', googString.isBreakingWhitespace('\t'));
    assertTrue('"\\r" is breaking', googString.isBreakingWhitespace('\r'));
    assertTrue(
        '"\\r\\n\\t " is breaking', googString.isBreakingWhitespace('\r\n\t '));

    assertFalse(
        'nbsp is non-breaking', googString.isBreakingWhitespace('\xa0'));
    assertFalse('"a" is non-breaking', googString.isBreakingWhitespace('a'));
    assertFalse(
        '"a\\r" is non-breaking', googString.isBreakingWhitespace('a\r'));
  },

  testIsSpace() {
    assertTrue('" " is a space', googString.isSpace(' '));

    assertFalse('"\\n" is not a space', googString.isSpace('\n'));
    assertFalse('"\\t" is not a space', googString.isSpace('\t'));
    assertFalse(
        '"  " is not a space, it\'s two spaces', googString.isSpace('  '));
    assertFalse('"a" is not a space', googString.isSpace('a'));
    assertFalse('"3" is not a space', googString.isSpace('3'));
    assertFalse('"#" is not a space', googString.isSpace('#'));
    assertFalse(
        'null is not a space', googString.isSpace(/** @type {?} */ (null)));
    assertFalse('nbsp is not a space', googString.isSpace('\xa0'));
  },

  testStripNewLines() {
    assertEquals(
        'Should replace new lines with spaces',
        googString.stripNewlines('some\nlines\rthat\r\nare\n\nsplit'),
        'some lines that are split');
  },

  testCanonicalizeNewlines() {
    assertEquals(
        'Should replace all types of new line with \\n',
        googString.canonicalizeNewlines('some\nlines\rthat\r\nare\n\nsplit'),
        'some\nlines\nthat\nare\n\nsplit');
  },

  testNormalizeWhitespace() {
    assertEquals(
        'All whitespace chars should be replaced with a normal space',
        googString.normalizeWhitespace('\xa0 \n\t \xa0 \n\t'), '         ');
  },

  testNormalizeSpaces() {
    assertEquals(
        'All whitespace chars should be replaced with a normal space',
        googString.normalizeSpaces('\xa0 \t \xa0 \t'), '    ');
  },

  testCollapseBreakingSpaces() {
    assertEquals(
        'breaking spaces are collapsed', 'a b',
        googString.collapseBreakingSpaces(' \t\r\n a \t\r\n b \t\r\n '));
    assertEquals(
        'non-breaking spaces are kept', 'a \u00a0\u2000 b',
        googString.collapseBreakingSpaces('a \u00a0\u2000 b'));
  },

  testTrim() {
    assertEquals(
        'Should be the same', googString.trim('nothing 2 trim'),
        'nothing 2 trim');
    assertEquals(
        'Remove spaces', googString.trim('   hello  goodbye   '),
        'hello  goodbye');
    assertEquals(
        'Trim other stuff', googString.trim('\n\r\xa0 hi \r\n\xa0'), 'hi');
  },

  testTrimLeft() {
    const f = googString.trimLeft;
    assertEquals('Should be the same', f('nothing to trim'), 'nothing to trim');
    assertEquals(
        'Remove spaces', f('   hello  goodbye   '), 'hello  goodbye   ');
    assertEquals('Trim other stuff', f('\xa0\n\r hi \r\n\xa0'), 'hi \r\n\xa0');
  },

  testTrimRight() {
    const f = googString.trimRight;
    assertEquals('Should be the same', f('nothing to trim'), 'nothing to trim');
    assertEquals(
        'Remove spaces', f('   hello  goodbye   '), '   hello  goodbye');
    assertEquals('Trim other stuff', f('\n\r\xa0 hi \r\n\xa0'), '\n\r\xa0 hi');
  },

  testStartsWith() {
    assertTrue('Should start with \'\'', googString.startsWith('abcd', ''));
    assertTrue('Should start with \'ab\'', googString.startsWith('abcd', 'ab'));
    assertTrue(
        'Should start with \'abcd\'', googString.startsWith('abcd', 'abcd'));
    assertFalse(
        'Should not start with \'bcd\'', googString.startsWith('abcd', 'bcd'));
  },

  testEndsWith() {
    assertTrue('Should end with \'\'', googString.endsWith('abcd', ''));
    assertTrue('Should end with \'ab\'', googString.endsWith('abcd', 'cd'));
    assertTrue('Should end with \'abcd\'', googString.endsWith('abcd', 'abcd'));
    assertFalse('Should not end \'abc\'', googString.endsWith('abcd', 'abc'));
    assertFalse(
        'Should not end \'abcde\'', googString.endsWith('abcd', 'abcde'));
  },

  testCaseInsensitiveStartsWith() {
    assertTrue(
        'Should start with \'\'',
        googString.caseInsensitiveStartsWith('abcd', ''));
    assertTrue(
        'Should start with \'ab\'',
        googString.caseInsensitiveStartsWith('abcd', 'Ab'));
    assertTrue(
        'Should start with \'abcd\'',
        googString.caseInsensitiveStartsWith('AbCd', 'abCd'));
    assertFalse(
        'Should not start with \'bcd\'',
        googString.caseInsensitiveStartsWith('ABCD', 'bcd'));
  },

  testCaseInsensitiveEndsWith() {
    assertTrue(
        'Should end with \'\'', googString.caseInsensitiveEndsWith('abcd', ''));
    assertTrue(
        'Should end with \'cd\'',
        googString.caseInsensitiveEndsWith('abCD', 'cd'));
    assertTrue(
        'Should end with \'abcd\'',
        googString.caseInsensitiveEndsWith('abcd', 'abCd'));
    assertFalse(
        'Should not end \'abc\'',
        googString.caseInsensitiveEndsWith('aBCd', 'ABc'));
    assertFalse(
        'Should not end \'abcde\'',
        googString.caseInsensitiveEndsWith('ABCD', 'abcde'));
  },

  testCaseInsensitiveEquals() {
    function assertCaseInsensitiveEquals(str1, str2) {
      assertTrue(googString.caseInsensitiveEquals(str1, str2));
    }

    function assertCaseInsensitiveNotEquals(str1, str2) {
      assertFalse(googString.caseInsensitiveEquals(str1, str2));
    }

    assertCaseInsensitiveEquals('abc', 'abc');
    assertCaseInsensitiveEquals('abc', 'abC');
    assertCaseInsensitiveEquals('d,e,F,G', 'd,e,F,G');
    assertCaseInsensitiveEquals('ABCD EFGH 1234', 'abcd efgh 1234');
    assertCaseInsensitiveEquals('FooBarBaz', 'fOObARbAZ');

    assertCaseInsensitiveNotEquals('ABCD EFGH', 'abcd efg');
    assertCaseInsensitiveNotEquals('ABC DEFGH', 'ABCD EFGH');
    assertCaseInsensitiveNotEquals('FooBarBaz', 'fOObARbAZ ');
  },

  testSubs() {
    assertEquals(
        'Should be the same', 'nothing to subs',
        googString.subs('nothing to subs'));
    assertEquals('Should be the same', '1', googString.subs('%s', '1'));
    assertEquals(
        'Should be the same', '12true',
        googString.subs('%s%s%s', '1', 2, true));
    function f() {
      fail('This should not be called');
    }
    f.toString = () => 'f';
    assertEquals('Should not call function', 'f', googString.subs('%s', f));

    // If the string that is to be substituted in contains $& then it will be
    // usually be replaced with %s, we need to check goog.string.subs, handles
    // this case.
    assertEquals(
        '$& should not be substituted with %s', 'Foo Bar $&',
        googString.subs('Foo %s', 'Bar $&'));

    assertEquals(
        '$$ should not be substituted', '_$$_', googString.subs('%s', '_$$_'));
    assertEquals(
        '$` should not be substituted', '_$`_', googString.subs('%s', '_$`_'));
    assertEquals(
        '$\' should not be substituted', '_$\'_',
        googString.subs('%s', '_$\'_'));
    for (let i = 0; i < 99; i += 9) {
      assertEquals(
          `\$${i} should not be substituted`, `_\$${i}_`,
          googString.subs('%s', `_\$${i}_`));
    }

    assertEquals(
        'Only the first three "%s" strings should be replaced.',
        'test foo test bar test baz test %s test %s test',
        googString.subs(
            'test %s test %s test %s test %s test %s test', 'foo', 'bar',
            'baz'));
  },

  /**
   * Verifies that if too many arguments are given, they are ignored.
   * Logic test for bug documented here: http://go/eusxz
   */
  testSubsTooManyArguments() {
    assertEquals('one', googString.subs('one', 'two', 'three'));
    assertEquals('onetwo', googString.subs('one%s', 'two', 'three'));
  },

  testCaseInsensitiveCompare() {
    const f = googString.caseInsensitiveCompare;

    assert('"ABC" should be less than "def"', f('ABC', 'def') == -1);
    assert('"abc" should be less than "DEF"', f('abc', 'DEF') == -1);

    assert('"XYZ" should equal "xyz"', f('XYZ', 'xyz') == 0);

    assert('"XYZ" should be greater than "UVW"', f('xyz', 'UVW') == 1);
    assert('"XYZ" should be greater than "uvw"', f('XYZ', 'uvw') == 1);
  },

  testFloatAwareCompare() {
    const comparisons = NUMERIC_COMPARISON_TEST_CASES.concat([['3.14', '3.2']]);
    for (let i = 0; i < comparisons.length; i++) {
      const terms = comparisons[i];
      assert(
          terms[0] + ' should be less than ' + terms[1],
          googString.floatAwareCompare(terms[0], terms[1]) < 0);
      assert(
          terms[1] + ' should be greater than ' + terms[0],
          googString.floatAwareCompare(terms[1], terms[0]) > 0);
      assert(
          terms[0] + ' should be equal to ' + terms[0],
          googString.floatAwareCompare(terms[0], terms[0]) == 0);
      assert(
          terms[1] + ' should be equal to ' + terms[1],
          googString.floatAwareCompare(terms[1], terms[1]) == 0);
    }
  },

  testIntAwareCompare() {
    const comparisons = NUMERIC_COMPARISON_TEST_CASES.concat([['3.2', '3.14']]);
    for (let i = 0; i < comparisons.length; i++) {
      const terms = comparisons[i];
      assert(
          terms[0] + ' should be less than ' + terms[1],
          googString.intAwareCompare(terms[0], terms[1]) < 0);
      assert(
          terms[1] + ' should be greater than ' + terms[0],
          googString.intAwareCompare(terms[1], terms[0]) > 0);
      assert(
          terms[0] + ' should be equal to ' + terms[0],
          googString.intAwareCompare(terms[0], terms[0]) == 0);
      assert(
          terms[1] + ' should be equal to ' + terms[1],
          googString.intAwareCompare(terms[1], terms[1]) == 0);
    }
  },

  testUrlEncodeAndDecode() {
    const input = '<p>"hello there," she said, "what is going on here?</p>';
    const output =
        '%3Cp%3E%22hello%20there%2C%22%20she%20said%2C%20%22what%20is' +
        '%20going%20on%20here%3F%3C%2Fp%3E';

    assertEquals(
        'urlEncode vs encodeURIComponent', encodeURIComponent(input),
        googString.urlEncode(input));

    assertEquals('urlEncode vs model', googString.urlEncode(input), output);

    assertEquals('urlDecode vs model', googString.urlDecode(output), input);

    assertEquals(
        'urlDecode vs urlEncode',
        googString.urlDecode(googString.urlEncode(input)), input);

    assertEquals(
        'urlDecode with +s instead of %20s',
        googString.urlDecode(output.replace(/%20/g, '+')), input);
  },

  testNewLineToBr() {
    const str = 'some\nlines\rthat\r\nare\n\nsplit';
    const html = 'some<br>lines<br>that<br>are<br><br>split';
    const xhtml = 'some<br />lines<br />that<br />are<br /><br />split';

    assertEquals('Should be html', googString.newLineToBr(str), html);
    assertEquals('Should be html', googString.newLineToBr(str, false), html);
    assertEquals('Should be xhtml', googString.newLineToBr(str, true), xhtml);
  },

  testHtmlEscapeAndUnescapeEntities() {
    const text = '\'"x1 < x2 && y2 > y1"\'';
    const html = '&#39;&quot;x1 &lt; x2 &amp;&amp; y2 &gt; y1&quot;&#39;';

    assertEquals('Testing htmlEscape', html, googString.htmlEscape(text));
    assertEquals(
        'Testing htmlEscape', html, googString.htmlEscape(text, false));
    assertEquals('Testing htmlEscape', html, googString.htmlEscape(text, true));
    assertEquals(
        'Testing unescapeEntities', text, googString.unescapeEntities(html));

    assertEquals(
        'escape -> unescape', text,
        googString.unescapeEntities(googString.htmlEscape(text)));
    assertEquals(
        'unescape -> escape', html,
        googString.htmlEscape(googString.unescapeEntities(html)));
  },

  testHtmlUnescapeEntitiesWithDocument() {
    /** @type {?} */
    const documentMock = {
      createElement: mockControl.createFunctionMock('createElement'),
    };
    const divMock = dom.createElement(TagName.DIV);
    documentMock.createElement('div').$returns(divMock);
    mockControl.$replayAll();

    const html = '&lt;a&b&gt;';
    const text = '<a&b>';

    assertEquals(
        'wrong unescaped value', text,
        googString.unescapeEntitiesWithDocument(html, documentMock));
    assertNotEquals(
        'divMock.innerHTML should have been used', '', divMock.innerHTML);
    mockControl.$verifyAll();
  },

  /** @suppress {visibility} */
  testHtmlEscapeAndUnescapeEntitiesUsingDom() {
    const text = '"x1 < x2 && y2 > y1"';
    const html = '&quot;x1 &lt; x2 &amp;&amp; y2 &gt; y1&quot;';

    assertEquals(
        'Testing unescapeEntities', googString.unescapeEntitiesUsingDom_(html),
        text);
    assertEquals(
        'escape -> unescape',
        googString.unescapeEntitiesUsingDom_(googString.htmlEscape(text)),
        text);
    assertEquals(
        'unescape -> escape',
        googString.htmlEscape(googString.unescapeEntitiesUsingDom_(html)),
        html);
  },

  /** @suppress {visibility} */
  testHtmlUnescapeEntitiesUsingDom_withAmpersands() {
    const html = '&lt;a&b&gt;';
    const text = '<a&b>';

    assertEquals(
        'wrong unescaped value', text,
        googString.unescapeEntitiesUsingDom_(html));
  },

  /** @suppress {visibility} */
  testHtmlEscapeAndUnescapePureXmlEntities_() {
    const text = '"x1 < x2 && y2 > y1"';
    const html = '&quot;x1 &lt; x2 &amp;&amp; y2 &gt; y1&quot;';

    assertEquals(
        'Testing unescapePureXmlEntities_',
        googString.unescapePureXmlEntities_(html), text);
    assertEquals(
        'escape -> unescape',
        googString.unescapePureXmlEntities_(googString.htmlEscape(text)), text);
    assertEquals(
        'unescape -> escape',
        googString.htmlEscape(googString.unescapePureXmlEntities_(html)), html);
  },

  testForceNonDomHtmlUnescaping() {
    stubs.set(googString, 'FORCE_NON_DOM_HTML_UNESCAPING', true);
    // Set document.createElement to empty object so that the call to
    // unescapeEntities will blow up if html unescaping is carried out with DOM.
    // Notice that we can't directly set document to empty object since IE8
    // won't let us do so.
    stubs.set(globalThis.document, 'createElement', {});
    googString.unescapeEntities('&quot;x1 &lt; x2 &amp;&amp; y2 &gt; y1&quot;');
  },

  testHtmlEscapeDetectDoubleEscaping() {
    stubs.set(googString, 'DETECT_DOUBLE_ESCAPING', true);
    assertEquals('&#101; &lt; pi', googString.htmlEscape('e < pi'));
    assertEquals('&#101; &lt; pi', googString.htmlEscape('e < pi', true));
  },

  testHtmlEscapeNullByte() {
    assertEquals('&#0;', googString.htmlEscape('\x00'));
    assertEquals('&#0;', googString.htmlEscape('\x00', true));
    assertEquals('\\x00', googString.htmlEscape('\\x00'));
    assertEquals('\\x00', googString.htmlEscape('\\x00', true));
  },

  testXssUnescapeEntities() {
    // This tests that we don't have any XSS exploits in unescapeEntities
    let test = '&amp;<script defer>globalXssVar=1;</' +
        'script>';
    let expected = '&<script defer>globalXssVar=1;</' +
        'script>';

    assertEquals(
        'Testing unescapeEntities', expected,
        googString.unescapeEntities(test));
    assertEquals('unescapeEntities is vulnarable to XSS', 0, globalXssVar);

    test = '&amp;<script>globalXssVar=1;</' +
        'script>';
    expected = '&<script>globalXssVar=1;</' +
        'script>';

    assertEquals(
        'Testing unescapeEntities', expected,
        googString.unescapeEntities(test));
    assertEquals('unescapeEntities is vulnarable to XSS', 0, globalXssVar);
  },

  /** @suppress {visibility} */
  testXssUnescapeEntitiesUsingDom() {
    // This tests that we don't have any XSS exploits in
    // unescapeEntitiesUsingDom
    let test = '&amp;<script defer>globalXssVar=1;</' +
        'script>';
    let expected = '&<script defer>globalXssVar=1;</' +
        'script>';

    assertEquals(
        'Testing unescapeEntitiesUsingDom_', expected,
        googString.unescapeEntitiesUsingDom_(test));
    assertEquals(
        'unescapeEntitiesUsingDom_ is vulnerable to XSS', 0, globalXssVar);

    test = '&amp;<script>globalXssVar=1;</' +
        'script>';
    expected = '&<script>globalXssVar=1;</' +
        'script>';

    assertEquals(
        'Testing unescapeEntitiesUsingDom_', expected,
        googString.unescapeEntitiesUsingDom_(test));
    assertEquals(
        'unescapeEntitiesUsingDom_ is vulnerable to XSS', 0, globalXssVar);
  },

  /** @suppress {visibility} */
  testXssUnescapePureXmlEntities() {
    // This tests that we don't have any XSS exploits in unescapePureXmlEntities
    let test = '&amp;<script defer>globalXssVar=1;</' +
        'script>';
    let expected = '&<script defer>globalXssVar=1;</' +
        'script>';

    assertEquals(
        'Testing unescapePureXmlEntities_', expected,
        googString.unescapePureXmlEntities_(test));
    assertEquals(
        'unescapePureXmlEntities_ is vulnarable to XSS', 0, globalXssVar);

    test = '&amp;<script>globalXssVar=1;</' +
        'script>';
    expected = '&<script>globalXssVar=1;</' +
        'script>';

    assertEquals(
        'Testing unescapePureXmlEntities_', expected,
        googString.unescapePureXmlEntities_(test));
    assertEquals(
        'unescapePureXmlEntities_ is vulnarable to XSS', 0, globalXssVar);
  },

  testUnescapeEntitiesPreservesWhitespace() {
    // This tests that whitespace is preserved (primarily for IE)
    // Also make sure leading and trailing whitespace are preserved.
    let test = '\nTesting\n\twhitespace\n    preservation\n';
    let expected = test;

    assertEquals(
        'Testing unescapeEntities', expected,
        googString.unescapeEntities(test));

    // Now with entities
    test += ' &amp;&nbsp;\n';
    expected += ' &\u00A0\n';
    assertEquals(
        'Testing unescapeEntities', expected,
        googString.unescapeEntities(test));
  },

  testWhitespaceEscape() {
    assertEquals(
        'Should be the same',
        googString.whitespaceEscape('one two  three   four    five     '),
        'one two &#160;three &#160; four &#160; &#160;five &#160; &#160; ');
  },

  testPreserveSpaces() {
    const nbsp = Unicode.NBSP;
    assertEquals('', googString.preserveSpaces(''));
    assertEquals(`${nbsp}a`, googString.preserveSpaces(' a'));
    assertEquals(`${nbsp} a`, googString.preserveSpaces('  a'));
    assertEquals(`${nbsp} ${nbsp}a`, googString.preserveSpaces('   a'));
    assertEquals(`a ${nbsp}b`, googString.preserveSpaces('a  b'));
    assertEquals('a\n' + nbsp + 'b', googString.preserveSpaces('a\n b'));

    // We don't care about trailing spaces.
    assertEquals('a ', googString.preserveSpaces('a '));
    assertEquals('a \n' + nbsp + 'b', googString.preserveSpaces('a \n b'));
  },

  testStripQuotes() {
    assertEquals(
        'Quotes should be stripped', googString.stripQuotes('"hello"', '"'),
        'hello');

    assertEquals(
        'Quotes should be stripped', googString.stripQuotes('\'hello\'', '\''),
        'hello');

    assertEquals(
        'Quotes should not be stripped',
        googString.stripQuotes('-"hello"', '"'), '-"hello"');
  },

  testStripQuotesMultiple() {
    assertEquals(
        'Quotes should be stripped', googString.stripQuotes('"hello"', '"\''),
        'hello');
    assertEquals(
        'Quotes should be stripped', googString.stripQuotes('\'hello\'', '"\''),
        'hello');

    assertEquals(
        'Quotes should be stripped', googString.stripQuotes('\'hello\'', ''),
        '\'hello\'');
  },

  testStripQuotesMultiple2() {
    // Makes sure we do not strip twice
    assertEquals(
        'Quotes should be stripped',
        googString.stripQuotes('"\'hello\'"', '"\''), '\'hello\'');
    assertEquals(
        'Quotes should be stripped',
        googString.stripQuotes('"\'hello\'"', '\'"'), '\'hello\'');
  },

  testTruncate() {
    const str = 'abcdefghijklmnopqrstuvwxyz';
    assertEquals('Should be equal', googString.truncate(str, 8), 'abcde...');
    assertEquals(
        'Should be equal', googString.truncate(str, 11), 'abcdefgh...');

    const html = 'true &amp;&amp; false == false';
    assertEquals(
        'Should clip html char', googString.truncate(html, 11), 'true &am...');
    assertEquals(
        'Should not clip html char', googString.truncate(html, 12, true),
        'true &amp;&amp; f...');
  },

  testTruncateMiddle() {
    const str = 'abcdefghijklmnopqrstuvwxyz';
    assertEquals('abc...xyz', googString.truncateMiddle(str, 6));
    assertEquals('abc...yz', googString.truncateMiddle(str, 5));
    assertEquals(str, googString.truncateMiddle(str, str.length));

    const html = 'true &amp;&amp; false == false';
    assertEquals(
        'Should clip html char', 'true &a...= false',
        googString.truncateMiddle(html, 14));
    assertEquals(
        'Should not clip html char', 'true &amp;&amp;...= false',
        googString.truncateMiddle(html, 14, true));

    assertEquals(
        'ab...xyz',
        googString.truncateMiddle(str, 5, /** @type {?} */ (null), 3));
    assertEquals(
        'abcdefg...xyz',
        googString.truncateMiddle(str, 10, /** @type {?} */ (null), 3));
    assertEquals(
        'abcdef...wxyz',
        googString.truncateMiddle(str, 10, /** @type {?} */ (null), 4));
    assertEquals(
        '...yz', googString.truncateMiddle(str, 2, /** @type {?} */ (null), 3));
    assertEquals(
        str, googString.truncateMiddle(str, 50, /** @type {?} */ (null), 3));

    assertEquals(
        'Should clip html char', 'true &amp;&...lse',
        googString.truncateMiddle(html, 14, /** @type {?} */ (null), 3));
    assertEquals(
        'Should not clip html char', 'true &amp;&amp; fal...lse',
        googString.truncateMiddle(html, 14, true, 3));
  },

  testQuote() {
    let str = allChars();
    assertEquals(str, eval(googString.quote(str)));

    // empty string
    assertEquals('', eval(googString.quote('')));

    // unicode
    str = allChars(0, 10000);
    assertEquals(str, eval(googString.quote(str)));
  },

  testQuoteSpecialChars() {
    assertEquals('"\\""', googString.quote('"'));
    assertEquals('"\'"', googString.quote('\''));
    assertEquals('"\\\\"', googString.quote('\\'));
    assertEquals('"\\u003C"', googString.quote('<'));

    const zeroQuoted = googString.quote('\0');
    assertTrue(
        'goog.string.quote mangles the 0 char: ',
        '"\\0"' == zeroQuoted || '"\\x00"' == zeroQuoted);
  },

  testCrossBrowserQuote() {
    // The vertical space char has weird semantics on jscript, so we don't test
    // that one.
    const vertChar = '\x0B'.charCodeAt(0);

    // The zero char has two alternate encodings (\0 and \x00) both are ok,
    // and tested above.
    const zeroChar = 0;

    const str =
        allChars(zeroChar + 1, vertChar) + allChars(vertChar + 1, 10000);
    const nativeQuote = googString.quote(str);

    stubs.set(String.prototype, 'quote', null);
    assertNull(''.quote);

    assertEquals(nativeQuote, googString.quote(str));
  },

  testEscapeString() {
    const expected = allChars(0, 10000);
    let actual;
    try {
      actual = eval('"' + googString.escapeString(expected) + '"');
    } catch (e) {
      fail('Quote failed: err ' + e.message);
    }
    assertEquals(expected, actual);
  },

  testCountOf() {
    assertEquals(
        googString.countOf('REDSOXROX', /** @type {?} */ (undefined)), 0);
    assertEquals(googString.countOf('REDSOXROX', /** @type {?} */ (null)), 0);
    assertEquals(googString.countOf('REDSOXROX', ''), 0);
    assertEquals(googString.countOf('', /** @type {?} */ (undefined)), 0);
    assertEquals(googString.countOf('', /** @type {?} */ (null)), 0);
    assertEquals(googString.countOf('', ''), 0);
    assertEquals(googString.countOf('', 'REDSOXROX'), 0);
    assertEquals(googString.countOf(/** @type {?} */ (undefined), 'R'), 0);
    assertEquals(googString.countOf(/** @type {?} */ (null), 'R'), 0);
    assertEquals(
        googString.countOf(
            /** @type {?} */ (undefined), /** @type {?} */ (undefined)),
        0);
    assertEquals(
        googString.countOf(/** @type {?} */ (null), /** @type {?} */ (null)),
        0);

    assertEquals(googString.countOf('REDSOXROX', 'R'), 2);
    assertEquals(googString.countOf('REDSOXROX', 'E'), 1);
    assertEquals(googString.countOf('REDSOXROX', 'X'), 2);
    assertEquals(googString.countOf('REDSOXROX', 'RED'), 1);
    assertEquals(googString.countOf('REDSOXROX', 'ROX'), 1);
    assertEquals(googString.countOf('REDSOXROX', 'OX'), 2);
    assertEquals(googString.countOf('REDSOXROX', 'Z'), 0);
    assertEquals(googString.countOf('REDSOXROX', 'REDSOXROX'), 1);
    assertEquals(googString.countOf('REDSOXROX', 'YANKEES'), 0);
    assertEquals(googString.countOf('REDSOXROX', 'EVIL_EMPIRE'), 0);

    assertEquals(googString.countOf('RRRRRRRRR', 'R'), 9);
    assertEquals(googString.countOf('RRRRRRRRR', 'RR'), 4);
    assertEquals(googString.countOf('RRRRRRRRR', 'RRR'), 3);
    assertEquals(googString.countOf('RRRRRRRRR', 'RRRR'), 2);
    assertEquals(googString.countOf('RRRRRRRRR', 'RRRRR'), 1);
    assertEquals(googString.countOf('RRRRRRRRR', 'RRRRRR'), 1);
  },

  testRemoveAt() {
    let str = 'barfoobarbazbar';
    str = googString.removeAt(str, 0, 3);
    assertEquals('Remove first bar', 'foobarbazbar', str);
    str = googString.removeAt(str, 3, 3);
    assertEquals('Remove middle bar', 'foobazbar', str);
    str = googString.removeAt(str, 6, 3);
    assertEquals('Remove last bar', 'foobaz', str);
    assertEquals(
        'Invalid negative index', 'foobaz', googString.removeAt(str, -1, 0));
    assertEquals(
        'Invalid overflow index', 'foobaz', googString.removeAt(str, 9, 0));
    assertEquals(
        'Invalid negative stringLength', 'foobaz',
        googString.removeAt(str, 0, -1));
    assertEquals(
        'Invalid overflow stringLength', '', googString.removeAt(str, 0, 9));
    assertEquals(
        'Invalid overflow index and stringLength', 'foobaz',
        googString.removeAt(str, 9, 9));
    assertEquals(
        'Invalid zero stringLength', 'foobaz', googString.removeAt(str, 0, 0));
  },

  testRemove() {
    let str = 'barfoobarbazbar';
    str = googString.remove(str, 'bar');
    assertEquals('Remove first bar', 'foobarbazbar', str);
    str = googString.remove(str, 'bar');
    assertEquals('Remove middle bar', 'foobazbar', str);
    str = googString.remove(str, 'bar');
    assertEquals('Remove last bar', 'foobaz', str);
    str = googString.remove(str, 'bar');
    assertEquals('Original string', 'foobaz', str);
  },

  testRemoveAll() {
    let str = 'foobarbazbarfoobazfoo';
    str = googString.removeAll(str, 'foo');
    assertEquals('Remove all occurrences of foo', 'barbazbarbaz', str);
    str = googString.removeAll(str, 'foo');
    assertEquals('Original string', 'barbazbarbaz', str);
  },

  testReplaceAll() {
    let str = 'foobarbazbarfoobazfoo';
    str = googString.replaceAll(str, 'foo', 'test');
    assertEquals(
        'Replace all occurrences of foo with test', 'testbarbazbartestbaztest',
        str);
    let str2 = 'foobarbazbar^foo$baz^foo$';
    str2 = googString.replaceAll(str2, '^foo', '$&test');
    assertEquals(
        'Replace all occurrences of ^foo with $&test',
        'foobarbazbar$&test$baz$&test$', str2);
  },

  testRegExpEscape() {
    const spec = '()[]{}+-?*.$^|,:#<!\\';
    const escapedSpec = '\\' + spec.split('').join('\\');
    assertEquals('special chars', escapedSpec, googString.regExpEscape(spec));
    assertEquals('backslash b', '\\x08', googString.regExpEscape('\b'));

    let s = allChars();
    let re = new RegExp('^' + googString.regExpEscape(s) + '$');
    assertTrue('All ASCII', re.test(s));
    s = '';
    re = new RegExp('^' + googString.regExpEscape(s) + '$');
    assertTrue('empty string', re.test(s));
    s = allChars(0, 10000);
    re = new RegExp('^' + googString.regExpEscape(s) + '$');
    assertTrue('Unicode', re.test(s));
  },

  testPadNumber() {
    assertEquals('01.250', googString.padNumber(1.25, 2, 3));
    assertEquals('01.25', googString.padNumber(1.25, 2));
    assertEquals('01.3', googString.padNumber(1.25, 2, 1));
    assertEquals('1.25', googString.padNumber(1.25, 0));
    assertEquals('10', googString.padNumber(9.9, 2, 0));
    assertEquals('7', googString.padNumber(7, 0));
    assertEquals('7', googString.padNumber(7, 1));
    assertEquals('07', googString.padNumber(7, 2));
  },

  testAsString() {
    assertEquals('', googString.makeSafe(null));
    assertEquals('', googString.makeSafe(undefined));
    assertEquals('', googString.makeSafe(''));

    assertEquals('abc', googString.makeSafe('abc'));
    assertEquals('123', googString.makeSafe(123));
    assertEquals('0', googString.makeSafe(0));

    assertEquals('true', googString.makeSafe(true));
    assertEquals('false', googString.makeSafe(false));

    const funky = () => {};
    funky.toString = () => 'funky-thing';
    assertEquals('funky-thing', googString.makeSafe(funky));
  },

  testStringRepeat() {
    assertEquals('', googString.repeat('*', 0));
    assertEquals('*', googString.repeat('*', 1));
    assertEquals('     ', googString.repeat(' ', 5));
    assertEquals('__________', googString.repeat('_', 10));
    assertEquals('aaa', googString.repeat('a', 3));
    assertEquals('foofoofoofoofoofoo', googString.repeat('foo', 6));
  },

  testBuildString() {
    assertEquals('', googString.buildString());
    assertEquals('a', googString.buildString('a'));
    assertEquals('ab', googString.buildString('ab'));
    assertEquals('ab', googString.buildString('a', 'b'));
    assertEquals('abcd', googString.buildString('a', 'b', 'c', 'd'));
    assertEquals('0', googString.buildString(0));
    assertEquals('0123', googString.buildString(0, 1, 2, 3));
    assertEquals('ab01', googString.buildString('a', 'b', 0, 1));
    assertEquals('', googString.buildString(null, undefined));
  },

  testCompareVersions() {
    const f = googString.compareVersions;
    assertTrue('numeric equality broken', f(1, 1) == 0);
    assertTrue('numeric less than broken', f(1.0, 1.1) < 0);
    assertTrue('numeric greater than broken', f(2.0, 1.1) > 0);

    assertTrue('exact equality broken', f('1.0', '1.0') == 0);
    assertTrue('mutlidot equality broken', f('1.0.0.0', '1.0') == 0);
    assertTrue('mutlidigit equality broken', f('1.000', '1.0') == 0);
    assertTrue('less than broken', f('1.0.2.1', '1.1') < 0);
    assertTrue('greater than broken', f('1.1', '1.0.2.1') > 0);

    assertTrue('substring less than broken', f('1', '1.1') < 0);
    assertTrue('substring greater than broken', f('2.2', '2') > 0);

    assertTrue('b greater than broken', f('1.1', '1.1b') > 0);
    assertTrue('b less than broken', f('1.1b', '1.1') < 0);
    assertTrue('b equality broken', f('1.1b', '1.1b') == 0);

    assertTrue('b > a broken', f('1.1b', '1.1a') > 0);
    assertTrue('a < b broken', f('1.1a', '1.1b') < 0);

    assertTrue('9.5 < 9.10 broken', f('9.5', '9.10') < 0);
    assertTrue('9.5 < 9.11 broken', f('9.5', '9.11') < 0);
    assertTrue('9.11 > 9.10 broken', f('9.11', '9.10') > 0);
    assertTrue('9.1 < 9.10 broken', f('9.1', '9.10') < 0);
    assertTrue('9.1.1 < 9.10 broken', f('9.1.1', '9.10') < 0);
    assertTrue('9.1.1 < 9.11 broken', f('9.1.1', '9.11') < 0);

    assertTrue('10a > 9b broken', f('1.10a', '1.9b') > 0);
    assertTrue('b < b2 broken', f('1.1b', '1.1b2') < 0);
    assertTrue('b10 > b9 broken', f('1.1b10', '1.1b9') > 0);

    assertTrue('7 > 6 broken with leading whitespace', f(' 7', '6') > 0);
    assertTrue('7 > 6 broken with trailing whitespace', f('7 ', '6') > 0);
  },

  testIsUnicodeChar() {
    assertFalse('empty string broken', googString.isUnicodeChar(''));
    assertFalse(
        'non-single char string broken', googString.isUnicodeChar('abc'));
    assertTrue('space broken', googString.isUnicodeChar(' '));
    assertTrue('single char broken', googString.isUnicodeChar('a'));
    assertTrue('upper case broken', googString.isUnicodeChar('A'));
    assertTrue('unicode char broken', googString.isUnicodeChar('\u0C07'));
  },

  /** Verify we get random-ish looking values for hash of Strings. */
  testHashCode() {
    try {
      googString.hashCode(/** @type {?} */ (null));
      fail('should throw exception for null');
    } catch (ex) {
      // success
    }
    assertHashcodeEquals(0, '');
    assertHashcodeEquals(101574, 'foo');
    assertHashcodeEquals(1301670364, '\uAAAAfoo');
    assertHashcodeEquals(92567585, googString.repeat('a', 5));
    assertHashcodeEquals(2869595232, googString.repeat('a', 6));
    assertHashcodeEquals(3058106369, googString.repeat('a', 7));
    assertHashcodeEquals(312017024, googString.repeat('a', 8));
    assertHashcodeEquals(2929737728, googString.repeat('a', 1024));
  },

  testUniqueString() {
    const TEST_COUNT = 20;

    const obj = {};
    for (let i = 0; i < TEST_COUNT; i++) {
      obj[googString.createUniqueString()] = true;
    }

    assertEquals(
        'All strings should be unique.', TEST_COUNT, googObject.getCount(obj));
  },

  testToNumber() {
    // First, test the cases goog.string.toNumber() was primarily written for,
    // because JS built-ins are dumb.
    assertNaN(googString.toNumber('123a'));
    assertNaN(googString.toNumber('123.456.78'));
    assertNaN(googString.toNumber(''));
    assertNaN(googString.toNumber(' '));

    // Now, sanity-check.
    assertEquals(123, googString.toNumber(' 123 '));
    assertEquals(321.123, googString.toNumber('321.123'));
    assertEquals(1.00001, googString.toNumber('1.00001'));
    assertEquals(1, googString.toNumber('1.00000'));
    assertEquals(0.2, googString.toNumber('0.20'));
    assertEquals(0, googString.toNumber('0'));
    assertEquals(0, googString.toNumber('0.0'));
    assertEquals(-1, googString.toNumber('-1'));
    assertEquals(-0.3, googString.toNumber('-.3'));
    assertEquals(-12.345, googString.toNumber('-12.345'));
    assertEquals(100, googString.toNumber('1e2'));
    assertEquals(0.123, googString.toNumber('12.3e-2'));
    assertNaN(googString.toNumber('abc'));
  },

  testGetRandomString() {
    stubs.set(goog, 'now', functions.constant(1295726605874));
    stubs.set(Math, 'random', functions.constant(0.6679361383522245));
    assertTrue(
        'String must be alphanumeric',
        googString.isAlphaNumeric(googString.getRandomString()));
  },

  testToCamelCase() {
    assertEquals('OneTwoThree', googString.toCamelCase('-one-two-three'));
    assertEquals('oneTwoThree', googString.toCamelCase('one-two-three'));
    assertEquals('oneTwo', googString.toCamelCase('one-two'));
    assertEquals('one', googString.toCamelCase('one'));
    assertEquals('oneTwo', googString.toCamelCase('oneTwo'));
    assertEquals(
        'String value matching a native function name.', 'toString',
        googString.toCamelCase('toString'));
  },

  testToSelectorCase() {
    assertEquals('-one-two-three', googString.toSelectorCase('OneTwoThree'));
    assertEquals('one-two-three', googString.toSelectorCase('oneTwoThree'));
    assertEquals('one-two', googString.toSelectorCase('oneTwo'));
    assertEquals('one', googString.toSelectorCase('one'));
    assertEquals('one-two', googString.toSelectorCase('one-two'));
    assertEquals(
        'String value matching a native function name.', 'to-string',
        googString.toSelectorCase('toString'));
  },

  testToTitleCase() {
    assertEquals('One', googString.toTitleCase('one'));
    assertEquals('CamelCase', googString.toTitleCase('camelCase'));
    assertEquals('Onelongword', googString.toTitleCase('onelongword'));
    assertEquals('One Two Three', googString.toTitleCase('one two three'));
    assertEquals(
        'One        Two    Three',
        googString.toTitleCase('one        two    three'));
    assertEquals('   Longword  ', googString.toTitleCase('   longword  '));
    assertEquals('One-two-three', googString.toTitleCase('one-two-three'));
    assertEquals('One_two_three', googString.toTitleCase('one_two_three'));
    assertEquals(
        'String value matching a native function name.', 'ToString',
        googString.toTitleCase('toString'));

    // Verify results with no delimiter.
    assertEquals('One two three', googString.toTitleCase('one two three', ''));
    assertEquals('One-two-three', googString.toTitleCase('one-two-three', ''));
    assertEquals(' onetwothree', googString.toTitleCase(' onetwothree', ''));

    // Verify results with one delimiter.
    assertEquals('One two', googString.toTitleCase('one two', '.'));
    assertEquals(' one two', googString.toTitleCase(' one two', '.'));
    assertEquals(' one.Two', googString.toTitleCase(' one.two', '.'));
    assertEquals('One.Two', googString.toTitleCase('one.two', '.'));
    assertEquals('One...Two...', googString.toTitleCase('one...two...', '.'));

    // Verify results with multiple delimiters.
    const delimiters = '_-.';
    assertEquals(
        'One two three', googString.toTitleCase('one two three', delimiters));
    assertEquals(
        '  one two three',
        googString.toTitleCase('  one two three', delimiters));
    assertEquals(
        'One-Two-Three', googString.toTitleCase('one-two-three', delimiters));
    assertEquals(
        'One_Two_Three', googString.toTitleCase('one_two_three', delimiters));
    assertEquals(
        'One...Two...Three',
        googString.toTitleCase('one...two...three', delimiters));
    assertEquals(
        'One.  two.  three',
        googString.toTitleCase('one.  two.  three', delimiters));
  },

  testCapitalize() {
    assertEquals('Reptar', googString.capitalize('reptar'));
    assertEquals('Reptar reptar', googString.capitalize('reptar reptar'));
    assertEquals('Reptar', googString.capitalize('REPTAR'));
    assertEquals('Reptar', googString.capitalize('Reptar'));
    assertEquals('1234', googString.capitalize('1234'));
    assertEquals('$#@!', googString.capitalize('$#@!'));
    assertEquals('', googString.capitalize(''));
    assertEquals('R', googString.capitalize('r'));
    assertEquals('R', googString.capitalize('R'));
  },

  testParseInt() {
    // Many example values borrowed from
    // http://trac.webkit.org/browser/trunk/LayoutTests/fast/js/kde/
    // GlobalObject-expected.txt

    // Check non-numbers and strings
    assertTrue(isNaN(googString.parseInt(undefined)));
    assertTrue(isNaN(googString.parseInt(null)));
    assertTrue(isNaN(googString.parseInt(/** @type {?} */ ({}))));

    assertTrue(isNaN(googString.parseInt('')));
    assertTrue(isNaN(googString.parseInt(' ')));
    assertTrue(isNaN(googString.parseInt('a')));
    assertTrue(isNaN(googString.parseInt('FFAA')));
    assertEquals(1, googString.parseInt(1));
    assertEquals(1234567890123456, googString.parseInt(1234567890123456));
    assertEquals(2, googString.parseInt(' 2.3'));
    assertEquals(16, googString.parseInt('0x10'));
    assertEquals(11, googString.parseInt('11'));
    assertEquals(15, googString.parseInt('0xF'));
    assertEquals(15, googString.parseInt('0XF'));
    assertEquals(3735928559, googString.parseInt('0XDEADBEEF'));
    assertEquals(3, googString.parseInt('3x'));
    assertEquals(3, googString.parseInt('3 x'));
    assertFalse(isFinite(googString.parseInt('Infinity')));
    assertEquals(15, googString.parseInt('15'));
    assertEquals(15, googString.parseInt('015'));
    assertEquals(15, googString.parseInt('0xf'));
    assertEquals(15, googString.parseInt('15'));
    assertEquals(15, googString.parseInt('0xF'));
    assertEquals(15, googString.parseInt('15.99'));
    assertTrue(isNaN(googString.parseInt('FXX123')));
    assertEquals(15, googString.parseInt('15*3'));
    assertEquals(7, googString.parseInt('0x7'));
    assertEquals(1, googString.parseInt('1x7'));

    // Strings have no special meaning
    assertTrue(isNaN(googString.parseInt('Infinity')));
    assertTrue(isNaN(googString.parseInt('NaN')));

    // Test numbers and values
    assertEquals(3, googString.parseInt(3.3));
    assertEquals(-3, googString.parseInt(-3.3));
    assertEquals(0, googString.parseInt(-0));
    assertTrue(isNaN(googString.parseInt(Infinity)));
    assertTrue(isNaN(googString.parseInt(NaN)));
    assertTrue(isNaN(googString.parseInt(Number.POSITIVE_INFINITY)));
    assertTrue(isNaN(googString.parseInt(Number.NEGATIVE_INFINITY)));

    // In Chrome (at least), parseInt(Number.MIN_VALUE) is 5 (5e-324) and
    // parseInt(Number.MAX_VALUE) is 1 (1.79...e+308) as they are converted
    // to strings.  We do not attempt to correct this behavior.

    // Additional values for negatives.
    assertEquals(-3, googString.parseInt('-3'));
    assertEquals(-32, googString.parseInt('-32    '));
    assertEquals(-32, googString.parseInt(' -32 '));
    assertEquals(-3, googString.parseInt('-0x3'));
    assertEquals(-50, googString.parseInt('-0x32    '));
    assertEquals(-243, googString.parseInt('   -0xF3    '));
    assertTrue(isNaN(googString.parseInt(' - 0x32 ')));
  },

  testIsLowerCamelCase() {
    assertTrue(googString.isLowerCamelCase('foo'));
    assertTrue(googString.isLowerCamelCase('fooBar'));
    assertTrue(googString.isLowerCamelCase('fooBarBaz'));
    assertTrue(googString.isLowerCamelCase('innerHTML'));

    assertFalse(googString.isLowerCamelCase(''));
    assertFalse(googString.isLowerCamelCase('a3a'));
    assertFalse(googString.isLowerCamelCase('goog.dom'));
    assertFalse(googString.isLowerCamelCase('Foo'));
    assertFalse(googString.isLowerCamelCase('FooBar'));
    assertFalse(googString.isLowerCamelCase('ABCBBD'));
  },

  testIsUpperCamelCase() {
    assertFalse(googString.isUpperCamelCase(''));
    assertFalse(googString.isUpperCamelCase('foo'));
    assertFalse(googString.isUpperCamelCase('fooBar'));
    assertFalse(googString.isUpperCamelCase('fooBarBaz'));
    assertFalse(googString.isUpperCamelCase('innerHTML'));
    assertFalse(googString.isUpperCamelCase('a3a'));
    assertFalse(googString.isUpperCamelCase('goog.dom'));
    assertFalse(googString.isUpperCamelCase('Boyz2Men'));

    assertTrue(googString.isUpperCamelCase('ABCBBD'));
    assertTrue(googString.isUpperCamelCase('Foo'));
    assertTrue(googString.isUpperCamelCase('FooBar'));
    assertTrue(googString.isUpperCamelCase('FooBarBaz'));
  },

  testSplitLimit() {
    assertArrayEquals(['a*a*a*a'], googString.splitLimit('a*a*a*a', '*', -1));
    assertArrayEquals(['a*a*a*a'], googString.splitLimit('a*a*a*a', '*', 0));
    assertArrayEquals(['a', 'a*a*a'], googString.splitLimit('a*a*a*a', '*', 1));
    assertArrayEquals(
        ['a', 'a', 'a*a'], googString.splitLimit('a*a*a*a', '*', 2));
    assertArrayEquals(
        ['a', 'a', 'a', 'a'], googString.splitLimit('a*a*a*a', '*', 3));
    assertArrayEquals(
        ['a', 'a', 'a', 'a'], googString.splitLimit('a*a*a*a', '*', 4));

    assertArrayEquals(
        ['bbbbbbbbbbbb'], googString.splitLimit('bbbbbbbbbbbb', 'a', 10));
    assertArrayEquals(
        ['babab', 'bab', 'abb'],
        googString.splitLimit('bababaababaaabb', 'aa', 10));
    assertArrayEquals(
        ['babab', 'babaaabb'],
        googString.splitLimit('bababaababaaabb', 'aa', 1));
    assertArrayEquals(
        ['b', 'a', 'b', 'a', 'b', 'a', 'a', 'b', 'a', 'b', 'aaabb'],
        googString.splitLimit('bababaababaaabb', '', 10));
  },

  testContains() {
    assertTrue(googString.contains('moot', 'moo'));
    assertFalse(googString.contains('moo', 'moot'));
    assertFalse(googString.contains('Moot', 'moo'));
    assertTrue(googString.contains('moo', 'moo'));
  },

  testCaseInsensitiveContains() {
    assertTrue(googString.caseInsensitiveContains('moot', 'moo'));
    assertFalse(googString.caseInsensitiveContains('moo', 'moot'));
    assertTrue(googString.caseInsensitiveContains('Moot', 'moo'));
    assertTrue(googString.caseInsensitiveContains('moo', 'moo'));
  },

  testEditDistance() {
    assertEquals(
        'Empty string should match to length of other string', 4,
        googString.editDistance('goat', ''));
    assertEquals(
        'Empty string should match to length of other string', 4,
        googString.editDistance('', 'moat'));

    assertEquals(
        'Equal strings should have zero edit distance', 0,
        googString.editDistance('abcd', 'abcd'));
    assertEquals(
        'Equal strings should have zero edit distance', 0,
        googString.editDistance('', ''));

    assertEquals(
        'Edit distance for adding characters incorrect', 4,
        googString.editDistance('bdf', 'abcdefg'));
    assertEquals(
        'Edit distance for removing characters incorrect', 4,
        googString.editDistance('abcdefg', 'bdf'));

    assertEquals(
        'Edit distance for substituting characters incorrect', 4,
        googString.editDistance('adef', 'ghij'));
    assertEquals(
        'Edit distance for substituting characters incorrect', 1,
        googString.editDistance('goat', 'boat'));

    assertEquals(
        'Substitution should be preferred over insert/delete', 4,
        googString.editDistance('abcd', 'defg'));
  },

  testLastComponent() {
    assertEquals(
        'Last component of a string without separators should be the string',
        'abcdefgh', googString.lastComponent('abcdefgh', []));
    assertEquals(
        'Last component of a string without separators should be the string',
        'abcdefgh',
        googString.lastComponent('abcdefgh', /** @type {?} */ (null)));
    assertEquals(
        'Last component of a string without separators should be the string',
        'abcdefgh',
        googString.lastComponent('abcdefgh', /** @type {?} */ (undefined)));
    assertEquals(
        'Last component of a string without separators should be the string',
        'abcdefgh', googString.lastComponent('abcdefgh', ''));
    assertEquals(
        'Giving a simple string separator instead of an array should work',
        'fgh', googString.lastComponent('abcdefgh', 'e'));
    assertEquals(
        'Last component of a string without separators should be the string',
        'abcdefgh', googString.lastComponent('abcdefgh', ['']));
    assertEquals(
        'Last component of a string without separators should be the string',
        'abcdefgh', googString.lastComponent('abcdefgh', ['', '']));
    assertEquals(
        'Last component of a string without separators should be the string',
        'abcdefgh', googString.lastComponent('abcdefgh', ['']));
    assertEquals(
        'Last component of a single character string should be the string', 'a',
        googString.lastComponent('a', ['']));
    assertEquals(
        'Last component of a single character string separated by its only' +
            'character should be the empty string',
        '', googString.lastComponent('a', ['a']));
    assertEquals(
        'Last component of the empty string should be the empty string', '',
        googString.lastComponent('', ['']));
    assertEquals(
        'Last component of the empty string should be the empty string', '',
        googString.lastComponent('', ['a']));
    assertEquals(
        'Last component of the empty string should be the empty string', '',
        googString.lastComponent('', ['']));
    assertEquals('ccc', googString.lastComponent('aaabbbccc', ['b']));
    assertEquals('baz', googString.lastComponent('foo/bar/baz', ['/']));
    assertEquals('baz', googString.lastComponent('foo.bar.baz', ['.']));
    assertEquals('baz', googString.lastComponent('foo.bar.baz', ['/', '.']));
    assertEquals('bar/baz', googString.lastComponent('foo.bar/baz', ['.']));
    assertEquals(
        'bar-baz', googString.lastComponent('foo.bar-baz', ['/', '.']));
    assertEquals(
        'baz', googString.lastComponent('foo.bar-baz', ['-', '', '.']));
  },
});
