/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.NodeIteratorTest');
goog.setTestOnly();

const DomNodeIterator = goog.require('goog.dom.NodeIterator');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');
const testingDom = goog.require('goog.testing.dom');

testSuite({
  testBasic() {
    testingDom.assertNodesMatch(
        new DomNodeIterator(dom.getElement('test')),
        ['#test', '#a1', 'T', '#b1', 'e', 'xt', '#span1', '#p1', 'Text']);
  },

  testUnclosed() {
    testingDom.assertNodesMatch(
        new DomNodeIterator(dom.getElement('test2')),
        ['#test2', '#li1', 'Not', '#li2', 'Closed']);
  },

  testReverse() {
    testingDom.assertNodesMatch(
        new DomNodeIterator(dom.getElement('test'), true),
        ['Text', '#p1', '#span1', 'xt', 'e', '#b1', 'T', '#a1', '#test']);
  },
});
