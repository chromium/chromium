/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Tools for testing Closure renderers against static markup
 * spec pages.
 */

goog.setTestOnly('goog.testing.ui.style');
goog.provide('goog.testing.ui.style');

goog.require('goog.asserts');
goog.require('goog.dom');
goog.require('goog.dom.classlist');
goog.require('goog.testing.asserts');


/**
 * Uses document.write to add an iFrame to the page with the reference path in
 * the src attribute. Used for loading an html file containing reference
 * structures to test against into the page. Should be called within the body of
 * the jsunit test page.
 * @param {string} referencePath A path to a reference HTML file.
 */
goog.testing.ui.style.writeReferenceFrame = function(referencePath) {
  'use strict';
  document.write(
      '<iframe id="reference" name="reference" ' +
      'src="' + referencePath + '"></iframe>');
};


/**
 * Returns a reference to the first element child of a node with the given id
 * from the page loaded into the reference iFrame. Used to retrieve a particular
 * reference DOM structure to test against.
 * @param {string} referenceId The id of a container element for a reference
 *   structure in the reference page.
 * @return {Node} The root element of the reference structure.
 */
goog.testing.ui.style.getReferenceNode = function(referenceId) {
  'use strict';
  return goog.dom.getFirstElementChild(
      window.frames['reference'].document.getElementById(referenceId));
};


/**
 * Returns an array of all element children of a given node.
 * @param {Node} element The node to get element children of.
 * @return {!Array<!Node>} An array of all the element children.
 */
goog.testing.ui.style.getElementChildren = function(element) {
  'use strict';
  const first = goog.dom.getFirstElementChild(element);
  if (!first) {
    return [];
  }
  const children = [first];
  let next;

  while (next = goog.dom.getNextElementSibling(children[children.length - 1])) {
    children.push(next);
  }
  return children;
};


/**
 * Tests whether a given node is a "content" node of a reference structure,
 * which means it is allowed to have arbitrary children.
 * @param {Node} element The node to test.
 * @return {boolean} Whether the given node is a content node or not.
 * @suppress {missingProperties} "className" not defined on Node
 */
goog.testing.ui.style.isContentNode = function(element) {
  'use strict';
  return element.className.indexOf('content') != -1;
};


/**
 * Tests that the structure, node names, and classes of the given element are
 * the same as the reference structure with the given id. Throws an error if the
 * element doesn't have the same nodes at each level of the DOM with the same
 * classes on each. The test ignores all DOM structure within content nodes.
 * @param {Node} element The root node of the DOM structure to test.
 * @param {string} referenceId The id of the container for the reference
 *   structure to test against.
 */
goog.testing.ui.style.assertStructureMatchesReference = function(
    element, referenceId) {
  'use strict';
  goog.testing.ui.style.assertStructureMatchesReferenceInner_(
      element, goog.testing.ui.style.getReferenceNode(referenceId));
};


/**
 * A recursive function for comparing structure, node names, and classes between
 * a test and reference DOM structure. Throws an error if one of these things
 * doesn't match. Used internally by
 * {@link goog.testing.ui.style.assertStructureMatchesReference}.
 * @param {Node} element DOM element to test.
 * @param {Node} reference DOM element to use as a reference (test against).
 * @private
 */
goog.testing.ui.style.assertStructureMatchesReferenceInner_ = function(
    element, reference) {
  'use strict';
  if (!element && !reference) {
    return;
  }
  assertTrue('Expected two elements.', !!element && !!reference);
  assertEquals(
      'Expected nodes to have the same nodeName.', element.nodeName,
      reference.nodeName);
  const testElem = goog.asserts.assertElement(element);
  const refElem = goog.asserts.assertElement(reference);
  const elementClasses = goog.dom.classlist.get(testElem);
  Array.prototype.forEach.call(
      goog.dom.classlist.get(refElem), function(referenceClass) {
        'use strict';
        assertContains(
            'Expected test node to have all reference classes.', referenceClass,
            elementClasses);
      });
  // Call assertStructureMatchesReferenceInner_ on all element children
  // unless this is a content node
  const elChildren = goog.testing.ui.style.getElementChildren(element);
  const refChildren = goog.testing.ui.style.getElementChildren(reference);

  if (!goog.testing.ui.style.isContentNode(reference)) {
    if (elChildren.length != refChildren.length) {
      assertEquals(
          'Expected same number of children for a non-content node.',
          elChildren.length, refChildren.length);
    }
    for (let i = 0; i < elChildren.length; i++) {
      goog.testing.ui.style.assertStructureMatchesReferenceInner_(
          elChildren[i], refChildren[i]);
    }
  }
};
