/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.asserts.domTest');
goog.setTestOnly();

const DomHelper = goog.require('goog.dom.DomHelper');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.asserts.dom');
const testSuite = goog.require('goog.testing.testSuite');

/**
 * @param {function():!Document} getDocument
 * @return {!Object<string, function()>}
 */
const getTestsObject = (getDocument) => {
  let domHelper;
  let text;
  let div;
  let a;
  let table;
  let svg;

  return {
    setUp() {
      const doc = getDocument();
      domHelper = new DomHelper(doc);
      text = domHelper.createTextNode('foo');
      div = domHelper.createElement(TagName.DIV);
      a = domHelper.createElement(TagName.A);
      table = domHelper.createElement(TagName.TABLE);
      if (doc.createElementNS) {
        svg = doc.createElementNS('http://www.w3.org/2000/svg', 'svg');
      }
    },

    testAssertIsElement() {
      assertThrows(dom.assertIsElement.bind(null, text));

      assertEquals(div, dom.assertIsElement(div));
      assertEquals(a, dom.assertIsElement(a));
      assertEquals(table, dom.assertIsElement(table));

      if (svg) {
        assertEquals(svg, dom.assertIsElement(svg));
      }
    },

    testAssertIsHtmlElement() {
      assertThrows(dom.assertIsHtmlElement.bind(null, text));

      assertEquals(div, dom.assertIsHtmlElement(div));
      assertEquals(a, dom.assertIsHtmlElement(a));
      assertEquals(table, dom.assertIsHtmlElement(table));

      if (svg) {
        assertThrows(dom.assertIsHtmlElement.bind(null, svg));
      }
    },

    testAssertIsHtmlElementOfType() {
      assertThrows(dom.assertIsHtmlElementOfType.bind(null, text, TagName.DIV));

      assertEquals(div, dom.assertIsHtmlElementOfType(div, TagName.DIV));
      assertThrows(dom.assertIsHtmlElementOfType.bind(null, div, TagName.A));

      assertEquals(a, dom.assertIsHtmlElementOfType(a, TagName.A));
      assertThrows(dom.assertIsHtmlElementOfType.bind(null, a, TagName.DIV));

      assertEquals(table, dom.assertIsHtmlElementOfType(table, TagName.TABLE));
      assertThrows(
          dom.assertIsHtmlElementOfType.bind(null, table, TagName.DIV));

      if (svg) {
        assertThrows(
            dom.assertIsHtmlElementOfType.bind(null, svg, TagName.DIV));
      }
    },

    testAssertIsHtmlAnchorElement() {
      assertEquals(a, dom.assertIsHtmlAnchorElement(a));

      assertThrows(dom.assertIsHtmlAnchorElement.bind(null, div));
      assertThrows(dom.assertIsHtmlAnchorElement.bind(null, table));

      if (svg) {
        assertThrows(dom.assertIsHtmlAnchorElement.bind(null, svg));
      }
    },
  };
};

/**
 * Gets a secondary document to help expose differences in DOM ownership.
 * @return {!Document}
 */
const getRemoteDocument = () => {
  const domHelper = new DomHelper();
  const iframe = domHelper.createElement(TagName.IFRAME);
  domHelper.appendChild(document.body, iframe);
  const doc = iframe.contentWindow.document;
  domHelper.removeNode(iframe);
  return doc;
};

testSuite({
  testLocalDocument: getTestsObject(() => document),
  testRemoteDocument: getTestsObject(getRemoteDocument),
});
