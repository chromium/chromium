/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.iframeTest');
goog.setTestOnly();

const Const = goog.require('goog.string.Const');
const SafeHtml = goog.require('goog.html.SafeHtml');
const SafeStyle = goog.require('goog.html.SafeStyle');
const dom = goog.require('goog.dom');
const domIframe = goog.require('goog.dom.iframe');
const testSuite = goog.require('goog.testing.testSuite');

let domHelper;
let sandbox;

testSuite({
  setUpPage() {
    domHelper = dom.getDomHelper();
    sandbox = domHelper.getElement('sandbox');
  },

  setUp() {
    dom.removeChildren(sandbox);
  },

  testCreateWithContent_safeTypes() {
    const head = SafeHtml.create('title', {}, 'Foo Title');
    const body = SafeHtml.create('div', {'id': 'blah'}, 'Test');
    const style = SafeStyle.fromConstant(Const.from('position: absolute;'));
    /** @suppress {checkTypes} suppression added to enable type checking */
    const iframe = domIframe.createWithContent(
        sandbox, head, body, style, false /* opt_quirks */);

    const doc = dom.getFrameContentDocument(iframe);
    assertNotNull(doc.getElementById('blah'));
    assertEquals('Foo Title', doc.title);
    assertEquals('absolute', iframe.style.position);
  },

  testCreateBlankYieldsIframeWithNoBorderOrPadding() {
    const iframe = domIframe.createBlank(domHelper);
    iframe.style.width = '350px';
    iframe.style.height = '250px';
    const blankElement = domHelper.getElement('blank');
    blankElement.appendChild(iframe);
    assertEquals(
        'Width should be as styled: no extra borders, padding, etc.', 350,
        blankElement.offsetWidth);
    assertEquals(
        'Height should be as styled: no extra borders, padding, etc.', 250,
        blankElement.offsetHeight);
  },

  testCreateBlankWithSafeStyles() {
    const iframe = domIframe.createBlank(
        domHelper, SafeStyle.fromConstant(Const.from('position:absolute;')));
    assertEquals('absolute', iframe.style.position);
    assertEquals('bottom', iframe.style.verticalAlign);
  },
});
