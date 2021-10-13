/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview Graphics utility functions and factory methods.
 * @see ../demos/graphics/advancedcoordinates.html
 * @see ../demos/graphics/advancedcoordinates2.html
 * @see ../demos/graphics/basicelements.html
 * @see ../demos/graphics/events.html
 * @see ../demos/graphics/modifyelements.html
 * @see ../demos/graphics/tiger.html
 */


goog.provide('goog.graphics');

goog.require('goog.dom');
goog.require('goog.graphics.CanvasGraphics');
goog.require('goog.graphics.SvgGraphics');
goog.require('goog.graphics.VmlGraphics');
goog.require('goog.userAgent');
goog.requireType('goog.graphics.AbstractGraphics');


/**
 * Returns an instance of goog.graphics.AbstractGraphics that knows how to draw
 * for the current platform (A factory for the proper Graphics implementation)
 * @param {string|number} width The width in pixels.  Strings
 *     expressing percentages of parent with (e.g. '80%') are also accepted.
 * @param {string|number} height The height in pixels.  Strings
 *     expressing percentages of parent with (e.g. '80%') are also accepted.
 * @param {?number=} opt_coordWidth The optional coordinate width - if
 *     omitted or null, defaults to same as width.
 * @param {?number=} opt_coordHeight The optional coordinate height - if
 *     omitted or null, defaults to same as height.
 * @param {goog.dom.DomHelper=} opt_domHelper The DOM helper object for the
 *     document we want to render in.
 * @return {!goog.graphics.AbstractGraphics} The created instance.
 * @deprecated goog.graphics is deprecated. It existed to abstract over browser
 *     differences before the canvas tag was widely supported.  See
 *     http://en.wikipedia.org/wiki/Canvas_element for details.
 */
goog.graphics.createGraphics = function(
    width, height, opt_coordWidth, opt_coordHeight, opt_domHelper) {
  'use strict';
  var graphics;
  // On IE9 and above, SVG is available, except in compatibility mode.
  // We check createElementNS on document object that is not exist in
  // compatibility mode.
  if (goog.userAgent.IE && (!goog.userAgent.isVersionOrHigher('9') ||
                            !(opt_domHelper || goog.dom.getDomHelper())
                                 .getDocument()
                                 .createElementNS)) {
    graphics = new goog.graphics.VmlGraphics(
        width, height, opt_coordWidth, opt_coordHeight, opt_domHelper);
  } else if (goog.userAgent.WEBKIT && goog.userAgent.MOBILE) {
    graphics = new goog.graphics.CanvasGraphics(
        width, height, opt_coordWidth, opt_coordHeight, opt_domHelper);
  } else {
    graphics = new goog.graphics.SvgGraphics(
        width, height, opt_coordWidth, opt_coordHeight, opt_domHelper);
  }

  // Create the dom now, because all drawing methods require that the
  // main dom element (the canvas) has been already created.
  graphics.createDom();

  return graphics;
};


/**
 * Returns an instance of goog.graphics.AbstractGraphics that knows how to draw
 * for the current platform (A factory for the proper Graphics implementation)
 * @param {string|number} width The width in pixels.  Strings
 *     expressing percentages of parent with (e.g. '80%') are also accepted.
 * @param {string|number} height The height in pixels.   Strings
 *     expressing percentages of parent with (e.g. '80%') are also accepted.
 * @param {?number=} opt_coordWidth The optional coordinate width, defaults to
 *     same as width.
 * @param {?number=} opt_coordHeight The optional coordinate height, defaults to
 *     same as height.
 * @param {goog.dom.DomHelper=} opt_domHelper The DOM helper object for the
 *     document we want to render in.
 * @return {!goog.graphics.AbstractGraphics} The created instance.
 * @deprecated goog.graphics is deprecated. It existed to abstract over browser
 *     differences before the canvas tag was widely supported.  See
 *     http://en.wikipedia.org/wiki/Canvas_element for details.
 */
goog.graphics.createSimpleGraphics = function(
    width, height, opt_coordWidth, opt_coordHeight, opt_domHelper) {
  'use strict';
  // Otherwise, defer to normal graphics object creation.
  return goog.graphics.createGraphics(
      width, height, opt_coordWidth, opt_coordHeight, opt_domHelper);
};


/**
 * Static function to check if the current browser has Graphics support.
 * @return {boolean} True if the current browser has Graphics support.
 * @deprecated goog.graphics is deprecated. It existed to abstract over browser
 *     differences before the canvas tag was widely supported.  See
 *     http://en.wikipedia.org/wiki/Canvas_element for details.
 */
goog.graphics.isBrowserSupported = function() {
  'use strict';
  return true;
};
