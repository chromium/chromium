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
    testingDom.assertNodesMatch(
        new SiblingIterator(test.firstChild), ['#br', 'def']);
  },

  testNextSiblingInclusive() {
    testingDom.assertNodesMatch(
        new SiblingIterator(test.firstChild, true), ['abc', '#br', 'def']);
  },

  testPreviousSibling() {
    testingDom.assertNodesMatch(
        new SiblingIterator(test.lastChild, false, true), ['#br', 'abc']);
  },

  testPreviousSiblingInclusive() {
    testingDom.assertNodesMatch(
        new SiblingIterator(test.lastChild, true, true), ['def', '#br', 'abc']);
  },

  testChildIterator() {
    testingDom.assertNodesMatch(new ChildIterator(test), ['abc', '#br', 'def']);
  },

  testChildIteratorIndex() {
    testingDom.assertNodesMatch(
        new ChildIterator(test, false, 1), ['#br', 'def']);
  },

  testChildIteratorReverse() {
    testingDom.assertNodesMatch(
        new ChildIterator(test, true), ['def', '#br', 'abc']);
  },

  testEmptyChildIteratorReverse() {
    testingDom.assertNodesMatch(new ChildIterator(br, true), []);
  },

  testChildIteratorIndexReverse() {
    testingDom.assertNodesMatch(
        new ChildIterator(test, true, 1), ['#br', 'abc']);
  },

  testAncestorIterator() {
    testingDom.assertNodesMatch(
        new AncestorIterator(br),
        ['#test', '#body', '#html', NodeType.DOCUMENT]);
  },

  testAncestorIteratorInclusive() {
    testingDom.assertNodesMatch(
        new AncestorIterator(br, true),
        ['#br', '#test', '#body', '#html', NodeType.DOCUMENT]);
  },
});
