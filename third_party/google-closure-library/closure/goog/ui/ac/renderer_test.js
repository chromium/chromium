/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.ac.RendererTest');
goog.setTestOnly();

const AutoComplete = goog.require('goog.ui.ac.AutoComplete');
const FadeInAndShow = goog.require('goog.fx.dom.FadeInAndShow');
const FadeOutAndHide = goog.require('goog.fx.dom.FadeOutAndHide');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const Renderer = goog.require('goog.ui.ac.Renderer');
const State = goog.require('goog.a11y.aria.State');
const TagName = goog.require('goog.dom.TagName');
const aria = goog.require('goog.a11y.aria');
const classlist = goog.require('goog.dom.classlist');
const dispose = goog.require('goog.dispose');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const googString = goog.require('goog.string');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');

let renderer;
const rendRows = [];
let someElement;
let target;
let viewport;
let viewportTarget;
let widthProvider;
let maxWidthProvider;
let propertyReplacer;

// One-time set up of rows formatted for the renderer.
const rows = [
  'Amanda Annie Anderson',
  'Frankie Manning',
  'Louis D Armstrong',
  // NOTE(user): sorry about this test input, but it has caused problems
  // in the past, so I want to make sure to test against it.
  'Foo Bar................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................',
  '<div><div>test</div></div>',
  '<div><div>test1</div><div>test2</div></div>',
  '<div>random test string<div>test1</div><div><div>test2</div><div>test3</div></div></div>',
];

for (let i = 0; i < rows.length; i++) {
  rendRows.push({id: i, data: rows[i]});
}

// ------- Helper functions -------

// The default rowRenderer will escape any HTML in the row content.
// Activating HTML rendering will allow HTML strings to be rendered to DOM
// instead of being escaped.
function enableHtmlRendering(renderer) {
  let customRendererInternal = {
    renderRow: function(row, token, node) {
      node.innerHTML = row.data.toString();
    },
  };
}

function assertNumBoldTags(boldTagElArray, expectedNum) {
  assertEquals(
      'Incorrect number of bold tags', expectedNum, boldTagElArray.length);
}

function assertPreviousNodeText(boldTag, expectedText) {
  const prevNode = boldTag.previousSibling;
  assertEquals(
      'Expected text before the token does not match', expectedText,
      prevNode.nodeValue);
}

function assertHighlightedText(boldTag, expectedHighlightedText) {
  assertEquals(
      'Incorrect text bolded', expectedHighlightedText, boldTag.innerHTML);
}

function assertLastNodeText(node, expectedText) {
  const lastNode = node.lastChild;
  assertEquals(
      'Incorrect text in the last node', expectedText, lastNode.nodeValue);
}
testSuite({
  setUpPage() {
    someElement = dom.getElement('someElement');
    target = dom.getElement('target');
    viewport = dom.getElement('viewport');
    viewportTarget = dom.getElement('viewportTarget');
    widthProvider = dom.getElement('widthProvider');
    maxWidthProvider = dom.getElement('maxWidthProvider');
    propertyReplacer = new PropertyReplacer();
  },

  setUp() {
    renderer = new Renderer();
    /** @suppress {visibility} suppression added to enable type checking */
    renderer.rowDivs_ = [];
    /** @suppress {visibility} suppression added to enable type checking */
    renderer.target_ = target;
  },

  tearDown() {
    renderer.dispose();
    propertyReplacer.reset();
  },

  testBasicMatchingWithHtmlRow() {
    // '<div><div>test</div></div>'
    const row = rendRows[4];
    const token = 'te';
    enableHtmlRendering(renderer);
    const node = renderer.renderRowHtml(row, token);
    const boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 1);
  },

  testShouldMatchOnlyOncePerDefaultWithComplexHtmlStrings() {
    // '<div><div>test1</div><div>test2</div></div>'
    const row = rendRows[5];
    const token = 'te';
    enableHtmlRendering(renderer);
    const node = renderer.renderRowHtml(row, token);
    const boldTagElArray = dom.getElementsByTagName(TagName.B, node);

    // It should match and render highlighting for the first 'test1' and
    // stop here. This is the default behavior of the renderer.
    assertNumBoldTags(boldTagElArray, 1);
  },

  testShouldMatchMultipleTimesWithComplexHtmlStrings() {
    renderer.setHighlightAllTokens(true);

    // '<div><div>test1</div><div>test2</div></div>'
    let row = rendRows[5];
    const token = 'te';
    enableHtmlRendering(renderer);
    let node = renderer.renderRowHtml(row, token);
    let boldTagElArray = dom.getElementsByTagName(TagName.B, node);

    // It should match and render highlighting for both 'test1' and 'test2'.
    assertNumBoldTags(boldTagElArray, 2);

    // Try again with a more complex HTML string.
    // '<div>random test
    // string<div>test1</div><div><div>test2</div><div>test3</div></div></div>'
    row = rendRows[6];
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    // It should match 'test', 'test1', 'test2' and 'test3' wherever
    // they are in the DOM tree.
    assertNumBoldTags(boldTagElArray, 4);
  },

  testBasicStringTokenHighlightingUsingUniversalMatching() {
    const row = rendRows[0];  // 'Amanda Annie Anderson'
    renderer.setMatchWordBoundary(false);

    // Should highlight first match only.
    let token = 'A';
    let node = renderer.renderRowHtml(row, token);
    let boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 1);
    assertPreviousNodeText(boldTagElArray[0], '');
    assertHighlightedText(boldTagElArray[0], 'A');
    assertLastNodeText(node, 'manda Annie Anderson');

    // Match should be case insensitive, and should match tokens in the
    // middle of words if useWordMatching is turned off ("an" in Amanda).
    token = 'an';
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 1);
    assertPreviousNodeText(boldTagElArray[0], 'Am');
    assertHighlightedText(boldTagElArray[0], 'an');
    assertLastNodeText(node, 'da Annie Anderson');

    // Should only match on non-empty strings.
    token = '';
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 0);
    assertLastNodeText(node, 'Amanda Annie Anderson');

    // Should not match leading whitespace.
    token = ' an';
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 0);
    assertLastNodeText(node, 'Amanda Annie Anderson');
  },

  testBasicStringTokenHighlighting() {
    let row = rendRows[0];  // 'Amanda Annie Anderson'

    // Should highlight first match only.
    let token = 'A';
    let node = renderer.renderRowHtml(row, token);
    let boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 1);
    assertPreviousNodeText(boldTagElArray[0], '');
    assertHighlightedText(boldTagElArray[0], 'A');
    assertLastNodeText(node, 'manda Annie Anderson');

    // Should only match on non-empty strings.
    token = '';
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 0);
    assertLastNodeText(node, 'Amanda Annie Anderson');

    // Match should be case insensitive, and should not match tokens in the
    // middle of words ("an" in Amanda).
    token = 'an';
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 1);
    assertPreviousNodeText(boldTagElArray[0], 'Amanda ');
    assertHighlightedText(boldTagElArray[0], 'An');
    assertLastNodeText(node, 'nie Anderson');

    // Should not match whitespace.
    token = ' ';
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 0);
    assertLastNodeText(node, 'Amanda Annie Anderson');

    // Should not match leading whitespace since all matches are at the start of
    // word boundaries.
    token = ' an';
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 0);
    assertLastNodeText(node, 'Amanda Annie Anderson');

    // Should match trailing whitespace.
    token = 'annie ';
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 1);
    assertPreviousNodeText(boldTagElArray[0], 'Amanda ');
    assertHighlightedText(boldTagElArray[0], 'Annie ');
    assertLastNodeText(node, 'Anderson');

    // Should match across whitespace.
    row = rendRows[2];  // 'Louis D Armstrong'
    token = 'd a';
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 1);
    assertPreviousNodeText(boldTagElArray[0], 'Louis ');
    assertHighlightedText(boldTagElArray[0], 'D A');
    assertLastNodeText(node, 'rmstrong');

    // Should match the last token.
    token = 'aRmStRoNg';
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 1);
    assertPreviousNodeText(boldTagElArray[0], 'Louis D ');
    assertHighlightedText(boldTagElArray[0], 'Armstrong');
    assertLastNodeText(node, '');
  },

  // The name of this function is fortuitous, in that it gets tested
  // last on FF. The lazy regexp on FF is particularly slow, and causes
  // the test to take a long time, and sometimes time out when run on forge
  // because it triggers the test runner to go back to the event loop...
  testPathologicalInput() {
    // Should not hang on bizarrely long strings
    const row = rendRows[3];  // pathological row
    const token = 'foo';
    const node = renderer.renderRowHtml(row, token);
    const boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 1);
    assertHighlightedText(boldTagElArray[0], 'Foo');
    assert(googString.startsWith(
        boldTagElArray[0].nextSibling.nodeValue, ' Bar...'));
  },

  testBasicArrayTokenHighlighting() {
    let row = rendRows[1];  // 'Frankie Manning'

    // Only the first match in the array should be highlighted.
    let token = ['f', 'm'];
    let node = renderer.renderRowHtml(row, token);
    let boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 1);
    assertPreviousNodeText(boldTagElArray[0], '');
    assertHighlightedText(boldTagElArray[0], 'F');
    assertLastNodeText(node, 'rankie Manning');

    // Only the first match in the array should be highlighted.
    token = ['m', 'f'];
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 1);
    assertPreviousNodeText(boldTagElArray[0], 'Frankie ');
    assertHighlightedText(boldTagElArray[0], 'M');
    assertLastNodeText(node, 'anning');

    // Skip tokens that do not match.
    token = ['asdf', 'f'];
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 1);
    assertPreviousNodeText(boldTagElArray[0], '');
    assertHighlightedText(boldTagElArray[0], 'F');
    assertLastNodeText(node, 'rankie Manning');

    // Highlight nothing if no tokens match.
    token = ['Foo', 'bar', 'baz'];
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 0);
    assertLastNodeText(node, 'Frankie Manning');

    // Empty array should not match.
    token = [];
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 0);
    assertLastNodeText(node, 'Frankie Manning');

    // Empty string in array should not match.
    token = [''];
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 0);
    assertLastNodeText(node, 'Frankie Manning');

    // Whitespace in array should not match.
    token = [' '];
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 0);
    assertLastNodeText(node, 'Frankie Manning');

    // Whitespace entries in array should not match.
    token = [' ', 'man'];
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 1);
    assertPreviousNodeText(boldTagElArray[0], 'Frankie ');
    assertHighlightedText(boldTagElArray[0], 'Man');
    assertLastNodeText(node, 'ning');

    // Whitespace in array entry should match as a whole token.
    row = rendRows[2];  // 'Louis D Armstrong'
    token = ['d arm', 'lou'];
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 1);
    assertPreviousNodeText(boldTagElArray[0], 'Louis ');
    assertHighlightedText(boldTagElArray[0], 'D Arm');
    assertLastNodeText(node, 'strong');
  },

  testHighlightAllTokensSingleTokenHighlighting() {
    renderer.setHighlightAllTokens(true);
    const row = rendRows[0];  // 'Amanda Annie Anderson'

    // All matches at the start of the word should be highlighted when
    // highlightAllTokens is set.
    let token = 'a';
    let node = renderer.renderRowHtml(row, token);
    let boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 3);
    assertPreviousNodeText(boldTagElArray[0], '');
    assertHighlightedText(boldTagElArray[0], 'A');
    assertPreviousNodeText(boldTagElArray[1], 'manda ');
    assertHighlightedText(boldTagElArray[1], 'A');
    assertPreviousNodeText(boldTagElArray[2], 'nnie ');
    assertHighlightedText(boldTagElArray[2], 'A');
    assertLastNodeText(node, 'nderson');

    // Should not match on empty string.
    token = '';
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 0);
    assertLastNodeText(node, 'Amanda Annie Anderson');

    // Match should be case insensitive.
    token = 'AN';
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 2);
    assertPreviousNodeText(boldTagElArray[0], 'Amanda ');
    assertHighlightedText(boldTagElArray[0], 'An');
    assertPreviousNodeText(boldTagElArray[1], 'nie ');
    assertHighlightedText(boldTagElArray[1], 'An');
    assertLastNodeText(node, 'derson');

    // Should not match on whitespace.
    token = ' ';
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 0);
    assertLastNodeText(node, 'Amanda Annie Anderson');

    // When highlighting all tokens, should match despite leading whitespace.
    token = '  am';
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 1);
    assertPreviousNodeText(boldTagElArray[0], '');
    assertHighlightedText(boldTagElArray[0], 'Am');
    assertLastNodeText(node, 'anda Annie Anderson');

    // Should match with trailing whitepsace.
    token = 'ann   ';
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 1);
    assertPreviousNodeText(boldTagElArray[0], 'Amanda ');
    assertHighlightedText(boldTagElArray[0], 'Ann');
    assertLastNodeText(node, 'ie Anderson');
  },

  testHighlightAllTokensMultipleStringTokenHighlighting() {
    renderer.setHighlightAllTokens(true);
    const row = rendRows[1];  // 'Frankie Manning'

    // Each individual space-separated token should match.
    const token = 'm F';
    const node = renderer.renderRowHtml(row, token);
    const boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 2);
    assertPreviousNodeText(boldTagElArray[0], '');
    assertHighlightedText(boldTagElArray[0], 'F');
    assertPreviousNodeText(boldTagElArray[1], 'rankie ');
    assertHighlightedText(boldTagElArray[1], 'M');
    assertLastNodeText(node, 'anning');
  },

  testHighlightAllTokensArrayTokenHighlighting() {
    renderer.setHighlightAllTokens(true);
    const row = rendRows[0];  // 'Amanda Annie Anderson'

    // All tokens in the array should match.
    let token = ['AM', 'AN'];
    let node = renderer.renderRowHtml(row, token);
    let boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 3);
    assertPreviousNodeText(boldTagElArray[0], '');
    assertHighlightedText(boldTagElArray[0], 'Am');
    assertPreviousNodeText(boldTagElArray[1], 'anda ');
    assertHighlightedText(boldTagElArray[1], 'An');
    assertPreviousNodeText(boldTagElArray[2], 'nie ');
    assertHighlightedText(boldTagElArray[2], 'An');
    assertLastNodeText(node, 'derson');

    // Empty array should not match.
    token = [];
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 0);
    assertLastNodeText(node, 'Amanda Annie Anderson');

    // Empty string in array should not match.
    token = [''];
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 0);
    assertLastNodeText(node, 'Amanda Annie Anderson');

    // Whitespace in array should not match.
    token = [' '];
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 0);
    assertLastNodeText(node, 'Amanda Annie Anderson');

    // Empty string entries in array should not match.
    token = ['', 'Ann'];
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 1);
    assertPreviousNodeText(boldTagElArray[0], 'Amanda ');
    assertHighlightedText(boldTagElArray[0], 'Ann');
    assertLastNodeText(node, 'ie Anderson');

    // Whitespace entries in array should not match.
    token = [' ', 'And'];
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 1);
    assertPreviousNodeText(boldTagElArray[0], 'Amanda Annie ');
    assertHighlightedText(boldTagElArray[0], 'And');
    assertLastNodeText(node, 'erson');

    // Whitespace in array entry should match as a whole token.
    token = ['annie a', 'Am'];
    node = renderer.renderRowHtml(row, token);
    boldTagElArray = dom.getElementsByTagName(TagName.B, node);
    assertNumBoldTags(boldTagElArray, 2);
    assertPreviousNodeText(boldTagElArray[0], '');
    assertHighlightedText(boldTagElArray[0], 'Am');
    assertPreviousNodeText(boldTagElArray[1], 'anda ');
    assertHighlightedText(boldTagElArray[1], 'Annie A');
    assertLastNodeText(node, 'nderson');
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testMenuFadeDuration() {
    renderer.maybeCreateElement_();

    let hideCalled = false;
    let hideAnimCalled = false;
    let showCalled = false;
    let showAnimCalled = false;

    propertyReplacer.set(style, 'setElementShown', (el, state) => {
      if (state) {
        showCalled = true;
      } else {
        hideCalled = true;
      }
    });

    propertyReplacer.set(FadeInAndShow.prototype, 'play', () => {
      showAnimCalled = true;
    });

    propertyReplacer.set(FadeOutAndHide.prototype, 'play', () => {
      hideAnimCalled = true;
    });

    // Default behavior does show/hide but not animations.

    renderer.show();
    assertTrue(showCalled);
    assertFalse(showAnimCalled);

    renderer.dismiss();
    assertTrue(hideCalled);
    assertFalse(hideAnimCalled);

    // But animations can be turned on.

    showCalled = false;
    hideCalled = false;
    renderer.setMenuFadeDuration(100);

    renderer.show();
    assertFalse(showCalled);
    assertTrue(showAnimCalled);

    renderer.dismiss();
    assertFalse(hideCalled);
    assertTrue(hideAnimCalled);
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testAriaTags() {
    renderer.maybeCreateElement_();

    assertNotNull(target);
    assertEvaluatesToFalse('The role should be empty.', aria.getRole(target));
    assertEquals('', aria.getState(target, State.HASPOPUP));
    assertEquals('', aria.getState(renderer.getElement(), State.EXPANDED));
    assertEquals('', aria.getState(target, State.OWNS));

    renderer.show();
    assertEquals('true', aria.getState(target, State.HASPOPUP));
    assertEquals('true', aria.getState(target, State.EXPANDED));
    assertEquals('true', aria.getState(renderer.getElement(), State.EXPANDED));
    assertEquals(renderer.getElement().id, aria.getState(target, State.OWNS));

    renderer.dismiss();
    assertEquals('false', aria.getState(target, State.HASPOPUP));
    assertEquals('false', aria.getState(target, State.EXPANDED));
    assertEquals('false', aria.getState(renderer.getElement(), State.EXPANDED));
    assertEquals('', aria.getState(target, State.OWNS));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testHiliteRowWithDefaultRenderer() {
    renderer.renderRows(rendRows, '');
    renderer.hiliteRow(2);
    assertEquals(2, renderer.hilitedRow_);
    assertTrue(
        classlist.contains(renderer.rowDivs_[2], renderer.activeClassName));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testHiliteRowWithCustomRenderer() {
    dispose(renderer);

    // Use a custom renderer that doesn't put the result divs as direct children
    // of this.element_.
    const customRenderer = {
      render: function(renderer, element, rows, token) {
        // Put all of the results into a results holder div that is a child of
        // this.element_.
        const resultsHolder = dom.createDom(TagName.DIV);
        dom.appendChild(element, resultsHolder);
        let row;
        for (let i = 0; row = rows[i]; ++i) {
          const node = renderer.renderRowHtml(row, token);
          dom.appendChild(resultsHolder, node);
        }
      },
    };
    renderer = new Renderer(null, customRenderer);

    // Make sure we can still highlight the row at position 2 even though
    // this.element_.childNodes contains only a single child.
    renderer.renderRows(rendRows, '');
    renderer.hiliteRow(2);
    assertEquals(2, renderer.hilitedRow_);
    assertTrue(
        classlist.contains(renderer.rowDivs_[2], renderer.activeClassName));
  },

  testReposition() {
    renderer.renderRows(rendRows, '', target);
    const el = renderer.getElement();
    el.style.position = 'absolute';
    el.style.width = '100px';

    renderer.setAutoPosition(true);
    renderer.redraw();

    const rendererOffset = style.getPageOffset(renderer.getElement());
    const rendererSize = style.getSize(renderer.getElement());
    const targetOffset = style.getPageOffset(target);
    const targetSize = style.getSize(target);

    assertEquals(0 + targetOffset.x, rendererOffset.x);
    assertRoughlyEquals(
        targetOffset.y + targetSize.height, rendererOffset.y, 1);
  },

  testSetWidthProvider() {
    renderer.setWidthProvider(widthProvider);
    renderer.renderRows(rendRows, '');
    const el = renderer.getElement();
    // Set a width that's smaller than widthProvider.
    el.style.width = '1px';

    renderer.redraw();

    const rendererSize = style.getSize(el);
    const widthProviderSize = style.getSize(widthProvider);
    assertEquals(rendererSize.width, widthProviderSize.width);
  },

  testSetWidthProviderWithBorderWidth() {
    const borderWidth = 5;
    renderer.setWidthProvider(widthProvider, borderWidth);
    renderer.renderRows(rendRows, '');
    const el = renderer.getElement();
    // Set a width that's smaller than widthProvider.
    el.style.width = '1px';

    renderer.redraw();

    const rendererSize = style.getSize(el);
    const widthProviderSize = style.getSize(widthProvider);
    assertEquals(rendererSize.width, widthProviderSize.width - borderWidth);
  },

  testSetWidthProviderWithBorderWidthAndMaxWidthProvider() {
    const borderWidth = 5;
    renderer.setWidthProvider(widthProvider, borderWidth, maxWidthProvider);
    renderer.renderRows(rendRows, '');
    const el = renderer.getElement();
    // Set a width that's larger than maxWidthProvider.
    el.style.width = '250px';

    renderer.redraw();

    const rendererSize = style.getSize(el);
    const maxWidthProviderSize = style.getSize(maxWidthProvider);
    assertEquals(rendererSize.width, maxWidthProviderSize.width - borderWidth);
  },

  testRepositionWithRightAlign() {
    renderer.renderRows(rendRows, '', target);
    const el = renderer.getElement();
    el.style.position = 'absolute';
    el.style.width = '150px';

    renderer.setAutoPosition(true);
    renderer.setRightAlign(true);
    renderer.redraw();

    const rendererOffset = style.getPageOffset(renderer.getElement());
    const rendererSize = style.getSize(renderer.getElement());
    const targetOffset = style.getPageOffset(target);
    const targetSize = style.getSize(target);

    assertRoughlyEquals(
        targetOffset.x + targetSize.width,
        rendererOffset.x + rendererSize.width, 1);
    assertRoughlyEquals(
        targetOffset.y + targetSize.height, rendererOffset.y, 1);
  },

  testRepositionResizeHeight() {
    renderer = new Renderer(viewport);
    // Render the first 4 rows from test set.
    renderer.renderRows(rendRows.slice(0, 4), '', viewportTarget);
    renderer.setAutoPosition(true);
    renderer.setShowScrollbarsIfTooLarge(true);

    // Stick a huge row in the dropdown element, to make sure it won't
    // fit in the viewport.
    const hugeRow = dom.createDom(TagName.DIV, {style: 'height:1000px'});
    dom.appendChild(renderer.getElement(), hugeRow);

    renderer.reposition();

    let rendererOffset = style.getPageOffset(renderer.getElement());
    let rendererSize = style.getSize(renderer.getElement());
    let viewportOffset = style.getPageOffset(viewport);
    let viewportSize = style.getSize(viewport);

    assertRoughlyEquals(
        viewportOffset.y + viewportSize.height,
        rendererSize.height + rendererOffset.y, 1);

    // Remove the huge row, and make sure that the dropdown element gets shrunk.
    renderer.getElement().removeChild(hugeRow);
    renderer.reposition();

    rendererOffset = style.getPageOffset(renderer.getElement());
    rendererSize = style.getSize(renderer.getElement());
    viewportOffset = style.getPageOffset(viewport);
    viewportSize = style.getSize(viewport);

    assertTrue(
        (rendererSize.height + rendererOffset.y) <
        (viewportOffset.y + viewportSize.height));
  },

  testHiliteEvent() {
    renderer.renderRows(rendRows, '');

    let hiliteEventFired = false;
    events.listenOnce(renderer, AutoComplete.EventType.ROW_HILITE, (e) => {
      hiliteEventFired = true;
      assertEquals(e.row, rendRows[1].data);
    });
    renderer.hiliteRow(1);
    assertTrue(hiliteEventFired);

    hiliteEventFired = false;
    events.listenOnce(renderer, AutoComplete.EventType.ROW_HILITE, (e) => {
      hiliteEventFired = true;
      assertNull(e.row);
    });
    renderer.hiliteRow(rendRows.length);  // i.e. out of bounds.
    assertTrue(hiliteEventFired);
  },
});
