/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.tree.TreeControlTest');
goog.setTestOnly();

const TreeControl = goog.require('goog.ui.tree.TreeControl');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

function makeATree() {
  const tree = new TreeControl('root');
  const testData = [
    'A',
    [['AA', [['AAA', []], ['AAB', []]]], ['AB', [['ABA', []], ['ABB', []]]]],
  ];

  createTreeFromTestData(tree, testData, 3);
  tree.render(dom.getElement('treeContainer'));
  return tree;
}

function createTreeFromTestData(node, data, maxLevels) {
  node.setText(data[0]);
  if (maxLevels < 0) {
    return;
  }

  const children = data[1];
  for (let i = 0; i < children.length; i++) {
    const child = children[i];
    const childNode = node.getTree().createNode('');
    node.add(childNode);
    createTreeFromTestData(childNode, child, maxLevels - 1);
  }
}

testSuite({
  /** Test moving a node to a greater depth. */
  testIndent() {
    const tree = makeATree();
    tree.expandAll();

    const node = tree.getChildren()[0].getChildren()[0];
    assertEquals('AAA', node.getHtml());
    assertNotNull(node.getElement());
    assertEquals('19px', node.getRowElement().style.paddingLeft);

    assertEquals(2, node.getDepth());

    const newParent = node.getNextSibling();
    assertEquals('AAB', newParent.getHtml());
    assertEquals(2, newParent.getDepth());

    newParent.add(node);

    assertEquals(newParent, node.getParent());
    assertEquals(node, newParent.getChildren()[0]);
    assertEquals(3, node.getDepth());
    assertEquals('38px', node.getRowElement().style.paddingLeft);
  },

  /** Test moving a node to a lesser depth. */
  testOutdent() {
    const tree = makeATree();
    tree.expandAll();

    const node = tree.getChildren()[0].getChildren()[0];
    assertEquals('AAA', node.getHtml());
    assertNotNull(node.getElement());
    assertEquals('19px', node.getRowElement().style.paddingLeft);

    assertEquals(2, node.getDepth());

    const newParent = tree;
    assertEquals('A', newParent.getHtml());
    assertEquals(0, newParent.getDepth());

    newParent.add(node);

    assertEquals(newParent, node.getParent());
    assertEquals(node, newParent.getChildren()[2]);
    assertEquals(1, node.getDepth());
    assertEquals('0px', node.getRowElement().style.paddingLeft);
  },
});
