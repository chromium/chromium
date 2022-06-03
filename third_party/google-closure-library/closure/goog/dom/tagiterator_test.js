/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.TagIteratorTest');
goog.setTestOnly();

const StopIteration = goog.require('goog.iter.StopIteration');
const TagIterator = goog.require('goog.dom.TagIterator');
const TagName = goog.require('goog.dom.TagName');
const TagWalkType = goog.require('goog.dom.TagWalkType');
const dom = goog.require('goog.dom');
const iter = goog.require('goog.iter');
const testSuite = goog.require('goog.testing.testSuite');
const testingDom = goog.require('goog.testing.dom');

let it;
let pos;

function assertStartTag(type) {
  assertEquals(
      `Position ${pos} should be start tag`, TagWalkType.START_TAG, it.tagType);
  assertTrue('isStartTag should return true', it.isStartTag());
  assertFalse('isEndTag should return false', it.isEndTag());
  assertFalse('isNonElement should return false', it.isNonElement());
  assertEquals(
      `Position ${pos} should be ${type}`, String(type), it.node.tagName);
}

function assertEndTag(type) {
  assertEquals(
      `Position ${pos} should be end tag`, TagWalkType.END_TAG, it.tagType);
  assertFalse('isStartTag should return false', it.isStartTag());
  assertTrue('isEndTag should return true', it.isEndTag());
  assertFalse('isNonElement should return false', it.isNonElement());
  assertEquals(
      `Position ${pos} should be ${type}`, String(type), it.node.tagName);
}

function assertTextNode(value) {
  assertEquals(
      `Position ${pos} should be text node`, TagWalkType.OTHER, it.tagType);
  assertFalse('isStartTag should return false', it.isStartTag());
  assertFalse('isEndTag should return false', it.isEndTag());
  assertTrue('isNonElement should return true', it.isNonElement());
  assertEquals(
      `Position ${pos} should be "${value}"`, value, it.node.nodeValue);
}

testSuite({
  testBasicHTML() {
    it = new TagIterator(dom.getElement('test'));
    pos = 0;

    iter.forEach(it, () => {
      pos++;
      switch (pos) {
        case 1:
          assertStartTag(TagName.DIV);
          break;
        case 2:
          assertStartTag(TagName.A);
          break;
        case 3:
          assertTextNode('T');
          break;
        case 4:
          assertStartTag(TagName.B);
          assertEquals('Depth at <B> should be 3', 3, it.depth);
          break;
        case 5:
          assertTextNode('e');
          break;
        case 6:
          assertEndTag(TagName.B);
          break;
        case 7:
          assertTextNode('xt');
          break;
        case 8:
          assertEndTag(TagName.A);
          break;
        case 9:
          assertStartTag(TagName.SPAN);
          break;
        case 10:
          assertEndTag(TagName.SPAN);
          break;
        case 11:
          assertStartTag(TagName.P);
          break;
        case 12:
          assertTextNode('Text');
          break;
        case 13:
          assertEndTag(TagName.P);
          break;
        case 14:
          assertEndTag(TagName.DIV);
          assertEquals('Depth at end should be 0', 0, it.depth);
          break;
        default:
          throw StopIteration;
      }
    });
  },

  testSkipTag() {
    it = new TagIterator(dom.getElement('test'));
    pos = 0;

    iter.forEach(it, () => {
      pos++;
      switch (pos) {
        case 1:
          assertStartTag(TagName.DIV);
          break;
        case 2:
          assertStartTag(TagName.A);
          it.skipTag();
          break;
        case 3:
          assertStartTag(TagName.SPAN);
          break;
        case 4:
          assertEndTag(TagName.SPAN);
          break;
        case 5:
          assertStartTag(TagName.P);
          break;
        case 6:
          assertTextNode('Text');
          break;
        case 7:
          assertEndTag(TagName.P);
          break;
        case 8:
          assertEndTag(TagName.DIV);
          assertEquals('Depth at end should be 0', 0, it.depth);
          break;
        default:
          throw StopIteration;
      }
    });
  },

  testRestartTag() {
    it = new TagIterator(dom.getElement('test'));
    pos = 0;
    let done = false;

    iter.forEach(it, () => {
      pos++;
      switch (pos) {
        case 1:
          assertStartTag(TagName.DIV);
          break;
        case 2:
          assertStartTag(TagName.A);
          it.skipTag();
          break;
        case 3:
          assertStartTag(TagName.SPAN);
          break;
        case 4:
          assertEndTag(TagName.SPAN);
          break;
        case 5:
          assertStartTag(TagName.P);
          break;
        case 6:
          assertTextNode('Text');
          break;
        case 7:
          assertEndTag(TagName.P);
          break;
        case 8:
          assertEndTag(TagName.DIV);
          assertEquals('Depth at end should be 0', 0, it.depth);

          // Do them all again, starting after this element.
          if (!done) {
            pos = 1;
            it.restartTag();
            done = true;
          }
          break;
        default:
          throw StopIteration;
      }
    });
  },

  testSkipTagReverse() {
    it = new TagIterator(dom.getElement('test'), true);
    pos = 9;

    iter.forEach(it, () => {
      pos--;
      switch (pos) {
        case 1:
          assertStartTag(TagName.DIV);
          assertEquals('Depth at end should be 0', 0, it.depth);
          break;
        case 2:
          assertEndTag(TagName.A);
          it.skipTag();
          break;
        case 3:
          assertStartTag(TagName.SPAN);
          break;
        case 4:
          assertEndTag(TagName.SPAN);
          break;
        case 5:
          assertStartTag(TagName.P);
          break;
        case 6:
          assertTextNode('Text');
          break;
        case 7:
          assertEndTag(TagName.P);
          break;
        case 8:
          assertEndTag(TagName.DIV);
          break;
        default:
          throw StopIteration;
      }
    });
  },

  testUnclosedLI() {
    it = new TagIterator(dom.getElement('test2'));
    pos = 0;

    iter.forEach(it, () => {
      pos++;
      switch (pos) {
        case 1:
          assertStartTag(TagName.UL);
          break;
        case 2:
          assertStartTag(TagName.LI);
          assertEquals('Depth at <LI> should be 2', 2, it.depth);
          break;
        case 3:
          assertTextNode('Not');
          break;
        case 4:
          assertEndTag(TagName.LI);
          break;
        case 5:
          assertStartTag(TagName.LI);
          assertEquals('Depth at second <LI> should be 2', 2, it.depth);
          break;
        case 6:
          assertTextNode('Closed');
          break;
        case 7:
          assertEndTag(TagName.LI);
          break;
        case 8:
          assertEndTag(TagName.UL);
          assertEquals('Depth at end should be 0', 0, it.depth);
          break;
        default:
          throw StopIteration;
      }
    });
  },

  testReversedUnclosedLI() {
    it = new TagIterator(dom.getElement('test2'), true);
    pos = 9;

    iter.forEach(it, () => {
      pos--;
      switch (pos) {
        case 1:
          assertStartTag(TagName.UL);
          assertEquals('Depth at start should be 0', 0, it.depth);
          break;
        case 2:
          assertStartTag(TagName.LI);
          break;
        case 3:
          assertTextNode('Not');
          break;
        case 4:
          assertEndTag(TagName.LI);
          assertEquals('Depth at <LI> should be 2', 2, it.depth);
          break;
        case 5:
          assertStartTag(TagName.LI);
          break;
        case 6:
          assertTextNode('Closed');
          break;
        case 7:
          assertEndTag(TagName.LI);
          assertEquals('Depth at second <LI> should be 2', 2, it.depth);
          break;
        case 8:
          assertEndTag(TagName.UL);
          break;
        default:
          throw StopIteration;
      }
    });
  },

  testConstrained() {
    it = new TagIterator(dom.getElement('test3'), false, false);
    pos = 0;

    iter.forEach(it, () => {
      pos++;
      switch (pos) {
        case 1:
          assertStartTag(TagName.DIV);
          break;
        case 2:
          assertTextNode('text');
          break;
        case 3:
          assertEndTag(TagName.DIV);
          break;
      }
    });

    assertEquals('Constrained iterator should stop at position 3.', 3, pos);
  },

  testUnconstrained() {
    it = new TagIterator(dom.getElement('test3'), false, true);
    pos = 0;

    iter.forEach(it, () => {
      pos++;
      switch (pos) {
        case 1:
          assertStartTag(TagName.DIV);
          break;
        case 2:
          assertTextNode('text');
          break;
        case 3:
          assertEndTag(TagName.DIV);
          break;
      }
    });

    assertNotEquals(
        'Unonstrained iterator should not stop at position 3.', 3, pos);
  },

  testConstrainedText() {
    it = new TagIterator(dom.getElement('test3').firstChild, false, false);
    pos = 0;

    iter.forEach(it, () => {
      pos++;
      switch (pos) {
        case 1:
          assertTextNode('text');
          break;
      }
    });

    assertEquals(
        'Constrained text iterator should stop at position 1.', 1, pos);
  },

  testReverseConstrained() {
    it = new TagIterator(dom.getElement('test3'), true, false);
    pos = 4;

    iter.forEach(it, () => {
      pos--;
      switch (pos) {
        case 1:
          assertStartTag(TagName.DIV);
          break;
        case 2:
          assertTextNode('text');
          break;
        case 3:
          assertEndTag(TagName.DIV);
          break;
      }
    });

    assertEquals(
        'Constrained reversed iterator should stop at position 1.', 1, pos);
  },

  testSpliceRemoveSingleNode() {
    const testDiv = dom.getElement('testSplice');
    testDiv.innerHTML = '<br/>';
    it = new TagIterator(testDiv.firstChild);

    iter.forEach(
        it, /**
               @suppress {strictMissingProperties} suppression added to enable
               type checking
             */
        (node, dummy, i) => {
          i.splice();
        });

    assertEquals('Node not removed', 0, testDiv.childNodes.length);
  },

  testSpliceRemoveFirstTextNode() {
    const testDiv = dom.getElement('testSplice');
    testDiv.innerHTML = 'hello<b>world</b><em>goodbye</em>';
    it = new TagIterator(testDiv.firstChild, false, true);

    iter.forEach(
        it, /**
               @suppress {strictMissingProperties} suppression added to enable
               type checking
             */
        (node, dummy, i) => {
          if (node.nodeType == 3 && node.data == 'hello') {
            i.splice();
          }
          if (node.nodeName == TagName.EM) {
            i.splice(dom.createDom(TagName.I, null, node.childNodes));
          }
        });

    testingDom.assertHtmlMatches(
        '<b>world</b><i>goodbye</i>', testDiv.innerHTML);
  },

  testSpliceReplaceFirstTextNode() {
    const testDiv = dom.getElement('testSplice');
    testDiv.innerHTML = 'hello<b>world</b>';
    it = new TagIterator(testDiv.firstChild, false, true);

    iter.forEach(
        it, /**
               @suppress {strictMissingProperties} suppression added to enable
               type checking
             */
        (node, dummy, i) => {
          if (node.nodeType == 3 && node.data == 'hello') {
            i.splice(dom.createDom(TagName.EM, null, 'HELLO'));
          } else if (node.nodeName == TagName.EM) {
            i.splice(dom.createDom(TagName.I, null, node.childNodes));
          }
        });

    testingDom.assertHtmlMatches('<i>HELLO</i><b>world</b>', testDiv.innerHTML);
  },

  testSpliceReplaceSingleNode() {
    const testDiv = dom.getElement('testSplice');
    testDiv.innerHTML = '<br/>';
    it = new TagIterator(testDiv.firstChild);

    iter.forEach(
        it, /**
               @suppress {strictMissingProperties} suppression added to enable
               type checking
             */
        (node, dummy, i) => {
          i.splice(dom.createDom(TagName.LINK), dom.createDom(TagName.IMG));
        });

    testingDom.assertHtmlMatches('<link><img>', testDiv.innerHTML);
  },

  testSpliceFlattenSingleNode() {
    const testDiv = dom.getElement('testSplice');
    testDiv.innerHTML = '<div><b>one</b>two<i>three</i></div>';
    it = new TagIterator(testDiv.firstChild);

    iter.forEach(
        it, /**
               @suppress {strictMissingProperties} suppression added to enable
               type checking
             */
        (node, dummy, i) => {
          i.splice(node.childNodes);
        });

    testingDom.assertHtmlMatches(
        '<b>one</b>two<i>three</i>', testDiv.innerHTML);
  },

  testSpliceMiddleNode() {
    const testDiv = dom.getElement('testSplice');
    testDiv.innerHTML = 'a<b>hello<span>world</span></b>c';
    it = new TagIterator(testDiv);

    iter.forEach(
        it, /**
               @suppress {strictMissingProperties} suppression added to enable
               type checking
             */
        (node, dummy, i) => {
          if (node.nodeName == TagName.B) {
            i.splice(dom.createDom(TagName.IMG));
          }
        });

    testingDom.assertHtmlMatches('a<img>c', testDiv.innerHTML);
  },

  testSpliceMiddleNodeReversed() {
    const testDiv = dom.getElement('testSplice');
    testDiv.innerHTML = 'a<b>hello<span>world</span></b>c';
    it = new TagIterator(testDiv, true);

    iter.forEach(
        it, /**
               @suppress {strictMissingProperties} suppression added to enable
               type checking
             */
        (node, dummy, i) => {
          if (node.nodeName == TagName.B) {
            i.splice(dom.createDom(TagName.IMG));
          }
        });

    testingDom.assertHtmlMatches('a<img>c', testDiv.innerHTML);
  },

  testSpliceMiddleNodeAtEndTag() {
    const testDiv = dom.getElement('testSplice');
    testDiv.innerHTML = 'a<b>hello<span>world</span></b>c';
    it = new TagIterator(testDiv);

    iter.forEach(
        it, /**
               @suppress {strictMissingProperties} suppression added to enable
               type checking
             */
        (node, dummy, i) => {
          if (node.tagName == TagName.B && i.isEndTag()) {
            i.splice(dom.createDom(TagName.IMG));
          }
        });

    testingDom.assertHtmlMatches('a<img>c', testDiv.innerHTML);
  },

  testSpliceMultipleNodes() {
    const testDiv = dom.getElement('testSplice');
    testDiv.innerHTML = '<strong>this</strong> is <em>from IE</em>';
    it = new TagIterator(testDiv);

    iter.forEach(
        it, /**
               @suppress {strictMissingProperties} suppression added to enable
               type checking
             */
        (node, dummy, i) => {
          let replace = null;
          if (node.nodeName == TagName.STRONG) {
            replace = dom.createDom(TagName.B, null, node.childNodes);
          } else if (node.nodeName == TagName.EM) {
            replace = dom.createDom(TagName.I, null, node.childNodes);
          }
          if (replace) {
            i.splice(replace);
          }
        });

    testingDom.assertHtmlMatches(
        '<b>this</b> is <i>from IE</i>', testDiv.innerHTML);
  },

  testSpliceMultipleNodesAtEnd() {
    const testDiv = dom.getElement('testSplice');
    testDiv.innerHTML = '<strong>this</strong> is <em>from IE</em>';
    it = new TagIterator(testDiv);

    iter.forEach(
        it, /**
               @suppress {strictMissingProperties} suppression added to enable
               type checking
             */
        (node, dummy, i) => {
          let replace = null;
          if (node.nodeName == TagName.STRONG && i.isEndTag()) {
            replace = dom.createDom(TagName.B, null, node.childNodes);
          } else if (node.nodeName == TagName.EM && i.isEndTag()) {
            replace = dom.createDom(TagName.I, null, node.childNodes);
          }
          if (replace) {
            i.splice(replace);
          }
        });

    testingDom.assertHtmlMatches(
        '<b>this</b> is <i>from IE</i>', testDiv.innerHTML);
  },

  testSpliceMultipleNodesReversed() {
    const testDiv = dom.getElement('testSplice');
    testDiv.innerHTML = '<strong>this</strong> is <em>from IE</em>';
    it = new TagIterator(testDiv, true);

    iter.forEach(
        it, /**
               @suppress {strictMissingProperties} suppression added to enable
               type checking
             */
        (node, dummy, i) => {
          let replace = null;
          if (node.nodeName == TagName.STRONG) {
            replace = dom.createDom(TagName.B, null, node.childNodes);
          } else if (node.nodeName == TagName.EM) {
            replace = dom.createDom(TagName.I, null, node.childNodes);
          }
          if (replace) {
            i.splice(replace);
          }
        });

    testingDom.assertHtmlMatches(
        '<b>this</b> is <i>from IE</i>', testDiv.innerHTML);
  },
});
