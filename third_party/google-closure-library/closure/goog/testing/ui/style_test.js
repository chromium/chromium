/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.ui.styleTest');
goog.setTestOnly();

const dom = goog.require('goog.dom');
const style = goog.require('goog.testing.ui.style');
const testSuite = goog.require('goog.testing.testSuite');

// Write iFrame tag to load reference FastUI markup. Then, our tests will
// compare the generated markup to the reference markup.
const refPath = 'style_reference.html';
style.writeReferenceFrame(refPath);

// assertStructureMatchesReference should succeed if the structure, node
// names, and classes match.

// assertStructureMatchesReference should fail if one of the nodes is
// missing a class.

// assertStructureMatchesReference should NOT fail if one of the nodes has
// an additional class.

// assertStructureMatchesReference should fail if there is a missing child
// node somewhere in the DOM structure.

// assertStructureMatchesReference should fail if there is an extra child
// node somewhere in the DOM structure.

testSuite({

  testCorrect() {
    const el = dom.getFirstElementChild(dom.getElement('correct'));
    style.assertStructureMatchesReference(el, 'reference');
  },

  testMissingClass() {
    const el = dom.getFirstElementChild(dom.getElement('missing-class'));
    const e = assertThrowsJsUnitException(() => {
      style.assertStructureMatchesReference(el, 'reference');
    });
    assertContains('all reference classes', e.message);
  },

  testExtraClass() {
    const el = dom.getFirstElementChild(dom.getElement('extra-class'));
    style.assertStructureMatchesReference(el, 'reference');
  },

  testMissingChild() {
    const el = dom.getFirstElementChild(dom.getElement('missing-child'));
    const e = assertThrowsJsUnitException(() => {
      style.assertStructureMatchesReference(el, 'reference');
    });
    assertContains('same number of children', e.message);
  },

  testExtraChild() {
    const el = dom.getFirstElementChild(dom.getElement('extra-child'));
    const e = assertThrowsJsUnitException(() => {
      style.assertStructureMatchesReference(el, 'reference');
    });
    assertContains('same number of children', e.message);
  },
});
