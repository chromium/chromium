/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.DomCompileTest');
goog.setTestOnly();

const TagName = goog.require('goog.dom.TagName');
const googDom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  /** Checks types with TagName. */
  testDomTagNameTypes() {
    /** @type {!HTMLAnchorElement} */
    const a = googDom.createDom(TagName.A);

    /** @type {!HTMLAnchorElement} */
    const el = googDom.createElement(TagName.A);

    /** @type {!IArrayLike<!HTMLAnchorElement>} */
    const anchors = googDom.getElementsByTagNameAndClass(TagName.A);

    // Check that goog.dom.HtmlElement is assignable to HTMLElement.
    /** @type {!HTMLElement} */
    const b = googDom.createElement(TagName.B);

    /** @type {?HTMLAnchorElement} */
    const anchor = googDom.getElementByTagNameAndClass(TagName.A);
  },

  /** Checks types with TagName. */
  testDomHelperTagNameTypes() {
    const dom = googDom.getDomHelper();

    /** @type {!HTMLAnchorElement} */
    const a = dom.createDom(TagName.A);

    /** @type {!HTMLAnchorElement} */
    const el = dom.createElement(TagName.A);

    /** @type {!IArrayLike<!HTMLAnchorElement>} */
    const anchors = dom.getElementsByTagNameAndClass(TagName.A);

    /** @type {?HTMLAnchorElement} */
    const anchor = dom.getElementByTagNameAndClass(TagName.A);
  },
});
