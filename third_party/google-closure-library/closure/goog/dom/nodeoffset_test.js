/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.NodeOffsetTest');
goog.setTestOnly();

const NodeOffset = goog.require('goog.dom.NodeOffset');
const NodeType = goog.require('goog.dom.NodeType');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

let test1;
let test2;
let i;
let empty;

testSuite({
  setUpPage() {
    test1 = dom.getElement('test1');
    i = dom.getElement('i');
    test2 = dom.getElement('test2');
    test2.innerHTML = test1.innerHTML;
    empty = dom.getElement('empty');
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testElementOffset() {
    const nodeOffset = new NodeOffset(i, test1);

    const recovered = nodeOffset.findTargetNode(test2);
    assertNotNull('Should recover a node.', recovered);
    assertEquals(
        'Should recover an I node.', String(TagName.I), recovered.tagName);
    assertTrue(
        'Should recover a child of test2', dom.contains(test2, recovered));
    assertFalse(
        'Should not recover a child of test1', dom.contains(test1, recovered));

    nodeOffset.dispose();
  },

  testNodeOffset() {
    const nodeOffset = new NodeOffset(i.firstChild, test1);

    const recovered = nodeOffset.findTargetNode(test2);
    assertNotNull('Should recover a node.', recovered);
    assertEquals(
        'Should recover a text node.', NodeType.TEXT, recovered.nodeType);
    assertEquals(
        'Should  have correct contents.', 'text.', recovered.nodeValue);
    assertTrue(
        'Should recover a child of test2', dom.contains(test2, recovered));
    assertFalse(
        'Should not recover a child of test1', dom.contains(test1, recovered));

    nodeOffset.dispose();
  },

  testToString() {
    const nodeOffset = new NodeOffset(i.firstChild, test1);

    assertEquals(
        'Should have correct string representation', '3,B\n1,I\n0,#text',
        nodeOffset.toString());

    nodeOffset.dispose();
  },

  testBadRecovery() {
    const nodeOffset = new NodeOffset(i.firstChild, test1);

    const recovered = nodeOffset.findTargetNode(empty);
    assertNull('Should recover nothing.', recovered);

    nodeOffset.dispose();
  },
});
