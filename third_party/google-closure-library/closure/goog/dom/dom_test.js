/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Shared code for dom_test.html and dom_quirks_test.html.
 */

/** @suppress {extraProvide} */
goog.module('goog.dom.dom_test');
goog.setTestOnly();

const Const = goog.require('goog.string.Const');
const DomHelper = goog.require('goog.dom.DomHelper');
const InputType = goog.require('goog.dom.InputType');
const NodeType = goog.require('goog.dom.NodeType');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const SafeUrl = goog.require('goog.html.SafeUrl');
const TagName = goog.require('goog.dom.TagName');
const Unicode = goog.require('goog.string.Unicode');
const asserts = goog.require('goog.asserts');
const functions = goog.require('goog.functions');
const googArray = goog.require('goog.array');
const googDom = goog.require('goog.dom');
const googObject = goog.require('goog.object');
const isVersion = goog.require('goog.userAgent.product.isVersion');
const testSuite = goog.require('goog.testing.testSuite');
const testing = goog.require('goog.html.testing');
/** @suppress {extraRequire} */
const testingAsserts = goog.require('goog.testing.asserts');
const userAgent = goog.require('goog.userAgent');

const $ = googDom.getElement;

let divForTestingScrolling;
let myIframe;
let myIframeDoc;
let stubs;

function createTestDom(txt) {
  const dom = googDom.createDom(TagName.DIV);
  dom.innerHTML = txt;
  return dom;
}

/**
 * Simple alternative implementation of googDom.isFocusable. Serves as a sanity
 * check whether the tests are correct. Unfortunately it can't replace the real
 * implementation because of the side effects.
 * @param {!Element} element
 * @return {boolean}
 * @suppress {strictMissingProperties} suppression added to enable type checking
 */
function isFocusableAlternativeImpl(element) {
  element.focus();
  return document.activeElement == element &&  // programmatically focusable
      element.tabIndex >= 0;  // keyboard focusing is not disabled
}

/** @param {!Element} element */
function assertFocusable(element) {
  const message = 'element with id=' + element.id + ' should be focusable';
  assertTrue(message, isFocusableAlternativeImpl(element));
  assertTrue(message, googDom.isFocusable(element));
}

/** @param {!Element} element */
function assertNotFocusable(element) {
  const message = 'element with id=' + element.id + ' should not be focusable';
  assertFalse(message, isFocusableAlternativeImpl(element));
  assertFalse(message, googDom.isFocusable(element));
}

// IE inserts line breaks and capitalizes nodenames.
function assertEqualsCaseAndLeadingWhitespaceInsensitive(value1, value2) {
  value1 = value1.replace(/^\s+|\s+$/g, '').toLowerCase();
  value2 = value2.replace(/^\s+|\s+$/g, '').toLowerCase();
  assertEquals(value1, value2);
}

/**
 * Assert that the given Const, when converted to a Node,
 * stringifies in one of the specified ways.
 * @param {!Array<string>} potentialStringifications
 * @param {...!Const} var_args The constants to use.
 */
function assertConstHtmlToNodeStringifiesToOneOf(
    potentialStringifications, var_args) {
  const node = googDom.constHtmlToNode.apply(
      undefined, Array.prototype.slice.call(arguments, 1));
  /** @suppress {checkTypes} suppression added to enable type checking */
  const stringified = googDom.getOuterHtml(node);
  if (potentialStringifications.find(element => element == stringified) ===
      null) {
    fail(
        'Unexpected stringification for a node built from "' +
        Array.prototype.slice.call(arguments, 1).map(Const.unwrap).join('') +
        '": "' + stringified + '"');
  }
}

/** @return {boolean} Returns true if the userAgent is IE8 or higher. */
function isIE8OrHigher() {
  return userAgent.IE && isVersion('8');
}

/**
 * Stub out googDom.getWindow with passed object.
 * @param {!Object} win Fake window object.
 */
function setWindow(win) {
  stubs.set(googDom, 'getWindow', functions.constant(win));
}

testSuite({
  setUpPage() {
    stubs = new PropertyReplacer();
    divForTestingScrolling = googDom.createElement(TagName.DIV);
    divForTestingScrolling.style.width = '5000px';
    divForTestingScrolling.style.height = '5000px';
    document.body.appendChild(divForTestingScrolling);

    // Setup for the iframe
    myIframe = $('myIframe');
    myIframeDoc = googDom.getFrameContentDocument(
        /** @type {HTMLIFrameElement} */ (myIframe));

    // Set up document for iframe: total height of elements in document is 65
    // If the elements are not create like below, IE will get a wrong height for
    // the document.
    myIframeDoc.open();
    // Make sure we progate the compat mode
    myIframeDoc.write(
        (googDom.isCss1CompatMode() ? '<!DOCTYPE html>' : '') +
        '<style>body{margin:0;padding:0}</style>' +
        '<div style="height:42px;font-size:1px;line-height:0;">' +
        'hello world</div>' +
        '<div style="height:23px;font-size:1px;line-height:0;">' +
        'hello world</div>');
    myIframeDoc.close();
  },

  tearDownPage() {
    document.body.removeChild(divForTestingScrolling);
  },

  tearDown() {
    window.scrollTo(0, 0);
    stubs.reset();
  },

  testDom() {
    assert('Dom library exists', typeof googDom != 'undefined');
  },

  testGetElement() {
    const el = $('testEl');
    assertEquals('Should be able to get id', el.id, 'testEl');

    assertEquals($, googDom.getElement);
    assertEquals(googDom.$, googDom.getElement);
  },

  testGetElementDomHelper() {
    const domHelper = new DomHelper();
    const el = domHelper.getElement('testEl');
    assertEquals('Should be able to get id', el.id, 'testEl');
  },

  testGetRequiredElement() {
    const el = googDom.getRequiredElement('testEl');
    assertTrue(el != null);
    assertEquals('testEl', el.id);
    assertThrows(() => {
      googDom.getRequiredElement('does_not_exist');
    });
  },

  testGetRequiredElementDomHelper() {
    const domHelper = new DomHelper();
    const el = domHelper.getRequiredElement('testEl');
    assertTrue(el != null);
    assertEquals('testEl', el.id);
    assertThrows(/**
                    @suppress {undefinedVars} suppression added to enable type
                    checking
                  */
                 () => {
                   googDom.getRequiredElementByClass(
                       'does_not_exist', container);
                 });
  },

  testGetRequiredElementByClassDomHelper() {
    const domHelper = new DomHelper();
    assertNotNull(domHelper.getRequiredElementByClass('test1'));
    assertNotNull(domHelper.getRequiredElementByClass('test2'));

    const container = domHelper.getElement('span-container');
    assertNotNull(domHelper.getElementByClass('test1', container));
    assertThrows(/**
                    @suppress {checkTypes} suppression added to enable type
                    checking
                  */
                 () => {
                   domHelper.getRequiredElementByClass(
                       'does_not_exist', container);
                 });
  },

  testGetElementsByTagName() {
    const divs = googDom.getElementsByTagName(TagName.DIV);
    assertTrue(divs.length > 0);
    const el = googDom.getRequiredElement('testEl');
    const spans = googDom.getElementsByTagName(TagName.SPAN, el);
    assertTrue(spans.length > 0);
  },

  testGetElementsByTagNameDomHelper() {
    const domHelper = new DomHelper();
    const divs = domHelper.getElementsByTagName(TagName.DIV);
    assertTrue(divs.length > 0);
    const el = domHelper.getRequiredElement('testEl');
    const spans = domHelper.getElementsByTagName(TagName.SPAN, el);
    assertTrue(spans.length > 0);
  },

  testGetElementsByTagNameAndClass() {
    assertEquals(
        'Should get 6 spans',
        googDom.getElementsByTagNameAndClass(TagName.SPAN).length, 6);
    assertEquals(
        'Should get 6 spans',
        googDom.getElementsByTagNameAndClass(TagName.SPAN).length, 6);
    assertEquals(
        'Should get 3 spans',
        googDom.getElementsByTagNameAndClass(TagName.SPAN, 'test1').length, 3);
    assertEquals(
        'Should get 1 span',
        googDom.getElementsByTagNameAndClass(TagName.SPAN, 'test2').length, 1);
    assertEquals(
        'Should get 1 span',
        googDom.getElementsByTagNameAndClass(TagName.SPAN, 'test2').length, 1);
    assertEquals(
        'Should get lots of elements',
        googDom.getElementsByTagNameAndClass().length,
        document.getElementsByTagName('*').length);

    assertEquals(
        'Should get 1 span',
        googDom.getElementsByTagNameAndClass(TagName.SPAN, null, $('testEl'))
            .length,
        1);

    // '*' as the tag name should be equivalent to all tags
    const container = googDom.getElement('span-container');
    assertEquals(
        5,
        googDom.getElementsByTagNameAndClass('*', undefined, container).length);
    assertEquals(
        3,
        googDom.getElementsByTagNameAndClass('*', 'test1', container).length);
    assertEquals(
        1,
        googDom.getElementsByTagNameAndClass('*', 'test2', container).length);

    // Some version of WebKit have problems with mixed-case class names
    assertEquals(
        1,
        googDom.getElementsByTagNameAndClass(undefined, 'mixedCaseClass')
            .length);

    // Make sure that out of bounds indices are OK
    assertUndefined(
        googDom.getElementsByTagNameAndClass(undefined, 'noSuchClass')[0]);

    assertEquals(
        googDom.getElementsByTagNameAndClass,
        googDom.getElementsByTagNameAndClass);
  },

  testGetElementsByClass() {
    assertEquals(3, googDom.getElementsByClass('test1').length);
    assertEquals(1, googDom.getElementsByClass('test2').length);
    assertEquals(0, googDom.getElementsByClass('nonexistant').length);

    const container = googDom.getElement('span-container');
    assertEquals(3, googDom.getElementsByClass('test1', container).length);
  },

  testGetElementByClass() {
    assertNotNull(googDom.getElementByClass('test1'));
    assertNotNull(googDom.getElementByClass('test2'));
    // assertNull(goog.dom.getElementByClass('nonexistant'));

    const container = googDom.getElement('span-container');
    assertNotNull(googDom.getElementByClass('test1', container));
  },

  testGetElementByTagNameAndClass() {
    assertNotNull(googDom.getElementByTagNameAndClass('', 'test1'));
    assertNotNull(googDom.getElementByTagNameAndClass('*', 'test1'));
    assertNotNull(googDom.getElementByTagNameAndClass('span', 'test1'));
    assertNull(googDom.getElementByTagNameAndClass('div', 'test1'));
    assertNull(googDom.getElementByTagNameAndClass('*', 'nonexistant'));

    const container = googDom.getElement('span-container');
    assertNotNull(googDom.getElementByTagNameAndClass('*', 'test1', container));
  },

  /**
     @suppress {strictMissingProperties,missingProperties} suppression added to
     enable type checking
   */
  testSetProperties() {
    const attrs = {
      'name': 'test3',
      'title': 'A title',
      'random': 'woop',
      'other-random': null,
      'href': SafeUrl.sanitize('https://google.com'),
      'stringWithTypedStringProp': 'http://example.com/',
      'numberWithTypedStringProp': 123,
      'booleanWithTypedStringProp': true,
    };

    // TODO(johnlenz): Attempting to set an property on a primitive throws in
    // strict mode
    /*
    // Primitives with properties that wrongly indicate that the text is of a
    // type that implements `goog.string.TypedString`. This simulates a property
    // renaming collision with a String, Number or Boolean property set
    // externally. renaming collision with a String property set externally
    // (b/80124112).
    attrs['stringWithTypedStringProp'].implementsGoogStringTypedString = true;
    attrs['numberWithTypedStringProp'].implementsGoogStringTypedString = true;
    attrs['booleanWithTypedStringProp'].implementsGoogStringTypedString = true;
    */

    const el = $('testEl');
    googDom.setProperties(el, attrs);
    assertEquals('test3', el.name);
    assertEquals('A title', el.title);
    assertEquals('woop', el.random);
    assertEquals('https://google.com', el.href);
    assertEquals('http://example.com/', el.stringWithTypedStringProp);
    assertEquals(123, el.numberWithTypedStringProp);
    assertEquals(true, el.booleanWithTypedStringProp);
  },

  testSetPropertiesDirectAttributeMap() {
    const attrs = {'usemap': '#myMap'};
    const el = googDom.createDom(TagName.IMG);

    const res = googDom.setProperties(el, attrs);
    assertEquals('Should be equal', '#myMap', el.getAttribute('usemap'));
  },

  testSetPropertiesDirectAttributeMapChecksForOwnProperties() {
    stubs.set(Object.prototype, 'customProp', 'sdflasdf.,m.,<>fsdflas213!@#');
    const attrs = {'usemap': '#myMap'};
    const el = googDom.createDom(TagName.IMG);

    const res = googDom.setProperties(el, attrs);
    assertEquals('Should be equal', '#myMap', el.getAttribute('usemap'));
  },

  testSetPropertiesAria() {
    const attrs = {
      'aria-hidden': 'true',
      'aria-label': 'This is a label',
      'role': 'presentation',
    };
    const el = googDom.createDom(TagName.DIV);

    googDom.setProperties(el, attrs);
    assertEquals('Should be equal', 'true', el.getAttribute('aria-hidden'));
    assertEquals(
        'Should be equal', 'This is a label', el.getAttribute('aria-label'));
    assertEquals('Should be equal', 'presentation', el.getAttribute('role'));
  },

  testSetPropertiesData() {
    const attrs = {
      'data-tooltip': 'This is a tooltip',
      'data-tooltip-delay': '100',
    };
    const el = googDom.createDom(TagName.DIV);

    googDom.setProperties(el, attrs);
    assertEquals(
        'Should be equal', 'This is a tooltip',
        el.getAttribute('data-tooltip'));
    assertEquals(
        'Should be equal', '100', el.getAttribute('data-tooltip-delay'));
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSetTableProperties() {
    const attrs = {
      'style': 'padding-left: 10px;',
      'class': 'mytestclass',
      'height': '101',
      'cellpadding': '15',
    };
    const el = $('testTable1');

    const res = googDom.setProperties(el, attrs);
    assertEquals('Should be equal', el.style.paddingLeft, '10px');
    assertEquals('Should be equal', el.className, 'mytestclass');
    assertEquals('Should be equal', el.getAttribute('height'), '101');
    assertEquals('Should be equal', el.cellPadding, '15');
  },

  testGetViewportSize() {
    // TODO: This is failing in the test runner now, fix later.
    // var dims = getViewportSize();
    // assertNotUndefined('Should be defined at least', dims.width);
    // assertNotUndefined('Should be defined at least', dims.height);
  },

  testGetViewportSizeInIframe() {
    const iframe =
        /** @type {HTMLIFrameElement} */ (googDom.getElement('iframe'));
    const contentDoc = googDom.getFrameContentDocument(iframe);
    const outerSize = googDom.getViewportSize();
    const innerSize = (new DomHelper(contentDoc)).getViewportSize();
    assert('Viewport sizes must not match', innerSize.width != outerSize.width);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetDocumentHeightInIframe() {
    const doc = googDom.getDomHelper(myIframeDoc).getDocument();
    const height = googDom.getDomHelper(myIframeDoc).getDocumentHeight();

    // Broken in webkit/edge quirks mode and in IE8+
    if ((googDom.isCss1CompatMode_(doc) ||
         !userAgent.WEBKIT && !userAgent.EDGE) &&
        !isIE8OrHigher()) {
      assertEquals('height should be 65', 42 + 23, height);
    }
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testCreateDom() {
    const el = googDom.createDom(
        TagName.DIV, {
          style: 'border: 1px solid black; width: 50%; background-color: #EEE;',
          onclick: 'alert(\'woo\')',
        },
        googDom.createDom(
            TagName.P, {style: 'font: normal 12px arial; color: red; '},
            'Para 1'),
        googDom.createDom(
            TagName.P, {style: 'font: bold 18px garamond; color: blue; '},
            'Para 2'),
        googDom.createDom(
            TagName.P, {style: 'font: normal 24px monospace; color: green'},
            'Para 3 ',
            googDom.createDom(
                TagName.A, {
                  name: 'link',
                  href: SafeUrl.sanitize('http://bbc.co.uk/'),
                },
                'has a link'),
            ', how cool is this?'));

    assertEquals('Tagname should be a DIV', String(TagName.DIV), el.tagName);
    assertEquals('Style width should be 50%', '50%', el.style.width);
    assertEquals(
        'first child is a P tag', String(TagName.P), el.childNodes[0].tagName);
    assertEquals(
        'second child .innerHTML', 'Para 2', el.childNodes[1].innerHTML);
    assertEquals(
        'Link href as SafeUrl', 'http://bbc.co.uk/',
        el.childNodes[2].childNodes[1].href);
  },

  testCreateDomNoChildren() {
    let el;

    // Test unspecified children.
    el = googDom.createDom(TagName.DIV);
    assertNull('firstChild should be null', el.firstChild);

    // Test null children.
    el = googDom.createDom(TagName.DIV, null, null);
    assertNull('firstChild should be null', el.firstChild);

    // Test empty array of children.
    el = googDom.createDom(TagName.DIV, null, []);
    assertNull('firstChild should be null', el.firstChild);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testCreateDomAcceptsArray() {
    const items = [
      googDom.createDom(TagName.LI, {}, 'Item 1'),
      googDom.createDom(TagName.LI, {}, 'Item 2'),
    ];
    const ul = googDom.createDom(TagName.UL, {}, items);
    assertEquals('List should have two children', 2, ul.childNodes.length);
    assertEquals(
        'First child should be an LI tag', String(TagName.LI),
        ul.firstChild.tagName);
    assertEquals('Item 1', ul.childNodes[0].innerHTML);
    assertEquals('Item 2', ul.childNodes[1].innerHTML);
  },

  testCreateDomStringArg() {
    let el;

    // Test string arg.
    el = googDom.createDom(TagName.DIV, null, 'Hello');
    assertEquals(
        'firstChild should be a text node', NodeType.TEXT,
        el.firstChild.nodeType);
    assertEquals(
        'firstChild should have node value "Hello"', 'Hello',
        el.firstChild.nodeValue);

    // Test text node arg.
    el = googDom.createDom(TagName.DIV, null, googDom.createTextNode('World'));
    assertEquals(
        'firstChild should be a text node', NodeType.TEXT,
        el.firstChild.nodeType);
    assertEquals(
        'firstChild should have node value "World"', 'World',
        el.firstChild.nodeValue);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testCreateDomNodeListArg() {
    let el;
    const emptyElem = googDom.createDom(TagName.DIV);
    const simpleElem = googDom.createDom(TagName.DIV, null, 'Hello, world!');
    const complexElem = googDom.createDom(
        TagName.DIV, null, 'Hello, ',
        googDom.createDom(TagName.B, null, 'world'),
        googDom.createTextNode('!'));

    // Test empty node list.
    el = googDom.createDom(TagName.DIV, null, emptyElem.childNodes);
    assertNull('emptyElem.firstChild should be null', emptyElem.firstChild);
    assertNull('firstChild should be null', el.firstChild);

    // Test simple node list.
    el = googDom.createDom(TagName.DIV, null, simpleElem.childNodes);
    assertNull('simpleElem.firstChild should be null', simpleElem.firstChild);
    assertEquals(
        'firstChild should be a text node with value "Hello, world!"',
        'Hello, world!', el.firstChild.nodeValue);

    // Test complex node list.
    el = googDom.createDom(TagName.DIV, null, complexElem.childNodes);
    assertNull('complexElem.firstChild should be null', complexElem.firstChild);
    assertEquals('Element should have 3 child nodes', 3, el.childNodes.length);
    assertEquals(
        'childNodes[0] should be a text node with value "Hello, "', 'Hello, ',
        el.childNodes[0].nodeValue);
    assertEquals(
        'childNodes[1] should be an element node with tagName "B"',
        String(TagName.B), el.childNodes[1].tagName);
    assertEquals(
        'childNodes[2] should be a text node with value "!"', '!',
        el.childNodes[2].nodeValue);
  },

  testCreateDomWithTypeAttribute() {
    const el = googDom.createDom(
        TagName.BUTTON, {'type': InputType.RESET, 'id': 'cool-button'},
        'Cool button');
    assertNotNull('Button with type attribute was created successfully', el);
    assertEquals('Button has correct type attribute', InputType.RESET, el.type);
    assertEquals('Button has correct id', 'cool-button', el.id);
  },

  testCreateDomWithClassList() {
    const el = googDom.createDom(TagName.DIV, ['foo', 'bar']);
    assertEquals('foo bar', el.className);
  },

  testContains() {
    assertTrue(
        'HTML should contain BODY',
        googDom.contains(document.documentElement, document.body));
    assertTrue(
        'Document should contain BODY',
        googDom.contains(document, document.body));

    const d = googDom.createDom(TagName.P, null, 'A paragraph');
    const t = d.firstChild;
    assertTrue('Same element', googDom.contains(d, d));
    assertTrue('Same text', googDom.contains(t, t));
    assertTrue('Nested text', googDom.contains(d, t));
    assertFalse('Nested text, reversed', googDom.contains(t, d));
    assertFalse('Disconnected element', googDom.contains(document, d));
    googDom.appendChild(document.body, d);
    assertTrue('Connected element', googDom.contains(document, d));
    googDom.removeNode(d);
  },

  testCreateDomWithClassName() {
    let el = googDom.createDom(TagName.DIV, 'cls');
    assertNull('firstChild should be null', el.firstChild);
    assertEquals('Tagname should be a DIV', String(TagName.DIV), el.tagName);
    assertEquals('ClassName should be cls', 'cls', el.className);

    el = googDom.createDom(TagName.DIV, '');
    assertEquals('ClassName should be empty', '', el.className);
  },

  testCompareNodeOrder() {
    const b1 = $('b1');
    const b2 = $('b2');
    const p2 = $('p2');

    assertEquals(
        'equal nodes should compare to 0', 0, googDom.compareNodeOrder(b1, b1));

    assertTrue(
        'parent should come before child',
        googDom.compareNodeOrder(p2, b1) < 0);
    assertTrue(
        'child should come after parent', googDom.compareNodeOrder(b1, p2) > 0);

    assertTrue(
        'parent should come before text child',
        googDom.compareNodeOrder(b1, b1.firstChild) < 0);
    assertTrue(
        'text child should come after parent',
        googDom.compareNodeOrder(b1.firstChild, b1) > 0);

    assertTrue(
        'first sibling should come before second',
        googDom.compareNodeOrder(b1, b2) < 0);
    assertTrue(
        'second sibling should come after first',
        googDom.compareNodeOrder(b2, b1) > 0);

    assertTrue(
        'text node after cousin element returns correct value',
        googDom.compareNodeOrder(b1.nextSibling, b1) > 0);
    assertTrue(
        'text node before cousin element returns correct value',
        googDom.compareNodeOrder(b1, b1.nextSibling) < 0);

    assertTrue(
        'text node is before once removed cousin element',
        googDom.compareNodeOrder(b1.firstChild, b2) < 0);
    assertTrue(
        'once removed cousin element is before text node',
        googDom.compareNodeOrder(b2, b1.firstChild) > 0);

    assertTrue(
        'text node is after once removed cousin text node',
        googDom.compareNodeOrder(b1.nextSibling, b1.firstChild) > 0);
    assertTrue(
        'once removed cousin text node is before text node',
        googDom.compareNodeOrder(b1.firstChild, b1.nextSibling) < 0);

    assertTrue(
        'first text node is before second text node',
        googDom.compareNodeOrder(b1.previousSibling, b1.nextSibling) < 0);
    assertTrue(
        'second text node is after first text node',
        googDom.compareNodeOrder(b1.nextSibling, b1.previousSibling) > 0);

    assertTrue(
        'grandchild is after grandparent',
        googDom.compareNodeOrder(b1.firstChild, b1.parentNode) > 0);
    assertTrue(
        'grandparent is after grandchild',
        googDom.compareNodeOrder(b1.parentNode, b1.firstChild) < 0);

    assertTrue(
        'grandchild is after grandparent',
        googDom.compareNodeOrder(b1.firstChild, b1.parentNode) > 0);
    assertTrue(
        'grandparent is after grandchild',
        googDom.compareNodeOrder(b1.parentNode, b1.firstChild) < 0);

    assertTrue(
        'second cousins compare correctly',
        googDom.compareNodeOrder(b1.firstChild, b2.firstChild) < 0);
    assertTrue(
        'second cousins compare correctly in reverse',
        googDom.compareNodeOrder(b2.firstChild, b1.firstChild) > 0);

    assertTrue(
        'testEl2 is after testEl',
        googDom.compareNodeOrder($('testEl2'), $('testEl')) > 0);
    assertTrue(
        'testEl is before testEl2',
        googDom.compareNodeOrder($('testEl'), $('testEl2')) < 0);

    const p = $('order-test');
    const text1 = document.createTextNode('1');
    p.appendChild(text1);
    const text2 = document.createTextNode('1');
    p.appendChild(text2);

    assertEquals(
        'Equal text nodes should compare to 0', 0,
        googDom.compareNodeOrder(text1, text1));
    assertTrue(
        'First text node is before second',
        googDom.compareNodeOrder(text1, text2) < 0);
    assertTrue(
        'Second text node is after first',
        googDom.compareNodeOrder(text2, text1) > 0);
    assertTrue(
        'Late text node is after b1',
        googDom.compareNodeOrder(text1, $('b1')) > 0);

    assertTrue(
        'Document node is before non-document node',
        googDom.compareNodeOrder(document, b1) < 0);
    assertTrue(
        'Non-document node is after document node',
        googDom.compareNodeOrder(b1, document) > 0);
  },

  testFindCommonAncestor() {
    const b1 = $('b1');
    const b2 = $('b2');
    const p1 = $('p1');
    const p2 = $('p2');
    const testEl2 = $('testEl2');

    assertNull('findCommonAncestor() = null', googDom.findCommonAncestor());
    assertEquals(
        'findCommonAncestor(b1) = b1', b1, googDom.findCommonAncestor(b1));
    assertEquals(
        'findCommonAncestor(b1, b1) = b1', b1,
        googDom.findCommonAncestor(b1, b1));
    assertEquals(
        'findCommonAncestor(b1, b2) = p2', p2,
        googDom.findCommonAncestor(b1, b2));
    assertEquals(
        'findCommonAncestor(p1, b2) = body', document.body,
        googDom.findCommonAncestor(p1, b2));
    assertEquals(
        'findCommonAncestor(testEl2, b1, b2, p1, p2) = body', document.body,
        googDom.findCommonAncestor(testEl2, b1, b2, p1, p2));

    const outOfDoc = googDom.createElement(TagName.DIV);
    assertNull(
        'findCommonAncestor(outOfDoc, b1) = null',
        googDom.findCommonAncestor(outOfDoc, b1));
  },

  testRemoveNode() {
    const b = googDom.createElement(TagName.B);
    const el = $('p1');
    el.appendChild(b);
    googDom.removeNode(b);
    assertTrue('b should have been removed', el.lastChild != b);
  },

  testReplaceNode() {
    const n = $('toReplace');
    const previousSibling = n.previousSibling;
    const goodNode = googDom.createDom(TagName.DIV, {'id': 'goodReplaceNode'});
    googDom.replaceNode(goodNode, n);

    assertEquals(
        'n should have been replaced', previousSibling.nextSibling, goodNode);
    assertNull('n should no longer be in the DOM tree', $('toReplace'));

    const badNode = googDom.createDom(TagName.DIV, {'id': 'badReplaceNode'});
    googDom.replaceNode(badNode, n);
    assertNull('badNode should not be in the DOM tree', $('badReplaceNode'));
  },

  testCopyContents() {
    const target =
        googDom.createDom('div', {}, 'a', googDom.createDom('span', {}, 'b'));
    const source =
        googDom.createDom('div', {}, googDom.createDom('span', {}, 'c'), 'd');
    googDom.copyContents(target, source);
    assertEquals('cd', target.textContent);
    assertEquals('cd', source.textContent);
    assertEquals('c', target.firstChild.textContent);
    googDom.copyContents(source, source);
    assertEquals('cd', source.textContent);
    assertEquals('c', source.firstChild.textContent);
  },

  testInsertChildAt() {
    const parent = $('p2');
    const origNumChildren = parent.childNodes.length;

    // Append, with last index.
    const child1 = googDom.createElement(TagName.DIV);
    googDom.insertChildAt(parent, child1, origNumChildren);
    assertEquals(origNumChildren + 1, parent.childNodes.length);
    assertEquals(child1, parent.childNodes[parent.childNodes.length - 1]);

    // Append, with value larger than last index.
    const child2 = googDom.createElement(TagName.DIV);
    googDom.insertChildAt(parent, child2, origNumChildren + 42);
    assertEquals(origNumChildren + 2, parent.childNodes.length);
    assertEquals(child2, parent.childNodes[parent.childNodes.length - 1]);

    // Prepend.
    const child3 = googDom.createElement(TagName.DIV);
    googDom.insertChildAt(parent, child3, 0);
    assertEquals(origNumChildren + 3, parent.childNodes.length);
    assertEquals(child3, parent.childNodes[0]);

    // Self move (no-op).
    googDom.insertChildAt(parent, child3, 0);
    assertEquals(origNumChildren + 3, parent.childNodes.length);
    assertEquals(child3, parent.childNodes[0]);

    // Move.
    googDom.insertChildAt(parent, child3, 2);
    assertEquals(origNumChildren + 3, parent.childNodes.length);
    assertEquals(child3, parent.childNodes[1]);

    parent.removeChild(child1);
    parent.removeChild(child2);
    parent.removeChild(child3);

    const emptyParentNotInDocument = googDom.createElement(TagName.DIV);
    googDom.insertChildAt(emptyParentNotInDocument, child1, 0);
    assertEquals(1, emptyParentNotInDocument.childNodes.length);
  },

  testFlattenElement() {
    const text = document.createTextNode('Text');
    const br = googDom.createElement(TagName.BR);
    const span = googDom.createDom(TagName.SPAN, null, text, br);
    assertEquals('span should have 2 children', 2, span.childNodes.length);

    const el = $('p1');
    el.appendChild(span);

    const ret = googDom.flattenElement(span);

    assertTrue('span should have been removed', el.lastChild != span);
    assertFalse(
        'span should have no parent',
        !!span.parentNode &&
            span.parentNode.nodeType != NodeType.DOCUMENT_FRAGMENT);
    assertEquals('span should have no children', 0, span.childNodes.length);
    assertEquals('Last child of p should be br', br, el.lastChild);
    assertEquals(
        'Previous sibling of br should be text', text, br.previousSibling);

    const outOfDoc = googDom.createDom(TagName.SPAN, null, '1 child');
    // Should do nothing.
    googDom.flattenElement(outOfDoc);
    assertEquals(
        'outOfDoc should still have 1 child', 1, outOfDoc.childNodes.length);
  },

  testIsNodeLike() {
    assertTrue('document should be node like', googDom.isNodeLike(document));
    assertTrue(
        'document.body should be node like', googDom.isNodeLike(document.body));
    assertTrue(
        'a text node should be node like',
        googDom.isNodeLike(document.createTextNode('')));

    assertFalse('null should not be node like', googDom.isNodeLike(null));
    assertFalse('a string should not be node like', googDom.isNodeLike('abcd'));

    assertTrue(
        'custom object should be node like', googDom.isNodeLike({nodeType: 1}));
  },

  testIsElement() {
    assertFalse('document is not an element', googDom.isElement(document));
    assertTrue('document.body is an element', googDom.isElement(document.body));
    assertFalse(
        'a text node is not an element',
        googDom.isElement(document.createTextNode('')));
    assertTrue(
        'an element created with createElement() is an element',
        googDom.isElement(googDom.createElement(TagName.A)));

    assertFalse('null is not an element', googDom.isElement(null));
    assertFalse('a string is not an element', googDom.isElement('abcd'));

    assertTrue('custom object is an element', googDom.isElement({nodeType: 1}));
    assertFalse(
        'custom non-element object is a not an element',
        googDom.isElement({someProperty: 'somevalue'}));
  },

  testIsWindow() {
    const global = globalThis;
    const frame = window.frames['frame'];
    const otherWindow = window.open('', 'blank');
    const object = {window: globalThis};
    const nullVar = null;
    let notDefined;

    try {
      // Use try/finally to ensure that we clean up the window we open, even if
      // an assertion fails or something else goes wrong.
      assertTrue(
          'global object in HTML context should be a window',
          googDom.isWindow(globalThis));
      assertTrue('iframe window should be a window', googDom.isWindow(frame));
      if (otherWindow) {
        assertTrue(
            'other window should be a window', googDom.isWindow(otherWindow));
      }
      assertFalse('object should not be a window', googDom.isWindow(object));
      assertFalse('null should not be a window', googDom.isWindow(nullVar));
      assertFalse(
          'undefined should not be a window', googDom.isWindow(notDefined));
    } finally {
      if (otherWindow) {
        otherWindow.close();
      }
    }
  },

  testIsInDocument() {
    assertThrows(() => {
      googDom.isInDocument(document);
    });

    assertTrue(googDom.isInDocument(document.documentElement));

    const div = document.createElement('div');
    assertFalse(googDom.isInDocument(div));
    document.body.appendChild(div);
    assertTrue(googDom.isInDocument(div));

    const textNode = document.createTextNode('');
    assertFalse(googDom.isInDocument(textNode));
    div.appendChild(textNode);
    assertTrue(googDom.isInDocument(textNode));

    const attribute = document.createAttribute('a');
    assertFalse(googDom.isInDocument(attribute));
    div.setAttributeNode(attribute);
    assertTrue(googDom.isInDocument(attribute));
  },

  testGetOwnerDocument() {
    assertEquals(googDom.getOwnerDocument($('p1')), document);
    assertEquals(googDom.getOwnerDocument(document.body), document);
    assertEquals(googDom.getOwnerDocument(document.documentElement), document);
  },

  // Tests the breakages resulting in rollback cl/64715474
  testGetOwnerDocumentNonNodeInput() {
    // We should fail on null.
    assertThrows(() => {
      googDom.getOwnerDocument(null);
    });
    assertEquals(document, googDom.getOwnerDocument(window));
  },

  testDomHelper() {
    const x = new DomHelper(window.frames['frame'].document);
    assertTrue(
        'Should have some HTML', x.getDocument().body.innerHTML.length > 0);
  },

  testGetFirstElementChild() {
    const p2 = $('p2');
    let b1 = googDom.getFirstElementChild(p2);
    assertNotNull('First element child of p2 should not be null', b1);
    assertEquals('First element child is b1', 'b1', b1.id);

    const c = googDom.getFirstElementChild(b1);
    assertNull('First element child of b1 should be null', c);

    // Test with an undefined firstElementChild attribute.
    const b2 = $('b2');
    const mockP2 = {
      childNodes: [b1, b2],
      firstChild: b1,
      firstElementChild: undefined,
    };

    /** @suppress {checkTypes} suppression added to enable type checking */
    b1 = googDom.getFirstElementChild(mockP2);
    assertNotNull('First element child of mockP2 should not be null', b1);
    assertEquals('First element child is b1', 'b1', b1.id);
  },

  testGetLastElementChild() {
    const p2 = $('p2');
    let b2 = googDom.getLastElementChild(p2);
    assertNotNull('Last element child of p2 should not be null', b2);
    assertEquals('Last element child is b2', 'b2', b2.id);

    const c = googDom.getLastElementChild(b2);
    assertNull('Last element child of b2 should be null', c);

    // Test with an undefined lastElementChild attribute.
    const b1 = $('b1');
    const mockP2 = {
      childNodes: [b1, b2],
      lastChild: b2,
      lastElementChild: undefined,
    };

    /** @suppress {checkTypes} suppression added to enable type checking */
    b2 = googDom.getLastElementChild(mockP2);
    assertNotNull('Last element child of mockP2 should not be null', b2);
    assertEquals('Last element child is b2', 'b2', b2.id);
  },

  testGetNextElementSibling() {
    const b1 = $('b1');
    let b2 = googDom.getNextElementSibling(b1);
    assertNotNull('Next element sibling of b1 should not be null', b1);
    assertEquals('Next element sibling is b2', 'b2', b2.id);

    const c = googDom.getNextElementSibling(b2);
    assertNull('Next element sibling of b2 should be null', c);

    // Test with an undefined nextElementSibling attribute.
    const mockB1 = {nextSibling: b2, nextElementSibling: undefined};

    /** @suppress {checkTypes} suppression added to enable type checking */
    b2 = googDom.getNextElementSibling(mockB1);
    assertNotNull('Next element sibling of mockB1 should not be null', b1);
    assertEquals('Next element sibling is b2', 'b2', b2.id);
  },

  testGetPreviousElementSibling() {
    const b2 = $('b2');
    let b1 = googDom.getPreviousElementSibling(b2);
    assertNotNull('Previous element sibling of b2 should not be null', b1);
    assertEquals('Previous element sibling is b1', 'b1', b1.id);

    const c = googDom.getPreviousElementSibling(b1);
    assertNull('Previous element sibling of b1 should be null', c);

    // Test with an undefined previousElementSibling attribute.
    const mockB2 = {previousSibling: b1, previousElementSibling: undefined};

    /** @suppress {checkTypes} suppression added to enable type checking */
    b1 = googDom.getPreviousElementSibling(mockB2);
    assertNotNull('Previous element sibling of mockB2 should not be null', b1);
    assertEquals('Previous element sibling is b1', 'b1', b1.id);
  },

  testGetChildren() {
    const p2 = $('p2');
    let children = googDom.getChildren(p2);
    assertNotNull('Elements array should not be null', children);
    assertEquals(
        'List of element children should be length two.', 2, children.length);

    const b1 = $('b1');
    const b2 = $('b2');
    assertObjectEquals('First element child should be b1.', b1, children[0]);
    assertObjectEquals('Second element child should be b2.', b2, children[1]);

    const noChildren = googDom.getChildren(b1);
    assertNotNull('Element children array should not be null', noChildren);
    assertEquals(
        'List of element children should be length zero.', 0,
        noChildren.length);

    // Test with an undefined children attribute.
    const mockP2 = {childNodes: [b1, b2], children: undefined};

    /** @suppress {checkTypes} suppression added to enable type checking */
    children = googDom.getChildren(mockP2);
    assertNotNull('Elements array should not be null', children);
    assertEquals(
        'List of element children should be length two.', 2, children.length);

    assertObjectEquals('First element child should be b1.', b1, children[0]);
    assertObjectEquals('Second element child should be b2.', b2, children[1]);
  },

  testGetNextNode() {
    const tree = googDom.safeHtmlToNode(testing.newSafeHtmlForTest(
        '<div>' +
        '<p>Some text</p>' +
        '<blockquote>Some <i>special</i> <b>text</b></blockquote>' +
        '<address><!-- comment -->Foo</address>' +
        '</div>'));

    assertNull(googDom.getNextNode(null));

    let node = tree;
    const next = () => node = googDom.getNextNode(node);

    assertEquals(String(TagName.P), next().tagName);
    assertEquals('Some text', next().nodeValue);
    assertEquals(String(TagName.BLOCKQUOTE), next().tagName);
    assertEquals('Some ', next().nodeValue);
    assertEquals(String(TagName.I), next().tagName);
    assertEquals('special', next().nodeValue);
    assertEquals(' ', next().nodeValue);
    assertEquals(String(TagName.B), next().tagName);
    assertEquals('text', next().nodeValue);
    assertEquals(String(TagName.ADDRESS), next().tagName);
    assertEquals(NodeType.COMMENT, next().nodeType);
    assertEquals('Foo', next().nodeValue);

    assertNull(next());
  },

  testGetPreviousNode() {
    const tree = googDom.safeHtmlToNode(testing.newSafeHtmlForTest(
        '<div>' +
        '<p>Some text</p>' +
        '<blockquote>Some <i>special</i> <b>text</b></blockquote>' +
        '<address><!-- comment -->Foo</address>' +
        '</div>'));

    assertNull(googDom.getPreviousNode(null));

    let node = tree.lastChild.lastChild;
    const previous = () => node = googDom.getPreviousNode(node);

    assertEquals(NodeType.COMMENT, previous().nodeType);
    assertEquals(String(TagName.ADDRESS), previous().tagName);
    assertEquals('text', previous().nodeValue);
    assertEquals(String(TagName.B), previous().tagName);
    assertEquals(' ', previous().nodeValue);
    assertEquals('special', previous().nodeValue);
    assertEquals(String(TagName.I), previous().tagName);
    assertEquals('Some ', previous().nodeValue);
    assertEquals(String(TagName.BLOCKQUOTE), previous().tagName);
    assertEquals('Some text', previous().nodeValue);
    assertEquals(String(TagName.P), previous().tagName);
    assertEquals(String(TagName.DIV), previous().tagName);

    if (!userAgent.IE) {
      // Internet Explorer maintains a parentNode for Elements after they are
      // removed from the hierarchy. Everyone else agrees on a null parentNode.
      assertNull(previous());
    }
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSetTextContent() {
    const p1 = $('p1');
    let s = 'hello world';
    googDom.setTextContent(p1, s);
    assertEquals(
        'We should have one childNode after setTextContent', 1,
        p1.childNodes.length);
    assertEquals(s, p1.firstChild.data);
    assertEquals(s, p1.innerHTML);

    s = 'four elefants < five ants';
    const sHtml = 'four elefants &lt; five ants';
    googDom.setTextContent(p1, s);
    assertEquals(
        'We should have one childNode after setTextContent', 1,
        p1.childNodes.length);
    assertEquals(s, p1.firstChild.data);
    assertEquals(sHtml, p1.innerHTML);

    // ensure that we remove existing children
    p1.innerHTML = 'a<b>b</b>c';
    s = 'hello world';
    googDom.setTextContent(p1, s);
    assertEquals(
        'We should have one childNode after setTextContent', 1,
        p1.childNodes.length);
    assertEquals(s, p1.firstChild.data);

    // same but start with an element
    p1.innerHTML = '<b>a</b>b<i>c</i>';
    s = 'hello world';
    googDom.setTextContent(p1, s);
    assertEquals(
        'We should have one childNode after setTextContent', 1,
        p1.childNodes.length);
    assertEquals(s, p1.firstChild.data);

    // Text/CharacterData
    googDom.setTextContent(p1, 'before');
    s = 'after';
    googDom.setTextContent(p1.firstChild, s);
    assertEquals(
        'We should have one childNode after setTextContent', 1,
        p1.childNodes.length);
    assertEquals(s, p1.firstChild.data);

    // DocumentFragment
    const df = document.createDocumentFragment();
    s = 'hello world';
    googDom.setTextContent(df, s);
    assertEquals(
        'We should have one childNode after setTextContent', 1,
        df.childNodes.length);
    assertEquals(s, df.firstChild.data);

    // clean up
    googDom.removeChildren(p1);
  },

  testFindNode() {
    let expected = document.body;
    let result = googDom.findNode(
        document,
        /**
           @suppress {strictMissingProperties} suppression added to enable type
           checking
         */
        (n) => n.nodeType == NodeType.ELEMENT && n.tagName == TagName.BODY);
    assertEquals(expected, result);

    expected = googDom.getElementsByTagName(TagName.P)[0];
    result = googDom.findNode(
        document,
        /**
           @suppress {strictMissingProperties} suppression added to enable type
           checking
         */
        (n) => n.nodeType == NodeType.ELEMENT && n.tagName == TagName.P);
    assertEquals(expected, result);

    result = googDom.findNode(document, (n) => false);
    assertUndefined(result);
  },

  testFindElement_works() {
    const isBody = (element) => element.tagName == 'BODY';
    const isP = (element) => element.tagName == 'P';
    const firstP = document.querySelector('p');
    const htmlElement = document.documentElement;

    // root is an element
    assertNull(googDom.findElement(document.body, functions.FALSE));
    assertEquals(firstP, googDom.findElement(document.body, isP));

    // root is the document
    assertEquals(htmlElement, googDom.findElement(document, functions.TRUE));
    assertNull(googDom.findElement(document, functions.FALSE));
    assertEquals(document.body, googDom.findElement(document, isBody));
    assertEquals(firstP, googDom.findElement(document, isP));
  },

  testFindElement_excludesRootElement() {
    assertNull(googDom.findElement(
        document.body, (element) => element.tagName == 'BODY'));
  },

  testFindElement_onlyCallsFilterFunctionWithElements() {
    googDom.findElement(document, (param) => {
      asserts.assertElement(param);
      return false;  // to visit all nodes
    });
  },

  testFindNodes() {
    const expected = googDom.getElementsByTagName(TagName.P);
    let result = googDom.findNodes(
        document,
        /**
           @suppress {strictMissingProperties} suppression added to enable type
           checking
         */
        (n) => n.nodeType == NodeType.ELEMENT && n.tagName == TagName.P);
    assertEquals(expected.length, result.length);
    assertEquals(expected[0], result[0]);
    assertEquals(expected[1], result[1]);

    result = googDom.findNodes(document, (n) => false).length;
    assertEquals(0, result);
  },

  testFindElements_works() {
    const isP = (element) => element.tagName == 'P';

    assertArrayEquals([], googDom.findElements(document, functions.FALSE));

    // Should return the elements in the same order as getElementsByTagName.
    assertArrayEquals(
        googArray.toArray(document.getElementsByTagName('p')),
        googDom.findElements(document, isP));
    assertArrayEquals(
        googArray.toArray(document.getElementsByTagName('*')),
        googDom.findElements(document, functions.TRUE));
    assertArrayEquals(
        googArray.toArray(document.body.getElementsByTagName('*')),
        googDom.findElements(document.body, functions.TRUE));
  },

  testFindElements_excludesRootElement() {
    const isBody = (element) => element.tagName == 'BODY';

    assertArrayEquals(
        [document.body],
        googDom.findElements(document.documentElement, isBody));
    assertArrayEquals([], googDom.findElements(document.body, isBody));
  },

  testFindElements_onlyCallsFilterFunctionWithElements() {
    googDom.findElements(document, (param) => {
      asserts.assertElement(param);
      return false;  // to visit all nodes
    });
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testIsFocusableTabIndex() {
    assertFalse(
        'isFocusableTabIndex() must be false for no tab index',
        googDom.isFocusableTabIndex(googDom.getElement('noTabIndex')));
    assertFalse(
        'isFocusableTabIndex() must be false for tab index -2',
        googDom.isFocusableTabIndex(googDom.getElement('tabIndexNegative2')));
    assertFalse(
        'isFocusableTabIndex() must be false for tab index -1',
        googDom.isFocusableTabIndex(googDom.getElement('tabIndexNegative1')));

    assertTrue(
        'isFocusableTabIndex() must be true for tab index 0',
        googDom.isFocusableTabIndex(googDom.getElement('tabIndex0')));
    assertTrue(
        'isFocusableTabIndex() must be true for tab index 1',
        googDom.isFocusableTabIndex(googDom.getElement('tabIndex1')));
    assertTrue(
        'isFocusableTabIndex() must be true for tab index 2',
        googDom.isFocusableTabIndex(googDom.getElement('tabIndex2')));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetFocusableTabIndex() {
    // Test enabling focusable tab index.
    googDom.setFocusableTabIndex(googDom.getElement('noTabIndex'), true);
    assertTrue(
        'isFocusableTabIndex() must be true after enabling tab index',
        googDom.isFocusableTabIndex(googDom.getElement('noTabIndex')));

    // Test disabling focusable tab index that was added programmatically.
    googDom.setFocusableTabIndex(googDom.getElement('noTabIndex'), false);
    assertFalse(
        'isFocusableTabIndex() must be false after disabling tab ' +
            'index that was programmatically added',
        googDom.isFocusableTabIndex(googDom.getElement('noTabIndex')));

    // Test disabling focusable tab index that was specified in markup.
    googDom.setFocusableTabIndex(googDom.getElement('tabIndex0'), false);
    assertFalse(
        'isFocusableTabIndex() must be false after disabling tab ' +
            'index that was specified in markup',
        googDom.isFocusableTabIndex(googDom.getElement('tabIndex0')));

    // Test re-enabling focusable tab index.
    googDom.setFocusableTabIndex(googDom.getElement('tabIndex0'), true);
    assertTrue(
        'isFocusableTabIndex() must be true after reenabling tabindex',
        googDom.isFocusableTabIndex(googDom.getElement('tabIndex0')));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testIsFocusable() {
    // Form elements without explicit tab index
    assertFocusable(googDom.getElement('noTabIndexAnchor'));  // <a href>
    assertNotFocusable(googDom.getElement('noTabIndexNoHrefAnchor'));  // <a>
    assertFocusable(googDom.getElement('noTabIndexInput'));     // <input>
    assertFocusable(googDom.getElement('noTabIndexTextArea'));  // <textarea>
    assertFocusable(googDom.getElement('noTabIndexSelect'));    // <select>
    assertFocusable(googDom.getElement('noTabIndexButton'));    // <button>

    // Form elements with explicit tab indices
    assertNotFocusable(googDom.getElement('negTabIndexButton'));  // tabIndex=-1
    assertFocusable(googDom.getElement('zeroTabIndexButton'));    // tabIndex=0
    assertFocusable(googDom.getElement('posTabIndexButton'));     // tabIndex=1

    // Disabled form elements with different tab indices
    assertNotFocusable(googDom.getElement('disabledNoTabIndexButton'));
    assertNotFocusable(googDom.getElement('disabledNegTabIndexButton'));
    assertNotFocusable(googDom.getElement('disabledZeroTabIndexButton'));
    assertNotFocusable(googDom.getElement('disabledPosTabIndexButton'));

    // Test non-form types should return same value as isFocusableTabIndex()
    assertEquals(
        'isFocusable() and isFocusableTabIndex() must agree for ' +
            ' no tab index',
        googDom.isFocusableTabIndex(googDom.getElement('noTabIndex')),
        googDom.isFocusable(googDom.getElement('noTabIndex')));
    assertEquals(
        'isFocusable() and isFocusableTabIndex() must agree for ' +
            ' tab index -2',
        googDom.isFocusableTabIndex(googDom.getElement('tabIndexNegative2')),
        googDom.isFocusable(googDom.getElement('tabIndexNegative2')));
    assertEquals(
        'isFocusable() and isFocusableTabIndex() must agree for ' +
            ' tab index -1',
        googDom.isFocusableTabIndex(googDom.getElement('tabIndexNegative1')),
        googDom.isFocusable(googDom.getElement('tabIndexNegative1')));

    // Make sure IE doesn't throw for detached elements. IE can't measure
    // detached elements, and calling getBoundingClientRect() will throw
    // Unspecified Error.
    googDom.isFocusable(googDom.createDom(TagName.BUTTON));
  },

  testGetTextContent() {
    function t(inp, out) {
      assertEquals(
          out.replace(/ /g, '_'),
          googDom.getTextContent(createTestDom(inp)).replace(/ /g, '_'));
    }

    t('abcde', 'abcde');
    t('a<b>bcd</b>efgh', 'abcdefgh');
    t('a<script type="text/javascript' +
          '">var a=1;<' +
          '/script>h',
      'ah');
    t('<html><head><style type="text/css">' +
          'p{margin:100%;padding:5px}\n.class{background-color:red;}</style>' +
          '</head><body><h1>Hello</h1>\n<p>One two three</p>\n<table><tr><td>a' +
          '<td>b</table><' +
          'script>var a = \'foo\';' +
          '</scrip' +
          't></body></html>',
      'HelloOne two threeab');
    t('abc<br>def', 'abc\ndef');
    t('abc<br>\ndef', 'abc\ndef');
    t('abc<br>\n\ndef', 'abc\ndef');
    t('abc<br><br>\ndef', 'abc\n\ndef');
    t(' <b>abcde  </b>   ', 'abcde ');
    t(' <b>abcde    </b> hi  ', 'abcde hi ');
    t(' \n<b>abcde  </b>   ', 'abcde ');
    t(' \n<b>abcde  </b>   \n\n\n', 'abcde ');
    t('<p>abcde</p>\nfg', 'abcdefg');
    t('\n <div>  <b>abcde  </b>   ', 'abcde ');
    t(' \n&shy;<b>abcde &shy; </b>   \n\n\n&shy;', 'abcde ');
    t(' \n&shy;\n\n&shy;\na   ', 'a ');
    t(' \n<wbr></wbr><b>abcde <wbr></wbr> </b>   \n\n\n<wbr></wbr>', 'abcde ');
    t('a&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;b', 'a\xA0\xA0\xA0\xA0\xA0b');
  },

  testGetNodeTextLength() {
    assertEquals(6, googDom.getNodeTextLength(createTestDom('abcdef')));
    assertEquals(
        8, googDom.getNodeTextLength(createTestDom('a<b>bcd</b>efgh')));
    assertEquals(
        2,
        googDom.getNodeTextLength(createTestDom(
            'a<script type="text/javascript' +
            '">var a = 1234;<' +
            '/script>h')));
    assertEquals(
        4,
        googDom.getNodeTextLength(
            createTestDom('a<br>\n<!-- some comments -->\nfo')));
    assertEquals(
        20,
        googDom.getNodeTextLength(createTestDom(
            '<html><head><style type="text/css">' +
            'p{margin:100%;padding:5px}\n.class{background-color:red;}</style>' +
            '</head><body><h1>Hello</h1><p>One two three</p><table><tr><td>a<td>b' +
            '</table><' +
            'script>var a = \'foo\';</scrip' +
            't></body></html>')));
    assertEquals(
        10, googDom.getNodeTextLength(createTestDom('a<b>bcd</b><br />efghi')));
  },

  testGetNodeTextOffset() {
    assertEquals(
        4, googDom.getNodeTextOffset($('offsetTest1'), $('offsetParent1')));
    assertEquals(12, googDom.getNodeTextOffset($('offsetTest1')));
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testGetNodeAtOffset() {
    const html =
        '<div id=a>123<b id=b>45</b><span id=c>67<b id=d>89<i id=e>01' +
        '</i>23<i id=f>45</i>67</b>890<i id=g>123</i><b id=h>456</b>' +
        '</span></div><div id=i>7890<i id=j>123</i></div>';
    const node = googDom.createElement(TagName.DIV);
    node.innerHTML = html;
    const rv = {};

    googDom.getNodeAtOffset(node, 2, rv);
    assertEquals('123', rv.node.nodeValue);
    assertEquals('a', rv.node.parentNode.id);
    assertEquals(1, rv.remainder);

    googDom.getNodeAtOffset(node, 3, rv);
    assertEquals('123', rv.node.nodeValue);
    assertEquals('a', rv.node.parentNode.id);
    assertEquals(2, rv.remainder);

    googDom.getNodeAtOffset(node, 5, rv);
    assertEquals('45', rv.node.nodeValue);
    assertEquals('b', rv.node.parentNode.id);
    assertEquals(1, rv.remainder);

    googDom.getNodeAtOffset(node, 6, rv);
    assertEquals('67', rv.node.nodeValue);
    assertEquals('c', rv.node.parentNode.id);
    assertEquals(0, rv.remainder);

    googDom.getNodeAtOffset(node, 23, rv);
    assertEquals('123', rv.node.nodeValue);
    assertEquals('g', rv.node.parentNode.id);
    assertEquals(2, rv.remainder);

    googDom.getNodeAtOffset(node, 30, rv);
    assertEquals('7890', rv.node.nodeValue);
    assertEquals('i', rv.node.parentNode.id);
    assertEquals(3, rv.remainder);
  },

  testGetOuterHtml() {
    const contents = '<b>foo</b>';
    const node = googDom.createElement(TagName.DIV);
    node.setAttribute('foo', 'bar');
    node.innerHTML = contents;
    assertEqualsCaseAndLeadingWhitespaceInsensitive(
        googDom.getOuterHtml(node), `<div foo="bar">${contents}</div>`);

    const imgNode = googDom.createElement(TagName.IMG);
    imgNode.setAttribute('foo', 'bar');
    assertEqualsCaseAndLeadingWhitespaceInsensitive(
        googDom.getOuterHtml(imgNode), '<img foo="bar">');
  },

  testGetWindowFrame() {
    const frameWindow = window.frames['frame'];
    const frameDocument = frameWindow.document;
    const frameDomHelper = new DomHelper(frameDocument);

    // Cannot use assertEquals since IE fails on ===
    assertTrue(frameWindow == frameDomHelper.getWindow());
  },

  testGetWindow() {
    const domHelper = new DomHelper();
    // Cannot use assertEquals since IE fails on ===
    assertTrue(window == domHelper.getWindow());
  },

  testGetWindowStatic() {
    // Cannot use assertEquals since IE fails on ===
    assertTrue(window == googDom.getWindow());
  },

  testIsNodeList() {
    const elem = document.getElementById('p2');
    const text = document.getElementById('b2').firstChild;

    assertTrue(
        'NodeList should be a node list', googDom.isNodeList(elem.childNodes));
    assertFalse('TextNode should not be a node list', googDom.isNodeList(text));
    assertFalse(
        'Array of nodes should not be a node list',
        googDom.isNodeList([elem.firstChild, elem.lastChild]));
  },

  testGetFrameContentDocument() {
    const iframe = googDom.getElementsByTagName(TagName.IFRAME)[0];
    const name = iframe.name;
    const iframeDoc = googDom.getFrameContentDocument(iframe);
    assertEquals(window.frames[name].document, iframeDoc);
  },

  testGetFrameContentWindow() {
    const iframe = googDom.getElementsByTagName(TagName.IFRAME)[0];
    const name = iframe.name;
    const iframeWin = googDom.getFrameContentWindow(iframe);
    assertEquals(window.frames[name], iframeWin);
  },

  testGetFrameContentWindowNotInitialized() {
    const iframe = googDom.createDom(TagName.IFRAME);
    assertNull(googDom.getFrameContentWindow(iframe));
  },

  testCanHaveChildren() {
    const EMPTY_ELEMENTS = googObject.createSet(
        TagName.APPLET, TagName.AREA, TagName.BASE, TagName.BR, TagName.COL,
        TagName.COMMAND, TagName.EMBED, TagName.FRAME, TagName.HR, TagName.IMG,
        TagName.INPUT, TagName.IFRAME, TagName.ISINDEX, TagName.KEYGEN,
        TagName.LINK, TagName.NOFRAMES, TagName.NOSCRIPT, TagName.META,
        TagName.OBJECT, TagName.PARAM, TagName.SCRIPT, TagName.SOURCE,
        TagName.STYLE, TagName.TRACK, TagName.WBR);

    // IE opens a dialog warning about using Java content if the following
    // elements are created.
    const IE_ILLEGAL_ELEMENTS =
        googObject.createSet(TagName.APPLET, TagName.EMBED);

    for (const tag in TagName) {
      if (userAgent.IE && tag in IE_ILLEGAL_ELEMENTS) {
        continue;
      }

      const expected = !(tag in EMPTY_ELEMENTS);
      const node = googDom.createElement(tag);
      assertEquals(
          `${tag} should ` + (expected ? '' : 'not ') + 'have children',
          expected, googDom.canHaveChildren(node));

      // Make sure we can _actually_ add a child if we identify the node as
      // allowing children.
      if (googDom.canHaveChildren(node)) {
        node.appendChild(googDom.createDom(TagName.DIV, null, 'foo'));
      }
    }
  },

  testGetAncestorNoElement() {
    assertNull(
        googDom.getAncestor(null /* element */, functions.TRUE /* matcher */));
    assertNull(googDom.getAncestor(
        null /* element */, functions.TRUE /* matcher */,
        true /* opt_includeNode */));
  },

  testGetAncestorNoMatch() {
    const elem = googDom.getElement('nestedElement');
    assertNull(googDom.getAncestor(elem, () => false));
  },

  testGetAncestorMatchSelf() {
    const elem = googDom.getElement('nestedElement');
    const matched = googDom.getAncestor(elem, () => true, true);
    assertEquals(elem, matched);
  },

  testGetAncestorNoMatchSelf() {
    const elem = googDom.getElement('nestedElement');
    const matched = googDom.getAncestor(elem, () => true);
    assertEquals(elem.parentNode, matched);
  },

  testGetAncestorWithMaxSearchStepsMatchSelf() {
    const elem = googDom.getElement('nestedElement');
    const matched = googDom.getAncestor(elem, () => true, true, 2);
    assertEquals(elem, matched);
  },

  testGetAncestorWithMaxSearchStepsMatch() {
    const elem = googDom.getElement('nestedElement');
    const searchEl = elem.parentNode.parentNode;
    const matched = googDom.getAncestor(elem, (el) => el == searchEl, false, 1);
    assertEquals(searchEl, matched);
  },

  testGetAncestorWithMaxSearchStepsNoMatch() {
    const elem = googDom.getElement('nestedElement');
    const searchEl = elem.parentNode.parentNode;
    const matched = googDom.getAncestor(elem, (el) => el == searchEl, false, 0);
    assertNull(matched);
  },

  testGetAncestorByTagWithMaxSearchStepsNoMatch() {
    const elem = googDom.getElement('nestedElement');
    const searchEl = elem.parentNode.parentNode;
    const matched = googDom.getAncestorByTagNameAndClass(
        elem, TagName.DIV, /* class */ undefined, 0);
    assertNull(matched);
  },

  testGetAncestorByTagNameNoMatch() {
    const elem = googDom.getElement('nestedElement');
    assertNull(googDom.getAncestorByTagNameAndClass(elem, TagName.IMG));
  },

  testGetAncestorByTagNameOnly() {
    const elem = googDom.getElement('nestedElement');
    const expected = googDom.getElement('testAncestorDiv');
    assertEquals(
        expected, googDom.getAncestorByTagNameAndClass(elem, TagName.DIV));
    assertEquals(expected, googDom.getAncestorByTagNameAndClass(elem, 'div'));
  },

  testGetAncestorByClassWithMaxSearchStepsNoMatch() {
    const elem = googDom.getElement('nestedElement');
    const searchEl = elem.parentNode.parentNode;
    const matched = googDom.getAncestorByClass(elem, 'testAncestor', 0);
    assertNull(matched);
  },

  testGetAncestorByClassNameNoMatch() {
    const elem = googDom.getElement('nestedElement');
    assertNull(googDom.getAncestorByClass(elem, 'bogusClassName'));
  },

  testGetAncestorByClassName() {
    const elem = googDom.getElement('nestedElement');
    const expected = googDom.getElement('testAncestorP');
    assertEquals(expected, googDom.getAncestorByClass(elem, 'testAncestor'));
  },

  testGetAncestorByTagNameAndClass() {
    const elem = googDom.getElement('nestedElement');
    const expected = googDom.getElement('testAncestorDiv');
    assertEquals(
        expected,
        googDom.getAncestorByTagNameAndClass(
            elem, TagName.DIV, 'testAncestor'));
    assertNull(
        'Should return null if no search criteria are given',
        googDom.getAncestorByTagNameAndClass(elem));
  },

  testCreateTable() {
    let table = googDom.createTable(2, 3, true);
    assertEquals(2, googDom.getElementsByTagName(TagName.TR, table).length);
    assertEquals(
        3,
        googDom.getElementsByTagName(TagName.TR, table)[0].childNodes.length);
    assertEquals(6, googDom.getElementsByTagName(TagName.TD, table).length);
    assertEquals(
        Unicode.NBSP,
        googDom.getElementsByTagName(TagName.TD, table)[0]
            .firstChild.nodeValue);

    table = googDom.createTable(2, 3, false);
    assertEquals(2, googDom.getElementsByTagName(TagName.TR, table).length);
    assertEquals(
        3,
        googDom.getElementsByTagName(TagName.TR, table)[0].childNodes.length);
    assertEquals(6, googDom.getElementsByTagName(TagName.TD, table).length);
    assertEquals(
        0,
        googDom.getElementsByTagName(TagName.TD, table)[0].childNodes.length);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSafeHtmlToNode() {
    const docFragment =
        googDom.safeHtmlToNode(testing.newSafeHtmlForTest('<a>1</a><b>2</b>'));
    assertNull(docFragment.parentNode);
    assertEquals(2, docFragment.childNodes.length);

    const div =
        googDom.safeHtmlToNode(testing.newSafeHtmlForTest('<div>3</div>'));
    assertEquals(String(TagName.DIV), div.tagName);

    const script =
        googDom.safeHtmlToNode(testing.newSafeHtmlForTest('<script></script>'));
    assertEquals(String(TagName.SCRIPT), script.tagName);

    if (userAgent.IE && !userAgent.isDocumentModeOrHigher(9)) {
      // Removing an Element from a DOM tree in IE sets its parentNode to a new
      // DocumentFragment. Bizarre!
      assertEquals(
          NodeType.DOCUMENT_FRAGMENT,
          googDom.removeNode(div).parentNode.nodeType);
    } else {
      assertNull(div.parentNode);
    }
  },

  testRegularConstHtmlToNodeStringifications() {
    assertConstHtmlToNodeStringifiesToOneOf(
        ['<b>foo</b>', '<B>foo</B>'], Const.from('<b>foo</b>'));

    assertConstHtmlToNodeStringifiesToOneOf(
        ['<br>', '<BR>'], Const.from('<br>'));

    assertConstHtmlToNodeStringifiesToOneOf(
        [
          '<SVG></B>',
          '<svg></svg>',
          '<svg xmlns="http://www.w3.org/2000/svg" />',
        ],
        Const.from('<svg></b>'));

    assertConstHtmlToNodeStringifiesToOneOf(
        ['<unknown></unknown>', '<unknown>', '<UNKNOWN />'],
        Const.from('<unknown />'));

    assertConstHtmlToNodeStringifiesToOneOf(
        ['&lt;"&amp;', '&lt;"'], Const.from('<"&'));
  },

  testConcatenatedConstHtmlToNodeStringifications() {
    assertConstHtmlToNodeStringifiesToOneOf(
        ['<b>foo</b>', '<B>foo</B>'], Const.from('<b>foo<'), Const.from('/b>'));

    assertConstHtmlToNodeStringifiesToOneOf(
        ['<b>foo</b>', '<B>foo</B>'], Const.from('<b>foo</b>'), Const.from(''));

    assertConstHtmlToNodeStringifiesToOneOf(['']);
  },

  testSpecialConstHtmlToNodeStringifications() {
    // body one is IE8, \r\n is opera.
    assertConstHtmlToNodeStringifiesToOneOf(
        [
          '<script></script>',
          '<SCRIPT></SCRIPT>',
          '<script></body></script>',
          '\r\n' +
              '<SCRIPT></SCRIPT>',
        ],
        Const.from('<script>'));

    assertConstHtmlToNodeStringifiesToOneOf(
        ['&lt;% %&gt;', '<% %>'], Const.from('<% %>'));

    assertConstHtmlToNodeStringifiesToOneOf(
        ['&lt;% <script> %></script>', '<% <script> %>'],
        Const.from('<% <script> %>'));

    assertConstHtmlToNodeStringifiesToOneOf(
        ['</ hi />', '<!-- hi /-->', ''], Const.from('</ hi />'));

    assertConstHtmlToNodeStringifiesToOneOf(
        ['<!-- <script --> /&gt;', '</ <script>/&gt;', ' /&gt;'],
        Const.from('</ <script > />'));
  },

  testAppend() {
    const div = googDom.createElement(TagName.DIV);
    const b = googDom.createElement(TagName.B);
    const c = document.createTextNode('c');
    googDom.append(div, 'a', b, c);
    assertEqualsCaseAndLeadingWhitespaceInsensitive('a<b></b>c', div.innerHTML);
  },

  testAppend2() {
    const dom = new DomHelper(myIframeDoc);
    const div = dom.createElement(TagName.DIV);
    const b = dom.createElement(TagName.B);
    const c = myIframeDoc.createTextNode('c');
    googDom.append(div, 'a', b, c);
    assertEqualsCaseAndLeadingWhitespaceInsensitive('a<b></b>c', div.innerHTML);
  },

  testAppend3() {
    const div = googDom.createElement(TagName.DIV);
    const b = googDom.createElement(TagName.B);
    const c = document.createTextNode('c');
    googDom.append(div, ['a', b, c]);
    assertEqualsCaseAndLeadingWhitespaceInsensitive('a<b></b>c', div.innerHTML);
  },

  testAppend4() {
    const div = googDom.createElement(TagName.DIV);
    const div2 = googDom.createElement(TagName.DIV);
    div2.innerHTML = 'a<b></b>c';
    googDom.append(div, div2.childNodes);
    assertEqualsCaseAndLeadingWhitespaceInsensitive('a<b></b>c', div.innerHTML);
    assertFalse(div2.hasChildNodes());
  },

  testGetDocumentScroll() {
    // setUpPage added divForTestingScrolling to the DOM. It's not init'd here
    // so it can be shared amonst other tests.
    window.scrollTo(100, 100);

    assertEquals(100, googDom.getDocumentScroll().x);
    assertEquals(100, googDom.getDocumentScroll().y);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetDocumentScrollOfFixedViewport() {
    // iOS and perhaps other environments don't actually support scrolling.
    // Instead, you view the document's fixed layout through a screen viewport.
    // We need getDocumentScroll to handle this case though.
    // In case of IE10 though, we do want to use scrollLeft/scrollTop
    // because the rest of the positioning is done off the scrolled away origin.
    const fakeDocumentScrollElement = {scrollLeft: 0, scrollTop: 0};
    const fakeDocument = {
      defaultView: {pageXOffset: 100, pageYOffset: 100},
      documentElement: fakeDocumentScrollElement,
      body: fakeDocumentScrollElement,
    };
    const dh = googDom.getDomHelper(document);
    dh.setDocument(fakeDocument);
    if (userAgent.IE && userAgent.isVersionOrHigher(10)) {
      assertEquals(0, dh.getDocumentScroll().x);
      assertEquals(0, dh.getDocumentScroll().y);
    } else {
      assertEquals(100, dh.getDocumentScroll().x);
      assertEquals(100, dh.getDocumentScroll().y);
    }
  },

  testGetDocumentScrollFromDocumentWithoutABody() {
    // Some documents, like SVG docs, do not have a body element. The document
    // element should be used when computing the document scroll for these
    // documents.
    const fakeDocument = {
      defaultView: {pageXOffset: 0, pageYOffset: 0},
      documentElement: {scrollLeft: 0, scrollTop: 0},
    };

    /** @suppress {checkTypes} suppression added to enable type checking */
    const dh = new DomHelper(fakeDocument);
    assertEquals(fakeDocument.documentElement, dh.getDocumentScrollElement());
    assertEquals(0, dh.getDocumentScroll().x);
    assertEquals(0, dh.getDocumentScroll().y);
    // OK if this does not throw.
  },

  testDefaultToScrollingElement() {
    const fakeDocument = {documentElement: {}, body: {}};
    /** @suppress {checkTypes} suppression added to enable type checking */
    const dh = new DomHelper(fakeDocument);

    // When scrollingElement isn't supported or is null (no element causes
    // scrolling), then behavior is UA-dependent for maximum compatibility.
    assertTrue(
        dh.getDocumentScrollElement() == fakeDocument.body ||
        dh.getDocumentScrollElement() == fakeDocument.documentElement);
    fakeDocument.scrollingElement = null;
    assertTrue(
        dh.getDocumentScrollElement() == fakeDocument.body ||
        dh.getDocumentScrollElement() == fakeDocument.documentElement);

    // But when scrollingElement is set, we use it directly.
    fakeDocument.scrollingElement = fakeDocument.documentElement;
    assertEquals(fakeDocument.documentElement, dh.getDocumentScrollElement());
    fakeDocument.scrollingElement = fakeDocument.body;
    assertEquals(fakeDocument.body, dh.getDocumentScrollElement());
  },

  testActiveElementIE() {
    if (!userAgent.IE) {
      return;
    }

    const link = googDom.getElement('link');
    link.focus();

    assertEquals(link.tagName, googDom.getActiveElement(document).tagName);
    assertEquals(link, googDom.getActiveElement(document));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testParentElement() {
    const testEl = $('testEl');
    const bodyEl = googDom.getParentElement(testEl);
    assertNotNull(bodyEl);
    const htmlEl = googDom.getParentElement(bodyEl);
    assertNotNull(htmlEl);
    const documentNotAnElement = googDom.getParentElement(htmlEl);
    assertNull(documentNotAnElement);

    const tree = googDom.safeHtmlToNode(testing.newSafeHtmlForTest(
        '<div>' +
        '<p>Some text</p>' +
        '<blockquote>Some <i>special</i> <b>text</b></blockquote>' +
        '<address><!-- comment -->Foo</address>' +
        '</div>'));
    assertNull(googDom.getParentElement(tree));
    let pEl = googDom.getNextNode(tree);
    /** @suppress {checkTypes} suppression added to enable type checking */
    const fragmentRootEl = googDom.getParentElement(pEl);
    assertEquals(tree, fragmentRootEl);

    const detachedEl = googDom.createDom(TagName.DIV);
    const detachedHasNoParent = googDom.getParentElement(detachedEl);
    assertNull(detachedHasNoParent);

    // svg is not supported in IE8 and below or in IE9 quirks mode
    const supported = !userAgent.IE || userAgent.isDocumentModeOrHigher(10) ||
        (googDom.isCss1CompatMode() && userAgent.isDocumentModeOrHigher(9));
    if (!supported) {
      return;
    }

    const svg = $('testSvg');
    assertNotNull(svg);
    const rect = $('testRect');
    assertNotNull(rect);
    const g = $('testG');
    assertNotNull(g);

    if (userAgent.IE) {
      // test to make sure IE9 is returning undefined for .parentElement
      assertUndefined(g.parentElement);
      assertUndefined(rect.parentElement);
      assertUndefined(svg.parentElement);
    }
    const shouldBeG = googDom.getParentElement(rect);
    assertEquals(g, shouldBeG);
    const shouldBeSvg = googDom.getParentElement(g);
    assertEquals(svg, shouldBeSvg);
    const shouldBeBody = googDom.getParentElement(svg);
    assertEquals(bodyEl, shouldBeBody);
  },

  testDevicePixelRatio() {
    const devicePixelRatio = 1.5;
    setWindow({
      'matchMedia': /**
                       @suppress {checkTypes} suppression added to enable type
                       checking
                     */
          function(query) {
            return {
              'matches':
                  devicePixelRatio >= parseFloat(query.split(': ')[1], 10),
            };
          },
    });

    assertEquals(devicePixelRatio, googDom.getPixelRatio());

    setWindow({'devicePixelRatio': 2.0});
    assertEquals(2, googDom.getPixelRatio());

    setWindow({});
    assertEquals(1, googDom.getPixelRatio());
  },
});
