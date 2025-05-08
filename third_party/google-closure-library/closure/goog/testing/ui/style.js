/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Tools for testing Closure renderers against static markup
 * spec pages.
 */
goog.module('goog.testing.ui.style');
goog.module.declareLegacyNamespace();
goog.setTestOnly();

const asserts = goog.require('goog.asserts');
const classlist = goog.require('goog.dom.classlist');
const dom = goog.require('goog.dom');
const testingAsserts = goog.require('goog.testing.asserts');

/**
 * Uses document.write to add an iFrame to the page with the reference path in
 * the src attribute. Used for loading an html file containing reference
 * structures to test against into the page. Should be called within the body of
 * the jsunit test page.
 * @param {string} referencePath A path to a reference HTML file.
 */
function writeReferenceFrame(referencePath) {
  document.write(
      '<iframe id="reference" name="reference" ' +
      'src="' + referencePath + '"></iframe>');
}

/**
 * Returns a reference to the first element child of a node with the given id
 * from the page loaded into the reference iFrame. Used to retrieve a particular
 * reference DOM structure to test against.
 * @param {string} referenceId The id of a container element for a reference
 *     structure in the reference page.
 * @return {?Node} The root element of the reference structure.
 */
function getReferenceNode(referenceId) {
  return dom.getFirstElementChild(
      window.frames['reference'].document.getElementById(referenceId));
}

/**
 * Returns an array of all element children of a given node.
 * @param {!Node} element The node to get element children of.
 * @return {!Array<!Node>} An array of all the element children.
 */
function getElementChildren(element) {
  const first = dom.getFirstElementChild(element);
  if (!first) {
    return [];
  }
  const children = [first];
  let next;

  while (next = dom.getNextElementSibling(children[children.length - 1])) {
    children.push(next);
  }
  return children;
}

/**
 * Tests whether a given node is a "content" node of a reference structure,
 * which means it is allowed to have arbitrary children.
 * @param {!Node} element The node to test.
 * @return {boolean} Whether the given node is a content node or not.
 * @suppress {missingProperties} "className" not defined on Node
 */
function isContentNode(element) {
  return element.className.indexOf('content') != -1;
}

/**
 * Tests that the structure, node names, and classes of the given element are
 * the same as the reference structure with the given id. Throws an error if the
 * element doesn't have the same nodes at each level of the DOM with the same
 * classes on each. The test ignores all DOM structure within content nodes.
 * @param {?Node} element The root node of the DOM structure to test.
 * @param {string} referenceId The id of the container for the reference
 *     structure to test against.
 */
function assertStructureMatchesReference(element, referenceId) {
  assertStructureMatchesReferenceInner(element, getReferenceNode(referenceId));
}

/**
 * A recursive function for comparing structure, node names, and classes between
 * a test and reference DOM structure. Throws an error if one of these things
 * doesn't match. Used internally by
 * {@link assertStructureMatchesReference}.
 * @param {?Node} element DOM element to test.
 * @param {?Node} reference DOM element to use as a reference (test against).
 */
function assertStructureMatchesReferenceInner(element, reference) {
  if (!element && !reference) {
    return;
  }
  testingAsserts.assertTrue('Expected two elements.', !!element && !!reference);
  testingAsserts.assertEquals(
      'Expected nodes to have the same nodeName.', element.nodeName,
      reference.nodeName);
  const testElem = asserts.assertElement(element);
  const refElem = asserts.assertElement(reference);
  const elementClasses = classlist.get(testElem);
  Array.prototype.forEach.call(classlist.get(refElem), (referenceClass) => {
    testingAsserts.assertContains(
        'Expected test node to have all reference classes.', referenceClass,
        elementClasses);
  });
  // Call assertStructureMatchesReferenceInner_ on all element children
  // unless this is a content node
  const elChildren = getElementChildren(element);
  const refChildren = getElementChildren(reference);

  if (!isContentNode(reference)) {
    if (elChildren.length != refChildren.length) {
      assertEquals(
          'Expected same number of children for a non-content node.',
          elChildren.length, refChildren.length);
    }
    for (let i = 0; i < elChildren.length; i++) {
      assertStructureMatchesReferenceInner(elChildren[i], refChildren[i]);
    }
  }
}

exports = {
  assertStructureMatchesReference,
  getElementChildren,
  getReferenceNode,
  isContentNode,
  writeReferenceFrame,
};
