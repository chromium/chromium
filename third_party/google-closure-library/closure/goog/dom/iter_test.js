/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.iterTest');
goog.setTestOnly();

const AncestorIterator = goog.require('goog.dom.iter.AncestorIterator');
const ChildIterator = goog.require('goog.dom.iter.ChildIterator');
const NodeType = goog.require('goog.dom.NodeType');
const SiblingIterator = goog.require('goog.dom.iter.SiblingIterator');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');
const testingDom = goog.require('goog.testing.dom');

let test;
let br;

testSuite({
  setUpPage() {
    test = dom.getElement('test');
    br = dom.getElement('br');
  },

  testNextSibling() {
    const expectedContent = ['#br', 'def'];
    testingDom.assertNodesMatch(
        new SiblingIterator(test.firstChild), expectedContent);
  },

  testNextSiblingInclusive() {
    const expectedContent = ['abc', '#br', 'def'];
    testingDom.assertNodesMatch(
        new SiblingIterator(test.firstChild, true), expectedContent);
  },

  testPreviousSibling() {
    const expectedContent = ['#br', 'abc'];
    testingDom.assertNodesMatch(
        new SiblingIterator(test.lastChild, false, true), expectedContent);
  },

  testPreviousSiblingInclusive() {
    const expectedContent = ['def', '#br', 'abc'];
    testingDom.assertNodesMatch(
        new SiblingIterator(test.lastChild, true, true), expectedContent);
  },

  testChildIterator() {
    const expectedContent = ['abc', '#br', 'def'];
    testingDom.assertNodesMatch(new ChildIterator(test), expectedContent);
  },

  testChildIteratorIndex() {
    const expectedContent = ['#br', 'def'];
    testingDom.assertNodesMatch(
        new ChildIterator(test, false, 1), expectedContent);
  },

  testChildIteratorReverse() {
    const expectedContent = ['def', '#br', 'abc'];
    testingDom.assertNodesMatch(new ChildIterator(test, true), expectedContent);
  },

  testEmptyChildIteratorReverse() {
    const expectedContent = [];
    testingDom.assertNodesMatch(new ChildIterator(br, true), expectedContent);
  },

  testChildIteratorIndexReverse() {
    const expectedContent = ['#br', 'abc'];
    testingDom.assertNodesMatch(
        new ChildIterator(test, true, 1), expectedContent);
  },

  testAncestorIterator() {
    const expectedContent = ['#test', '#body', '#html', NodeType.DOCUMENT];
    testingDom.assertNodesMatch(new AncestorIterator(br), expectedContent);
  },

  testAncestorIteratorInclusive() {
    const expectedContent =
        ['#br', '#test', '#body', '#html', NodeType.DOCUMENT];
    testingDom.assertNodesMatch(
        new AncestorIterator(br, true), expectedContent);
  },
});
