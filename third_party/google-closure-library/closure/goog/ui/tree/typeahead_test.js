/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.tree.TypeAheadTest');
goog.setTestOnly();

const KeyCodes = goog.require('goog.events.KeyCodes');
const TreeControl = goog.require('goog.ui.tree.TreeControl');
const TypeAhead = goog.require('goog.ui.tree.TypeAhead');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

function makeATree() {
  const tree = new TreeControl('root');
  const testData = [
    'level1',
    [
      ['level2', [['eve', []], ['eve2', [['eve4', []]]]]],
      ['level22', [['eve', []], ['eve3', []], ['eve5', []]]],
    ],
  ];

  createTreeNodeFromTestData(tree, testData, 3);

  tree.createDom();
  dom.getElement('treeContainer').appendChild(tree.getElement());
  tree.enterDocument();

  return tree;
}

function createTreeNodeFromTestData(node, data, maxLevels) {
  node.setText(data[0]);
  if (maxLevels < 0) {
    return;
  }

  const children = data[1];
  for (let i = 0; i < children.length; i++) {
    const child = children[i];
    const childNode = node.getTree().createNode('');
    node.add(childNode);
    createTreeNodeFromTestData(childNode, child, maxLevels - 1);
  }
}

testSuite({
  /** Test jumpToLabel_ functionality. */
  testJumpToLabel() {
    const tree = makeATree();
    const typeAhead = tree.typeAhead_;

    // Test the case when only one matching entry exists.
    let handled = typeAhead.jumpToLabel_('level1');
    let selectedItem = tree.getSelectedItem();
    assertTrue(handled && selectedItem.getHtml() == 'level1');

    // Test the case when more than one matching entry exists.
    handled = typeAhead.jumpToLabel_('eve');
    selectedItem = tree.getSelectedItem();
    assertTrue(handled && selectedItem.getHtml() == 'eve');
    const firstEveNode = selectedItem;

    // Test the case when the matching entry is at a deeper level.
    handled = typeAhead.jumpToLabel_('eve3');
    selectedItem = tree.getSelectedItem();
    assertTrue(handled && selectedItem.getHtml() == 'eve3');
    const leafNode = selectedItem;  // eve3 is a leaf

    // Test the case after leaf node removal; ensure no node is picked.
    leafNode.getParent().removeChild(leafNode);
    handled = typeAhead.jumpToLabel_('eve3');
    selectedItem = tree.getSelectedItem();
    assertTrue(!handled);

    // Test the case after duplicate node removal; ensure another node is
    // picked.
    firstEveNode.getParent().removeChild(firstEveNode);
    handled = typeAhead.jumpToLabel_('eve');
    selectedItem = tree.getSelectedItem();
    assertTrue(handled && selectedItem.getHtml() == 'eve');
    const secondEveNode = selectedItem;

    // Test the case after all exact matching node removal.
    secondEveNode.getParent().removeChild(secondEveNode);
    handled = typeAhead.jumpToLabel_('eve');
    selectedItem = tree.getSelectedItem();
    assertTrue(handled && selectedItem.getHtml() == 'eve2');
    const parentNode = selectedItem;  // eve2 is a parent

    // Test the case after prior parent node removal of node with similar
    // prefix.
    parentNode.getParent().removeChild(parentNode);
    handled = typeAhead.jumpToLabel_('eve');
    selectedItem = tree.getSelectedItem();
    assertTrue(handled && selectedItem.getHtml() == 'eve5');
  },

  /** Test jumpTo_ functionality. */
  testJumpTo() {
    const tree = makeATree();
    const typeAhead = tree.typeAhead_;

    // Jump to the first matching 'eve', followed by Ctrl+DOWN to jump to
    // second matching 'eve'
    let handled = typeAhead.jumpToLabel_('eve') &&
        typeAhead.jumpTo_(TypeAhead.Offset.DOWN);
    let selectedItem = tree.getSelectedItem();
    assertTrue(handled && selectedItem.getHtml() == 'eve');

    // Simulate a DOWN key on the tree, now the selection should be on 'eve3'
    const e = new Object();
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.keyCode = KeyCodes.DOWN;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.preventDefault = () => {};
    handled = tree.handleKeyEvent(e);
    selectedItem = tree.getSelectedItem();
    assertTrue(handled && selectedItem.getHtml() == 'eve3');
  },

  /** Test handleTypeAheadChar functionality. */
  testHandleTypeAheadChar() {
    const tree = makeATree();
    const typeAhead = tree.typeAhead_;
    const e = new Object();

    // Period character('.'): keyCode = 190, charCode = 46
    // String.fromCharCode(190) = '3/4'  <-- incorrect
    // String.fromCharCode(46) = '.'  <-- correct
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.keyCode = KeyCodes.PERIOD;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.charCode = 46;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.preventDefault = () => {};
    typeAhead.handleTypeAheadChar(e);
    assertEquals('.', typeAhead.buffer_);

    // charCode not supplied.
    // This is expected to work only for alpha-num characters.
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.keyCode = KeyCodes.A;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.charCode = undefined;
    typeAhead.buffer_ = '';
    typeAhead.handleTypeAheadChar(e);
    assertEquals('a', typeAhead.buffer_);

    // keyCodes should not be used for keys that produce non-printable chars.
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.keyCode = KeyCodes.F5;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.charCode = 0;
    typeAhead.buffer_ = '';
    typeAhead.handleTypeAheadChar(e);
    assertEquals('', typeAhead.buffer_);
  },
});
