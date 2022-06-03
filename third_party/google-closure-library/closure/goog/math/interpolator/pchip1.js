/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A one dimensional monotone cubic spline interpolator.
 *
 * See http://en.wikipedia.org/wiki/Monotone_cubic_interpolation.
 */

goog.provide('goog.math.interpolator.Pchip1');

goog.require('goog.math');
goog.require('goog.math.interpolator.Spline1');



/**
 * A one dimensional monotone cubic spline interpolator.
 * @extends {goog.math.interpolator.Spline1}
 * @constructor
 * @final
 */
goog.math.interpolator.Pchip1 = function() {
  'use strict';
  goog.math.interpolator.Pchip1.base(this, 'constructor');
};
goog.inherits(goog.math.interpolator.Pchip1, goog.math.interpolator.Spline1);


/** @override */
goog.math.interpolator.Pchip1.prototype.computeDerivatives = function(
    dx, slope) {
  'use strict';
  const len = dx.length;
  const deriv = new Array(len + 1);
  for (let i = 1; i < len; ++i) {
    if (goog.math.sign(slope[i - 1]) * goog.math.sign(slope[i]) <= 0) {
      deriv[i] = 0;
    } else {
      const w1 = 2 * dx[i] + dx[i - 1];
      const w2 = dx[i] + 2 * dx[i - 1];
      deriv[i] = (w1 + w2) / (w1 / slope[i - 1] + w2 / slope[i]);
    }
  }
  deriv[0] =
      this.computeDerivativeAtBoundary_(dx[0], dx[1], slope[0], slope[1]);
  deriv[len] = this.computeDerivativeAtBoundary_(
      dx[len - 1], dx[len - 2], slope[len - 1], slope[len - 2]);
  return deriv;
};


/**
 * Computes the derivative of a data point at a boundary.
 * @param {number} dx0 The spacing of the 1st data point.
 * @param {number} dx1 The spacing of the 2nd data point.
 * @param {number} slope0 The slope of the 1st data point.
 * @param {number} slope1 The slope of the 2nd data point.
 * @return {number} The derivative at the 1st data point.
 * @private
 */
goog.math.interpolator.Pchip1.prototype.computeDerivativeAtBoundary_ = function(
    dx0, dx1, slope0, slope1) {
  'use strict';
  let deriv = ((2 * dx0 + dx1) * slope0 - dx0 * slope1) / (dx0 + dx1);
  if (goog.math.sign(deriv) != goog.math.sign(slope0)) {
    deriv = 0;
  } else if (
      goog.math.sign(slope0) != goog.math.sign(slope1) &&
      Math.abs(deriv) > Math.abs(3 * slope0)) {
    deriv = 3 * slope0;
  }
  return deriv;
};
