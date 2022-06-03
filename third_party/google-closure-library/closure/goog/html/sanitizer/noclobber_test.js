/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Tests for {@link goog.html.sanitizer.noclobber} */

goog.module('goog.html.sanitizer.noclobberTest');
goog.setTestOnly();

const NodeType = goog.require('goog.dom.NodeType');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const noclobber = goog.require('goog.html.sanitizer.noclobber');
const testSuite = goog.require('goog.testing.testSuite');
const testingDom = goog.require('goog.testing.dom');

const userAgentProduct = goog.require('goog.userAgent.product');

/** Whether we support functions that operate on Node and Element. */
const elementAndNodeSupported =
    !userAgentProduct.IE || document.documentMode >= 10;

/**
 * @param {string} html
 * @return {!Element}
 */
function htmlToElement(html) {
  const div = document.createElement('div');
  div.innerHTML = html;
  return div.children[0];
}

/**
 * @param {string} name
 * @return {!Element}
 */
function createElement(name) {
  return htmlToElement('<form id="foo"><input name="' + name + '"></form>');
}

testSuite({
  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testElement() {
    if (!elementAndNodeSupported) {
      return;
    }

    let element = createElement('attributes');
    const attributes = noclobber.getElementAttributes(element);
    assertEquals('id', attributes[0].name);

    element = createElement('hasAttribute');
    assertTrue(noclobber.hasElementAttribute(element, 'id'));
    assertFalse(noclobber.hasElementAttribute(element, 'bar'));

    element = createElement('getAttribute');
    assertEquals('foo', noclobber.getElementAttribute(element, 'id'));

    element = createElement('setAttribute');
    noclobber.setElementAttribute(element, 'id', 'bar');
    assertEquals('bar', noclobber.getElementAttribute(element, 'id'));

    element = createElement('removeAttribute');
    assertTrue(element.hasAttribute('id'));
    noclobber.removeElementAttribute(element, 'id');
    assertFalse(element.hasAttribute('id'));

    element = createElement('innerHTML');
    const innerHTML = noclobber.getElementInnerHTML(element);
    testingDom.assertHtmlMatches('<input name="innerHTML">', innerHTML);

    element = createElement('style');
    const style = noclobber.getElementStyle(element);
    assertTrue(style instanceof CSSStyleDeclaration);

    element = createElement('getElementsByTagName');
    assertArrayEquals(
        Array.from(element.children),
        noclobber.getElementsByTagName(element, 'input'));

    element = htmlToElement(
        '<form><input name="sheet"><style>color:red</style></form>');
    document.body.appendChild(element);  // needs to be rooted into the DOM.
    assertEquals(
        element.children[1].sheet,
        noclobber.getElementStyleSheet(element.children[1]));

    element = createElement('matches');
    assertTrue(noclobber.elementMatches(element, '#foo'));
    assertFalse(noclobber.elementMatches(element, '#bar'));

    element = createElement('namespaceURI');
    assertEquals(
        'http://www.w3.org/1999/xhtml',
        noclobber.getElementNamespaceURI(element));
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testElementClobbered() {
    if (!elementAndNodeSupported) {
      return;
    }

    // There's currently no browser in our test suite that throws on clobbering,
    // so we delete the saved prototypes to simulate such case.
    const replacer = new PropertyReplacer();

    let element = createElement('attributes');
    replacer.set(noclobber.Methods, 'ATTRIBUTES_GETTER', null);
    assertThrows(function() {
      noclobber.getElementAttributes(element);
    });

    element = createElement('hasAttribute');
    replacer.set(noclobber.Methods, 'HAS_ATTRIBUTE', null);
    assertThrows(function() {
      noclobber.hasElementAttribute(element, 'id');
    });

    element = createElement('getAttribute');
    replacer.set(noclobber.Methods, 'GET_ATTRIBUTE', null);
    assertThrows(function() {
      noclobber.getElementAttribute(element, 'id');
    });

    element = createElement('setAttribute');
    replacer.set(noclobber.Methods, 'SET_ATTRIBUTE', null);
    assertThrows(function() {
      noclobber.setElementAttribute(element, 'id', 'bar');
    });

    element = createElement('removeAttribute');
    replacer.set(noclobber.Methods, 'REMOVE_ATTRIBUTE', null);
    assertThrows(function() {
      noclobber.removeElementAttribute(element, 'id');
    });

    element = createElement('innerHTML');
    replacer.set(noclobber.Methods, 'INNER_HTML_GETTER', null);
    assertThrows(function() {
      noclobber.getElementInnerHTML(element);
    });

    element = createElement('style');
    replacer.set(noclobber.Methods, 'STYLE_GETTER', null);
    assertThrows(function() {
      noclobber.getElementStyle(element);
    });

    element = createElement('getElementsByTagName');
    replacer.set(noclobber.Methods, 'GET_ELEMENTS_BY_TAG_NAME', null);
    assertThrows(function() {
      noclobber.getElementsByTagName(element, 'input');
    });

    // Sheet can't be clobbered, we only test that it works on browsers without
    // prototypes.
    element = htmlToElement(
        '<form><input name="foo"><style>color:red</style></form>');
    document.body.appendChild(element);  // needs to be rooted into the DOM.
    replacer.set(noclobber.Methods, 'SHEET_GETTER', null);
    assertEquals(
        element.children[1].sheet,
        noclobber.getElementStyleSheet(element.children[1]));

    element = createElement('matches');
    replacer.set(noclobber.Methods, 'MATCHES', null);
    assertThrows(function() {
      noclobber.elementMatches(element, '#foo');
    });

    replacer.reset();
  },

  testNode() {
    if (!elementAndNodeSupported) {
      return;
    }

    let element = createElement('nodeName');
    assertEquals('FORM', noclobber.getNodeName(element));

    element = createElement('nodeType');
    noclobber.assertNodeIsElement(element);
    assertEquals(NodeType.ELEMENT, noclobber.getNodeType(element));

    element = createElement('parentNode');
    assertEquals('DIV', noclobber.getParentNode(element).nodeName);

    element = createElement('childNodes');
    assertTrue(noclobber.getChildNodes(element) instanceof NodeList);

    element = createElement('appendChild');
    noclobber.appendNodeChild(element, document.createElement('div'));
    assertEquals(
        'DIV', element.childNodes[element.childNodes.length - 1].nodeName);
  },

  testNodeClobbered() {
    if (!elementAndNodeSupported) {
      return;
    }

    const replacer = new PropertyReplacer();

    let element = createElement('nodeName');
    replacer.set(noclobber.Methods, 'NODE_NAME_GETTER', null);
    assertThrows(function() {
      noclobber.getNodeName(element);
    });

    element = createElement('nodeType');
    replacer.set(noclobber.Methods, 'NODE_TYPE_GETTER', null);
    assertThrows(function() {
      noclobber.getNodeType(element);
    });

    element = createElement('parentNode');
    replacer.set(noclobber.Methods, 'PARENT_NODE_GETTER', null);
    assertThrows(function() {
      noclobber.getParentNode(element);
    });

    element = createElement('childNodes');
    replacer.set(noclobber.Methods, 'CHILD_NODES_GETTER', null);
    assertThrows(function() {
      noclobber.getChildNodes(element);
    });

    element = createElement('appendChild');
    replacer.set(noclobber.Methods, 'APPEND_CHILD', null);
    assertThrows(function() {
      noclobber.appendNodeChild(element, document.createElement('div'));
    });

    replacer.reset();
  },

  testCSSStyleDeclaration() {
    // Properties on the CSSStyleDeclaration object can't be clobbered.
    const element =
        htmlToElement('<form style="color:red"><input name="test"></form>');
    assertEquals('red', noclobber.getCssPropertyValue(element.style, 'color'));
    noclobber.setCssProperty(element.style, 'color', 'black');
    assertEquals('black', element.style.color);
  },

  testCSSStyleDeclarationOldBrowser() {
    // Properties on the CSSStyleDeclaration object can't be clobbered, we only
    // test that they work on browsers without prototypes.
    const replacer = new PropertyReplacer();

    replacer.set(noclobber.Methods, 'GET_PROPERTY_VALUE', null);
    const element = htmlToElement(
        '<form style="color:red"><input name="' +
        (userAgentProduct.IE ? 'getAttribute' : 'getPropertyValue') +
        '" style="color: green"></form>');
    assertEquals('red', noclobber.getCssPropertyValue(element.style, 'color'));
    noclobber.setCssProperty(element.style, 'color', 'black');
    assertEquals('black', element.style.color);

    replacer.reset();
  }
});
