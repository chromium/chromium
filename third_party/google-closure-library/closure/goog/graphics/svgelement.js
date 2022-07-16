/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview Thin wrappers around the DOM element returned from
 * the different draw methods of the graphics. This is the SVG implementation.
 */

goog.provide('goog.graphics.SvgEllipseElement');
goog.provide('goog.graphics.SvgGroupElement');
goog.provide('goog.graphics.SvgImageElement');
goog.provide('goog.graphics.SvgPathElement');
goog.provide('goog.graphics.SvgRectElement');
goog.provide('goog.graphics.SvgTextElement');


goog.require('goog.dom');
goog.require('goog.graphics.EllipseElement');
goog.require('goog.graphics.GroupElement');
goog.require('goog.graphics.ImageElement');
goog.require('goog.graphics.PathElement');
goog.require('goog.graphics.RectElement');
goog.require('goog.graphics.TextElement');
goog.requireType('goog.graphics.Fill');
goog.requireType('goog.graphics.Path');
goog.requireType('goog.graphics.Stroke');
goog.requireType('goog.graphics.SvgGraphics');



/**
 * Thin wrapper for SVG group elements.
 * You should not construct objects from this constructor. The graphics
 * will return the object for you.
 * @param {Element} element The DOM element to wrap.
 * @param {goog.graphics.SvgGraphics} graphics The graphics creating
 *     this element.
 * @constructor
 * @extends {goog.graphics.GroupElement}
 * @deprecated goog.graphics is deprecated. It existed to abstract over browser
 *     differences before the canvas tag was widely supported.  See
 *     http://en.wikipedia.org/wiki/Canvas_element for details.
 * @final
 */
goog.graphics.SvgGroupElement = function(element, graphics) {
  'use strict';
  goog.graphics.GroupElement.call(this, element, graphics);
};
goog.inherits(goog.graphics.SvgGroupElement, goog.graphics.GroupElement);


/**
 * Remove all drawing elements from the group.
 * @override
 */
goog.graphics.SvgGroupElement.prototype.clear = function() {
  'use strict';
  goog.dom.removeChildren(this.getElement());
};


/**
 * Set the size of the group element.
 * @param {number|string} width The width of the group element.
 * @param {number|string} height The height of the group element.
 * @override
 */
goog.graphics.SvgGroupElement.prototype.setSize = function(width, height) {
  'use strict';
  this.getGraphics().setElementAttributes(
      this.getElement(), {'width': width, 'height': height});
};



/**
 * Thin wrapper for SVG ellipse elements.
 * This is an implementation of the goog.graphics.EllipseElement interface.
 * You should not construct objects from this constructor. The graphics
 * will return the object for you.
 * @param {Element} element The DOM element to wrap.
 * @param {goog.graphics.SvgGraphics} graphics The graphics creating
 *     this element.
 * @param {goog.graphics.Stroke?} stroke The stroke to use for this element.
 * @param {goog.graphics.Fill?} fill The fill to use for this element.
 * @constructor
 * @extends {goog.graphics.EllipseElement}
 * @final
 */
goog.graphics.SvgEllipseElement = function(element, graphics, stroke, fill) {
  'use strict';
  goog.graphics.EllipseElement.call(this, element, graphics, stroke, fill);
};
goog.inherits(goog.graphics.SvgEllipseElement, goog.graphics.EllipseElement);


/**
 * Update the center point of the ellipse.
 * @param {number} cx Center X coordinate.
 * @param {number} cy Center Y coordinate.
 * @override
 */
goog.graphics.SvgEllipseElement.prototype.setCenter = function(cx, cy) {
  'use strict';
  this.getGraphics().setElementAttributes(
      this.getElement(), {'cx': cx, 'cy': cy});
};


/**
 * Update the radius of the ellipse.
 * @param {number} rx Radius length for the x-axis.
 * @param {number} ry Radius length for the y-axis.
 * @override
 */
goog.graphics.SvgEllipseElement.prototype.setRadius = function(rx, ry) {
  'use strict';
  this.getGraphics().setElementAttributes(
      this.getElement(), {'rx': rx, 'ry': ry});
};



/**
 * Thin wrapper for SVG rectangle elements.
 * This is an implementation of the goog.graphics.RectElement interface.
 * You should not construct objects from this constructor. The graphics
 * will return the object for you.
 * @param {Element} element The DOM element to wrap.
 * @param {goog.graphics.SvgGraphics} graphics The graphics creating
 *     this element.
 * @param {goog.graphics.Stroke?} stroke The stroke to use for this element.
 * @param {goog.graphics.Fill?} fill The fill to use for this element.
 * @constructor
 * @extends {goog.graphics.RectElement}
 * @final
 */
goog.graphics.SvgRectElement = function(element, graphics, stroke, fill) {
  'use strict';
  goog.graphics.RectElement.call(this, element, graphics, stroke, fill);
};
goog.inherits(goog.graphics.SvgRectElement, goog.graphics.RectElement);


/**
 * Update the position of the rectangle.
 * @param {number} x X coordinate (left).
 * @param {number} y Y coordinate (top).
 * @override
 */
goog.graphics.SvgRectElement.prototype.setPosition = function(x, y) {
  'use strict';
  this.getGraphics().setElementAttributes(this.getElement(), {'x': x, 'y': y});
};


/**
 * Update the size of the rectangle.
 * @param {number} width Width of rectangle.
 * @param {number} height Height of rectangle.
 * @override
 */
goog.graphics.SvgRectElement.prototype.setSize = function(width, height) {
  'use strict';
  this.getGraphics().setElementAttributes(
      this.getElement(), {'width': width, 'height': height});
};



/**
 * Thin wrapper for SVG path elements.
 * This is an implementation of the goog.graphics.PathElement interface.
 * You should not construct objects from this constructor. The graphics
 * will return the object for you.
 * @param {Element} element The DOM element to wrap.
 * @param {goog.graphics.SvgGraphics} graphics The graphics creating
 *     this element.
 * @param {goog.graphics.Stroke?} stroke The stroke to use for this element.
 * @param {goog.graphics.Fill?} fill The fill to use for this element.
 * @constructor
 * @extends {goog.graphics.PathElement}
 * @final
 */
goog.graphics.SvgPathElement = function(element, graphics, stroke, fill) {
  'use strict';
  goog.graphics.PathElement.call(this, element, graphics, stroke, fill);
};
goog.inherits(goog.graphics.SvgPathElement, goog.graphics.PathElement);


/**
 * Update the underlying path.
 * @param {!goog.graphics.Path} path The path object to draw.
 * @override
 * @suppress {missingRequire} goog.graphics.SvgGraphics
 */
goog.graphics.SvgPathElement.prototype.setPath = function(path) {
  'use strict';
  this.getGraphics().setElementAttributes(
      this.getElement(), {'d': goog.graphics.SvgGraphics.getSvgPath(path)});
};



/**
 * Thin wrapper for SVG text elements.
 * This is an implementation of the goog.graphics.TextElement interface.
 * You should not construct objects from this constructor. The graphics
 * will return the object for you.
 * @param {Element} element The DOM element to wrap.
 * @param {goog.graphics.SvgGraphics} graphics The graphics creating
 *     this element.
 * @param {goog.graphics.Stroke?} stroke The stroke to use for this element.
 * @param {goog.graphics.Fill?} fill The fill to use for this element.
 * @constructor
 * @extends {goog.graphics.TextElement}
 * @final
 */
goog.graphics.SvgTextElement = function(element, graphics, stroke, fill) {
  'use strict';
  goog.graphics.TextElement.call(this, element, graphics, stroke, fill);
};
goog.inherits(goog.graphics.SvgTextElement, goog.graphics.TextElement);


/**
 * Update the displayed text of the element.
 * @param {string} text The text to draw.
 * @override
 */
goog.graphics.SvgTextElement.prototype.setText = function(text) {
  'use strict';
  // This is actually SVGTextElement but we don't have it in externs.
  /** @type {!Text} */ (this.getElement().firstChild).data = text;
};



/**
 * Thin wrapper for SVG image elements.
 * This is an implementation of the goog.graphics.ImageElement interface.
 * You should not construct objects from this constructor. The graphics
 * will return the object for you.
 * @param {Element} element The DOM element to wrap.
 * @param {goog.graphics.SvgGraphics} graphics The graphics creating
 *     this element.
 * @constructor
 * @extends {goog.graphics.ImageElement}
 * @final
 */
goog.graphics.SvgImageElement = function(element, graphics) {
  'use strict';
  goog.graphics.ImageElement.call(this, element, graphics);
};
goog.inherits(goog.graphics.SvgImageElement, goog.graphics.ImageElement);


/**
 * Update the position of the image.
 * @param {number} x X coordinate (left).
 * @param {number} y Y coordinate (top).
 * @override
 */
goog.graphics.SvgImageElement.prototype.setPosition = function(x, y) {
  'use strict';
  this.getGraphics().setElementAttributes(this.getElement(), {'x': x, 'y': y});
};


/**
 * Update the size of the image.
 * @param {number} width Width of image.
 * @param {number} height Height of image.
 * @override
 */
goog.graphics.SvgImageElement.prototype.setSize = function(width, height) {
  'use strict';
  this.getGraphics().setElementAttributes(
      this.getElement(), {'width': width, 'height': height});
};


/**
 * Update the source of the image.
 * @param {string} src Source of the image.
 * @override
 */
goog.graphics.SvgImageElement.prototype.setSource = function(src) {
  'use strict';
  this.getGraphics().setElementAttributes(
      this.getElement(), {'xlink:href': src});
};
