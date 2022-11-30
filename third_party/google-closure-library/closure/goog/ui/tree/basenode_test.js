/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.tree.BaseNodeTest');
goog.setTestOnly();

const BaseNode = goog.require('goog.ui.tree.BaseNode');
const Component = goog.require('goog.ui.Component');
const TagName = goog.require('goog.dom.TagName');
const TreeControl = goog.require('goog.ui.tree.TreeControl');
const TreeNode = goog.require('goog.ui.tree.TreeNode');
const aria = goog.require('goog.a11y.aria');
const classlist = goog.require('goog.dom.classlist');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');
const testing = goog.require('goog.html.testing');

testSuite({
  testAdd() {
    const node1 = new TreeNode('node1');
    const node2 = new TreeNode('node2');
    const node3 = new TreeNode('node3');
    const node4 = new TreeNode('node4');

    assertEquals('node2 added', node2, node1.add(node2));
    assertEquals('node3 added', node3, node1.add(node3));
    assertEquals('node4 added', node4, node1.add(node4, node3));

    assertEquals('node1 has 3 children', 3, node1.getChildCount());
    assertEquals('first child', node2, node1.getChildAt(0));
    assertEquals('second child', node4, node1.getChildAt(1));
    assertEquals('third child', node3, node1.getChildAt(2));
    assertNull('node1 has no parent', node1.getParent());
    assertEquals('the parent of node2 is node1', node1, node2.getParent());

    assertEquals('node4 moved under node2', node4, node2.add(node4));
    assertEquals('node1 has 2 children', 2, node1.getChildCount());
    assertEquals('node2 has 1 child', 1, node2.getChildCount());
    assertEquals('the child of node2 is node4', node4, node2.getChildAt(0));
    assertEquals('the parent of node4 is node2', node2, node4.getParent());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAriaExpandedFalseSetByDefaultOnParentAfterAddChild() {
    const tree = new TreeControl('root');
    const node1 = new TreeNode('node1');
    const nodeA = new TreeNode('nodeA');
    tree.render(dom.createDom(TagName.DIV));
    tree.addChild(node1);
    assertFalse(
        'node1 should not have aria-expanded state',
        aria.hasState(node1.getElement(), 'expanded'));

    node1.add(nodeA);

    assertEquals(
        'node1 should have aria-expanded state', 'false',
        aria.getState(node1.getElement(), 'expanded'));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAriaExpandedSetProperlyAfterSetExpandedCalledOnLeafNode() {
    const tree = new TreeControl('root');
    const node1 = new TreeNode('node1');
    const nodeA = new TreeNode('nodeA');
    tree.render(dom.createDom(TagName.DIV));
    tree.addChild(node1);
    assertFalse(
        'node1 should not have aria-expanded state',
        aria.hasState(node1.getElement(), 'expanded'));

    node1.setExpanded(true);
    node1.add(nodeA);

    assertEquals(
        'node1 should have aria-expanded=true', 'true',
        aria.getState(node1.getElement(), 'expanded'));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testExpandIconAfterAddChild() {
    const tree = new TreeControl('root');
    const node1 = new TreeNode('node1');
    const node2 = new TreeNode('node2');
    tree.render(dom.createDom(TagName.DIV));
    tree.addChild(node1);

    node1.addChild(node2);
    assertTrue(
        'expand icon of node1 changed to L+',
        classlist.contains(
            node1.getExpandIconElement(), 'goog-tree-expand-icon-lplus'));

    node1.removeChild(node2);
    assertFalse(
        'expand icon of node1 changed back to L',
        classlist.contains(
            node1.getExpandIconElement(), 'goog-tree-expand-icon-lplus'));
  },

  testExpandEvents() {
    const n = new BaseNode('');
    n.getTree = () => {};
    const expanded = false;
    n.setExpanded(expanded);
    assertEquals(expanded, n.getExpanded());
    let callCount = 0;
    n.addEventListener(BaseNode.EventType.BEFORE_EXPAND, (e) => {
      assertEquals(expanded, n.getExpanded());
      callCount++;
    });
    n.addEventListener(BaseNode.EventType.EXPAND, (e) => {
      assertEquals(!expanded, n.getExpanded());
      callCount++;
    });
    n.setExpanded(!expanded);
    assertEquals(2, callCount);
  },

  testExpandEvents2() {
    const n = new BaseNode('');
    n.getTree = () => {};
    const expanded = true;
    n.setExpanded(expanded);
    assertEquals(expanded, n.getExpanded());
    let callCount = 0;
    n.addEventListener(BaseNode.EventType.BEFORE_COLLAPSE, (e) => {
      assertEquals(expanded, n.getExpanded());
      callCount++;
    });
    n.addEventListener(BaseNode.EventType.COLLAPSE, (e) => {
      assertEquals(!expanded, n.getExpanded());
      callCount++;
    });
    n.setExpanded(!expanded);
    assertEquals(2, callCount);
  },

  testExpandEventsPreventDefault() {
    const n = new BaseNode('');
    n.getTree = () => {};
    const expanded = true;
    n.setExpanded(expanded);
    assertEquals(expanded, n.getExpanded());
    let callCount = 0;
    n.addEventListener(BaseNode.EventType.BEFORE_COLLAPSE, (e) => {
      assertEquals(expanded, n.getExpanded());
      e.preventDefault();
      callCount++;
    });
    n.addEventListener(BaseNode.EventType.COLLAPSE, (e) => {
      fail('Should not fire COLLAPSE');
    });
    n.setExpanded(!expanded);
    assertEquals(1, callCount);
  },

  testExpandEventsPreventDefault2() {
    const n = new BaseNode('');
    n.getTree = () => {};
    const expanded = false;
    n.setExpanded(expanded);
    assertEquals(expanded, n.getExpanded());
    let callCount = 0;
    n.addEventListener(BaseNode.EventType.BEFORE_EXPAND, (e) => {
      assertEquals(expanded, n.getExpanded());
      e.preventDefault();
      callCount++;
    });
    n.addEventListener(BaseNode.EventType.EXPAND, (e) => {
      fail('Should not fire EXPAND');
    });
    n.setExpanded(!expanded);
    assertEquals(1, callCount);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testRemoveChild() {
    const tree = new TreeControl('root');
    const node1 = new TreeNode('node1');
    const nodeA = new TreeNode('nodeA');
    const nodeB = new TreeNode('nodeB');
    tree.render(dom.createDom(TagName.DIV));
    tree.addChild(node1);
    node1.add(nodeA);
    node1.add(nodeB);

    node1.removeChild(nodeA);

    assertFalse(
        'nodeA should be removed from tree of node1', node1.contains(nodeA));
    assertTrue(
        'node1 still has children; node1 should still have aria-expanded state',
        aria.hasState(node1.getElement(), 'expanded'));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testRemoveLastChildRemovesAriaExpandedState() {
    const tree = new TreeControl('root');
    const node1 = new TreeNode('node1');
    const node2 = new TreeNode('node2');
    tree.render(dom.createDom(TagName.DIV));
    tree.addChild(node1);
    node1.add(node2);

    node1.removeChild(node2);

    assertFalse(
        'node1 has no more children; node1 should not have aria-expanded state',
        aria.hasState(node1.getElement(), 'expanded'));
  },

  testGetNextShownNode() {
    const tree = new TreeControl('tree');
    assertNull('next node for unpopulated tree', tree.getNextShownNode());

    const node1 = new TreeNode('node1');
    const node2 = new TreeNode('node2');
    const node3 = new TreeNode('node3');
    node1.add(node2);
    node1.add(node3);

    assertNull('next node for unexpanded node1', node1.getNextShownNode());
    node1.expand();
    assertEquals(
        'next node for expanded node1', node2, node1.getNextShownNode());
    assertEquals('next node for node2', node3, node2.getNextShownNode());
    assertNull('next node for node3', node3.getNextShownNode());

    tree.add(node1);
    assertEquals(
        'next node for populated tree', node1, tree.getNextShownNode());
    assertNull('next node for node3 inside the tree', node3.getNextShownNode());

    const component = new Component();
    component.addChild(tree);
    assertNull(
        'next node for node3 inside the tree if the tree has parent',
        node3.getNextShownNode());
  },

  testGetPreviousShownNode() {
    const tree = new TreeControl('tree');
    assertNull('next node for unpopulated tree', tree.getPreviousShownNode());

    const node1 = new TreeNode('node1');
    const node2 = new TreeNode('node2');
    const node3 = new TreeNode('node3');
    tree.add(node1);
    node1.add(node2);
    tree.add(node3);

    assertEquals(
        'prev node for node3 when node1 is unexpanded', node1,
        node3.getPreviousShownNode());
    node1.expand();
    assertEquals(
        'prev node for node3 when node1 is expanded', node2,
        node3.getPreviousShownNode());
    assertEquals(
        'prev node for node2 when node1 is expanded', node1,
        node2.getPreviousShownNode());
    assertEquals(
        'prev node for node1 when root is shown', tree,
        node1.getPreviousShownNode());
    tree.setShowRootNode(false);
    assertNull(
        'next node for node1 when root is not shown',
        node1.getPreviousShownNode());

    const component = new Component();
    component.addChild(tree);
    assertNull(
        'prev node for root if the tree has parent',
        tree.getPreviousShownNode());
  },

  testInvisibleNodesInUnrenderedTree() {
    const tree = new TreeControl('tree');
    const a = new TreeNode('a');
    const b = new TreeNode('b');
    tree.add(a);
    a.add(b);
    tree.render();

    let textContent =
        tree.getElement().textContent || tree.getElement().innerText;
    assertContains('Node should be rendered.', 'tree', textContent);
    assertContains('Node should be rendered.', 'a', textContent);
    assertNotContains(
        'Unexpanded node child should not be rendered.', 'b', textContent);

    a.expand();
    textContent = tree.getElement().textContent || tree.getElement().innerText;
    assertContains('Node should be rendered.', 'tree', textContent);
    assertContains('Node should be rendered.', 'a', textContent);
    assertContains('Expanded node child should be rendered.', 'b', textContent);
    tree.dispose();
  },

  testInvisibleNodesInRenderedTree() {
    const tree = new TreeControl('tree');
    tree.render();
    const a = new TreeNode('a');
    const b = new TreeNode('b');
    tree.add(a);
    a.add(b);

    let textContent =
        tree.getElement().textContent || tree.getElement().innerText;
    assertContains('Node should be rendered.', 'tree', textContent);
    assertContains('Node should be rendered.', 'a', textContent);
    assertNotContains(
        'Unexpanded node child should not be rendered.', 'b', textContent);

    a.expand();
    textContent = tree.getElement().textContent || tree.getElement().innerText;
    assertContains('Node should be rendered.', 'tree', textContent);
    assertContains('Node should be rendered.', 'a', textContent);
    assertContains('Expanded node child should be rendered.', 'b', textContent);
    tree.dispose();
  },

  testConstructor() {
    let tree = new TreeControl('tree');
    assertEquals('tree', tree.getHtml());

    tree = new TreeControl(testing.newSafeHtmlForTest('tree'));
    assertEquals('tree', tree.getHtml());
  },

  testConstructor_allowsPlainText() {
    const tree = new TreeControl('tree <3');
    assertEquals('tree &lt;3', tree.getHtml());
  },

  testSetSafeHtml() {
    const tree = new TreeControl('');
    tree.setSafeHtml(testing.newSafeHtmlForTest('<b>tree</b>'));
    assertEquals('<b>tree</b>', tree.getHtml());
  },
});
