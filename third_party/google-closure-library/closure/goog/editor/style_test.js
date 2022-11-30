/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.styleTest');
goog.setTestOnly();

const BrowserFeature = goog.require('goog.editor.BrowserFeature');
const EventHandler = goog.require('goog.events.EventHandler');
const EventType = goog.require('goog.events.EventType');
const LooseMock = goog.require('goog.testing.LooseMock');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const googStyle = goog.require('goog.style');
const mockmatchers = goog.require('goog.testing.mockmatchers');
const style = goog.require('goog.editor.style');
const testSuite = goog.require('goog.testing.testSuite');

let parentNode = null;
let childNode1 = null;
let childNode2 = null;
let childNode3 = null;
let gChildWsNode1 = null;
let gChildTextNode1 = null;
let gChildNbspNode1 = null;
let gChildMixedNode1 = null;
let gChildWsNode2a = null;
let gChildWsNode2b = null;
let gChildTextNode3a = null;
let gChildWsNode3 = null;
let gChildTextNode3b = null;

const $dom = dom.createDom;
const $text = dom.createTextNode;

function setUpGetNodeFunctions() {
  parentNode = $dom(
      TagName.P, {id: 'parentNode'},
      childNode1 = $dom(
          TagName.DIV, null, gChildWsNode1 = $text(' \t\r\n'),
          gChildTextNode1 = $text('Child node'),
          gChildNbspNode1 = $text('\u00a0'),
          gChildMixedNode1 = $text('Text\n plus\u00a0')),
      childNode2 = $dom(
          TagName.DIV, null, gChildWsNode2a = $text(''),
          gChildWsNode2b = $text(' ')),
      childNode3 = $dom(
          TagName.DIV, null, gChildTextNode3a = $text('I am a grand child'),
          gChildWsNode3 = $text('   \t  \r   \n'),
          gChildTextNode3b = $text('I am also a grand child')));

  document.body.appendChild(parentNode);
}

function tearDownGetNodeFunctions() {
  document.body.removeChild(parentNode);

  parentNode = null;
  childNode1 = null;
  childNode2 = null;
  childNode3 = null;
  gChildWsNode1 = null;
  gChildTextNode1 = null;
  gChildNbspNode1 = null;
  gChildMixedNode1 = null;
  gChildWsNode2a = null;
  gChildWsNode2b = null;
  gChildTextNode3a = null;
  gChildWsNode3 = null;
  gChildTextNode3b = null;
}

testSuite({
  /**
     Test isBlockLevel with a node that is block style and a node that is not
   */
  testIsDisplayBlock() {
    assertTrue('Body is block style', style.isDisplayBlock(document.body));
    const tableNode = $dom(TagName.TABLE);
    assertFalse('Table is not block style', style.isDisplayBlock(tableNode));
  },

  /**
   * Test that isContainer returns true when the node is of non-inline HTML and
   * false when it is not
   */
  testIsContainer() {
    const tableNode = $dom(TagName.TABLE);
    const liNode = $dom(TagName.LI);
    const textNode = $text('I am text');
    document.body.appendChild(textNode);

    assertTrue('Table is a container', style.isContainer(tableNode));
    assertTrue('Body is a container', style.isContainer(document.body));
    assertTrue('List item is a container', style.isContainer(liNode));
    assertFalse('Text node is not a container', style.isContainer(textNode));
  },

  /**
   * Test that getContainer properly returns the node itself if it is a
   * container, an ancestor node if it is a container, and null otherwise
   */
  testGetContainer() {
    setUpGetNodeFunctions();
    assertEquals(
        'Should return self', childNode1, style.getContainer(childNode1));
    assertEquals(
        'Should return parent', childNode1, style.getContainer(gChildWsNode1));
    assertNull('Document has no ancestors', style.getContainer(document));
    tearDownGetNodeFunctions();
  },

  /**
     @suppress {missingProperties,checkTypes} suppression added to enable type
     checking
   */
  testMakeUnselectable() {
    const div = dom.createElement(TagName.DIV);
    div.innerHTML = '<div>No input</div>' +
        '<p><input type="checkbox">Checkbox</p>' +
        '<span><input type="text"></span>';
    document.body.appendChild(div);

    const eventHandler = new LooseMock(EventHandler);
    if (BrowserFeature.HAS_UNSELECTABLE_STYLE) {
      eventHandler.listen(
          div, EventType.MOUSEDOWN, mockmatchers.isFunction, true);
    }
    eventHandler.$replay();

    const childDiv = div.firstChild;
    const p = div.childNodes[1];
    const span = div.lastChild;
    const checkbox = p.firstChild;
    const text = span.firstChild;

    style.makeUnselectable(div, eventHandler);

    assertEquals(
        'For browsers with non-overridable selectability, the root should be ' +
            'selectable.  Otherwise it should be unselectable.',
        !BrowserFeature.HAS_UNSELECTABLE_STYLE, googStyle.isUnselectable(div));
    assertTrue(googStyle.isUnselectable(childDiv));
    assertTrue(googStyle.isUnselectable(p));
    assertTrue(googStyle.isUnselectable(checkbox));

    assertEquals(
        'For browsers with non-overridable selectability, the span will be ' +
            'selectable.  Otherwise it will be unselectable. ',
        !BrowserFeature.HAS_UNSELECTABLE_STYLE, googStyle.isUnselectable(span));
    assertFalse(googStyle.isUnselectable(text));

    eventHandler.$verify();
  },
});
