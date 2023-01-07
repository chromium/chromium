/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.RangeTest');
goog.setTestOnly();

const DomTextRange = goog.require('goog.dom.TextRange');
const NodeType = goog.require('goog.dom.NodeType');
const Range = goog.require('goog.dom.Range');
const RangeType = goog.require('goog.dom.RangeType');
const TagName = goog.require('goog.dom.TagName');
const browserrange = goog.require('goog.dom.browserrange');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');
const testingDom = goog.require('goog.testing.dom');
const userAgent = goog.require('goog.userAgent');

const assertRangeEquals = testingDom.assertRangeEquals;

function normalizeHtml(str) {
  return str.toLowerCase()
      .replace(/[\n\r\f"]/g, '')
      .replace(/<\/li>/g, '');  // " for emacs
}

// TODO(robbyw): Test iteration over a strange document fragment.

function removeHelper(
    testNumber, range, outer, expectedChildCount, expectedContent) {
  range.removeContents();
  assertTrue(
      `${testNumber}: Removed range should now be collapsed`,
      range.isCollapsed());
  assertEquals(
      `${testNumber}: Removed range content should be ""`, '', range.getText());
  assertEquals(
      `${testNumber}: Outer div should contain correct text`, expectedContent,
      outer.innerHTML.toLowerCase());
  assertEquals(
      `${testNumber}: Outer div should have ${expectedChildCount}` +
          ' children now',
      expectedChildCount, outer.childNodes.length);
  assertNotNull(
      `${testNumber}: Empty node should still exist`, dom.getElement('empty'));
}

/**
 * Given two offsets into the 'foobar' node, make sure that inserting
 * nodes at those offsets doesn't change a selection of 'oba'.
 * @bug 1480638
 */
function assertSurroundDoesntChangeSelectionWithOffsets(
    offset1, offset2, expectedHtml) {
  const div = dom.getElement('bug1480638');
  dom.setTextContent(div, 'foobar');
  const rangeToSelect =
      Range.createFromNodes(div.firstChild, 2, div.firstChild, 5);
  rangeToSelect.select();

  const rangeToSurround =
      Range.createFromNodes(div.firstChild, offset1, div.firstChild, offset2);
  rangeToSurround.surroundWithNodes(
      dom.createDom(TagName.SPAN), dom.createDom(TagName.SPAN));

  // Make sure that the selection didn't change.
  assertHTMLEquals(
      'Selection must not change when contents are surrounded.', expectedHtml,
      Range.createFromWindow().getHtmlFragment());
}

function assertForward(string, startNode, startOffset, endNode, endOffset) {
  const root = dom.getElement('test2');
  const originalInnerHtml = root.innerHTML;

  assertFalse(
      string, Range.isReversed(startNode, startOffset, endNode, endOffset));
  assertTrue(
      string, Range.isReversed(endNode, endOffset, startNode, startOffset));
  assertEquals(
      `Contents should be unaffected after: ${string}`, root.innerHTML,
      originalInnerHtml);
}

function assertNodeEquals(expected, actual) {
  assertEquals(
      'Expected: ' + testingDom.exposeNode(expected) +
          '\nActual: ' + testingDom.exposeNode(actual),
      expected, actual);
}
testSuite({
  setUp() {
    // Reset the focus; some tests may invalidate the focus to exercise various
    // browser bugs.
    const focusableElement = dom.getElement('focusableElement');
    focusableElement.focus();
    focusableElement.blur();
  },

  testCreate() {
    assertNotNull(
        'Browser range object can be created for node',
        Range.createFromNodeContents(dom.getElement('test1')));
  },

  testTableRange() {
    const tr = dom.getElement('cell').parentNode;
    const range = Range.createFromNodeContents(tr);
    assertEquals('Selection should have correct text', '12', range.getText());
    assertEquals(
        'Selection should have correct html fragment', '1</td><td>2',
        normalizeHtml(range.getHtmlFragment()));

    // TODO(robbyw): On IE the TR is included, on FF it is not.
    // assertEquals('Selection should have correct valid html',
    //    '<tr id=row><td>1</td><td>2</td></tr>',
    //    normalizeHtml(range.getValidHtml()));

    assertEquals(
        'Selection should have correct pastable html',
        '<table><tbody><tr><td id=cell>1</td><td>2</td></tr></tbody></table>',
        normalizeHtml(range.getPastableHtml()));
  },

  testUnorderedListRange() {
    const ul = dom.getElement('ulTest').firstChild;
    const range = Range.createFromNodeContents(ul);
    assertEquals(
        'Selection should have correct html fragment', '1<li>2',
        normalizeHtml(range.getHtmlFragment()));

    // TODO(robbyw): On IE the UL is included, on FF it is not.
    // assertEquals('Selection should have correct valid html',
    //    '<li>1</li><li>2</li>', normalizeHtml(range.getValidHtml()));

    assertEquals(
        'Selection should have correct pastable html', '<ul><li>1<li>2</ul>',
        normalizeHtml(range.getPastableHtml()));
  },

  testOrderedListRange() {
    const ol = dom.getElement('olTest').firstChild;
    const range = Range.createFromNodeContents(ol);
    assertEquals(
        'Selection should have correct html fragment', '1<li>2',
        normalizeHtml(range.getHtmlFragment()));

    // TODO(robbyw): On IE the OL is included, on FF it is not.
    // assertEquals('Selection should have correct valid html',
    //    '<li>1</li><li>2</li>', normalizeHtml(range.getValidHtml()));

    assertEquals(
        'Selection should have correct pastable html', '<ol><li>1<li>2</ol>',
        normalizeHtml(range.getPastableHtml()));
  },

  testCreateFromNodes() {
    const start = dom.getElement('test1').firstChild;
    const end = dom.getElement('br');
    const range = Range.createFromNodes(start, 2, end, 0);
    assertNotNull(
        'Browser range object can be created for W3C node range', range);

    assertEquals(
        'Start node should be selected at start endpoint', start,
        range.getStartNode());
    assertEquals(
        'Selection should start at offset 2', 2, range.getStartOffset());
    assertEquals(
        'Start node should be selected at anchor endpoint', start,
        range.getAnchorNode());
    assertEquals(
        'Selection should be anchored at offset 2', 2, range.getAnchorOffset());

    const div = dom.getElement('test2');
    assertEquals(
        'DIV node should be selected at end endpoint', div, range.getEndNode());
    assertEquals('Selection should end at offset 1', 1, range.getEndOffset());
    assertEquals(
        'DIV node should be selected at focus endpoint', div,
        range.getFocusNode());
    assertEquals(
        'Selection should be focused at offset 1', 1, range.getFocusOffset());

    assertTrue(
        'Text content should be "xt\\s*abc"', /xt\s*abc/.test(range.getText()));
    assertFalse('Nodes range is not collapsed', range.isCollapsed());
  },

  testCreateControlRange() {
    if (!userAgent.IE) {
      return;
    }
    const cr = document.body.createControlRange();
    cr.addElement(dom.getElement('logo'));

    const range = Range.createFromBrowserRange(cr);
    assertNotNull(
        'Control range object can be created from browser range', range);
    assertEquals(
        'Created range is a control range', RangeType.CONTROL, range.getType());
  },

  testTextNode() {
    const range =
        Range.createFromNodeContents(dom.getElement('test1').firstChild);

    assertEquals(
        'Created range is a text range', RangeType.TEXT, range.getType());
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
  },

  testDiv() {
    const range = Range.createFromNodeContents(dom.getElement('test2'));

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
        'Container should be DIV', dom.getElement('test2'),
        range.getContainer());

    assertTrue(
        'Div text content should be "abc\\s*def"',
        /abc\s*def/.test(range.getText()));
    assertFalse('Div range is not collapsed', range.isCollapsed());
  },

  testEmptyNode() {
    const range = Range.createFromNodeContents(dom.getElement('empty'));

    assertEquals(
        'DIV be selected at start endpoint', dom.getElement('empty'),
        range.getStartNode());
    assertEquals(
        'Selection should start at offset 0', 0, range.getStartOffset());

    assertEquals(
        'DIV should be selected at end endpoint', dom.getElement('empty'),
        range.getEndNode());
    assertEquals('Selection should end at offset 0', 0, range.getEndOffset());

    assertEquals(
        'Container should be DIV', dom.getElement('empty'),
        range.getContainer());

    assertEquals('Empty text content should be ""', '', range.getText());
    assertTrue('Empty range is collapsed', range.isCollapsed());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testCollapse() {
    let range = Range.createFromNodeContents(dom.getElement('test2'));
    assertFalse('Div range is not collapsed', range.isCollapsed());
    range.collapse();
    assertTrue(
        'Div range is collapsed after call to empty()', range.isCollapsed());

    range = Range.createFromNodeContents(dom.getElement('empty'));
    assertTrue('Empty range is collapsed', range.isCollapsed());
    range.collapse();
    assertTrue('Empty range is still collapsed', range.isCollapsed());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testIterator() {
    testingDom.assertNodesMatch(
        Range.createFromNodeContents(dom.getElement('test2')),
        ['abc', '#br', '#br', 'def']);
  },

  testReversedNodes() {
    let node = dom.getElement('test1').firstChild;
    let range = Range.createFromNodes(node, 4, node, 0);
    assertTrue('Range is reversed', range.isReversed());
    node = dom.getElement('test3');
    range = Range.createFromNodes(node, 0, node, 1);
    assertFalse('Range is not reversed', range.isReversed());
  },

  testReversedContents() {
    const range = Range.createFromNodeContents(dom.getElement('test1'), true);
    assertTrue('Range is reversed', range.isReversed());
    assertEquals('Range should select "Text"', 'Text', range.getText());
    assertEquals('Range start offset should be 0', 0, range.getStartOffset());
    assertEquals('Range end offset should be 4', 4, range.getEndOffset());
    assertEquals('Range anchor offset should be 4', 4, range.getAnchorOffset());
    assertEquals('Range focus offset should be 0', 0, range.getFocusOffset());

    const range2 = range.clone();

    range.collapse(true);
    assertTrue('Range is collapsed', range.isCollapsed());
    assertFalse('Collapsed range is not reversed', range.isReversed());
    assertEquals(
        'Post collapse start offset should be 4', 4, range.getStartOffset());

    range2.collapse(false);
    assertTrue('Range 2 is collapsed', range2.isCollapsed());
    assertFalse('Collapsed range 2 is not reversed', range2.isReversed());
    assertEquals(
        'Post collapse start offset 2 should be 0', 0, range2.getStartOffset());
  },

  testRemoveContents() {
    const outer = dom.getElement('removeTest');
    const range = Range.createFromNodeContents(outer.firstChild);

    range.removeContents();

    assertEquals('Removed range content should be ""', '', range.getText());
    assertTrue('Removed range should be collapsed', range.isCollapsed());
    assertEquals(
        'Outer div should have 1 child now', 1, outer.childNodes.length);
    assertEquals(
        'Inner div should be empty', 0, outer.firstChild.childNodes.length);
  },

  testRemovePartialContents() {
    const outer = dom.getElement('removePartialTest');
    const originalText = dom.getTextContent(outer);

    try {
      let range =
          Range.createFromNodes(outer.firstChild, 2, outer.firstChild, 4);
      removeHelper(1, range, outer, 1, '0145');

      range = Range.createFromNodes(outer.firstChild, 0, outer.firstChild, 1);
      removeHelper(2, range, outer, 1, '145');

      range = Range.createFromNodes(outer.firstChild, 2, outer.firstChild, 3);
      removeHelper(3, range, outer, 1, '14');

      const br = dom.createDom(TagName.BR);
      outer.appendChild(br);
      range = Range.createFromNodes(outer.firstChild, 1, outer, 1);
      removeHelper(4, range, outer, 2, '1<br>');

      outer.innerHTML = '<br>123';
      range = Range.createFromNodes(outer, 0, outer.lastChild, 2);
      removeHelper(5, range, outer, 1, '3');

      outer.innerHTML = '123<br>456';
      range = Range.createFromNodes(outer.firstChild, 1, outer.lastChild, 2);
      removeHelper(6, range, outer, 2, '16');

      outer.innerHTML = '123<br>456';
      range = Range.createFromNodes(outer.firstChild, 0, outer.lastChild, 2);
      removeHelper(7, range, outer, 1, '6');

      outer.innerHTML = '<div></div>';
      range = Range.createFromNodeContents(outer.firstChild);
      removeHelper(8, range, outer, 1, '<div></div>');
    } finally {
      // Restore the original text state for repeated runs.
      dom.setTextContent(outer, originalText);
    }

    // TODO(robbyw): Fix the following edge cases:
    //    * Selecting contents of a node containing multiply empty divs
    //    * Selecting via createFromNodes(x, 0, x, x.childNodes.length)
    //    * Consistent handling of nodeContents(<div><div></div></div>).remove
  },

  testSurroundContents() {
    const outer = dom.getElement('surroundTest');
    outer.innerHTML = '---Text that<br>will be surrounded---';
    const range = Range.createFromNodes(
        outer.firstChild, 3, outer.lastChild,
        outer.lastChild.nodeValue.length - 3);

    const div = dom.createDom(TagName.DIV, {'style': 'color: red'});
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    const output = range.surroundContents(div);

    assertEquals(
        'Outer element should contain new element', outer, output.parentNode);
    assertFalse('New element should have no id', !!output.id);
    assertEquals('New element should be red', 'red', output.style.color);
    assertEquals(
        'Outer element should have three children', 3, outer.childNodes.length);
    assertEquals(
        'New element should have three children', 3, output.childNodes.length);

    // TODO(robbyw): Ensure the range stays in a reasonable state.
  },

  testSurroundWithNodesDoesntChangeSelection1() {
    assertSurroundDoesntChangeSelectionWithOffsets(
        3, 4, 'o<span></span>b<span></span>a');
  },

  testSurroundWithNodesDoesntChangeSelection2() {
    assertSurroundDoesntChangeSelectionWithOffsets(3, 6, 'o<span></span>ba');
  },

  testSurroundWithNodesDoesntChangeSelection3() {
    assertSurroundDoesntChangeSelectionWithOffsets(1, 3, 'o<span></span>ba');
  },

  testSurroundWithNodesDoesntChangeSelection4() {
    assertSurroundDoesntChangeSelectionWithOffsets(1, 6, 'oba');
  },

  testInsertNode() {
    const outer = dom.getElement('insertTest');
    dom.setTextContent(outer, 'ACD');

    let range = Range.createFromNodes(outer.firstChild, 1, outer.firstChild, 2);
    range.insertNode(dom.createTextNode('B'), true);
    assertEquals(
        'Element should have correct innerHTML', 'ABCD', outer.innerHTML);

    dom.setTextContent(outer, '12');
    range = Range.createFromNodes(outer.firstChild, 0, outer.firstChild, 1);
    const br = range.insertNode(dom.createDom(TagName.BR), false);
    assertEquals(
        'New element should have correct innerHTML', '1<br>2',
        outer.innerHTML.toLowerCase());
    assertEquals('BR should be in outer', outer, br.parentNode);
  },

  testReplaceContentsWithNode() {
    const outer = dom.getElement('insertTest');
    dom.setTextContent(outer, 'AXC');

    let range = Range.createFromNodes(outer.firstChild, 1, outer.firstChild, 2);
    range.replaceContentsWithNode(dom.createTextNode('B'));
    assertEquals(
        'Element should have correct innerHTML', 'ABC', outer.innerHTML);

    dom.setTextContent(outer, 'ABC');
    range = Range.createFromNodes(outer.firstChild, 3, outer.firstChild, 3);
    range.replaceContentsWithNode(dom.createTextNode('D'));
    assertEquals(
        'Element should have correct innerHTML after collapsed replace', 'ABCD',
        outer.innerHTML);

    outer.innerHTML = 'AX<b>X</b>XC';
    range = Range.createFromNodes(outer.firstChild, 1, outer.lastChild, 1);
    range.replaceContentsWithNode(dom.createTextNode('B'));
    testingDom.assertHtmlContentsMatch('ABC', outer);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSurroundWithNodes() {
    const outer = dom.getElement('insertTest');
    dom.setTextContent(outer, 'ACE');
    const range =
        Range.createFromNodes(outer.firstChild, 1, outer.firstChild, 2);

    range.surroundWithNodes(dom.createTextNode('B'), dom.createTextNode('D'));

    assertEquals(
        'New element should have correct innerHTML', 'ABCDE', outer.innerHTML);
  },

  testIsRangeInDocument() {
    const outer = dom.getElement('insertTest');
    outer.innerHTML = '<br>ABC';
    const range = Range.createCaret(outer.lastChild, 1);

    assertEquals(
        'Should get correct start element', 'ABC',
        range.getStartNode().nodeValue);
    assertTrue('Should be considered in document', range.isRangeInDocument());

    dom.setTextContent(outer, 'DEF');

    assertFalse(
        'Should be marked as out of document', range.isRangeInDocument());
  },

  testRemovedNode() {
    const node = dom.getElement('removeNodeTest');
    const range = browserrange.createRangeFromNodeContents(node);
    range.select();
    dom.removeNode(node);

    const newRange = Range.createFromWindow(window);

    assertTrue(
        'The other browsers will just have an empty range.',
        newRange.isCollapsed());
  },

  testReversedRange() {
    if (userAgent.EDGE_OR_IE) return;  // IE doesn't make this distinction.

    Range
        .createFromNodes(dom.getElement('test2'), 0, dom.getElement('test1'), 0)
        .select();

    const range = Range.createFromWindow(window);
    assertTrue('Range should be reversed', range.isReversed());
  },

  testUnreversedRange() {
    Range
        .createFromNodes(dom.getElement('test1'), 0, dom.getElement('test2'), 0)
        .select();

    const range = Range.createFromWindow(window);
    assertFalse('Range should not be reversed', range.isReversed());
  },

  testReversedThenUnreversedRange() {
    // This tests a workaround for a webkit bug where webkit caches selections
    // incorrectly.
    Range
        .createFromNodes(dom.getElement('test2'), 0, dom.getElement('test1'), 0)
        .select();
    Range
        .createFromNodes(dom.getElement('test1'), 0, dom.getElement('test2'), 0)
        .select();

    const range = Range.createFromWindow(window);
    assertFalse('Range should not be reversed', range.isReversed());
  },

  testHasAndClearSelection() {
    Range.createFromNodeContents(dom.getElement('test1')).select();

    assertTrue('Selection should exist', Range.hasSelection());

    Range.clearSelection();

    assertFalse('Selection should not exist', Range.hasSelection());
  },

  testIsReversed() {
    const root = dom.getElement('test2');
    const text1 = root.firstChild;  // Text content: 'abc'.
    const br = root.childNodes[1];
    const text2 = root.lastChild;  // Text content: 'def'.

    assertFalse(
        'Same element position gives false',
        Range.isReversed(root, 0, root, 0));
    assertFalse(
        'Same text position gives false', Range.isReversed(text1, 0, text2, 0));
    assertForward(
        'Element offsets should compare against each other', root, 0, root, 2);
    assertForward(
        'Text node offsets should compare against each other', text1, 0, text2,
        2);
    assertForward('Text nodes should compare correctly', text1, 0, text2, 0);
    assertForward(
        'Text nodes should compare to later elements', text1, 0, br, 0);
    assertForward(
        'Text nodes should compare to earlier elements', br, 0, text2, 0);
    assertForward('Parent is before element child', root, 0, br, 0);
    assertForward('Parent is before text child', root, 0, text1, 0);
    assertFalse(
        'Equivalent position gives false', Range.isReversed(root, 0, text1, 0));
    assertFalse(
        'Equivalent position gives false', Range.isReversed(root, 1, br, 0));
    assertForward('End of element is after children', text1, 0, root, 3);
    assertForward('End of element is after children', br, 0, root, 3);
    assertForward('End of element is after children', text2, 0, root, 3);
    assertForward(
        'End of element is after end of last child', text2, 3, root, 3);
  },

  testSelectAroundSpaces() {
    // set the selection
    const textNode = dom.getElement('textWithSpaces').firstChild;
    DomTextRange.createFromNodes(textNode, 5, textNode, 12).select();

    // get the selection and check that it matches what we set it to
    const range = Range.createFromWindow();
    assertEquals(' world ', range.getText());
    assertEquals(5, range.getStartOffset());
    assertEquals(12, range.getEndOffset());
    assertEquals(textNode, range.getContainer());

    // Check the contents again, because there used to be a bug where
    // it changed after calling getContainer().
    assertEquals(' world ', range.getText());
  },

  testSelectInsideSpaces() {
    // set the selection
    const textNode = dom.getElement('textWithSpaces').firstChild;
    DomTextRange.createFromNodes(textNode, 6, textNode, 11).select();

    // get the selection and check that it matches what we set it to
    const range = Range.createFromWindow();
    assertEquals('world', range.getText());
    assertEquals(6, range.getStartOffset());
    assertEquals(11, range.getEndOffset());
    assertEquals(textNode, range.getContainer());

    // Check the contents again, because there used to be a bug where
    // it changed after calling getContainer().
    assertEquals('world', range.getText());
  },

  testRangeBeforeBreak() {
    const container = dom.getElement('rangeAroundBreaks');
    const text = container.firstChild;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    const offset = text.length;
    assertEquals(4, offset);

    const br = container.childNodes[1];
    const caret = Range.createCaret(text, offset);
    caret.select();
    assertEquals(offset, caret.getStartOffset());

    const range = Range.createFromWindow();
    assertFalse('Should not contain whole <br>', range.containsNode(br, false));
    if (userAgent.IE && !userAgent.isDocumentModeOrHigher(9)) {
      assertTrue(
          'Range over <br> is adjacent to the immediate range before it',
          range.containsNode(br, true));
    } else {
      assertFalse(
          'Should not contain partial <br>', range.containsNode(br, true));
    }

    assertEquals(offset, range.getStartOffset());
    assertEquals(text, range.getStartNode());
  },

  testRangeAfterBreak() {
    const container = dom.getElement('rangeAroundBreaks');
    const br = container.childNodes[1];
    const caret = Range.createCaret(container.lastChild, 0);
    caret.select();
    assertEquals(0, caret.getStartOffset());

    const range = Range.createFromWindow();
    assertFalse('Should not contain whole <br>', range.containsNode(br, false));
    const isSafari3 = false;

    if (userAgent.IE && !userAgent.isDocumentModeOrHigher(9) || isSafari3) {
      assertTrue(
          'Range over <br> is adjacent to the immediate range after it',
          range.containsNode(br, true));
    } else {
      assertFalse(
          'Should not contain partial <br>', range.containsNode(br, true));
    }

    if (isSafari3) {
      assertEquals(2, range.getStartOffset());
      assertEquals(container, range.getStartNode());
    } else {
      assertEquals(0, range.getStartOffset());
      assertEquals(container.lastChild, range.getStartNode());
    }
  },

  testRangeAtBreakAtStart() {
    const container = dom.getElement('breaksAroundNode');
    const br = container.firstChild;
    const caret = Range.createCaret(container.firstChild, 0);
    caret.select();
    assertEquals(0, caret.getStartOffset());

    const range = Range.createFromWindow();
    assertTrue(
        'Range over <br> is adjacent to the immediate range before it',
        range.containsNode(br, true));
    assertFalse('Should not contain whole <br>', range.containsNode(br, false));

    assertRangeEquals(container, 0, container, 0, range);
  },

  testFocusedElementDisappears() {
    // This reproduces a failure case specific to Gecko, where an element is
    // created, contentEditable is set, is focused, and removed.  After that
    // happens, calling selection.collapse fails.
    // https://bugzilla.mozilla.org/show_bug.cgi?id=773137
    const disappearingElement = dom.createDom(TagName.DIV);
    document.body.appendChild(disappearingElement);
    disappearingElement.contentEditable = true;
    disappearingElement.focus();
    document.body.removeChild(disappearingElement);
    const container = dom.getElement('empty');
    const caret = Range.createCaret(container, 0);
    // This should not throw.
    caret.select();
    assertEquals(0, caret.getStartOffset());
  },
});
