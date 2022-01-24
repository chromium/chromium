/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.SavedCaretRangeTest');
goog.setTestOnly();

const Range = goog.require('goog.dom.Range');
const SavedCaretRange = goog.require('goog.dom.SavedCaretRange');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');
const testingDom = goog.require('goog.testing.dom');
const userAgent = goog.require('goog.userAgent');

/*
   TODO(user): Look into why removeCarets test doesn't pass.
   function testRemoveCarets() {
   var def = goog.dom.getElement('def');
   var jkl = goog.dom.getElement('jkl');

   var range = goog.dom.Range.createFromNodes(
   def.firstChild, 1, jkl.firstChild, 2);
   range.select();

   var saved = range.saveUsingCarets();
   assertHTMLEquals(
   "d<span id="" + saved.startCaretId_ + ""></span>ef", def.innerHTML);
   assertHTMLEquals(
   "jk<span id="" + saved.endCaretId_ + ""></span>l", jkl.innerHTML);

   saved.removeCarets();
   assertHTMLEquals("def", def.innerHTML);
   assertHTMLEquals("jkl", jkl.innerHTML);

   var selection = goog.dom.Range.createFromWindow(window);

   assertEquals('Wrong start node', def.firstChild, selection.getStartNode());
   assertEquals('Wrong end node', jkl.firstChild, selection.getEndNode());
   assertEquals('Wrong start offset', 1, selection.getStartOffset());
   assertEquals('Wrong end offset', 2, selection.getEndOffset());
   }
   */

/**
 * Clear the selection by re-parsing the DOM. Then restore the saved
 * selection.
 * @param {Node} parent The node containing the current selection.
 * @param {dom.SavedRange} saved The saved range.
 * @return {dom.AbstractRange} Restored range.
 */
function clearSelectionAndRestoreSaved(parent, saved) {
  Range.clearSelection();
  assertFalse(Range.hasSelection(window));
  const range = saved.restore();
  assertTrue(Range.hasSelection(window));
  return range;
}
testSuite({
  setUp() {
    document.body.normalize();
  },

  /** @bug 1480638 */
  testSavedCaretRangeDoesntChangeSelection() {
    // NOTE(nicksantos): We cannot detect this bug programatically. The only
    // way to detect it is to run this test manually and look at the selection
    // when it ends.
    const div = dom.getElement('bug1480638');
    const range = Range.createFromNodes(div.firstChild, 0, div.lastChild, 1);
    range.select();

    // Observe visible selection.  Then move to next line and see it change.
    // If the bug exists, it starts with "foo" selected and ends with
    // it not selected.
    // debugger;
    const saved = range.saveUsingCarets();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSavedCaretRange() {
    if (userAgent.IE && !userAgent.isDocumentModeOrHigher(8)) {
      // testSavedCaretRange fails in IE7 unless the source files are loaded in
      // a certain order. Adding goog.require('goog.dom.classes') to dom.js or
      // goog.require('goog.array') to savedcaretrange_test.js after the
      // goog.require('goog.dom') line fixes the test, but it's better to not
      // rely on such hacks without understanding the reason of the failure.
      return;
    }

    const parent = dom.getElement('caretRangeTest');
    let def = dom.getElement('def');
    let jkl = dom.getElement('jkl');

    const range = Range.createFromNodes(def.firstChild, 1, jkl.firstChild, 2);
    assertFalse(range.isReversed());
    range.select();

    const saved = range.saveUsingCarets();
    assertHTMLEquals(
        'd<span id="' + saved.startCaretId_ + '"></span>ef', def.innerHTML);
    assertHTMLEquals(
        'jk<span id="' + saved.endCaretId_ + '"></span>l', jkl.innerHTML);

    testingDom.assertRangeEquals(
        def.childNodes[1], 0, jkl.childNodes[1], 0, saved.toAbstractRange());

    def = dom.getElement('def');
    jkl = dom.getElement('jkl');

    const restoredRange = clearSelectionAndRestoreSaved(parent, saved);
    assertFalse(restoredRange.isReversed());
    testingDom.assertRangeEquals(def, 1, jkl, 1, restoredRange);

    const selection = Range.createFromWindow(window);
    assertHTMLEquals('def', def.innerHTML);
    assertHTMLEquals('jkl', jkl.innerHTML);

    // def and jkl now contain fragmented text nodes.
    const endNode = selection.getEndNode();
    if (endNode == jkl.childNodes[0]) {
      // Webkit (up to Chrome 57) and IE < 9.
      testingDom.assertRangeEquals(
          def.childNodes[1], 0, jkl.childNodes[0], 2, selection);
    } else if (endNode == jkl.childNodes[1]) {
      // Opera
      testingDom.assertRangeEquals(
          def.childNodes[1], 0, jkl.childNodes[1], 0, selection);
    } else {
      // Gecko, newer Chromes
      testingDom.assertRangeEquals(def, 1, jkl, 1, selection);
    }
  },

  testReversedSavedCaretRange() {
    const parent = dom.getElement('caretRangeTest');
    const def = dom.getElement('def-5');
    const jkl = dom.getElement('jkl-5');

    const range = Range.createFromNodes(jkl.firstChild, 1, def.firstChild, 2);
    assertTrue(range.isReversed());
    range.select();

    const saved = range.saveUsingCarets();
    const restoredRange = clearSelectionAndRestoreSaved(parent, saved);
    assertTrue(restoredRange.isReversed());
    testingDom.assertRangeEquals(def, 1, jkl, 1, restoredRange);
  },

  testRemoveContents() {
    const def = dom.getElement('def-4');
    const jkl = dom.getElement('jkl-4');

    // Sanity check.
    const container = dom.getElement('removeContentsTest');
    assertEquals(7, container.childNodes.length);
    assertEquals('def', def.innerHTML);
    assertEquals('jkl', jkl.innerHTML);

    const range = Range.createFromNodes(def.firstChild, 1, jkl.firstChild, 2);
    range.select();

    const saved = range.saveUsingCarets();
    const restored = saved.restore();
    restored.removeContents();

    assertEquals(6, container.childNodes.length);
    assertEquals('d', def.innerHTML);
    assertEquals('l', jkl.innerHTML);
  },

  testHtmlEqual() {
    const parent = dom.getElement('caretRangeTest-2');
    const def = dom.getElement('def-2');
    const jkl = dom.getElement('jkl-2');

    const range = Range.createFromNodes(def.firstChild, 1, jkl.firstChild, 2);
    range.select();
    const saved = range.saveUsingCarets();
    const html1 = parent.innerHTML;
    saved.removeCarets();

    const saved2 = range.saveUsingCarets();
    const html2 = parent.innerHTML;
    saved2.removeCarets();

    assertNotEquals(
        'Same selection with different saved caret range carets ' +
            'must have different html.',
        html1, html2);

    assertTrue(
        'Same selection with different saved caret range carets must ' +
            'be considered equal by htmlEqual',
        SavedCaretRange.htmlEqual(html1, html2));

    saved.dispose();
    saved2.dispose();
  },

  testStartCaretIsAtEndOfParent() {
    const parent = dom.getElement('caretRangeTest-3');
    const def = dom.getElement('def-3');
    const jkl = dom.getElement('jkl-3');

    let range = Range.createFromNodes(def, 1, jkl, 1);
    range.select();
    const saved = range.saveUsingCarets();
    clearSelectionAndRestoreSaved(parent, saved);
    range = Range.createFromWindow();
    assertEquals('ghijkl', range.getText().replace(/\s/g, ''));
  },
});
