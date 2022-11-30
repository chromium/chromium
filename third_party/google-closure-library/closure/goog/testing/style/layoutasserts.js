/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A utility class for making layout assertions. This is a port
 * of http://go/layoutbot.java
 * See {@link http://go/layouttesting}.
 */

goog.setTestOnly('goog.testing.style.layoutasserts');
goog.provide('goog.testing.style.layoutasserts');

goog.require('goog.style');
goog.require('goog.testing.asserts');
goog.require('goog.testing.style');


/**
 * Asserts that an element has:
 *   1 - a CSS rendering the makes the element visible.
 *   2 - a non-zero width and height.
 * @param {Element|string} a The element or optionally the comment string.
 * @param {Element=} opt_b The element when a comment string is present.
 */
const assertIsVisible = function(a, opt_b) {
  'use strict';
  _validateArguments(1, arguments);
  const element = nonCommentArg(1, 1, arguments);

  _assert(
      commentArg(1, arguments), goog.testing.style.isVisible(element) &&
          goog.testing.style.hasVisibleDimensions(element),
      'Specified element should be visible.');
};


/**
 * The counter assertion of assertIsVisible().
 * @param {Element|string} a The element or optionally the comment string.
 * @param {Element=} opt_b The element when a comment string is present.
 */
const assertNotVisible = function(a, opt_b) {
  'use strict';
  _validateArguments(1, arguments);
  const element = nonCommentArg(1, 1, arguments);
  if (!element) {
    return;
  }

  _assert(
      commentArg(1, arguments), !goog.testing.style.isVisible(element) ||
          !goog.testing.style.hasVisibleDimensions(element),
      'Specified element should not be visible.');
};


/**
 * Asserts that the two specified elements intersect.
 * @param {Element|string} a The first element or optionally the comment string.
 * @param {Element} b The second element or the first element if comment string
 *     is present.
 * @param {Element=} opt_c The second element if comment string is present.
 */
const assertIntersect = function(a, b, opt_c) {
  'use strict';
  _validateArguments(2, arguments);
  const element = nonCommentArg(1, 2, arguments);
  const otherElement = nonCommentArg(2, 2, arguments);

  _assert(
      commentArg(1, arguments),
      goog.testing.style.intersects(element, otherElement),
      'Elements should intersect.');
};


/**
 * Asserts that the two specified elements do not intersect.
 * @param {Element|string} a The first element or optionally the comment string.
 * @param {Element} b The second element or the first element if comment string
 *     is present.
 * @param {Element=} opt_c The second element if comment string is present.
 */
const assertNoIntersect = function(a, b, opt_c) {
  'use strict';
  _validateArguments(2, arguments);
  const element = nonCommentArg(1, 2, arguments);
  const otherElement = nonCommentArg(2, 2, arguments);

  _assert(
      commentArg(1, arguments),
      !goog.testing.style.intersects(element, otherElement),
      'Elements should not intersect.');
};


/**
 * Asserts that the element must have the specified width.
 * @param {Element|string} a The first element or optionally the comment string.
 * @param {Element|number} b The second element or the first element if comment string
 *     is present.
 * @param {(Element|number)=} opt_c The second element if comment string is present.
 */
const assertWidth = function(a, b, opt_c) {
  'use strict';
  _validateArguments(2, arguments);
  const element = nonCommentArg(1, 2, arguments);
  const width = nonCommentArg(2, 2, arguments);
  const size = goog.style.getSize(element);
  const elementWidth = size.width;

  _assert(
      commentArg(1, arguments),
      goog.testing.style.layoutasserts.isWithinThreshold_(
          width, elementWidth, 0 /* tolerance */),
      'Element should have width ' + width + ' but was ' + elementWidth + '.');
};


/**
 * Asserts that the element must have the specified width within the specified
 * tolerance.
 * @param {Element|string} a The element or optionally the comment string.
 * @param {number|Element} b The height or the element if comment string is
 *     present.
 * @param {number} c The tolerance or the height if comment string is
 *     present.
 * @param {number=} opt_d The tolerance if comment string is present.
 */
const assertWidthWithinTolerance = function(a, b, c, opt_d) {
  'use strict';
  _validateArguments(3, arguments);
  const element = nonCommentArg(1, 3, arguments);
  const width = nonCommentArg(2, 3, arguments);
  const tolerance = nonCommentArg(3, 3, arguments);
  const size = goog.style.getSize(element);
  const elementWidth = size.width;

  _assert(
      commentArg(1, arguments),
      goog.testing.style.layoutasserts.isWithinThreshold_(
          width, elementWidth, tolerance),
      'Element width(' + elementWidth + ') should be within given width(' +
          width + ') with tolerance value of ' + tolerance + '.');
};


/**
 * Asserts that the element must have the specified height.
 * @param {Element|string} a The first element or optionally the comment string.
 * @param {Element|number} b The second element or the first element if comment string
 *     is present.
 * @param {(Element|number)=} opt_c The second element if comment string is present.
 */
const assertHeight = function(a, b, opt_c) {
  'use strict';
  _validateArguments(2, arguments);
  const element = nonCommentArg(1, 2, arguments);
  const height = nonCommentArg(2, 2, arguments);
  const size = goog.style.getSize(element);
  const elementHeight = size.height;

  _assert(
      commentArg(1, arguments),
      goog.testing.style.layoutasserts.isWithinThreshold_(
          height, elementHeight, 0 /* tolerance */),
      'Element should have height ' + height + '.');
};


/**
 * Asserts that the element must have the specified height within the specified
 * tolerance.
 * @param {Element|string} a The element or optionally the comment string.
 * @param {number|Element} b The height or the element if comment string is
 *     present.
 * @param {number} c The tolerance or the height if comment string is
 *     present.
 * @param {number=} opt_d The tolerance if comment string is present.
 */
const assertHeightWithinTolerance = function(a, b, c, opt_d) {
  'use strict';
  _validateArguments(3, arguments);
  const element = nonCommentArg(1, 3, arguments);
  const height = nonCommentArg(2, 3, arguments);
  const tolerance = nonCommentArg(3, 3, arguments);
  const size = goog.style.getSize(element);
  const elementHeight = size.height;

  _assert(
      commentArg(1, arguments),
      goog.testing.style.layoutasserts.isWithinThreshold_(
          height, elementHeight, tolerance),
      'Element width(' + elementHeight + ') should be within given height(' +
          height + ') with tolerance value of ' + tolerance + '.');
};


/**
 * Asserts that the first element is to the left of the second element.
 * @param {Element|string} a The first element or optionally the comment string.
 * @param {Element} b The second element or the first element if comment string
 *     is present.
 * @param {Element=} opt_c The second element if comment string is present.
 */
const assertIsLeftOf = function(a, b, opt_c) {
  'use strict';
  _validateArguments(2, arguments);
  const element = nonCommentArg(1, 2, arguments);
  const otherElement = nonCommentArg(2, 2, arguments);
  const elementRect = goog.style.getBounds(element);
  const otherElementRect = goog.style.getBounds(otherElement);

  _assert(
      commentArg(1, arguments), elementRect.left < otherElementRect.left,
      'Elements should be left to right.');
};


/**
 * Asserts that the first element is strictly left of the second element.
 * @param {Element|string} a The first element or optionally the comment string.
 * @param {Element} b The second element or the first element if comment string
 *     is present.
 * @param {Element=} opt_c The second element if comment string is present.
 */
const assertIsStrictlyLeftOf = function(a, b, opt_c) {
  'use strict';
  _validateArguments(2, arguments);
  const element = nonCommentArg(1, 2, arguments);
  const otherElement = nonCommentArg(2, 2, arguments);
  const elementRect = goog.style.getBounds(element);
  const otherElementRect = goog.style.getBounds(otherElement);

  _assert(
      commentArg(1, arguments),
      elementRect.left + elementRect.width < otherElementRect.left,
      'Elements should be strictly left to right.');
};


/**
 * Asserts that the first element is higher than the second element.
 * @param {Element|string} a The first element or optionally the comment string.
 * @param {Element} b The second element or the first element if comment string
 *     is present.
 * @param {Element=} opt_c The second element if comment string is present.
 */
const assertIsAbove = function(a, b, opt_c) {
  'use strict';
  _validateArguments(2, arguments);
  const element = nonCommentArg(1, 2, arguments);
  const otherElement = nonCommentArg(2, 2, arguments);
  const elementRect = goog.style.getBounds(element);
  const otherElementRect = goog.style.getBounds(otherElement);

  _assert(
      commentArg(1, arguments), elementRect.top < otherElementRect.top,
      'Elements should be top to bottom.');
};


/**
 * Asserts that the first element is strictly higher than the second element.
 * @param {Element|string} a The first element or optionally the comment string.
 * @param {Element} b The second element or the first element if comment string
 *     is present.
 * @param {Element=} opt_c The second element if comment string is present.
 */
const assertIsStrictlyAbove = function(a, b, opt_c) {
  'use strict';
  _validateArguments(2, arguments);
  const element = nonCommentArg(1, 2, arguments);
  const otherElement = nonCommentArg(2, 2, arguments);
  const elementRect = goog.style.getBounds(element);
  const otherElementRect = goog.style.getBounds(otherElement);

  _assert(
      commentArg(1, arguments),
      elementRect.top + elementRect.height < otherElementRect.top,
      'Elements should be strictly top to bottom.');
};


/**
 * Asserts that the first element's bounds contain the bounds of the second
 * element.
 * @param {Element|string} a The first element or optionally the comment string.
 * @param {Element} b The second element or the first element if comment string
 *     is present.
 * @param {Element=} opt_c The second element if comment string is present.
 */
const assertContained = function(a, b, opt_c) {
  'use strict';
  _validateArguments(2, arguments);
  const element = nonCommentArg(1, 2, arguments);
  const otherElement = nonCommentArg(2, 2, arguments);
  const elementRect = goog.style.getBounds(element);
  const otherElementRect = goog.style.getBounds(otherElement);

  _assert(
      commentArg(1, arguments), elementRect.contains(otherElementRect),
      'Element should be contained within the other element.');
};


/**
 * Returns true if the difference between val1 and val2 is less than or equal to
 * the threashold.
 * @param {number} val1 The first value.
 * @param {number} val2 The second value.
 * @param {number} threshold The threshold value.
 * @return {boolean} Whether or not the values are within the threshold.
 * @private
 */
goog.testing.style.layoutasserts.isWithinThreshold_ = function(
    val1, val2, threshold) {
  'use strict';
  return Math.abs(val1 - val2) <= threshold;
};
