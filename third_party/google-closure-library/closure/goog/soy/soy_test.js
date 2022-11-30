/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.soyTest');
goog.setTestOnly();

const NodeType = goog.require('goog.dom.NodeType');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const functions = goog.require('goog.functions');
const soy = goog.require('goog.soy');
/** @suppress {extraRequire} */
const testHelper = goog.require('goog.soy.testHelper');
const testSuite = goog.require('goog.testing.testSuite');

/**
 * Asserts that the function throws an error for unsafe templates.
 * @param {Function} func Callback to test.
 * @suppress {checkTypes} suppression added to enable type checking
 */
function assertUnsafeTemplateOutputErrorThrown(func) {
  assertContains(
      'Sanitized content was not of kind HTML.', assertThrows(func).message);
}

testSuite({
  /** @suppress {checkTypes} suppression added to enable type checking */
  testRenderHtml() {
    const testDiv = dom.createElement(TagName.DIV);
    soy.renderHtml(testDiv, example.sanitizedHtmlTemplate());
    assertEquals('hello <b>world</b>', testDiv.innerHTML.toLowerCase());
  },

  testRenderElement() {
    const testDiv = dom.createElement(TagName.DIV);
    soy.renderElement(testDiv, example.multiRootTemplate, {name: 'Boo'});
    assertEquals('<div>Hello</div><div>Boo</div>', elementToInnerHtml(testDiv));
  },

  testRenderElementWithNoTemplateData() {
    const testDiv = dom.createElement(TagName.DIV);
    soy.renderElement(testDiv, example.noDataTemplate);
    assertEquals('<div>Hello</div>', elementToInnerHtml(testDiv));
  },

  testRenderAsFragmentTextNode() {
    const fragment =
        soy.renderAsFragment(example.textNodeTemplate, {name: 'Boo'});
    assertEquals(NodeType.TEXT, fragment.nodeType);
    assertEquals('Boo', fragmentToHtml(fragment));
  },

  testRenderAsFragmentInjectedData() {
    const fragment = soy.renderAsFragment(
        example.injectedDataTemplate, {name: 'Boo'}, {name: 'ijBoo'});
    assertEquals(NodeType.TEXT, fragment.nodeType);
    assertEquals('BooijBoo', fragmentToHtml(fragment));
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testRenderAsFragmentSingleRoot() {
    const fragment =
        soy.renderAsFragment(example.singleRootTemplate, {name: 'Boo'});
    assertEquals(NodeType.ELEMENT, fragment.nodeType);
    assertEquals(String(TagName.SPAN), fragment.tagName);
    assertEquals('Boo', fragment.innerHTML);
  },

  testRenderAsFragmentMultiRoot() {
    const fragment =
        soy.renderAsFragment(example.multiRootTemplate, {name: 'Boo'});
    assertEquals(NodeType.DOCUMENT_FRAGMENT, fragment.nodeType);
    assertEquals('<div>Hello</div><div>Boo</div>', fragmentToHtml(fragment));
  },

  testRenderAsFragmentNoData() {
    const fragment = soy.renderAsFragment(example.noDataTemplate);
    assertEquals(NodeType.ELEMENT, fragment.nodeType);
    assertEquals('<div>Hello</div>', fragmentToHtml(fragment));
  },

  testRenderAsElementTextNode() {
    const elem = soy.renderAsElement(example.textNodeTemplate, {name: 'Boo'});
    assertEquals(NodeType.ELEMENT, elem.nodeType);
    assertEquals(String(TagName.DIV), elem.tagName);
    assertEquals('Boo', elementToInnerHtml(elem));
  },

  testRenderAsElementInjectedData() {
    const elem = soy.renderAsElement(
        example.injectedDataTemplate, {name: 'Boo'}, {name: 'ijBoo'});
    assertEquals(NodeType.ELEMENT, elem.nodeType);
    assertEquals(String(TagName.DIV), elem.tagName);
    assertEquals('BooijBoo', elementToInnerHtml(elem));
  },

  testRenderAsElementSingleRoot() {
    const elem = soy.renderAsElement(example.singleRootTemplate, {name: 'Boo'});
    assertEquals(NodeType.ELEMENT, elem.nodeType);
    assertEquals(String(TagName.SPAN), elem.tagName);
    assertEquals('Boo', elementToInnerHtml(elem));
  },

  testRenderAsElementMultiRoot() {
    const elem = soy.renderAsElement(example.multiRootTemplate, {name: 'Boo'});
    assertEquals(NodeType.ELEMENT, elem.nodeType);
    assertEquals(String(TagName.DIV), elem.tagName);
    assertEquals('<div>Hello</div><div>Boo</div>', elementToInnerHtml(elem));
  },

  testRenderAsElementWithNoData() {
    const elem = soy.renderAsElement(example.noDataTemplate);
    assertEquals('Hello', elementToInnerHtml(elem));
  },

  testConvertToElement() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const elem = soy.convertToElement(example.sanitizedHtmlTemplate());
    assertEquals(NodeType.ELEMENT, elem.nodeType);
    assertEquals(String(TagName.DIV), elem.tagName);
    assertEquals('hello <b>world</b>', elem.innerHTML.toLowerCase());
  },

  testAllowButEscapeTextTemplate() {
    const div = dom.createElement(TagName.DIV);
    soy.renderElement(div, example.stringTemplate);
    assertEquals('&lt;b&gt;XSS&lt;/b&gt;', div.innerHTML);
    const fragment = soy.renderAsFragment(example.stringTemplate);
    assertEquals('<b>XSS</b>', fragment.nodeValue);
    assertEquals(
        '&lt;b&gt;XSS&lt;/b&gt;',
        soy.renderAsElement(example.stringTemplate).innerHTML);
  },

  testRejectSanitizedCss() {
    assertUnsafeTemplateOutputErrorThrown(() => {
      soy.renderAsElement(example.sanitizedCssTemplate);
    });
  },

  testRejectSpoofingSanitizedContent() {
    assertUnsafeTemplateOutputErrorThrown(() => {
      soy.renderAsElement(example.templateSpoofingSanitizedContentString);
    });
  },

  testStringTemplatesRenderedAsText() {
    assertEquals(
        '<b>XSS</b>',
        dom.getTextContent(soy.renderAsFragment(example.stringTemplate)));
  },

  testAcceptSanitizedHtml() {
    assertEquals(
        'Hello World',
        dom.getTextContent(soy.renderAsElement(example.sanitizedHtmlTemplate)));
  },

  testRejectSanitizedHtmlAttributes() {
    // Attributes context has nothing to do with html.
    assertUnsafeTemplateOutputErrorThrown(
        () => dom.getTextContent(
            soy.renderAsElement(example.sanitizedHtmlAttributesTemplate)));
  },

  testAcceptNonObject() {
    // Some templates, or things that spoof templates in unit tests, might
    // return non-strings in unusual cases.
    assertEquals(
        'null',
        dom.getTextContent(soy.renderAsElement(functions.constant(null))));
  },

  testDebugAssertionWithBadFirstTag() {
    try {
      soy.renderAsElement(example.tableRowTemplate);
      // Expect no exception in production code.
      assert(!goog.DEBUG);
    } catch (e) {
      // Expect exception in debug code.
      assert(goog.DEBUG);
      // Make sure to let the developer know which tag caused the problem.
      assertContains('<tr>', e.message);
    }

    try {
      soy.renderAsFragment(example.tableRowTemplate);
      // Expect no exception in production code.
      assert(!goog.DEBUG);
    } catch (e) {
      // Expect exception in debug code.
      assert(goog.DEBUG);
      // Make sure to let the developer know which tag caused the problem.
      assertContains('<tr>', e.message);
    }

    try {
      soy.renderAsElement(example.colGroupTemplateCaps);
      // Expect no exception in production code.
      assert(!goog.DEBUG);
    } catch (e) {
      // Expect exception in debug code.
      assert(goog.DEBUG);
      // Make sure to let the developer know which tag caused the problem.
      assertContains('<COLGROUP>', e.message);
    }
  },

  /**
   * When innerHTML is assigned on an element in IE, IE recursively severs all
   * parent-children links in the removed content. This test ensures that that
   * doesn't happen when re-rendering an element with soy.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testRerenderLeavesChildrenInIE() {
    // Given a div with existing content.
    const grandchildDiv = dom.createElement(TagName.DIV);
    const childDiv = dom.createDom(TagName.DIV, null, [grandchildDiv]);
    const testDiv = dom.createDom(TagName.DIV, null, [childDiv]);
    // Expect parent/children links.
    assertArrayEquals(
        'Expect testDiv to contain childDiv.', [childDiv],
        Array.from(testDiv.children));
    assertEquals(
        'Expect childDiv to be contained in testDiv.', testDiv,
        childDiv.parentElement);
    assertArrayEquals(
        'Expect childDiv to contain grandchildDiv.', [grandchildDiv],
        Array.from(childDiv.children));
    assertEquals(
        'Expect grandchildDiv to be contained in childDiv.', childDiv,
        grandchildDiv.parentElement);

    // When the div's content is re-rendered.
    soy.renderHtml(testDiv, example.sanitizedHtmlTemplate());
    assertEquals(
        `Expect testDiv's contents to complete change`, 'hello <b>world</b>',
        testDiv.innerHTML.toLowerCase());
    // Expect the previous childDiv tree to retain its parent-child connections.
    assertArrayEquals(
        'Expect childDiv to still contain grandchildDiv.', [grandchildDiv],
        Array.from(childDiv.children));
    assertEquals(
        'Expect grandchildDiv to still be contained in childDiv.', childDiv,
        grandchildDiv.parentElement);
  },
});
