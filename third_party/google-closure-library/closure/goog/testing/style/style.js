/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Utilities for inspecting page layout. This is a port of
 *     http://go/layoutbot.java
 *     See {@link http://go/layouttesting}.
 */

goog.setTestOnly('goog.testing.style');
goog.provide('goog.testing.style');

goog.require('goog.dom');
goog.require('goog.math.Rect');
goog.require('goog.style');


/**
 * Determines whether the bounding rectangles of the given elements intersect.
 * @param {Element} element The first element.
 * @param {Element} otherElement The second element.
 * @return {boolean} Whether the bounding rectangles of the given elements
 *     intersect.
 */
goog.testing.style.intersects = function(element, otherElement) {
  'use strict';
  const elementRect = goog.style.getBounds(element);
  const otherElementRect = goog.style.getBounds(otherElement);
  return goog.math.Rect.intersects(elementRect, otherElementRect);
};


/**
 * Determines whether the element has visible dimensions, i.e. x > 0 && y > 0.
 * @param {Element} element The element to check.
 * @return {boolean} Whether the element has visible dimensions.
 */
goog.testing.style.hasVisibleDimensions = function(element) {
  'use strict';
  const elSize = goog.style.getSize(element);
  const shortest = elSize.getShortest();
  if (shortest <= 0) {
    return false;
  }

  return true;
};


/**
 * Determines whether the CSS style of the element renders it visible.
 * Elements detached from the document are considered invisible.
 * @param {!Element} element The element to check.
 * @return {boolean} Whether the CSS style of the element renders it visible.
 */
goog.testing.style.isVisible = function(element) {
  'use strict';
  if (!goog.dom.isInDocument(element)) {
    return false;
  }
  const style = getComputedStyle(element);
  return style.visibility != 'hidden' && style.display != 'none';
};


/**
 * Test whether the given element is on screen.
 * @param {!Element} el The element to test.
 * @return {boolean} Whether the element is on the screen.
 */
goog.testing.style.isOnScreen = function(el) {
  'use strict';
  const doc = goog.dom.getDomHelper(el).getDocument();
  const viewport = goog.style.getVisibleRectForElement(doc.body);
  const viewportRect = goog.math.Rect.createFromBox(viewport);
  return goog.dom.contains(doc, el) &&
      goog.style.getBounds(el).intersects(viewportRect);
};
