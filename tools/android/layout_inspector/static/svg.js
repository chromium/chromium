// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/******** Constants ********/

const SVG_NS = 'http://www.w3.org/2000/svg';

/******** SvgEditor ********/
/**
 * A fluent, builder-style interface for chaining SVG element creation and
 * attribute mutation. Automatically handles namespaces and CSS mappings.
 */
class SvgEditor {
  constructor() {
    this.cur = null;
  }

  get() {
    return this.cur;
  }

  edit(el) {
    this.cur = el;
    return this;
  }

  init(tag) {
    return this.edit(document.createElementNS(SVG_NS, tag));
  }

  attr(name, value) {
    if (value == null) {
      this.cur.removeAttribute(name);
    } else {
      this.cur.setAttribute(name, value);
    }
    return this;
  }

  append(child) {
    this.cur.appendChild(child);
    return this;
  }

  /**
   * Given integer rectangle pixel (boundary) coordinates, updates <path>
   * (assumed selected) to render an inner rectangle with sharp boundaries (if
   * 1px thick), degrading to a 1px line for thin rectangles and an empty path
   * for zero-area rectangles.
   * @param {number} iLeft
   * @param {number} iTop
   * @param {number} iRight
   * @param {number} iBottom
   * @return {!SvgEditor}
   */
  pathInnerRect(iLeft, iTop, iRight, iBottom) {
    const w = iRight - iLeft;
    const h = iBottom - iTop;
    let d = '';
    if (w >= 2 && h >= 2) {
      d = `M${iLeft + 0.5} ${iTop + 0.5}` +
          ` H${iRight - 0.5} V${iBottom - 0.5} H${iLeft + 0.5} Z`;
    } else if (w >= 1 && h >= 0 && h <= 1) {
      // Degenerate horizontal line (or 1x1 pixel when w === 1).
      d = `M${iLeft} ${iTop + 0.5} H${iRight}`;
    } else if (h >= 1 && w >= 0 && w <= 1) {
      // Degenerate vertical line.
      d = `M${iLeft + 0.5} ${iTop} V${iBottom}`;
    }
    return this.attr('d', d);
  }

  /**
   * Sets the 'viewBox' attribute from width and height dimensions with origin
   * (0, 0).
   * @param {number} w
   * @param {number} h
   * @return {!SvgEditor}
   */
  viewBoxDims(w, h) {
    return this.attr('viewBox', `0 0 ${w} ${h}`);
  }

  static extend(...attrs) {
    attrs.forEach(attr => {
      const methodName = attr.replace(/-([a-z])/g, g => g[1].toUpperCase());
      if (this.prototype[methodName]) throw new Error('Name collision!');
      this.prototype[methodName] = function(value) {
        return this.attr(attr, value);
      };
    });
  }
}

// clang-format off
SvgEditor.extend(
    'x', 'y', 'width', 'height', 'fill', 'stroke', 'stroke-width',
    'stroke-dasharray', 'visibility', 'display', 'viewBox'
  );
// clang-format on

/**
 * @type {!SvgEditor} Global instance for conciseness and churn reduction.
 */
const g_svg = new SvgEditor();
