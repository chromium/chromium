/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A collection of CSS3 targeted animation, based on
 * `goog.fx.css3.Transition`.
 */

goog.provide('goog.fx.css3');

goog.require('goog.fx.css3.Transition');


/**
 * Creates a transition to fade the element.
 * @param {Element} element The element to fade.
 * @param {number} duration Duration in seconds.
 * @param {string} timing The CSS3 timing function.
 * @param {number} startOpacity Starting opacity.
 * @param {number} endOpacity Ending opacity.
 * @return {!goog.fx.css3.Transition} The transition object.
 */
goog.fx.css3.fade = function(
    element, duration, timing, startOpacity, endOpacity) {
  'use strict';
  return new goog.fx.css3.Transition(
      element, duration, {'opacity': startOpacity}, {'opacity': endOpacity},
      {property: 'opacity', duration: duration, timing: timing, delay: 0});
};


/**
 * Creates a transition to fade in the element.
 * @param {Element} element The element to fade in.
 * @param {number} duration Duration in seconds.
 * @return {!goog.fx.css3.Transition} The transition object.
 */
goog.fx.css3.fadeIn = function(element, duration) {
  'use strict';
  return goog.fx.css3.fade(element, duration, 'ease-out', 0, 1);
};


/**
 * Creates a transition to fade out the element.
 * @param {Element} element The element to fade out.
 * @param {number} duration Duration in seconds.
 * @return {!goog.fx.css3.Transition} The transition object.
 */
goog.fx.css3.fadeOut = function(element, duration) {
  'use strict';
  return goog.fx.css3.fade(element, duration, 'ease-in', 1, 0);
};
