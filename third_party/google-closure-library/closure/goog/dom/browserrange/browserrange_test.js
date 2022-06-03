/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.browserrangeTest');
goog.setTestOnly();

const NodeType = goog.require('goog.dom.NodeType');
const Range = goog.require('goog.dom.Range');
const RangeEndpoint = goog.require('goog.dom.RangeEndpoint');
const TagName = goog.require('goog.dom.TagName');
const browserrange = goog.require('goog.dom.browserrange');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');
const testing = goog.require('goog.html.testing');
const testingDom = goog.require('goog.testing.dom');

let test1;
let test2;
let cetest;
let empty;
let dynamic;
let onlybrdiv;

/**
 * @param {string} str
 * @return {string}
 */
function normalizeHtml(str) {
  return str.toLowerCase().replace(/[\n\r\f"]/g, '');
}

// TODO(robbyw): We really need tests for (and code fixes for)
// createRangeFromNodes in the following cases:
// * BR boundary (before + after)

testSuite({
  setUpPage() {
    test1 = dom.getElement('test1');
    test2 = dom.getElement('test2');
    cetest = dom.getElement('cetest');
    empty = dom.getElement('empty');
    dynamic = dom.getElement('dynamic');
    onlybrdiv = dom.getElement('onlybr');
  },

  testCreate() {
    assertNotNull(
        'Browser range object can be created for node',
        browserrange.createRangeFromNodeContents(test1));
  },

  testRangeEndPoints() {
    const container = cetest.firstChild;
    const range = browserrange.createRangeFromNodes(container, 2, container, 2);
    range.select();

    const selRange = Range.createFromWindow();
    const startNode = selRange.getStartNode();
    const endNode = selRange.getEndNode();
    const startOffset = selRange.getStartOffset();
    const endOffset = selRange.getEndOffset();
    if (startNode.nodeType == NodeType.TEXT) {
      // Special case for Safari.
      assertEquals(
          'Start node should have text: abc', 'abc', startNode.nodeValue);
      assertEquals('End node should have text: abc', 'abc', endNode.nodeValue);
      assertEquals('Start offset should be 3', 3, startOffset);
      assertEquals('End offset should be 3', 3, endOffset);
    } else {
      assertEquals('Start node should be the first div', container, startNode);
      assertEquals('End node should be the first div', container, endNode);
      assertEquals('Start offset should be 2', 2, startOffset);
      assertEquals('End offset should be 2', 2, endOffset);
    }
  },

  testCreateFromNodeContents() {
    const range = Range.createFromNodeContents(onlybrdiv);
    testingDom.assertRangeEquals(onlybrdiv, 0, onlybrdiv, 1, range);
  },

  testCreateFromNodes() {
    const start = test1.firstChild;
    const range =
        browserrange.createRangeFromNodes(start, 2, test2.firstChild, 2);
    assertNotNull(
        'Browser range object can be created for W3C node range', range);

    assertEquals(
        'Start node should be selected at start endpoint', start,
        range.getStartNode());
    assertEquals(
        'Selection should start at offset 2', 2, range.getStartOffset());

    assertEquals(
        'Text node should be selected at end endpoint', test2.firstChild,
        range.getEndNode());
    assertEquals('Selection should end at offset 2', 2, range.getEndOffset());

    assertTrue(
        'Text content should be "xt\\s*ab"', /xt\s*ab/.test(range.getText()));
    assertFalse('Nodes range is not collapsed', range.isCollapsed());
    assertEquals(
        'Should contain correct html fragment', 'xt</div><div id=test2>ab',
        normalizeHtml(range.getHtmlFragment()));
    assertEquals(
        'Should contain correct valid html',
        '<div id=test1>xt</div><div id=test2>ab</div>',
        normalizeHtml(range.getValidHtml()));
  },

  testTextNode() {
    const range = browserrange.createRangeFromNodeContents(test1.firstChild);

    assertEquals(
        'Text node should be selected at start endpoint', 'Text',
        range.getStartNode().nodeValue);
    assertEquals(
        'Selection should start at offset 0', 0, range.getStartOffset());

    assertEquals(
        'Text node should be selected at end endpoint', 'Text',
        range.getEndNode().nodeValue);
    assertEquals(
        'Selection should end at offset 4', 'Text'.length,
        range.getEndOffset());

    assertEquals(
        'Container should be text node', NodeType.TEXT,
        range.getContainer().nodeType);

    assertEquals('Text content should be "Text"', 'Text', range.getText());
    assertFalse('Text range is not collapsed', range.isCollapsed());
    assertEquals(
        'Should contain correct html fragment', 'Text',
        range.getHtmlFragment());
    assertEquals(
        'Should contain correct valid html', 'Text', range.getValidHtml());
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testTextNodes() {
    dom.removeChildren(dynamic);
    dynamic.appendChild(dom.createTextNode('Part1'));
    dynamic.appendChild(dom.createTextNode('Part2'));
    const range = browserrange.createRangeFromNodes(
        dynamic.firstChild, 0, dynamic.lastChild, 5);

    assertEquals(
        'Text node 1 should be selected at start endpoint', 'Part1',
        range.getStartNode().nodeValue);
    assertEquals(
        'Selection should start at offset 0', 0, range.getStartOffset());

    assertEquals(
        'Text node 2 should be selected at end endpoint', 'Part2',
        range.getEndNode().nodeValue);
    assertEquals(
        'Selection should end at offset 5', 'Part2'.length,
        range.getEndOffset());

    assertEquals(
        'Container should be DIV', String(TagName.DIV),
        range.getContainer().tagName);

    assertEquals(
        'Text content should be "Part1Part2"', 'Part1Part2', range.getText());
    assertFalse('Text range is not collapsed', range.isCollapsed());
    assertEquals(
        'Should contain correct html fragment', 'Part1Part2',
        range.getHtmlFragment());
    assertEquals(
        'Should contain correct valid html', 'part1part2',
        normalizeHtml(range.getValidHtml()));
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testDiv() {
    const range = browserrange.createRangeFromNodeContents(test2);

    assertEquals(
        'Text node "abc" should be selected at start endpoint', 'abc',
        range.getStartNode().nodeValue);
    assertEquals(
        'Selection should start at offset 0', 0, range.getStartOffset());

    assertEquals(
        'Text node "def" should be selected at end endpoint', 'def',
        range.getEndNode().nodeValue);
    assertEquals(
        'Selection should end at offset 3', 'def'.length, range.getEndOffset());

    assertEquals(
        'Container should be DIV', 'DIV', range.getContainer().tagName);

    assertTrue(
        'Div text content should be "abc\\s*def"',
        /abc\s*def/.test(range.getText()));
    assertEquals(
        'Should contain correct html fragment', 'abc<br id=br>def',
        normalizeHtml(range.getHtmlFragment()));
    assertEquals(
        'Should contain correct valid html',
        '<div id=test2>abc<br id=br>def</div>',
        normalizeHtml(range.getValidHtml()));
    assertFalse('Div range is not collapsed', range.isCollapsed());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testEmptyNodeHtmlInsert() {
    const range = browserrange.createRangeFromNodeContents(empty);
    const html = '<b>hello</b>';
    range.insertNode(dom.safeHtmlToNode(testing.newSafeHtmlForTest(html)));
    assertEquals(
        'Html is not inserted correctly', html, normalizeHtml(empty.innerHTML));
    dom.removeChildren(empty);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testEmptyNode() {
    const range = browserrange.createRangeFromNodeContents(empty);

    assertEquals(
        'DIV be selected at start endpoint', 'DIV',
        range.getStartNode().tagName);
    assertEquals(
        'Selection should start at offset 0', 0, range.getStartOffset());

    assertEquals(
        'DIV should be selected at end endpoint', 'DIV',
        range.getEndNode().tagName);
    assertEquals('Selection should end at offset 0', 0, range.getEndOffset());

    assertEquals(
        'Container should be DIV', 'DIV', range.getContainer().tagName);

    assertEquals('Empty text content should be ""', '', range.getText());
    assertTrue('Empty range is collapsed', range.isCollapsed());
    assertEquals(
        'Should contain correct valid html', '<div id=empty></div>',
        normalizeHtml(range.getValidHtml()));
    assertEquals(
        'Should contain no html fragment', '', range.getHtmlFragment());
  },

  testRemoveContents() {
    const outer = dom.getElement('removeTest');
    const range = browserrange.createRangeFromNodeContents(outer.firstChild);

    range.removeContents();

    assertEquals('Removed range content should be ""', '', range.getText());
    assertTrue('Removed range is now collapsed', range.isCollapsed());
    assertEquals('Outer div has 1 child now', 1, outer.childNodes.length);
    assertEquals('Inner div is empty', 0, outer.firstChild.childNodes.length);
  },

  testRemoveContentsEmptyNode() {
    const outer = dom.getElement('removeTestEmptyNode');
    const range = browserrange.createRangeFromNodeContents(outer);

    range.removeContents();

    assertEquals('Removed range content should be ""', '', range.getText());
    assertTrue('Removed range is now collapsed', range.isCollapsed());
    assertEquals(
        'Outer div should have 0 children now', 0, outer.childNodes.length);
  },

  testRemoveContentsSingleNode() {
    const outer = dom.getElement('removeTestSingleNode');
    const range = browserrange.createRangeFromNodeContents(outer.firstChild);

    range.removeContents();

    assertEquals('Removed range content should be ""', '', range.getText());
    assertTrue('Removed range is now collapsed', range.isCollapsed());
    assertEquals('', dom.getTextContent(outer));
  },

  testRemoveContentsMidNode() {
    const outer = dom.getElement('removeTestMidNode');
    const textNode = outer.firstChild.firstChild;
    const range = browserrange.createRangeFromNodes(textNode, 1, textNode, 4);

    assertEquals(
        'Previous range content should be "123"', '123', range.getText());
    range.removeContents();

    assertEquals(
        'Removed range content should be "0456789"', '0456789',
        dom.getTextContent(outer));
  },

  testRemoveContentsMidMultipleNodes() {
    const outer = dom.getElement('removeTestMidMultipleNodes');
    const firstTextNode = outer.firstChild.firstChild;
    const lastTextNode = outer.lastChild.firstChild;
    const range =
        browserrange.createRangeFromNodes(firstTextNode, 1, lastTextNode, 4);

    assertEquals(
        'Previous range content', '1234567890123',
        range.getText().replace(/\s/g, ''));
    range.removeContents();

    assertEquals(
        'Removed range content should be "0456789"', '0456789',
        dom.getTextContent(outer).replace(/\s/g, ''));
  },

  testRemoveDivCaretRange() {
    const outer = dom.getElement('sandbox');
    outer.innerHTML = '<div>Test1</div><div></div>';
    const range = browserrange.createRangeFromNodes(
        outer.lastChild, 0, outer.lastChild, 0);

    range.removeContents();
    range.insertNode(dom.createDom(TagName.SPAN, undefined, 'Hello'), true);

    assertEquals(
        'Resulting contents', 'Test1Hello',
        dom.getTextContent(outer).replace(/\s/g, ''));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testCollapse() {
    let range = browserrange.createRangeFromNodeContents(test2);
    assertFalse('Div range is not collapsed', range.isCollapsed());
    range.collapse();
    assertTrue(
        'Div range is collapsed after call to empty()', range.isCollapsed());

    range = browserrange.createRangeFromNodeContents(empty);
    assertTrue('Empty range is collapsed', range.isCollapsed());
    range.collapse();
    assertTrue('Empty range is still collapsed', range.isCollapsed());
  },

  testIdWithSpecialCharacters() {
    dom.removeChildren(dynamic);
    dynamic.appendChild(dom.createTextNode('1'));
    dynamic.appendChild(dom.createDom(TagName.DIV, {id: '<>'}));
    dynamic.appendChild(dom.createTextNode('2'));
    const range = browserrange.createRangeFromNodes(
        dynamic.firstChild, 0, dynamic.lastChild, 1);

    // Difference in special character handling is ok.
    assertEquals(
        'Should have correct html fragment', '1<div id=<>></div>2',
        normalizeHtml(range.getHtmlFragment()));
  },

  testEndOfChildren() {
    dynamic.innerHTML =
        '<span id="a">123<br>456</span><span id="b">text</span>';
    const range = browserrange.createRangeFromNodes(
        dom.getElement('a'), 3, dom.getElement('b'), 1);
    assertEquals('Should have correct text.', 'text', range.getText());
  },

  testEndOfDiv() {
    dynamic.innerHTML = '<div id="a">abc</div><div id="b">def</div>';
    const a = dom.getElement('a');
    const range = browserrange.createRangeFromNodes(a, 1, a, 1);
    const expectedStartNode = a;
    const expectedStartOffset = 1;
    const expectedEndNode = a;
    const expectedEndOffset = 1;
    assertEquals('startNode is wrong', expectedStartNode, range.getStartNode());
    assertEquals(
        'startOffset is wrong', expectedStartOffset, range.getStartOffset());
    assertEquals('endNode is wrong', expectedEndNode, range.getEndNode());
    assertEquals('endOffset is wrong', expectedEndOffset, range.getEndOffset());
  },

  testRangeEndingWithBR() {
    dynamic.innerHTML = '<span id="a">123<br>456</span>';
    const spanElem = dom.getElement('a');
    const range = browserrange.createRangeFromNodes(spanElem, 0, spanElem, 2);
    const htmlText = range.getValidHtml().toLowerCase();
    assertContains('Should include BR in HTML.', 'br', htmlText);
    assertEquals('Should have correct text.', '123', range.getText());

    range.select();

    const selRange = Range.createFromWindow();
    const startNode = selRange.getStartNode();
    if (startNode.nodeType == NodeType.TEXT) {
      // Special case for Safari.
      assertEquals(
          'Startnode should have text:123', '123', startNode.nodeValue);
    } else {
      assertEquals('Start node should be span', spanElem, startNode);
    }
    assertEquals('Startoffset should be 0', 0, selRange.getStartOffset());
    const endNode = selRange.getEndNode();
    assertEquals('Endnode should be span', spanElem, endNode);
    assertEquals('Endoffset should be 2', 2, selRange.getEndOffset());
  },

  testRangeEndingWithBR2() {
    dynamic.innerHTML = '<span id="a">123<br></span>';
    const spanElem = dom.getElement('a');
    const range = browserrange.createRangeFromNodes(spanElem, 0, spanElem, 2);
    const htmlText = range.getValidHtml().toLowerCase();
    assertContains('Should include BR in HTML.', 'br', htmlText);
    assertEquals('Should have correct text.', '123', range.getText());

    range.select();

    const selRange = Range.createFromWindow();
    const startNode = selRange.getStartNode();
    const endNode = selRange.getEndNode();
    if (startNode.nodeType == NodeType.TEXT) {
      // Special case for Safari.
      assertEquals(
          'Start node should have text:123', '123', startNode.nodeValue);
    } else {
      assertEquals('Start node should be span', spanElem, startNode);
    }
    assertEquals('Startoffset should be 0', 0, selRange.getStartOffset());
    if (endNode.nodeType == NodeType.TEXT) {
      // Special case for Safari.
      assertEquals('Endnode should have text', '123', endNode.nodeValue);
      assertEquals('Endoffset should be 3', 3, selRange.getEndOffset());
    } else {
      assertEquals('Endnode should be span', spanElem, endNode);
      assertEquals('Endoffset should be 2', 2, selRange.getEndOffset());
    }
  },

  testRangeEndingBeforeBR() {
    dynamic.innerHTML = '<span id="a">123<br>456</span>';
    const spanElem = dom.getElement('a');
    const range = browserrange.createRangeFromNodes(spanElem, 0, spanElem, 1);
    const htmlText = range.getValidHtml().toLowerCase();
    assertNotContains('Should not include BR in HTML.', 'br', htmlText);
    assertEquals('Should have correct text.', '123', range.getText());
    range.select();

    const selRange = Range.createFromWindow();
    const startNode = selRange.getStartNode();
    if (startNode.nodeType == NodeType.TEXT) {
      // Special case for Safari.
      assertEquals(
          'Startnode should have text:123', '123', startNode.nodeValue);
    } else {
      assertEquals('Start node should be span', spanElem, startNode);
    }
    assertEquals('Startoffset should be 0', 0, selRange.getStartOffset());
    const endNode = selRange.getEndNode();
    if (endNode.nodeType == NodeType.TEXT) {
      // Special case for Safari.
      assertEquals('Endnode should have text:123', '123', endNode.nodeValue);
      assertEquals('Endoffset should be 3', 3, selRange.getEndOffset());
    } else {
      assertEquals('Endnode should be span', spanElem, endNode);
      assertEquals('Endoffset should be 1', 1, selRange.getEndOffset());
    }
  },

  testRangeStartingWithBR() {
    dynamic.innerHTML = '<span id="a">123<br>456</span>';
    const spanElem = dom.getElement('a');
    const range = browserrange.createRangeFromNodes(spanElem, 1, spanElem, 3);
    const htmlText = range.getValidHtml().toLowerCase();
    assertContains('Should include BR in HTML.', 'br', htmlText);
    // Firefox returns '456' as the range text while IE returns '\r\n456'.
    // Therefore skipping the text check.

    range.select();
    const selRange = Range.createFromWindow();
    const startNode = selRange.getStartNode();
    const endNode = selRange.getEndNode();
    if (startNode.nodeType == NodeType.TEXT) {
      // Special case for Safari.
      assertEquals('Start node should be text:123', '123', startNode.nodeValue);
      assertEquals('Startoffset should be 1', 1, selRange.getStartOffset());
    } else {
      assertEquals('Start node should be span', spanElem, startNode);
      assertEquals('Startoffset should be 1', 1, selRange.getStartOffset());
    }
    if (endNode.nodeType == NodeType.TEXT) {
      assertEquals('Endnode should have text:456', '456', endNode.nodeValue);
      assertEquals('Endoffset should be 3', 3, selRange.getEndOffset());
    } else {
      assertEquals('Endnode should be span', spanElem, endNode);
      assertEquals('Endoffset should be 3', 3, selRange.getEndOffset());
    }
  },

  testRangeStartingAfterBR() {
    dynamic.innerHTML = '<span id="a">123<br>4567</span>';
    const spanElem = dom.getElement('a');
    const range = browserrange.createRangeFromNodes(spanElem, 2, spanElem, 3);
    const htmlText = range.getValidHtml().toLowerCase();
    assertNotContains('Should not include BR in HTML.', 'br', htmlText);
    assertEquals('Should have correct text.', '4567', range.getText());

    range.select();

    const selRange = Range.createFromWindow();
    const startNode = selRange.getStartNode();
    if (startNode.nodeType == NodeType.TEXT) {
      // Special case for Safari.
      assertEquals(
          'Startnode should have text:4567', '4567', startNode.nodeValue);
      assertEquals('Startoffset should be 0', 0, selRange.getStartOffset());
    } else {
      assertEquals('Start node should be span', spanElem, startNode);
      assertEquals('Startoffset should be 2', 2, selRange.getStartOffset());
    }
    const endNode = selRange.getEndNode();
    if (startNode.nodeType == NodeType.TEXT) {
      // Special case for Safari.
      assertEquals('Endnode should have text:4567', '4567', endNode.nodeValue);
      assertEquals('Endoffset should be 4', 4, selRange.getEndOffset());
    } else {
      assertEquals('Endnode should be span', spanElem, endNode);
      assertEquals('Endoffset should be 3', 3, selRange.getEndOffset());
    }
  },

  testCollapsedRangeBeforeBR() {
    dynamic.innerHTML = '<span id="a">123<br>456</span>';
    const range = browserrange.createRangeFromNodes(
        dom.getElement('a'), 1, dom.getElement('a'), 1);
    // Firefox returns <span id="a"></span> as the range HTML while IE returns
    // empty string. Therefore skipping the HTML check.
    assertEquals('Should have no text.', '', range.getText());
  },

  testCollapsedRangeAfterBR() {
    dynamic.innerHTML = '<span id="a">123<br>456</span>';
    const range = browserrange.createRangeFromNodes(
        dom.getElement('a'), 2, dom.getElement('a'), 2);
    // Firefox returns <span id="a"></span> as the range HTML while IE returns
    // empty string. Therefore skipping the HTML check.
    assertEquals('Should have no text.', '', range.getText());
  },

  testCompareBrowserRangeEndpoints() {
    const outer = dom.getElement('outer');
    const inner = dom.getElement('inner');
    const range_outer = browserrange.createRangeFromNodeContents(outer);
    const range_inner = browserrange.createRangeFromNodeContents(inner);

    assertEquals(
        'The start of the inner selection should be after the outer.', 1,
        range_inner.compareBrowserRangeEndpoints(
            range_outer.getBrowserRange(), RangeEndpoint.START,
            RangeEndpoint.START));

    assertEquals(
        'The start of the inner selection should be before the outer\'s end.',
        -1,
        range_inner.compareBrowserRangeEndpoints(
            range_outer.getBrowserRange(), RangeEndpoint.START,
            RangeEndpoint.END));

    assertEquals(
        'The end of the inner selection should be after the outer\'s start.', 1,
        range_inner.compareBrowserRangeEndpoints(
            range_outer.getBrowserRange(), RangeEndpoint.END,
            RangeEndpoint.START));

    assertEquals(
        'The end of the inner selection should be before the outer\'s end.', -1,
        range_inner.compareBrowserRangeEndpoints(
            range_outer.getBrowserRange(), RangeEndpoint.END,
            RangeEndpoint.END));
  },

  testSelectOverwritesOldSelection() {
    browserrange.createRangeFromNodes(test1, 0, test1, 1).select();
    browserrange.createRangeFromNodes(test2, 0, test2, 1).select();
    assertEquals(
        'The old selection must be replaced with the new one', 'abc',
        Range.createFromWindow().getText());
  },

  testGetContainerInTextNodesAroundEmptySpan() {
    dynamic.innerHTML = 'abc<span></span>def';
    const abc = dynamic.firstChild;
    const def = dynamic.lastChild;

    let range;
    range = browserrange.createRangeFromNodes(abc, 1, abc, 1);
    assertEquals(
        'textNode abc should be the range container', abc,
        range.getContainer());
    assertEquals(
        'textNode abc should be the range start node', abc,
        range.getStartNode());
    assertEquals(
        'textNode abc should be the range end node', abc, range.getEndNode());

    range = browserrange.createRangeFromNodes(def, 1, def, 1);
    assertEquals(
        'textNode def should be the range container', def,
        range.getContainer());
    assertEquals(
        'textNode def should be the range start node', def,
        range.getStartNode());
    assertEquals(
        'textNode def should be the range end node', def, range.getEndNode());
  },
});
