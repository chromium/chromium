/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.cssom.iframe.styleTest');
goog.setTestOnly();

const DomHelper = goog.require('goog.dom.DomHelper');
const TagName = goog.require('goog.dom.TagName');
const cssom = goog.require('goog.cssom');
const dom = goog.require('goog.dom');
const style = goog.require('goog.cssom.iframe.style');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

// unit tests
const propertiesToTest = [
  'color',
  'font-family',
  'font-style',
  'font-size',
  'font-variant',
  'border-top-style',
  'border-top-width',
  'border-top-color',
  'background-color',
  'margin-bottom',
];

function crawlDom(startNode, func) {
  if (startNode.nodeType != 1) {
    return;
  }
  func(startNode);
  for (let i = 0; i < startNode.childNodes.length; i++) {
    crawlDom(startNode.childNodes[i], func);
  }
}

function getCurrentCssProperties(node, propList) {
  const props = {};
  if (node.nodeType != 1) {
    return;
  }
  for (let i = 0; i < propList.length; i++) {
    const prop = propList[i];
    if (node.currentStyle) {  // IE
      let propCamelCase = '';
      const propParts = prop.split('-');
      for (let j = 0; j < propParts.length; j++) {
        propCamelCase += propParts[j].charAt(0).toUpperCase() +
            propParts[j].substring(1, propParts[j].length);
      }
      props[prop] = node.currentStyle[propCamelCase];
    } else {  // standards-compliant browsers
      props[prop] = node.ownerDocument.defaultView.getComputedStyle(node, '')
                        .getPropertyValue(prop);
    }
  }
  return props;
}

class CssPropertyCollector {
  constructor() {
    const propsList = [];
    this.propsList = propsList;

    this.collectProps = (node) => {
      const nodeProps = getCurrentCssProperties(node, propertiesToTest);
      if (nodeProps) {
        propsList.push([nodeProps, node]);
      }
    };
  }
}

function recursivelyListCssProperties(el) {
  const collector = new CssPropertyCollector();
  crawlDom(el, collector.collectProps);
  return collector.propsList;
}

function makeIframeDocument(iframe) {
  const doc = dom.getFrameContentDocument(iframe);
  doc.open();
  doc.write('<html><head>');
  doc.write('<style>html,body { background-color: transparent; }</style>');
  doc.write('</head><body></body></html>');
  doc.close();
  return doc;
}

function normalizeCssText(cssText) {
  // Normalize cssText for testing purposes.
  return cssText.replace(/\s/g, '').toLowerCase();
}

testSuite({
  /**
   * @suppress {visibility,missingProperties} CssSelector_ property is private
   * and selectorPartIndex is unknown.
   */
  testMatchCssSelector() {
    const container = dom.createElement(TagName.DIV);
    container.className = 'container';
    const el = dom.createElement(TagName.DIV);
    el.id = 'mydiv';
    el.className = 'colorful foo';
    // set some arbirtrary content
    el.innerHTML = '<div><ul><li>One</li><li>Two</li></ul></div>';
    container.appendChild(el);
    document.body.appendChild(container);

    const elementAncestry = new style.NodeAncestry_(el);
    assertEquals(5, elementAncestry.nodes.length);

    // list of input/output results. Output is the index of the selector
    // that we expect to match - for example, in 'body div div.colorful',
    // 'div.colorful' has an index of 2.
    const expectedResults = [
      ['body div', [4, 1]],
      ['h1', null],
      ['body div h1', [4, 1]],
      ['body div.colorful h1', [4, 1]],
      ['body div div', [4, 2]],
      ['body div div div', [4, 2]],
      ['body div div.somethingelse div', [4, 1]],
      ['body div.somethingelse div', [2, 0]],
      ['div.container', [3, 0]],
      ['div.container div', [4, 1]],
      ['#mydiv', [4, 0]],
      ['div#mydiv', [4, 0]],
      ['div.colorful', [4, 0]],
      ['div#mydiv .colorful', [4, 0]],
      ['.colorful', [4, 0]],
      ['body * div', [4, 2]],
      ['body * *', [4, 2]],
    ];
    for (let i = 0; i < expectedResults.length; i++) {
      const input = expectedResults[i][0];
      const expectedResult = expectedResults[i][1];
      const selector = new style.CssSelector_(input);
      const result =
          /** @type {?} */ (selector.matchElementAncestry(elementAncestry));
      if (expectedResult == null) {
        assertEquals('Expected null result', expectedResult, result);
      } else {
        assertEquals(
            `Expected element index for ${input}`, expectedResult[0],
            result.elementIndex);
        assertEquals(
            `Expected selector part index for ${input}`, expectedResult[1],
            result.selectorPartIndex);
      }
    }
    document.body.removeChild(container);
  },

  testCopyCss() {
    for (let i = 1; i <= 4; i++) {
      const sourceElement = document.getElementById(`source${i}`);
      const newFrame = dom.createElement(TagName.IFRAME);
      newFrame.allowTransparency = true;
      sourceElement.parentNode.insertBefore(
          newFrame, sourceElement.nextSibling);
      const doc = makeIframeDocument(newFrame);
      cssom.addCssText(
          style.getElementContext(sourceElement), new DomHelper(doc));
      doc.body.innerHTML = sourceElement.innerHTML;

      const oldProps = recursivelyListCssProperties(sourceElement);
      const newProps = recursivelyListCssProperties(doc.body);

      assertEquals(oldProps.length, newProps.length);
      for (let j = 0; j < oldProps.length; j++) {
        for (let k = 0; k < propertiesToTest.length; k++) {
          assertEquals(
              'testing property ' + propertiesToTest[k],
              oldProps[j][0][propertiesToTest[k]],
              newProps[j][0][propertiesToTest[k]]);
        }
      }
    }
  },

  testAImportantInFF2() {
    const testDiv = document.getElementById('source1');
    const cssText = normalizeCssText(style.getElementContext(testDiv));
    const color = standardizeCSSValue('color', 'red');
    const NORMAL_RULE = `a{color:${color}`;
    const FF_2_RULE = `a{color:${color}!important`;
    assertContains(NORMAL_RULE, cssText);
    assertNotContains(FF_2_RULE, cssText);
  },

  testCopyBackgroundContext() {
    const testDiv = document.getElementById('backgroundTest');
    const cssText = style.getElementContext(testDiv, undefined, true);
    const iframe = dom.createElement(TagName.IFRAME);
    const ancestor = document.getElementById('backgroundTest-ancestor-1');
    ancestor.parentNode.insertBefore(iframe, ancestor.nextSibling);
    iframe.style.width = '100%';
    iframe.style.height = '100px';
    iframe.style.borderWidth = '0px';
    const doc = makeIframeDocument(iframe);
    cssom.addCssText(cssText, new DomHelper(doc));
    doc.body.innerHTML = testDiv.innerHTML;
    const normalizedCssText = normalizeCssText(cssText);
    assertTrue(
        'Background color should be copied from parent element',
        /body{[^{]*background-color:(?:rgb\(128,0,128\)|#800080)/.test(
            normalizedCssText));
    assertTrue(
        'Background image should be copied from ancestor element',
        /body{[^{]*background-image:url\(/.test(normalizedCssText));
    // Expected x position is:
    // originalBackgroundPositionX - elementOffsetLeft
    // 40px - (1px + 8px) == 31px
    // Expected y position is:
    // originalBackgroundPositionY - elementOffsetLeft
    // 70px - (1px + 10px + 5px) == 54px;
    assertTrue(
        'Background image position should be adjusted correctly',
        /body{[^{]*background-position:31px54px/.test(normalizedCssText));
  },

  testCopyBackgroundContextFromIframe() {
    const testDiv = document.getElementById('backgroundTest');
    const iframe = dom.createElement(TagName.IFRAME);
    iframe.allowTransparency = true;
    iframe.style.position = 'absolute';
    iframe.style.top = '5px';
    iframe.style.left = '5px';
    iframe.style.borderWidth = '2px';
    iframe.style.borderStyle = 'solid';
    testDiv.appendChild(iframe);
    const doc = makeIframeDocument(iframe);
    doc.body.backgroundColor = 'transparent';
    doc.body.style.margin = '0';
    doc.body.style.padding = '0';
    doc.body.innerHTML = '<p style="margin: 0">I am transparent!</p>';
    const normalizedCssText = normalizeCssText(
        style.getElementContext(doc.body.firstChild, undefined, true));
    // Background properties should get copied through from the parent
    // document since the iframe is transparent
    assertTrue(
        'Background color should be copied from parent element',
        /body{[^{]*background-color:(?:rgb\(128,0,128\)|#800080)/.test(
            normalizedCssText));
    assertTrue(
        'Background image should be copied from ancestor element',
        /body{[^{]*background-image:url\(/.test(normalizedCssText));
    // Image offset should have been calculated to be the same as the
    // above example, but adding iframe offset and borderWidth.
    // Expected x position is:
    // originalBackgroundPositionX - elementOffsetLeft
    // 40px - (1px + 8px + 5px + 2px) == 24px
    // Expected y position is:
    // originalBackgroundPositionY - elementOffsetLeft
    // 70px - (1px + 10px + 5px + 5px + 2px) == 47px;
    assertTrue(
        'Background image position should be adjusted correctly',
        !!/body{[^{]*background-position:24px47px/.exec(normalizedCssText));

    iframe.parentNode.removeChild(iframe);
  },

  testCopyFontFaceRules() {
    const isFontFaceCssomSupported = userAgent.WEBKIT || userAgent.GECKO;
    // We cannot use goog.testing.ExpectedFailures since it dynamically
    // brings in CSS which causes the background context tests to fail
    // in IE6.
    if (isFontFaceCssomSupported) {
      const cssText =
          style.getElementContext(document.getElementById('cavalier'));
      assertTrue(
          'The font face rule should have been copied correctly',
          /@font-face/.test(cssText));
    }
  },
});
