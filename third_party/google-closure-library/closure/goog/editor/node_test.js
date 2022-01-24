/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.nodeTest');
goog.setTestOnly();

const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const NodeType = goog.require('goog.dom.NodeType');
const TagName = goog.require('goog.dom.TagName');
const editorNode = goog.require('goog.editor.node');
const googArray = goog.require('goog.array');
const googDom = goog.require('goog.dom');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const testingDom = goog.require('goog.testing.dom');
const userAgent = goog.require('goog.userAgent');

let expectedFailures;
let parentNode;
let childNode1;
let childNode2;
let childNode3;

let gChildWsNode1 = null;
let gChildTextNode1 = null;
let gChildNbspNode1 = null;
let gChildMixedNode1 = null;
let gChildWsNode2a = null;
let gChildWsNode2b = null;
let gChildTextNode3a = null;
let gChildWsNode3 = null;
let gChildTextNode3b = null;

function setUpDomTree() {
  gChildWsNode1 = document.createTextNode(' \t\r\n');
  gChildTextNode1 = document.createTextNode('Child node');
  gChildNbspNode1 = document.createTextNode('\u00a0');
  gChildMixedNode1 = document.createTextNode('Text\n plus\u00a0');
  gChildWsNode2a = document.createTextNode('');
  gChildWsNode2b = document.createTextNode(' ');
  gChildTextNode3a = document.createTextNode('I am a grand child');
  gChildWsNode3 = document.createTextNode('   \t  \r   \n');
  gChildTextNode3b = document.createTextNode('I am also a grand child');

  childNode3.appendChild(gChildTextNode3a);
  childNode3.appendChild(gChildWsNode3);
  childNode3.appendChild(gChildTextNode3b);

  childNode1.appendChild(gChildMixedNode1);
  childNode1.appendChild(gChildWsNode1);
  childNode1.appendChild(gChildNbspNode1);
  childNode1.appendChild(gChildTextNode1);

  childNode2.appendChild(gChildWsNode2a);
  childNode2.appendChild(gChildWsNode2b);
  document.body.appendChild(parentNode);
}

function tearDownDomTree() {
  googDom.removeChildren(childNode1);
  googDom.removeChildren(childNode2);
  googDom.removeChildren(childNode3);
  gChildWsNode1 = null;
  gChildTextNode1 = null;
  gChildNbspNode1 = null;
  gChildMixedNode1 = null;
  gChildWsNode2a = null;
  gChildWsNode2b = null;
  gChildTextNode3a = null;
  gChildWsNode3 = null;
  gChildTextNode3b = null;
}

function createDivWithTextNodes(var_args) {
  const dom = googDom.createDom(TagName.DIV);
  for (let i = 0; i < arguments.length; i++) {
    googDom.appendChild(dom, googDom.createTextNode(arguments[i]));
  }
  return dom;
}

testSuite({
  setUpPage() {
    expectedFailures = new ExpectedFailures();
    parentNode = document.getElementById('parentNode');
    childNode1 = parentNode.childNodes[0];
    childNode2 = parentNode.childNodes[1];
    childNode3 = parentNode.childNodes[2];
  },

  tearDown() {
    expectedFailures.handleTearDown();
  },

  testGetCompatModeQuirks() {
    const quirksIfr = googDom.createElement(TagName.IFRAME);
    document.body.appendChild(quirksIfr);
    // Webkit used to default to standards mode, but fixed this in
    // Safari 4/Chrome 2, aka, WebKit 530.
    // Also IE10 fails here.
    // TODO(johnlenz):  IE10+ inherit quirks mode from the owner document
    // according to:
    // http://msdn.microsoft.com/en-us/library/ff955402(v=vs.85).aspx
    // but this test shows different behavior for IE10 and 11. If we discover
    // that we care about quirks mode documents we should investigate
    // this failure.
    expectedFailures.expectFailureFor(
        (userAgent.IE && userAgent.isVersionOrHigher('10') &&
         !userAgent.isVersionOrHigher('11')));
    expectedFailures.run(() => {
      assertFalse(
          'Empty sourceless iframe is quirks mode, not standards mode',
          editorNode.isStandardsMode(
              googDom.getFrameContentDocument(quirksIfr)));
    });
    document.body.removeChild(quirksIfr);
  },

  testGetCompatModeStandards() {
    const standardsIfr = googDom.createElement(TagName.IFRAME);
    document.body.appendChild(standardsIfr);
    const doc = googDom.getFrameContentDocument(standardsIfr);
    doc.open();
    doc.write('<!DOCTYPE HTML><html><head></head><body>&nbsp;</body></html>');
    doc.close();
    assertTrue(
        'Iframe with DOCTYPE written in is standards mode',
        editorNode.isStandardsMode(doc));
    document.body.removeChild(standardsIfr);
  },

  /** Creates a DOM tree and tests that getLeftMostLeaf returns proper node */
  testGetLeftMostLeaf() {
    setUpDomTree();

    assertEquals(
        'Should skip ws node', gChildMixedNode1,
        editorNode.getLeftMostLeaf(parentNode));
    assertEquals(
        'Should skip ws node', gChildMixedNode1,
        editorNode.getLeftMostLeaf(childNode1));
    assertEquals(
        'Has no non ws leaves', childNode2,
        editorNode.getLeftMostLeaf(childNode2));
    assertEquals(
        'Should return first child', gChildTextNode3a,
        editorNode.getLeftMostLeaf(childNode3));
    assertEquals(
        'Has no children', gChildTextNode1,
        editorNode.getLeftMostLeaf(gChildTextNode1));

    tearDownDomTree();
  },

  /** Creates a DOM tree and tests that getRightMostLeaf returns proper node */
  testGetRightMostLeaf() {
    setUpDomTree();

    assertEquals(
        'Should return child3\'s rightmost child', gChildTextNode3b,
        editorNode.getRightMostLeaf(parentNode));
    assertEquals(
        'Should skip ws node', gChildTextNode1,
        editorNode.getRightMostLeaf(childNode1));
    assertEquals(
        'Has no non ws leaves', childNode2,
        editorNode.getRightMostLeaf(childNode2));
    assertEquals(
        'Should return last child', gChildTextNode3b,
        editorNode.getRightMostLeaf(childNode3));
    assertEquals(
        'Has no children', gChildTextNode1,
        editorNode.getRightMostLeaf(gChildTextNode1));

    tearDownDomTree();
  },

  /**
   * Creates a DOM tree and tests that getFirstChild properly ignores
   * ignorable nodes
   */
  testGetFirstChild() {
    setUpDomTree();

    assertNull('Has no none ws children', editorNode.getFirstChild(childNode2));
    assertEquals(
        'Should skip first child, as it is ws', gChildMixedNode1,
        editorNode.getFirstChild(childNode1));
    assertEquals(
        'Should just return first child', gChildTextNode3a,
        editorNode.getFirstChild(childNode3));
    assertEquals(
        'Should return first child', childNode1,
        editorNode.getFirstChild(parentNode));

    assertNull(
        'First child of a text node should return null',
        editorNode.getFirstChild(gChildTextNode1));
    assertNull(
        'First child of null should return null',
        editorNode.getFirstChild(null));

    tearDownDomTree();
  },

  /**
   * Create a DOM tree and test that getLastChild properly ignores
   * ignorable nodes
   */
  testGetLastChild() {
    setUpDomTree();

    assertNull('Has no none ws children', editorNode.getLastChild(childNode2));
    assertEquals(
        'Should skip last child, as it is ws', gChildTextNode1,
        editorNode.getLastChild(childNode1));
    assertEquals(
        'Should just return last child', gChildTextNode3b,
        editorNode.getLastChild(childNode3));
    assertEquals(
        'Should return last child', childNode3,
        editorNode.getLastChild(parentNode));

    assertNull(
        'Last child of a text node should return null',
        editorNode.getLastChild(gChildTextNode1));
    assertNull(
        'Last child of null should return null',
        editorNode.getLastChild(gChildTextNode1));

    tearDownDomTree();
  },

  /**
   * Test if nodes that should be ignorable return false and nodes that should
   * not be ignored return true.
   */
  testIsImportant() {
    const wsNode = document.createTextNode(' \t\r\n');
    assertFalse(
        'White space node is ignorable', editorNode.isImportant(wsNode));
    const textNode = document.createTextNode('Hello');
    assertTrue('Text node is important', editorNode.isImportant(textNode));
    const nbspNode = document.createTextNode('\u00a0');
    assertTrue('Node with nbsp is important', editorNode.isImportant(nbspNode));
    const imageNode = googDom.createElement(TagName.IMG);
    assertTrue('Image node is important', editorNode.isImportant(imageNode));
  },

  /**
   * Test that isAllNonNbspWhiteSpace returns true if node contains only
   * whitespace that is not nbsp and false otherwise
   */
  testIsAllNonNbspWhiteSpace() {
    const wsNode = document.createTextNode(' \t\r\n');
    assertTrue(
        'String is all non nbsp', editorNode.isAllNonNbspWhiteSpace(wsNode));
    const textNode = document.createTextNode('Hello');
    assertFalse(
        'String should not be whitespace',
        editorNode.isAllNonNbspWhiteSpace(textNode));
    const nbspNode = document.createTextNode('\u00a0');
    assertFalse('String has nbsp', editorNode.isAllNonNbspWhiteSpace(nbspNode));
  },

  /**
   * Creates a DOM tree and Test that getPreviousSibling properly ignores
   * ignorable nodes
   */
  testGetPreviousSibling() {
    setUpDomTree();

    assertNull(
        'No previous sibling', editorNode.getPreviousSibling(gChildTextNode3a));
    assertEquals(
        'Should have text sibling', gChildTextNode3a,
        editorNode.getPreviousSibling(gChildWsNode3));
    assertEquals(
        'Should skip over white space sibling', gChildTextNode3a,
        editorNode.getPreviousSibling(gChildTextNode3b));
    assertNull(
        'No previous sibling', editorNode.getPreviousSibling(gChildMixedNode1));
    assertEquals(
        'Should have mixed text sibling', gChildMixedNode1,
        editorNode.getPreviousSibling(gChildWsNode1));
    assertEquals(
        'Should skip over white space sibling', gChildMixedNode1,
        editorNode.getPreviousSibling(gChildNbspNode1));
    assertNotEquals(
        'Should not move past ws and nbsp', gChildMixedNode1,
        editorNode.getPreviousSibling(gChildTextNode1));
    assertEquals(
        'Should go to child 2', childNode2,
        editorNode.getPreviousSibling(childNode3));
    assertEquals(
        'Should go to child 1', childNode1,
        editorNode.getPreviousSibling(childNode2));
    assertNull(
        'Only has white space siblings',
        editorNode.getPreviousSibling(gChildWsNode2b));

    tearDownDomTree();
  },

  /**
   * Creates a DOM tree and tests that getNextSibling properly ignores
   * igrnorable nodes when determining the next sibling
   */
  testGetNextSibling() {
    setUpDomTree();

    assertEquals(
        'Child 1 should have Child 2', childNode2,
        editorNode.getNextSibling(childNode1));
    assertEquals(
        'Child 2 should have child 3', childNode3,
        editorNode.getNextSibling(childNode2));
    assertNull(
        'Child 3 has no next sibling', editorNode.getNextSibling(childNode3));
    assertNotEquals(
        'Should not skip ws and nbsp nodes', gChildTextNode1,
        editorNode.getNextSibling(gChildMixedNode1));
    assertNotEquals(
        'Should not skip nbsp node', gChildTextNode1,
        editorNode.getNextSibling(gChildWsNode1));
    assertEquals(
        'Should have sibling', gChildTextNode1,
        editorNode.getNextSibling(gChildNbspNode1));
    assertNull(
        'Should have no next sibling',
        editorNode.getNextSibling(gChildTextNode1));
    assertNull(
        'Only has ws sibling', editorNode.getNextSibling(gChildWsNode2a));
    assertNull(
        'Has no next sibling', editorNode.getNextSibling(gChildWsNode2b));
    assertEquals(
        'Should skip ws node', gChildTextNode3b,
        editorNode.getNextSibling(gChildTextNode3a));

    tearDownDomTree();
  },

  testIsEmpty() {
    const textNode = document.createTextNode('');
    assertTrue(
        'Text node with no content should be empty',
        editorNode.isEmpty(textNode));
    textNode.data = '\xa0';
    assertTrue(
        'Text node with nbsp should be empty', editorNode.isEmpty(textNode));
    assertFalse(
        'Text node with nbsp should not be empty when prohibited',
        editorNode.isEmpty(textNode, true));

    textNode.data = '     ';
    assertTrue(
        'Text node with whitespace should be empty',
        editorNode.isEmpty(textNode));
    textNode.data = 'notEmpty';
    assertFalse(
        'Text node with text should not be empty',
        editorNode.isEmpty(textNode));

    const div = googDom.createElement(TagName.DIV);
    assertTrue('Empty div should be empty', editorNode.isEmpty(div));
    div.innerHTML = '<iframe></iframe>';
    assertFalse(
        'Div containing an iframe is not empty', editorNode.isEmpty(div));
    div.innerHTML = '<img></img>';
    assertFalse(
        'Div containing an image is not empty', editorNode.isEmpty(div));
    div.innerHTML = '<embed></embed>';
    assertFalse(
        'Div containing an embed is not empty', editorNode.isEmpty(div));
    div.innerHTML = '<div><span></span></div>';
    assertTrue(
        'Div containing other empty tags is empty', editorNode.isEmpty(div));
    div.innerHTML = '<div><span>  </span></div>';
    assertTrue(
        'Div containing other empty tags and whitespace is empty',
        editorNode.isEmpty(div));
    div.innerHTML = '<div><span>Not empty</span></div>';
    assertFalse(
        'Div containing tags and text is not empty', editorNode.isEmpty(div));

    const img = googDom.createElement(TagName.IMG);
    assertFalse('Empty img should not be empty', editorNode.isEmpty(img));

    const iframe = googDom.createElement(TagName.IFRAME);
    assertFalse('Empty iframe should not be empty', editorNode.isEmpty(iframe));

    const embed = googDom.createElement(TagName.EMBED);
    assertFalse('Empty embed should not be empty', editorNode.isEmpty(embed));
  },

  /**
   * Test that getLength returns 0 if the node has no length and no children,
   * the # of children if the node has no length but does have children,
   * and the length of the node if the node does have length
   */
  testGetLength() {
    const parentNode = googDom.createElement(TagName.P);

    assertEquals(
        'Length 0 and no children', 0, editorNode.getLength(parentNode));

    const childNode1 = document.createTextNode('node 1');
    const childNode2 = document.createTextNode('node number 2');
    const childNode3 = document.createTextNode('');
    parentNode.appendChild(childNode1);
    parentNode.appendChild(childNode2);
    parentNode.appendChild(childNode3);
    assertEquals(
        'Length 0 and 3 children', 3, editorNode.getLength(parentNode));
    assertEquals('Text node, length 6', 6, editorNode.getLength(childNode1));
    assertEquals('Text node, length 0', 0, editorNode.getLength(childNode3));
  },

  testFindInChildrenSuccess() {
    const parentNode = googDom.createElement(TagName.DIV);
    parentNode.innerHTML = '<div>foo</div><b>foo2</b>';

    const index = editorNode.findInChildren(
        parentNode, /**
                       @suppress {strictMissingProperties} suppression added to
                       enable type checking
                     */
        (node) => node.tagName == TagName.B);
    assertEquals('Should find second child', index, 1);
  },

  testFindInChildrenFailure() {
    const parentNode = googDom.createElement(TagName.DIV);
    parentNode.innerHTML = '<div>foo</div><b>foo2</b>';

    const index = editorNode.findInChildren(parentNode, (node) => false);
    assertNull('Shouldn\'t find a child', index);
  },

  testFindHighestMatchingAncestor() {
    setUpDomTree();
    let predicateFunc = (node) => node.tagName == TagName.DIV;
    let node =
        editorNode.findHighestMatchingAncestor(gChildTextNode3a, predicateFunc);
    assertNotNull('Should return an ancestor', node);
    assertEquals(
        'Should have found "parentNode" as the last ' +
            'ancestor matching the predicate',
        parentNode, node);

    predicateFunc = (node) => node.childNodes.length == 1;
    node =
        editorNode.findHighestMatchingAncestor(gChildTextNode3a, predicateFunc);
    assertNull('Shouldn\'t return an ancestor', node);

    tearDownDomTree();
  },

  testIsBlock() {
    const blockDisplays = [
      'block',
      'list-item',
      'table',
      'table-caption',
      'table-cell',
      'table-column',
      'table-column-group',
      'table-footer',
      'table-footer-group',
      'table-header-group',
      'table-row',
      'table-row-group',
    ];

    const structuralTags = [
      TagName.BODY,
      TagName.FRAME,
      TagName.FRAMESET,
      TagName.HEAD,
      TagName.HTML,
    ];

    // The following tags are considered inline in IE, except LEGEND which is
    // only a block element in WEBKIT.
    const ambiguousTags = [
      TagName.DETAILS,
      TagName.HR,
      TagName.ISINDEX,
      TagName.LEGEND,
      TagName.MAIN,
      TagName.MAP,
      TagName.NOFRAMES,
      TagName.OPTGROUP,
      TagName.OPTION,
      TagName.SUMMARY,
    ];

    // Older versions of IE and Gecko consider the following elements to be
    // inline, but IE9+ and Gecko 2.0+ recognize the new elements.
    const legacyAmbiguousTags = [
      TagName.ARTICLE,
      TagName.ASIDE,
      TagName.FIGCAPTION,
      TagName.FIGURE,
      TagName.FOOTER,
      TagName.HEADER,
      TagName.HGROUP,
      TagName.NAV,
      TagName.SECTION,
    ];

    const tagsToIgnore = googArray.flatten(structuralTags, ambiguousTags);

    if (userAgent.IE && !userAgent.isDocumentModeOrHigher(9)) {
      googArray.extend(tagsToIgnore, legacyAmbiguousTags);
    }

    // Appending an applet tag can cause the test to hang if Java is blocked on
    // the system.
    tagsToIgnore.push(TagName.APPLET);

    // Appending an embed tag to the page in IE brings up a warning dialog about
    // loading Java content.
    if (userAgent.IE) {
      tagsToIgnore.push(TagName.EMBED);
    }

    const failures = [];
    for (let tag in TagName) {
      if (googArray.contains(tagsToIgnore, TagName[tag])) {
        continue;
      }

      const el = googDom.createElement(tag);
      document.body.appendChild(el);
      const display = style.getCascadedStyle(el, 'display') ||
          style.getComputedStyle(el, 'display');
      googDom.removeNode(el);

      if (editorNode.isBlockTag(el)) {
        if (!googArray.contains(blockDisplays, display)) {
          failures.push(`Display for ${tag} should be block-like`);
        }
      } else {
        if (googArray.contains(blockDisplays, display)) {
          failures.push(`Display for ${tag} should not be block-like`);
        }
      }
    }
    if (failures.length) {
      fail(failures.join('\n'));
    }
  },

  testSkipEmptyTextNodes() {
    assertNull(
        'skipEmptyTextNodes should gracefully handle null',
        editorNode.skipEmptyTextNodes(null));

    const dom1 = createDivWithTextNodes('abc', '', 'xyz', '', '');
    assertEquals(
        'expected not to skip first child', dom1.firstChild,
        editorNode.skipEmptyTextNodes(dom1.firstChild));
    assertEquals(
        'expected to skip second child', dom1.childNodes[2],
        editorNode.skipEmptyTextNodes(dom1.childNodes[1]));
    assertNull(
        'expected to skip all the rest of the children',
        editorNode.skipEmptyTextNodes(dom1.childNodes[3]));
  },

  testIsEditableContainer() {
    const editableContainerElement = document.getElementById('editableTest');
    assertTrue(
        'Container element should be considered editable container',
        editorNode.isEditableContainer(editableContainerElement));

    const nonEditableContainerElement = document.getElementById('parentNode');
    assertFalse(
        'Other element should not be considered editable container',
        editorNode.isEditableContainer(nonEditableContainerElement));
  },

  testIsEditable() {
    const editableContainerElement = document.getElementById('editableTest');
    const childNode = editableContainerElement.firstChild;
    const childElement =
        googDom.getElementsByTagName(TagName.SPAN, editableContainerElement)[0];

    assertFalse(
        'Container element should not be considered editable',
        editorNode.isEditable(editableContainerElement));
    assertTrue(
        'Child text node should be considered editable',
        editorNode.isEditable(childNode));
    assertTrue(
        'Child element should be considered editable',
        editorNode.isEditable(childElement));
    assertTrue(
        'Grandchild node should be considered editable',
        editorNode.isEditable(childElement.firstChild));
    assertFalse(
        'Other element should not be considered editable',
        editorNode.isEditable(document.getElementById('parentNode')));
  },

  testFindTopMostEditableAncestor() {
    const root = document.getElementById('editableTest');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const span = googDom.getElementsByTagName(TagName.SPAN, root)[0];
    const textNode = span.firstChild;

    assertEquals(
        'Should return self if self is matched.', textNode,
        editorNode.findTopMostEditableAncestor(
            textNode, (node) => node.nodeType == NodeType.TEXT));
    assertEquals(
        'Should not walk out of editable node.', null,
        editorNode.findTopMostEditableAncestor(
            textNode, /**
                         @suppress {strictMissingProperties} suppression added
                         to enable type checking
                       */
            (node) => node.tagName == TagName.BODY));
    assertEquals(
        'Should not match editable container.', null,
        editorNode.findTopMostEditableAncestor(
            textNode, /**
                         @suppress {strictMissingProperties} suppression added
                         to enable type checking
                       */
            (node) => node.tagName == TagName.DIV));
    assertEquals(
        'Should find node in editable container.', span,
        editorNode.findTopMostEditableAncestor(
            textNode, /**
                         @suppress {strictMissingProperties} suppression added
                         to enable type checking
                       */
            (node) => node.tagName == TagName.SPAN));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSplitDomTreeAt() {
    const innerHTML = '<p>1<b>2</b>3</p>';
    const root = googDom.createElement(TagName.DIV);

    root.innerHTML = innerHTML;
    let result = editorNode.splitDomTreeAt(
        googDom.getElementsByTagName(TagName.B, root)[0], null, root);
    testingDom.assertHtmlContentsMatch('<p>1<b>2</b></p>', root);
    testingDom.assertHtmlContentsMatch('<p>3</p>', result);

    root.innerHTML = innerHTML;
    result = editorNode.splitDomTreeAt(
        googDom.getElementsByTagName(TagName.B, root)[0],
        googDom.createTextNode('and'), root);
    testingDom.assertHtmlContentsMatch('<p>1<b>2</b></p>', root);
    testingDom.assertHtmlContentsMatch('<p>and3</p>', result);
  },

  testTransferChildren() {
    const prefix = '<b>Bold 1</b>';
    const innerHTML = '<b>Bold</b><ul><li>Item 1</li><li>Item 2</li></ul>';

    const root1 = googDom.createElement(TagName.DIV);
    root1.innerHTML = innerHTML;

    const root2 = googDom.createElement(TagName.P);
    root2.innerHTML = prefix;

    const b = googDom.getElementsByTagName(TagName.B, root1)[0];

    // Transfer the children.
    editorNode.transferChildren(root2, root1);
    assertEquals(0, root1.childNodes.length);
    testingDom.assertHtmlContentsMatch(prefix + innerHTML, root2);
    assertEquals(b, googDom.getElementsByTagName(TagName.B, root2)[1]);

    // Transfer them back.
    editorNode.transferChildren(root1, root2);
    assertEquals(0, root2.childNodes.length);
    testingDom.assertHtmlContentsMatch(prefix + innerHTML, root1);
    assertEquals(b, googDom.getElementsByTagName(TagName.B, root1)[1]);
  },
});
