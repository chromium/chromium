/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.rangeTest');
goog.setTestOnly();

const Point = goog.require('goog.editor.range.Point');
const Range = goog.require('goog.dom.Range');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const editorRange = goog.require('goog.editor.range');
const googString = goog.require('goog.string');
const testSuite = goog.require('goog.testing.testSuite');
const testingDom = goog.require('goog.testing.dom');
const userAgent = goog.require('goog.userAgent');

let savedHtml;
let $;

// Ojan didn't believe this code worked, this was the case he
// thought was broken.  Keeping just as a regression test.

/** Normalize the body and return the normalized range. */
function normalizeBody(range) {
  const rangeFactory = editorRange.normalize(range);
  document.body.normalize();
  return rangeFactory();
}

/** Break a text node up into lots of little fragments. */
function fragmentText(text) {
  // NOTE(nicksantos): For some reason, splitText makes IE deeply
  // unhappy to the point where normalize and other normal DOM operations
  // start failing. It's a useful test for Firefox though, because different
  // versions of FireFox handle empty text nodes differently.
  // See goog.editor.BrowserFeature.
  if (userAgent.IE) {
    manualSplitText(text, 2);
    manualSplitText(text, 1);
    manualSplitText(text, 0);
    manualSplitText(text, 0);
  } else {
    text.splitText(2);
    text.splitText(1);

    text.splitText(0);
    text.splitText(0);
  }
}

/**
 * Clear the selection by re-parsing the DOM. Then restore the saved
 * selection.
 * @param {dom.SavedRange} saved The saved range.
 */
function clearSelectionAndRestoreSaved(saved) {
  Range.clearSelection(window);
  assertFalse(Range.hasSelection(window));
  saved.restore();
  assertTrue(Range.hasSelection(window));
}

function manualSplitText(node, pos) {
  const newNodeString = node.nodeValue.substr(pos);
  node.nodeValue = node.nodeValue.substr(0, pos);
  dom.insertSiblingAfter(document.createTextNode(newNodeString), node);
}

function assertPointEquals(node, offset, actualPoint) {
  assertEquals('Point has wrong node', node, actualPoint.node);
  assertEquals('Point has wrong offset', offset, actualPoint.offset);
}
testSuite({
  setUpPage() {
    $ = dom.getElement;
  },

  setUp() {
    savedHtml = $('root').innerHTML;
  },

  tearDown() {
    $('root').innerHTML = savedHtml;
  },

  testNoNarrow() {
    const def = $('def');
    const jkl = $('jkl');
    let range = Range.createFromNodes(def.firstChild, 1, jkl.firstChild, 2);

    range = editorRange.narrow(range, $('parentNode'));
    testingDom.assertRangeEquals(def.firstChild, 1, jkl.firstChild, 2, range);
  },

  testNarrowAtEndEdge() {
    const def = $('def');
    const jkl = $('jkl');
    let range = Range.createFromNodes(def.firstChild, 1, jkl.firstChild, 2);

    range = editorRange.narrow(range, def);
    testingDom.assertRangeEquals(def.firstChild, 1, def.firstChild, 3, range);
  },

  testNarrowAtStartEdge() {
    const def = $('def');
    const jkl = $('jkl');
    let range = Range.createFromNodes(def.firstChild, 1, jkl.firstChild, 2);

    range = editorRange.narrow(range, jkl);

    testingDom.assertRangeEquals(jkl.firstChild, 0, jkl.firstChild, 2, range);
  },

  testNarrowOutsideElement() {
    const def = $('def');
    const jkl = $('jkl');
    let range = Range.createFromNodes(def.firstChild, 1, jkl.firstChild, 2);

    range = editorRange.narrow(range, $('pqr'));
    assertNull(range);
  },

  testNoExpand() {
    const div = $('parentNode');
    div.innerHTML = '<div>longword</div>';
    // Select "ongwo" and make sure we don't expand since this is not
    // a full container.
    const textNode = div.firstChild.firstChild;
    let range = Range.createFromNodes(textNode, 1, textNode, 6);

    range = editorRange.expand(range);
    testingDom.assertRangeEquals(textNode, 1, textNode, 6, range);
  },

  testSimpleExpand() {
    const div = $('parentNode');
    div.innerHTML = '<div>longword</div>foo';
    // Select "longword" and make sure we do expand to include the div since
    // the full container text is selected.
    const textNode = div.firstChild.firstChild;
    let range = Range.createFromNodes(textNode, 0, textNode, 8);

    range = editorRange.expand(range);
    testingDom.assertRangeEquals(div, 0, div, 1, range);

    // Select "foo" and make sure we expand out to the parent div.
    const fooNode = div.lastChild;
    range = Range.createFromNodes(fooNode, 0, fooNode, 3);

    range = editorRange.expand(range);
    testingDom.assertRangeEquals(div, 1, div, 2, range);
  },

  testDoubleExpand() {
    const div = $('parentNode');
    div.innerHTML = '<div><span>longword</span></div>foo';
    // Select "longword" and make sure we do expand to include the span
    // and the div since both of their full contents are selected.
    const textNode = div.firstChild.firstChild.firstChild;
    let range = Range.createFromNodes(textNode, 0, textNode, 8);

    range = editorRange.expand(range);
    testingDom.assertRangeEquals(div, 0, div, 1, range);

    // Same visible position, different dom position.
    // Start in text node, end in span.
    range = Range.createFromNodes(textNode, 0, textNode.parentNode, 1);

    range = editorRange.expand(range);
    testingDom.assertRangeEquals(div, 0, div, 1, range);
  },

  testMultipleChildrenExpand() {
    const div = $('parentNode');
    div.innerHTML = '<ol><li>one</li><li>two</li><li>three</li></ol>';
    // Select "two" and make sure we expand to the li, but not the ol.
    const li = div.firstChild.childNodes[1];
    const textNode = li.firstChild;
    let range = Range.createFromNodes(textNode, 0, textNode, 3);

    range = editorRange.expand(range);
    testingDom.assertRangeEquals(li.parentNode, 1, li.parentNode, 2, range);

    // Make the same visible selection, only slightly different dom position.
    // Select starting from the text node, but ending in the li.
    range = Range.createFromNodes(textNode, 0, li, 1);

    range = editorRange.expand(range);
    testingDom.assertRangeEquals(li.parentNode, 1, li.parentNode, 2, range);
  },

  testSimpleDifferentContainersExpand() {
    const div = $('parentNode');
    div.innerHTML = '<ol><li>1</li><li><b>bold</b><i>italic</i></li></ol>';
    // Select all of "bold" and "italic" at the text node level, and
    // make sure we expand to the li.
    const li = div.firstChild.childNodes[1];
    const boldNode = li.childNodes[0];
    const italicNode = li.childNodes[1];
    let range =
        Range.createFromNodes(boldNode.firstChild, 0, italicNode.firstChild, 6);

    range = editorRange.expand(range);
    testingDom.assertRangeEquals(li.parentNode, 1, li.parentNode, 2, range);

    // Make the same visible selection, only slightly different dom position.
    // Select "bold" at the b node level and "italic" at the text node level.
    range = Range.createFromNodes(boldNode, 0, italicNode.firstChild, 6);

    range = editorRange.expand(range);
    testingDom.assertRangeEquals(li.parentNode, 1, li.parentNode, 2, range);
  },

  testSimpleDifferentContainersSmallExpand() {
    const div = $('parentNode');
    div.innerHTML = '<ol><li>1</li><li><b>bold</b><i>italic</i>' +
        '<u>under</u></li></ol>';
    // Select all of "bold" and "italic", but we can't expand to the
    // entire li since we didn't select "under".
    const li = div.firstChild.childNodes[1];
    const boldNode = li.childNodes[0];
    const italicNode = li.childNodes[1];
    let range =
        Range.createFromNodes(boldNode.firstChild, 0, italicNode.firstChild, 6);

    range = editorRange.expand(range);
    testingDom.assertRangeEquals(li, 0, li, 2, range);

    // Same visible position, different dom position.
    // Select "bold" starting in text node, "italic" at i node.
    range = Range.createFromNodes(boldNode.firstChild, 0, italicNode, 1);

    range = editorRange.expand(range);
    testingDom.assertRangeEquals(li, 0, li, 2, range);
  },

  testEmbeddedDifferentContainersExpand() {
    const div = $('parentNode');
    div.innerHTML = '<div><b><i>italic</i>after</b><u>under</u></div>foo';
    // Select "italic" "after" "under", should expand all the way to parent.
    const boldNode = div.firstChild.childNodes[0];
    const italicNode = boldNode.childNodes[0];
    const underNode = div.firstChild.childNodes[1];
    let range = Range.createFromNodes(
        italicNode.firstChild, 0, underNode.firstChild, 5);

    range = editorRange.expand(range);
    testingDom.assertRangeEquals(div, 0, div, 1, range);
  },

  testReverseSimpleExpand() {
    const div = $('parentNode');
    div.innerHTML = '<div>longword</div>foo';
    // Select "longword" and make sure we do expand to include the div since
    // the full container text is selected.
    const textNode = div.firstChild.firstChild;
    let range = Range.createFromNodes(textNode, 8, textNode, 0);

    range = editorRange.expand(range);

    testingDom.assertRangeEquals(div, 0, div, 1, range);
  },

  testExpandWithStopNode() {
    const div = $('parentNode');
    div.innerHTML = '<div><span>word</span></div>foo';
    // Select "word".
    const span = div.firstChild.firstChild;
    const textNode = span.firstChild;
    let range = Range.createFromNodes(textNode, 0, textNode, 4);

    range = editorRange.expand(range);

    testingDom.assertRangeEquals(div, 0, div, 1, range);

    // Same selection, but force stop at the span.
    range = Range.createFromNodes(textNode, 0, textNode, 4);

    range = editorRange.expand(range, span);

    testingDom.assertRangeEquals(span, 0, span, 1, range);
  },

  testOjanCase() {
    const div = $('parentNode');
    div.innerHTML = '<em><i><b>foo</b>bar</i></em>';
    // Select "foo", at text node level.
    const iNode = div.firstChild.firstChild;
    const textNode = iNode.firstChild.firstChild;
    let range = Range.createFromNodes(textNode, 0, textNode, 3);

    range = editorRange.expand(range);

    testingDom.assertRangeEquals(iNode, 0, iNode, 1, range);

    // Same selection, at b node level.
    range = Range.createFromNodes(iNode.firstChild, 0, iNode.firstChild, 1);
    range = editorRange.expand(range);

    testingDom.assertRangeEquals(iNode, 0, iNode, 1, range);
  },

  testPlaceCursorNextToLeft() {
    const div = $('parentNode');
    div.innerHTML = 'foo<div id="bar">bar</div>baz';
    const node = $('bar');
    const range = editorRange.placeCursorNextTo(node, true);

    const expose = testingDom.exposeNode;
    assertEquals(
        'Selection should be to the left of the node ' + expose(node) + ',' +
            expose(range.getStartNode().nextSibling),
        node, range.getStartNode().nextSibling);
    assertEquals('Selection should be collapsed', true, range.isCollapsed());
  },

  testPlaceCursorNextToRight() {
    const div = $('parentNode');
    div.innerHTML = 'foo<div id="bar">bar</div>baz';
    const node = $('bar');
    const range = editorRange.placeCursorNextTo(node, false);

    assertEquals(
        'Selection should be to the right of the node', node,
        range.getStartNode().previousSibling);
    assertEquals('Selection should be collapsed', true, range.isCollapsed());
  },

  testPlaceCursorNextTo_rightOfLineBreak() {
    const div = $('parentNode');
    div.innerHTML = '<div contentEditable="true">hhhh<br />h</div>';
    const children = div.firstChild.childNodes;
    assertEquals(3, children.length);
    const node = children[1];
    const range = editorRange.placeCursorNextTo(node, false);
    assertEquals(node.nextSibling, range.getStartNode());
  },

  testPlaceCursorNextTo_leftOfHr() {
    const div = $('parentNode');
    div.innerHTML = '<hr />aaa';
    const children = div.childNodes;
    assertEquals(2, children.length);
    const node = children[0];
    const range = editorRange.placeCursorNextTo(node, true);

    assertEquals(div, range.getStartNode());
    assertEquals(0, range.getStartOffset());
  },

  testPlaceCursorNextTo_rightOfHr() {
    const div = $('parentNode');
    div.innerHTML = 'aaa<hr>';
    const children = div.childNodes;
    assertEquals(2, children.length);
    const node = children[1];
    const range = editorRange.placeCursorNextTo(node, false);

    assertEquals(div, range.getStartNode());
    assertEquals(2, range.getStartOffset());
  },

  testPlaceCursorNextTo_rightOfImg() {
    const div = $('parentNode');
    div.innerHTML =
        'aaa<img src="https://www.google.com/images/srpr/logo3w.png">bbb';
    const children = div.childNodes;
    assertEquals(3, children.length);
    const imgNode = children[1];
    const range = editorRange.placeCursorNextTo(imgNode, false);

    assertEquals(
        'range node should be the right sibling of img tag', children[2],
        range.getStartNode());
    assertEquals(0, range.getStartOffset());
  },

  testPlaceCursorNextTo_rightOfImgAtEnd() {
    const div = $('parentNode');
    div.innerHTML =
        'aaa<img src="https://www.google.com/images/srpr/logo3w.png">';
    const children = div.childNodes;
    assertEquals(2, children.length);
    const imgNode = children[1];
    const range = editorRange.placeCursorNextTo(imgNode, false);

    assertEquals(
        'range node should be the parent of img', div, range.getStartNode());
    assertEquals(
        'offset should be right after the img tag', 2, range.getStartOffset());
  },

  testPlaceCursorNextTo_leftOfImg() {
    const div = $('parentNode');
    div.innerHTML =
        '<img src="https://www.google.com/images/srpr/logo3w.png">xxx';
    const children = div.childNodes;
    assertEquals(2, children.length);
    const imgNode = children[0];
    const range = editorRange.placeCursorNextTo(imgNode, true);

    assertEquals(
        'range node should be the parent of img', div, range.getStartNode());
    assertEquals(
        'offset should point to the img tag', 0, range.getStartOffset());
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testPlaceCursorNextTo_rightOfFirstOfTwoImgTags() {
    const div = $('parentNode');
    div.innerHTML =
        'aaa<img src="https://www.google.com/images/srpr/logo3w.png">' +
        '<img src="https://www.google.com/images/srpr/logo3w.png">';
    const children = div.childNodes;
    assertEquals(3, children.length);
    const imgNode = children[1];  // First of two IMG nodes
    const range = editorRange.placeCursorNextTo(imgNode, false);

    assertEquals(
        'range node should be the parent of img instead of ' +
            'node with innerHTML=' + range.getStartNode().innerHTML,
        div, range.getStartNode());
    assertEquals(
        'offset should be right after the img tag', 2, range.getStartOffset());
  },

  testGetDeepEndPoint() {
    const div = $('parentNode');
    const def = $('def');
    const jkl = $('jkl');

    assertPointEquals(
        div.firstChild, 0,
        editorRange.getDeepEndPoint(Range.createFromNodeContents(div), true));
    assertPointEquals(
        div.lastChild, div.lastChild.length,
        editorRange.getDeepEndPoint(Range.createFromNodeContents(div), false));

    assertPointEquals(
        def.firstChild, 0,
        editorRange.getDeepEndPoint(Range.createCaret(div, 1), true));
    assertPointEquals(
        def.nextSibling, 0,
        editorRange.getDeepEndPoint(Range.createCaret(div, 2), true));
  },

  testNormalizeOnNormalizedDom() {
    const defText = $('def').firstChild;
    const jklText = $('jkl').firstChild;
    const range = Range.createFromNodes(defText, 1, jklText, 2);

    const newRange = normalizeBody(range);
    testingDom.assertRangeEquals(defText, 1, jklText, 2, newRange);
  },

  testDeepPointFindingOnNormalizedDom() {
    const def = $('def');
    const jkl = $('jkl');
    const range = Range.createFromNodes(def, 0, jkl, 1);

    const newRange = normalizeBody(range);

    // Make sure that newRange is measured relative to the text nodes,
    // not the DIV elements.
    testingDom.assertRangeEquals(
        def.firstChild, 0, jkl.firstChild, 3, newRange);
  },

  testNormalizeOnVeryFragmentedDom() {
    let defText = $('def').firstChild;
    let jklText = $('jkl').firstChild;
    const range = Range.createFromNodes(defText, 1, jklText, 2);

    // Fragment the DOM a bunch.
    fragmentText(defText);
    fragmentText(jklText);

    const newRange = normalizeBody(range);

    // our old text nodes may not be valid anymore. find new ones.
    defText = $('def').firstChild;
    jklText = $('jkl').firstChild;

    testingDom.assertRangeEquals(defText, 1, jklText, 2, newRange);
  },

  testNormalizeOnDivWithEmptyTextNodes() {
    const emptyDiv = $('normalizeTest-with-empty-text-nodes');

    // Append empty text nodes to the emptyDiv.
    const tnode1 = dom.createTextNode('');
    const tnode2 = dom.createTextNode('');
    const tnode3 = dom.createTextNode('');

    dom.appendChild(emptyDiv, tnode1);
    dom.appendChild(emptyDiv, tnode2);
    dom.appendChild(emptyDiv, tnode3);

    const range = Range.createFromNodes(emptyDiv, 1, emptyDiv, 2);

    // Cannot use document.body.normalize() as it fails to normalize the div
    // (in IE) if it has nothing but empty text nodes.
    const newRange = editorRange.rangePreservingNormalize(emptyDiv, range);

    if (userAgent.GECKO &&
        googString.compareVersions(userAgent.VERSION, '1.9') == -1) {
      // In FF2, node.normalize() leaves an empty textNode in the div, unlike
      // other browsers where the div is left with no children.
      testingDom.assertRangeEquals(
          emptyDiv.firstChild, 0, emptyDiv.firstChild, 0, newRange);
    } else {
      testingDom.assertRangeEquals(emptyDiv, 0, emptyDiv, 0, newRange);
    }
  },

  testRangeCreatedInVeryFragmentedDom() {
    const def = $('def');
    let defText = def.firstChild;
    const jkl = $('jkl');
    let jklText = jkl.firstChild;

    // Fragment the DOM a bunch.
    fragmentText(defText);
    fragmentText(jklText);

    // Notice that there are two empty text nodes at the beginning of each
    // fragmented node.
    const range = Range.createFromNodes(def, 3, jkl, 4);

    const newRange = normalizeBody(range);

    // our old text nodes may not be valid anymore. find new ones.
    defText = $('def').firstChild;
    jklText = $('jkl').firstChild;
    testingDom.assertRangeEquals(defText, 1, jklText, 2, newRange);
  },

  testNormalizeInFragmentedDomWithPreviousSiblings() {
    let ghiText = $('def').nextSibling;
    let mnoText = $('jkl').nextSibling;
    const range = Range.createFromNodes(ghiText, 1, mnoText, 2);

    // Fragment the DOM a bunch.
    fragmentText($('def').previousSibling);  // fragment abc
    fragmentText(ghiText);
    fragmentText(mnoText);

    const newRange = normalizeBody(range);

    // our old text nodes may not be valid anymore. find new ones.
    ghiText = $('def').nextSibling;
    mnoText = $('jkl').nextSibling;

    testingDom.assertRangeEquals(ghiText, 1, mnoText, 2, newRange);
  },

  testRangeCreatedInFragmentedDomWithPreviousSiblings() {
    const def = $('def');
    let ghiText = $('def').nextSibling;
    const jkl = $('jkl');
    let mnoText = $('jkl').nextSibling;

    // Fragment the DOM a bunch.
    fragmentText($('def').previousSibling);  // fragment abc
    fragmentText(ghiText);
    fragmentText(mnoText);

    // Notice that there are two empty text nodes at the beginning of each
    // fragmented node.
    const root = $('parentNode');
    const range = Range.createFromNodes(root, 9, root, 16);

    const newRange = normalizeBody(range);

    // our old text nodes may not be valid anymore. find new ones.
    ghiText = $('def').nextSibling;
    mnoText = $('jkl').nextSibling;
    testingDom.assertRangeEquals(ghiText, 1, mnoText, 2, newRange);
  },

  /**
   * Branched from the tests for dom.SavedCaretRange.
   * @suppress {visibility} suppression added to enable type checking
   */
  testSavedCaretRange() {
    let def = $('def-1');
    let jkl = $('jkl-1');

    const range = Range.createFromNodes(def.firstChild, 1, jkl.firstChild, 2);
    range.select();

    const saved = editorRange.saveUsingNormalizedCarets(range);
    assertHTMLEquals(
        'd<span id="' + saved.startCaretId_ + '"></span>ef', def.innerHTML);
    assertHTMLEquals(
        'jk<span id="' + saved.endCaretId_ + '"></span>l', jkl.innerHTML);

    clearSelectionAndRestoreSaved(saved);

    const selection = Range.createFromWindow(window);
    def = $('def-1');
    jkl = $('jkl-1');
    assertHTMLEquals('def', def.innerHTML);
    assertHTMLEquals('jkl', jkl.innerHTML);

    // Check that everything was normalized ok.
    assertEquals(1, def.childNodes.length);
    assertEquals(1, jkl.childNodes.length);
    testingDom.assertRangeEquals(
        def.firstChild, 1, jkl.firstChild, 2, selection);
  },

  testRangePreservingNormalize() {
    const parent = $('normalizeTest-4');
    const def = $('def-4');
    const jkl = $('jkl-4');
    fragmentText(def.firstChild);
    fragmentText(jkl.firstChild);

    let range = Range.createFromNodes(def, 3, jkl, 4);
    const oldRangeDescription = testingDom.exposeRange(range);
    range = editorRange.rangePreservingNormalize(parent, range);

    // Check that everything was normalized ok.
    assertEquals(
        'def should have 1 child; range is ' + testingDom.exposeRange(range) +
            ', range was ' + oldRangeDescription,
        1, def.childNodes.length);
    assertEquals(
        'jkl should have 1 child; range is ' + testingDom.exposeRange(range) +
            ', range was ' + oldRangeDescription,
        1, jkl.childNodes.length);
    testingDom.assertRangeEquals(def.firstChild, 1, jkl.firstChild, 2, range);
  },

  testRangePreservingNormalizeWhereEndNodePreviousSiblingIsSplit() {
    const parent = $('normalizeTest-with-br');
    const br = parent.childNodes[1];
    fragmentText(parent.firstChild);

    let range = Range.createFromNodes(parent, 3, br, 0);
    range = editorRange.rangePreservingNormalize(parent, range);

    // Code used to throw an error here.

    assertEquals('parent should have 3 children', 3, parent.childNodes.length);
    testingDom.assertRangeEquals(parent.firstChild, 1, parent, 1, range);
  },

  testRangePreservingNormalizeWhereStartNodePreviousSiblingIsSplit() {
    const parent = $('normalizeTest-with-br');
    const br = parent.childNodes[1];
    fragmentText(parent.firstChild);
    fragmentText(parent.lastChild);

    let range = Range.createFromNodes(br, 0, parent, 9);
    range = editorRange.rangePreservingNormalize(parent, range);

    // Code used to throw an error here.

    assertEquals('parent should have 3 children', 3, parent.childNodes.length);
    testingDom.assertRangeEquals(parent, 1, parent.lastChild, 1, range);
  },

  testSelectionPreservingNormalize1() {
    const parent = $('normalizeTest-2');
    const def = $('def-2');
    const jkl = $('jkl-2');
    fragmentText(def.firstChild);
    fragmentText(jkl.firstChild);

    Range.createFromNodes(def, 3, jkl, 4).select();
    assertFalse(Range.createFromWindow(window).isReversed());

    const oldRangeDescription =
        testingDom.exposeRange(Range.createFromWindow(window));
    editorRange.selectionPreservingNormalize(parent);

    // Check that everything was normalized ok.
    const range = Range.createFromWindow(window);
    assertFalse(range.isReversed());

    assertEquals(
        'def should have 1 child; range is ' + testingDom.exposeRange(range) +
            ', range was ' + oldRangeDescription,
        1, def.childNodes.length);
    assertEquals(
        'jkl should have 1 child; range is ' + testingDom.exposeRange(range) +
            ', range was ' + oldRangeDescription,
        1, jkl.childNodes.length);
    testingDom.assertRangeEquals(def.firstChild, 1, jkl.firstChild, 2, range);
  },

  /**
   * Make sure that selectionPreservingNormalize doesn't explode with no
   * selection in the document.
   */
  testSelectionPreservingNormalize2() {
    const parent = $('normalizeTest-3');
    const def = $('def-3');
    const jkl = $('jkl-3');
    def.firstChild.splitText(1);
    jkl.firstChild.splitText(2);

    Range.clearSelection(window);
    editorRange.selectionPreservingNormalize(parent);

    // Check that everything was normalized ok.
    assertEquals(1, def.childNodes.length);
    assertEquals(1, jkl.childNodes.length);
    assertFalse(Range.hasSelection(window));
  },

  testSelectionPreservingNormalize3() {
    if (userAgent.EDGE_OR_IE) {
      return;
    }
    const parent = $('normalizeTest-2');
    const def = $('def-2');
    const jkl = $('jkl-2');
    fragmentText(def.firstChild);
    fragmentText(jkl.firstChild);

    Range.createFromNodes(jkl, 4, def, 3).select();
    assertTrue(Range.createFromWindow(window).isReversed());

    const oldRangeDescription =
        testingDom.exposeRange(Range.createFromWindow(window));
    editorRange.selectionPreservingNormalize(parent);

    // Check that everything was normalized ok.
    const range = Range.createFromWindow(window);
    assertTrue(range.isReversed());

    assertEquals(
        'def should have 1 child; range is ' + testingDom.exposeRange(range) +
            ', range was ' + oldRangeDescription,
        1, def.childNodes.length);
    assertEquals(
        'jkl should have 1 child; range is ' + testingDom.exposeRange(range) +
            ', range was ' + oldRangeDescription,
        1, jkl.childNodes.length);
    testingDom.assertRangeEquals(def.firstChild, 1, jkl.firstChild, 2, range);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSelectionPreservingNormalizeAfterPlaceCursorNextTo() {
    const parent = $('normalizeTest-with-div');
    editorRange.placeCursorNextTo(parent.firstChild);
    editorRange.selectionPreservingNormalize(parent);

    // Code used to throw an exception here.
  },

  testSelectNodeStartSimple() {
    const div = $('parentNode');
    div.innerHTML = '<p>Cursor should go in here</p>';

    editorRange.selectNodeStart(div);
    const range = Range.createFromWindow(window);

    testingDom.assertRangeEquals(
        div.firstChild.firstChild, 0, div.firstChild.firstChild, 0, range);
  },

  testSelectNodeStartBr() {
    const div = $('parentNode');
    div.innerHTML = '<p><br>Cursor should go in here</p>';

    editorRange.selectNodeStart(div);
    const range = Range.createFromWindow(window);
    // We have to skip the BR since Gecko can't render a cursor at a BR.
    testingDom.assertRangeEquals(div.firstChild, 0, div.firstChild, 0, range);
  },

  testIsEditable() {
    const containerElement = document.getElementById('editableTest');
    // Find editable container element's index.
    let containerIndex = 0;
    let currentSibling = containerElement;
    while (currentSibling = currentSibling.previousSibling) {
      containerIndex++;
    }

    const editableContainer = Range.createFromNodes(
        containerElement.parentNode, containerIndex,
        containerElement.parentNode, containerIndex + 1);
    assertFalse(
        'Range containing container element not considered editable',
        editorRange.isEditable(editableContainer));

    const allEditableChildren = Range.createFromNodes(
        containerElement, 0, containerElement,
        containerElement.childNodes.length);
    assertTrue(
        'Range of all of container element children considered editable',
        editorRange.isEditable(allEditableChildren));

    const someEditableChildren =
        Range.createFromNodes(containerElement, 2, containerElement, 6);
    assertTrue(
        'Range of some container element children considered editable',
        editorRange.isEditable(someEditableChildren));

    const mixedEditableNonEditable = Range.createFromNodes(
        containerElement.previousSibling, 0, containerElement, 2);
    assertFalse(
        'Range overlapping some content not considered editable',
        editorRange.isEditable(mixedEditableNonEditable));
  },

  testIntersectsTag() {
    const root = $('root');
    root.innerHTML =
        '<b>Bold</b><p><span><code>x</code></span></p><p>y</p><i>Italic</i>';

    // Select the whole thing.
    let range = Range.createFromNodeContents(root);
    assertTrue(editorRange.intersectsTag(range, TagName.DIV));
    assertTrue(editorRange.intersectsTag(range, TagName.B));
    assertTrue(editorRange.intersectsTag(range, TagName.I));
    assertTrue(editorRange.intersectsTag(range, TagName.CODE));
    assertFalse(editorRange.intersectsTag(range, TagName.U));

    // Just select italic.
    range = Range.createFromNodes(root, 3, root, 4);
    assertTrue(editorRange.intersectsTag(range, TagName.DIV));
    assertFalse(editorRange.intersectsTag(range, TagName.B));
    assertTrue(editorRange.intersectsTag(range, TagName.I));
    assertFalse(editorRange.intersectsTag(range, TagName.CODE));
    assertFalse(editorRange.intersectsTag(range, TagName.U));

    // Select "ld x y".
    range = Range.createFromNodes(
        root.firstChild.firstChild, 2, root.childNodes[2], 1);
    assertTrue(editorRange.intersectsTag(range, TagName.DIV));
    assertTrue(editorRange.intersectsTag(range, TagName.B));
    assertFalse(editorRange.intersectsTag(range, TagName.I));
    assertTrue(editorRange.intersectsTag(range, TagName.CODE));
    assertFalse(editorRange.intersectsTag(range, TagName.U));

    // Select ol.
    range = Range.createFromNodes(
        root.firstChild.firstChild, 1, root.firstChild.firstChild, 3);
    assertTrue(editorRange.intersectsTag(range, TagName.DIV));
    assertTrue(editorRange.intersectsTag(range, TagName.B));
    assertFalse(editorRange.intersectsTag(range, TagName.I));
    assertFalse(editorRange.intersectsTag(range, TagName.CODE));
    assertFalse(editorRange.intersectsTag(range, TagName.U));
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testNormalizeNode() {
    let div = dom.createDom(TagName.DIV, null, 'a', 'b', 'c');
    assertEquals(3, div.childNodes.length);
    editorRange.normalizeNode(div);
    assertEquals(1, div.childNodes.length);
    assertEquals('abc', div.firstChild.nodeValue);

    div = dom.createDom(
        TagName.DIV, null, dom.createDom(TagName.SPAN, null, '1', '2'),
        dom.createTextNode(''), dom.createDom(TagName.BR), 'b', 'c');
    assertEquals(5, div.childNodes.length);
    assertEquals(2, div.firstChild.childNodes.length);
    editorRange.normalizeNode(div);
    if (userAgent.GECKO && !userAgent.isVersionOrHigher(1.9) ||
        userAgent.WEBKIT && !userAgent.isVersionOrHigher(526)) {
      // Old Gecko and Webkit versions don't delete the empty node.
      assertEquals(4, div.childNodes.length);
    } else {
      assertEquals(3, div.childNodes.length);
    }
    assertEquals(1, div.firstChild.childNodes.length);
    assertEquals('12', div.firstChild.firstChild.nodeValue);
    assertEquals('bc', div.lastChild.nodeValue);
    assertEquals(String(TagName.BR), div.lastChild.previousSibling.tagName);
  },

  testDeepestPoint() {
    const parent = $('parentNode');
    const def = $('def');

    assertEquals(def, parent.childNodes[1]);

    const deepestPoint = Point.createDeepestPoint;

    const defStartLeft = deepestPoint(parent, 1, true);
    assertPointEquals(
        def.previousSibling, def.previousSibling.nodeValue.length,
        defStartLeft);

    const defStartRight = deepestPoint(parent, 1, false);
    assertPointEquals(def.firstChild, 0, defStartRight);

    const defEndLeft = deepestPoint(parent, 2, true);
    assertPointEquals(
        def.firstChild, def.firstChild.nodeValue.length, defEndLeft);

    const defEndRight = deepestPoint(parent, 2, false);
    assertPointEquals(def.nextSibling, 0, defEndRight);
  },
});
