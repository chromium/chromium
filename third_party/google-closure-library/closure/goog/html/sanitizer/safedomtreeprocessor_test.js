/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Tests for {@link goog.html.sanitizer.SafeDomTreeProcessor} */

goog.module('goog.html.sanitizer.SafeDomTreeProcessorTest');
goog.setTestOnly();

const SafeDomTreeProcessor = goog.require('goog.html.sanitizer.SafeDomTreeProcessor');
const noclobber = goog.require('goog.html.sanitizer.noclobber');
const testSuite = goog.require('goog.testing.testSuite');
const testingDom = goog.require('goog.testing.dom');

/**
 * Concrete subclass that defines an identity transformation function.
 * @final
 */
class NoopProcessor extends SafeDomTreeProcessor {
  /** @override */
  processRoot(newRoot) {}

  /** @override */
  preProcessHtml(html) {
    return html;
  }

  /** @override */
  createTextNode(originalNode) {
    return document.createTextNode(originalNode.data);
  }

  /** @override */
  createElementWithoutAttributes(originalElement) {
    return document.createElement(noclobber.getNodeName(originalElement));
  }

  /** @override */
  processElementAttribute(element, attribute) {
    return attribute.value;
  }
}

testSuite({
  /** @suppress {visibility} suppression added to enable type checking */
  testBasic() {
    let input = '';
    assertHtmlMatchesOnSupportedBrowser(
        input, new NoopProcessor().processToString(input));

    input = 'foo';
    assertHtmlMatchesOnSupportedBrowser(
        input, new NoopProcessor().processToString(input));

    input = '<p id="foo">foo</p>';
    assertHtmlMatchesOnSupportedBrowser(
        input, new NoopProcessor().processToString(input));

    input = '<p id="foo"><b>foo</b></p>';
    assertHtmlMatchesOnSupportedBrowser(
        input, new NoopProcessor().processToString(input));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testTagChanged() {
    const processor = new NoopProcessor();
    /** @suppress {visibility} suppression added to enable type checking */
    processor.createElementWithoutAttributes = anchorToFoo;
    const input = '<a href="bar"><p>baz</p></a>';
    const expected = '<foo href="bar"><p>baz</p></foo>';
    assertHtmlMatchesOnSupportedBrowser(
        expected, processor.processToString(input));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testTagDropped() {
    const processor = new NoopProcessor();
    /** @suppress {visibility} suppression added to enable type checking */
    processor.createElementWithoutAttributes = (originalElement) =>
        originalElement.tagName.toUpperCase() == 'A' ?
        null :
        document.createElement(originalElement.tagName);

    let input = '<a href="bar"><p>baz</p></a>';
    let expected = '';
    assertHtmlMatchesOnSupportedBrowser(
        expected, processor.processToString(input));

    input = '<p>foo<a>b</a></p><a href="bar"><p>baz</p></a>';
    expected = '<p>foo</p>';
    assertHtmlMatchesOnSupportedBrowser(
        expected, processor.processToString(input));

    input = '<a href="bar"><p>baz</p></a><p>foo<a>b</a></p>';
    expected = '<p>foo</p>';
    assertHtmlMatchesOnSupportedBrowser(
        expected, processor.processToString(input));

    input = '<div><p>foo<a href="a">b</a></p></div>';
    expected = '<div><p>foo</p></div>';
    assertHtmlMatchesOnSupportedBrowser(
        expected, processor.processToString(input));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testAttributeDropped() {
    const processor = new NoopProcessor();
    /** @suppress {visibility} suppression added to enable type checking */
    processor.processElementAttribute = (element, attribute) =>
        attribute.name == 'src' ? null : attribute.value;

    const input = '<img src="aaa" id="foo" />';
    const expected = '<img id="foo" />';
    assertHtmlMatchesOnSupportedBrowser(
        expected, processor.processToString(input));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testTemplateDropped() {
    const input = '<div><template id="foo"><p>foo</p></template></div>';
    const expected = '<div></div>';
    assertHtmlMatchesOnSupportedBrowser(
        expected, new NoopProcessor().processToString(input));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testProcessRoot() {
    const processor = new NoopProcessor();
    /** @suppress {visibility} suppression added to enable type checking */
    processor.processRoot = (spanElement) => {
      spanElement.id = 'bar';
    };

    const input = '<p>foo</p>';
    const expected = '<span id="bar"><p>foo</p></span>';
    assertHtmlMatchesOnSupportedBrowser(
        expected, processor.processToString(input));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testPreprocessHtml() {
    const processor = new NoopProcessor();
    /** @suppress {visibility} suppression added to enable type checking */
    processor.preProcessHtml = (html) => html.toLowerCase();

    const input = '<p id="BAR">FOO</p>';
    const expected = '<p id="bar">foo</p>';
    assertHtmlMatchesOnSupportedBrowser(
        expected, processor.processToString(input));
  },
});

/**
 * @param {!Element} originalElement
 * @return {!Element}
 */
function anchorToFoo(originalElement) {
  return document.createElement(
      originalElement.tagName.toUpperCase() == 'A' ? 'foo' :
                                                     originalElement.tagName);
}

/**
 * @param {string} expected
 * @param {string} actual
 */
function assertHtmlMatchesOnSupportedBrowser(expected, actual) {
  if (SafeDomTreeProcessor.SAFE_PARSING_SUPPORTED) {
    testingDom.assertHtmlMatches(
        expected, actual, true /* opt_strictAttributes */);
  } else {
    assertEquals('', actual);
  }
}
