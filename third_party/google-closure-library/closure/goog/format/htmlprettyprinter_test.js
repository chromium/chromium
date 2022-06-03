/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.format.HtmlPrettyPrinterTest');
goog.setTestOnly();

const HtmlPrettyPrinter = goog.require('goog.format.HtmlPrettyPrinter');
const MockClock = goog.require('goog.testing.MockClock');
const testSuite = goog.require('goog.testing.testSuite');

const COMPLEX_HTML = '<!DOCTYPE root-element [SYSTEM OR PUBLIC FPI] "uri" [' +
    '<!-- internal declarations -->]>' +
    '<html><head><title>My HTML</title><!-- my comment --></head>' +
    '<script> if(i<0)\nfoo; </script>' +
    '<body><h1>My Header</h1>My text.<br><b>My bold text.</b><hr>' +
    '<pre>My\npreformatted <br> HTML.</pre>5 < 10</body>' +
    '</html>';
let mockClock;
let mockClockTicks;

testSuite({
  setUp() {
    mockClockTicks = 0;
    mockClock = new MockClock();
    mockClock.getCurrentTime = () => mockClockTicks++;
    mockClock.install();
  },

  tearDown() {
    if (mockClock) {
      mockClock.uninstall();
    }
  },

  testSimpleHtml() {
    const actual = HtmlPrettyPrinter.format('<br><b>bold</b>');
    assertEquals('<br>\n<b>bold</b>\n', actual);
    assertEquals(actual, HtmlPrettyPrinter.format(actual));
  },

  testSimpleHtmlMixedCase() {
    const actual = HtmlPrettyPrinter.format('<BR><b>bold</b>');
    assertEquals('<BR>\n<b>bold</b>\n', actual);
    assertEquals(actual, HtmlPrettyPrinter.format(actual));
  },

  testComplexHtml() {
    const actual = HtmlPrettyPrinter.format(COMPLEX_HTML);
    const expected = '<!DOCTYPE root-element [SYSTEM OR PUBLIC FPI] "uri" [' +
        '<!-- internal declarations -->]>\n' +
        '<html>\n' +
        '<head>\n' +
        '<title>My HTML</title>\n' +
        '<!-- my comment -->' +
        '</head>\n' +
        '<script> if(i<0)\nfoo; </script>\n' +
        '<body>\n' +
        '<h1>My Header</h1>\n' +
        'My text.<br>\n' +
        '<b>My bold text.</b>\n' +
        '<hr>\n' +
        '<pre>My\npreformatted <br> HTML.</pre>\n' +
        '5 < 10' +
        '</body>\n' +
        '</html>\n';
    assertEquals(expected, actual);
    assertEquals(actual, HtmlPrettyPrinter.format(actual));
  },

  testTimeout() {
    const pp = new HtmlPrettyPrinter(3);
    const actual = pp.format(COMPLEX_HTML);
    const expected = '<!DOCTYPE root-element [SYSTEM OR PUBLIC FPI] "uri" [' +
        '<!-- internal declarations -->]>\n' +
        '<html>\n' +
        '<head><title>My HTML</title><!-- my comment --></head>' +
        '<script> if(i<0)\nfoo; </script>' +
        '<body><h1>My Header</h1>My text.<br><b>My bold text.</b><hr>' +
        '<pre>My\npreformatted <br> HTML.</pre>5 < 10</body>' +
        '</html>\n';
    assertEquals(expected, actual);
  },

  testKeepLeadingIndent() {
    const original = ' <b>Bold</b> <i>Ital</i> ';
    const expected = ' <b>Bold</b> <i>Ital</i>\n';
    assertEquals(expected, HtmlPrettyPrinter.format(original));
  },

  testTrimLeadingLineBreaks() {
    const original = '\n \t\r\n  \n <b>Bold</b> <i>Ital</i> ';
    const expected = ' <b>Bold</b> <i>Ital</i>\n';
    assertEquals(expected, HtmlPrettyPrinter.format(original));
  },

  testExtraLines() {
    const original = '<br>\ntombrat';
    assertEquals(
        `${original}
`,
        HtmlPrettyPrinter.format(original));
  },

  testCrlf() {
    const original = '<br>\r\none\r\ntwo<br>';
    assertEquals(
        `${original}
`,
        HtmlPrettyPrinter.format(original));
  },

  testEndInLineBreak() {
    assertEquals('foo\n', HtmlPrettyPrinter.format('foo'));
    assertEquals('foo\n', HtmlPrettyPrinter.format('foo\n'));
    assertEquals('foo\n', HtmlPrettyPrinter.format('foo\n\n'));
    assertEquals('foo<br>\n', HtmlPrettyPrinter.format('foo<br>'));
    assertEquals('foo<br>\n', HtmlPrettyPrinter.format('foo<br>\n'));
  },

  testTable() {
    const original = '<table>' +
        '<tr><td>one.one</td><td>one.two</td></tr>' +
        '<tr><td>two.one</td><td>two.two</td></tr>' +
        '</table>';
    const expected = '<table>\n' +
        '<tr>\n<td>one.one</td>\n<td>one.two</td>\n</tr>\n' +
        '<tr>\n<td>two.one</td>\n<td>two.two</td>\n</tr>\n' +
        '</table>\n';
    assertEquals(expected, HtmlPrettyPrinter.format(original));
  },

  /**
   * We have a sanity check in HtmlPrettyPrinter to make sure the regex index
   * advances after every match. We should never hit this, but we include it on
   * the chance there is some corner case where the pattern would match but not
   * process a new token. It's not generally a good idea to break the
   * implementation to test behavior, but this is the easiest way to mimic a
   * bad internal state.
   */
  testRegexMakesProgress() {
    /** @suppress {visibility} suppression added to enable type checking */
    const original = HtmlPrettyPrinter.TOKEN_REGEX_;

    try {
      // This regex matches \B, an index between 2 word characters, so the regex
      // index does not advance when matching this.
      /**
       * @suppress {visibility,const} suppression added to enable type checking
       */
      HtmlPrettyPrinter.TOKEN_REGEX_ =
          /(?:\B|<!--.*?-->|<!.*?>|<(\/?)(\w+)[^>]*>|[^<]+|<)/g;

      // It would work on this string.
      assertEquals('f o o\n', HtmlPrettyPrinter.format('f o o'));

      // But not this one.
      const ex = assertThrows(
          'should have failed for invalid regex - endless loop',
          goog.partial(HtmlPrettyPrinter.format, COMPLEX_HTML));
      assertEquals(
          'Regex failed to make progress through source html.', ex.message);
    } finally {
      /**
       * @suppress {visibility,constantProperty} suppression added to enable
       * type checking
       */
      HtmlPrettyPrinter.TOKEN_REGEX_ = original;
    }
  },

  /**
   * FF3.0 doesn't like \n between <code></li></code> and <code></ul></code>.
   * See b/1520665.
   */
  testLists() {
    const original = '<ul><li>one</li><ul><li>two</li></UL><li>three</li></ul>';
    const expected =
        '<ul><li>one</li>\n<ul><li>two</li></UL>\n<li>three</li></ul>\n';
    assertEquals(expected, HtmlPrettyPrinter.format(original));
  },

  /**
   * We have a sanity check in HtmlPrettyPrinter to make sure the regex fully
   * tokenizes the string. We should never hit this, but we include it on the
   * chance there is some corner case where the pattern would miss a section of
   * original string. It's not generally a good idea to break the
   * implementation to test behavior, but this is the easiest way to mimic a
   * bad internal state.
   */
  testAvoidDataLoss() {
    /** @suppress {visibility} suppression added to enable type checking */
    const original = HtmlPrettyPrinter.TOKEN_REGEX_;

    try {
      // This regex does not match stranded '<' characters, so does not fully
      // tokenize the string.
      /**
       * @suppress {visibility,constantProperty} suppression added to enable
       * type checking
       */
      HtmlPrettyPrinter.TOKEN_REGEX_ =
          /(?:<!--.*?-->|<!.*?>|<(\/?)(\w+)[^>]*>|[^<]+)/g;

      // It would work on this string.
      assertEquals('foo\n', HtmlPrettyPrinter.format('foo'));

      // But not this one.
      const ex = assertThrows(
          'should have failed for invalid regex - data loss',
          goog.partial(HtmlPrettyPrinter.format, COMPLEX_HTML));
      assertEquals('Lost data pretty printing html.', ex.message);
    } finally {
      /**
       * @suppress {visibility,constantProperty} suppression added to enable
       * type checking
       */
      HtmlPrettyPrinter.TOKEN_REGEX_ = original;
    }
  },
});
