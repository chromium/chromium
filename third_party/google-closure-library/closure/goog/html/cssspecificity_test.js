/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for the CSS specificity calculator. */

goog.module('goog.html.CssSpecificityTest');
goog.setTestOnly();

const CssSpecificity = goog.require('goog.html.CssSpecificity');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');
const userAgentProduct = goog.require('goog.userAgent.product');


/**
 * @param {!Array<number>} expected
 * @param {string} selector
 */
function assertSpecificityEquals(expected, selector) {
  const specificity = CssSpecificity.getSpecificity(selector);
  if (userAgentProduct.IE && !userAgent.isVersionOrHigher(9)) {
    assertArrayEquals([0, 0, 0, 0], specificity);
  } else {
    assertArrayEquals(expected, specificity);
  }
}

testSuite({
  testGetSpecificity: function() {
    // @see http://css-tricks.com/specifics-on-css-specificity/
    assertSpecificityEquals([0, 1, 1, 3], 'ul#nav li.active a');
    assertSpecificityEquals([0, 0, 2, 3], 'body.ie7 .col_3 h2 ~ h2');
    assertSpecificityEquals([0, 1, 0, 2], '#footer *:not(nav) li');
    assertSpecificityEquals([0, 0, 0, 7], 'ul > li ul li ol li:first-letter');

    // @see http://reference.sitepoint.com/css/specificity
    assertSpecificityEquals([0, 2, 1, 3], 'body#home div#warning p.message');
    assertSpecificityEquals([0, 2, 1, 3], '* body#home>div#warning p.message');
    assertSpecificityEquals([0, 2, 1, 1], '#home #warning p.message');
    assertSpecificityEquals([0, 1, 1, 1], '#warning p.message');
    assertSpecificityEquals([0, 1, 0, 1], '#warning p');
    assertSpecificityEquals([0, 0, 1, 1], 'p.message');
    assertSpecificityEquals([0, 0, 0, 1], 'p');

    // Test pseudo-element with uppercase letters.
    assertSpecificityEquals([0, 0, 0, 2], 'li:bEfoRE');

    // Pseudo-class tests.
    assertSpecificityEquals([0, 0, 1, 2], 'li:first-child+p');
    assertSpecificityEquals([0, 0, 1, 2], 'li:nth-child(even)+p');
    assertSpecificityEquals([0, 0, 1, 2], 'li:nth-child(2n+1)+p');
    assertSpecificityEquals([0, 0, 1, 2], 'li:nth-child( 2n + 1 )+p');
    assertSpecificityEquals([0, 0, 1, 2], 'li:nth-child(2n-1)+p');
    assertSpecificityEquals([0, 0, 1, 2], 'li:nth-child(2n-1) p');
    assertSpecificityEquals([0, 0, 1, 0], ':lang(nl-be)');
  }
});
