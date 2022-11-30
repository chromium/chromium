/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Tests for the textExtractor module.
 */

goog.module('goog.html.textExtractorTest');
goog.setTestOnly();

const testSuite = goog.require('goog.testing.testSuite');
const textExtractor = goog.require('goog.html.textExtractor');


/**
 * Verifies that the expected text is extracted from the HTML string.
 * @param {string} html The HTML string containing text mixed in HTML markup.
 * @param {string} expectedText The expected text extracted from the HTML
 * string.
 */
function assertExtractedTextEquals(html, expectedText) {
  const actualText = textExtractor.extractTextContent(html);
  if (textExtractor.isSupported()) {
    assertEquals(actualText, expectedText);
  } else {
    assertEquals(actualText, '');
  }
}

testSuite({
  testExtractTextContent_justText: function() {
    const html = 'Hello';
    assertExtractedTextEquals(html, html);
  },

  testExtractTextContent_basic: function() {
    const html = '<p>Hello</p>';
    const expectedText = 'Hello';
    assertExtractedTextEquals(html, expectedText);
  },

  testExtractTextContent_removesScript: function() {
    const html = '<p>Foo<script>Bar</script>Baz</p>';
    const expectedText = 'FooBaz';
    assertExtractedTextEquals(html, expectedText);
  },

  testExtractTextContent_blocks: function() {
    const html = '<div>Foo</div><div>Bar</div>';
    const expectedText = 'Foo\n\nBar';
    assertExtractedTextEquals(html, expectedText);
  },

  testExtractTextContent_extraNewlines: function() {
    const html = '<p>Foo</p>\n<p>Bar</p>';
    const expectedText = 'Foo\n\nBar';
    assertExtractedTextEquals(html, expectedText);
  },

  testExtractTextContent_inline: function() {
    const html = '<h1>Foo<span>Bar</span></h1>';
    const expectedText = 'FooBar';
    assertExtractedTextEquals(html, expectedText);
  },

  testExtractTextContent_complex: function() {
    const html = '<div>\n' +
        '  \n' +
        '  A\n' +
        '\n' +
        '  mind\n' +
        '\n' +
        '  needs books<br>as a sword needs a whetstone<p>' +  // no line break
        'if it is to <span style="display: block">keep</span> its edge.\n' +
        '  </p>\n' +
        '\n' +
        '</div>';
    const expectedText = 'A mind needs books\n' +
        'as a sword needs a whetstone\n' +
        'if it is to\n' +
        'keep\n' +
        'its edge.';
    assertExtractedTextEquals(html, expectedText);
  },

  testExtractTextContent_newlines: function() {
    const html = 'Hello\nWorld';
    const expectedText = 'Hello World';
    assertExtractedTextEquals(html, expectedText);
  },

  testExtractTextContent_br: function() {
    const html = 'Hello\n<br>World';
    const expectedText = 'Hello\nWorld';
    assertExtractedTextEquals(html, expectedText);
  },

  testExtractTextContent_brAndBlock: function() {
    const html = 'Hello\n\n<br>\n<p>World</p>';
    const expectedText = 'Hello\n\nWorld';
    assertExtractedTextEquals(html, expectedText);
  }
});
