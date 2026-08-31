// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/******** Math Utilities ********/

/** Constrains a value between a lower and upper bound. */
function clip(lo, v, hi) {
  return Math.max(lo, Math.min(v, hi));
}

/******** Point2D ********/
/**
 * Represents a 2D vector or point using an Array backend for performance.
 * Base class for specialized coordinate types like PointXY and Dims2D.
 */
class Point2D extends Array {
  /**
   * @param {number} v0 The first component (e.g., X or U).
   * @param {number} v1 The second component (e.g., Y or V).
   */
  constructor(v0, v1) {
    super(v0, v1);
  }

  /**
   * Updates both components in-place.
   * @return {!Point2D} This point for chaining.
   */
  assign(v0, v1) {
    this[0] = v0;
    this[1] = v1;
    return this;
  }

  // clang-format off
  /** @return {number} Dot product with another point. */
  dot(o) { return this[0] * o[0] + this[1] * o[1]; }

  /** @return {number} Dot product with literal X/Y components. */
  dotxy(x, y) { return this[0] * x + this[1] * y; }
  // clang-format on
}

/******** PointXY ********/
/**
 * Spatial coordinates X / Y, typically for global World or Screen domains.
 */
class PointXY extends Point2D {
  // clang-format off
  get x() { return this[0]; }
  get y() { return this[1]; }

  set x(t) { this[0] = t; }
  set y(t) { this[1] = t; }
  // clang-format on
}

/******** Dims2D ********/
/**
 * Sizing quantity {w, h}, should have non-negative values.
 */
class Dims2D extends Point2D {
  // clang-format off
  /**
   * @param {Point2D} axisAlignedVect
   * @return {number} Size projected along an axis vector (non-negative).
   */
  sizeAlong(axisAlignedVect) {
    return Math.abs(this.dot(axisAlignedVect));
  }

  get w() { return this[0]; }
  get h() { return this[1]; }

  set w(t) { this[0] = t; }
  set h(t) { this[1] = t; }
  // clang-format on
}
