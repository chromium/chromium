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
    const expectedContent =
        ['#test', '#a1', 'T', '#b1', 'e', 'xt', '#span1', '#p1', 'Text'];
    testingDom.assertNodesMatch(
        new DomNodeIterator(dom.getElement('test')), expectedContent);
  },

  testUnclosed() {
    const expectedContent = ['#test2', '#li1', 'Not', '#li2', 'Closed'];
    testingDom.assertNodesMatch(
        new DomNodeIterator(dom.getElement('test2')), expectedContent);
  },

  testReverse() {
    const expectedContent =
        ['Text', '#p1', '#span1', 'xt', 'e', '#b1', 'T', '#a1', '#test'];
    testingDom.assertNodesMatch(
        new DomNodeIterator(dom.getElement('test'), true), expectedContent);
  },
});
