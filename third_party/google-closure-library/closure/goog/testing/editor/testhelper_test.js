/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.editor.TestHelperTest');
goog.setTestOnly();

const TagName = goog.require('goog.dom.TagName');
const TestHelper = goog.require('goog.testing.editor.TestHelper');
const dom = goog.require('goog.dom');
const node = goog.require('goog.editor.node');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let root;
let helper;

function findNodeWithHierarchy() {
  // Test a more complicated hierarchy.
  root.innerHTML = '<div>a<p>b<span>c</span>d</p>e</div>';
  assertEquals(
      String(TagName.DIV), helper.findTextNode('a').parentNode.tagName);
  assertEquals(String(TagName.P), helper.findTextNode('b').parentNode.tagName);
  assertEquals(
      String(TagName.SPAN), helper.findTextNode('c').parentNode.tagName);
  assertEquals(String(TagName.P), helper.findTextNode('d').parentNode.tagName);
  assertEquals(
      String(TagName.DIV), helper.findTextNode('e').parentNode.tagName);
}

function setUpAssertHtmlMatches() {
  let tag1;
  let tag2;

  if (userAgent.EDGE_OR_IE) {
    tag1 = TagName.DIV;
  } else if (userAgent.WEBKIT) {
    tag1 = TagName.P;
    tag2 = TagName.BR;
  } else if (userAgent.GECKO) {
    tag1 = TagName.SPAN;
    tag2 = TagName.BR;
  }

  let parent = dom.createDom(TagName.DIV);
  root.appendChild(parent);
  parent.style.fontSize = '2em';
  parent.style.display = 'none';
  if (userAgent.EDGE_OR_IE || userAgent.GECKO) {
    parent.appendChild(dom.createTextNode('NonWebKitText'));
  }

  if (tag1) {
    const e1 = dom.createDom(tag1);
    parent.appendChild(e1);
    parent = e1;
  }
  if (tag2) {
    parent.appendChild(dom.createDom(tag2));
  }
  parent.appendChild(dom.createTextNode('Text'));
  if (userAgent.WEBKIT) {
    root.firstChild.appendChild(dom.createTextNode('WebKitText'));
  }
}

testSuite({
  setUp() {
    root = dom.getElement('root');
    dom.removeChildren(root);
    helper = new TestHelper(root);
  },

  tearDown() {
    helper.dispose();
  },

  testSetRoot() {
    helper.setRoot(dom.getElement('root2'));
    helper.assertHtmlMatches('Root 2');
  },

  testSetupEditableElement() {
    helper.setUpEditableElement();
    assertTrue(node.isEditableContainer(root));
  },

  testTearDownEditableElement() {
    helper.setUpEditableElement();
    assertTrue(node.isEditableContainer(root));

    helper.tearDownEditableElement();
    assertFalse(node.isEditableContainer(root));
  },

  testFindNode() {
    // Test the easiest case.
    root.innerHTML = 'a<br>b';
    assertEquals(helper.findTextNode('a'), root.firstChild);
    assertEquals(helper.findTextNode('b'), root.lastChild);
    assertNull(helper.findTextNode('c'));
  },

  testFindNodeDuplicate() {
    // Test duplicate.
    root.innerHTML = 'c<br>c';
    assertEquals(
        'Should return first duplicate', helper.findTextNode('c'),
        root.firstChild);
  },

  testAssertHtmlMatches() {
    setUpAssertHtmlMatches();

    helper.assertHtmlMatches(
        '<div style="display: none; font-size: 2em">' +
        '[[IE EDGE GECKO]]NonWebKitText<div class="IE EDGE"><p class="WEBKIT">' +
        '<span class="GECKO"><br class="GECKO WEBKIT">Text</span></p></div>' +
        '</div>[[WEBKIT]]WebKitText');
  },

  testAssertHtmlMismatchText() {
    setUpAssertHtmlMatches();

    // Should fail due to mismatched text
    const e = assertThrowsJsUnitException(() => {
      helper.assertHtmlMatches(
          '<div style="display: none; font-size: 2em">' +
          '[[IE GECKO]]NonWebKitText<div class="IE"><p class="WEBKIT">' +
          '<span class="GECKO"><br class="GECKO WEBKIT">Bad</span></p></div>' +
          '</div>[[WEBKIT]]Extra');
    });
    assertContains('Text should match', e.message);
  },

  testAssertHtmlMismatchTag() {
    setUpAssertHtmlMatches();

    // Should fail due to mismatched tag
    const e = assertThrowsJsUnitException(() => {
      helper.assertHtmlMatches(
          '<span style="display: none; font-size: 2em">[[IE EDGE GECKO]]' +
          'NonWebKitText<div class="IE EDGE"><p class="WEBKIT">' +
          '<span class="GECKO"><br class="GECKO WEBKIT">Text</span></p></div>' +
          '</span>[[WEBKIT]]Extra');
    });
    assertContains('Tag names should match', e.message);
  },

  testAssertHtmlMismatchStyle() {
    setUpAssertHtmlMatches();

    // Should fail due to mismatched style
    const e = assertThrowsJsUnitException(() => {
      helper.assertHtmlMatches(
          '<div style="display: none; font-size: 3em">[[IE EDGE GECKO]]' +
          'NonWebKitText<div class="IE EDGE"><p class="WEBKIT">' +
          '<span class="GECKO"><br class="GECKO WEBKIT">Text</span></p></div>' +
          '</div>[[WEBKIT]]Extra');
    });
    assertContains('Should have same styles', e.message);
  },

  testAssertHtmlMismatchOptionalText() {
    setUpAssertHtmlMatches();

    // Should fail due to mismatched style
    const e = assertThrowsJsUnitException(() => {
      helper.assertHtmlMatches(
          '<div style="display: none; font-size: 2em">' +
          '[[IE EDGE GECKO]]Bad<div class="IE EDGE"><p class="WEBKIT">' +
          '<span class="GECKO"><br class="GECKO WEBKIT">Text</span></p></div>' +
          '</div>[[WEBKIT]]Bad');
    });
    assertContains('Text should match', e.message);
  },
});
