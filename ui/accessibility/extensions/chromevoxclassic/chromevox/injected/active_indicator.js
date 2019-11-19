// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Draws and animates the graphical indicator around the active
 *    object or text range, and handles animation when the indicator is moving.
 */


goog.provide('cvox.ActiveIndicator');

goog.require('cvox.Cursor');
goog.require('cvox.DomUtil');


/**
 * Constructs and ActiveIndicator, a glowing outline around whatever
 * node or text range is currently active. Initially it won't display
 * anything; call syncToNode, syncToRange, or syncToCursorSelection to
 * make it animate and move. It only displays when this window/iframe
 * has focus.
 *
 * @constructor
 */
cvox.ActiveIndicator = function() {
  /**
   * The time when the indicator was most recently moved.
   * @type {number}
   * @private
   */
  this.lastMoveTime_ = 0;

  /**
   * An estimate of the current zoom factor of the webpage. This is
   * needed in order to accurately line up the different pieces of the
   * indicator border and avoid rounding errors.
   * @type {number}
   * @private
   */
  this.zoom_ = 1;

  /**
   * The parent element of the indicator.
   * @type {?Element}
   * @private
   */
  this.container_ = null;

  /**
   * The current indicator rects.
   * @type {Array<ClientRect>}
   * @private
   */
  this.rects_ = null;

  /**
   * The most recent target of a call to syncToNode, syncToRange, or
   * syncToCursorSelection.
   * @type {Array<Node>|Range}
   * @private
   */
  this.lastSyncTarget_ = null;

  /**
   * The most recent client rects for the active indicator, so we
   * can tell when it moved.
   * @type {ClientRectList|Array<ClientRect>}
   * @private
   */
  this.lastClientRects_ = null;

  /**
   * The id from window.setTimeout when updating the indicator if needed.
   * @type {?number}
   * @private
   */
  this.updateIndicatorTimeoutId_ = null;

  /**
   * True if this window is blurred and we shouldn't show the indicator.
   * @type {boolean}
   * @private
   */
  this.blurred_ = false;

  /**
   * A cached value of window height.
   * @type {number|undefined}
   * @private
   */
  this.innerHeight_;

  /**
   * A cached value of window width.
   * @type {number|undefined}
   * @private
   */
  this.innerWidth_;

  // Hide the indicator when the window doesn't have focus.
  window.addEventListener('focus', goog.bind(function() {
    this.blurred_ = false;
    if (this.container_) {
      this.container_.classList.remove('cvox_indicator_window_not_focused');
    }
  }, this), false);
  window.addEventListener('blur', goog.bind(function() {
    this.blurred_ = true;
    if (this.container_) {
      this.container_.classList.add('cvox_indicator_window_not_focused');
    }
  }, this), false);
};

/**
 * CSS for the active indicator. The basic hierarchy looks like this:
 *
 * container (pulsing) (animate_normal, animate_quick)
 *   region (visible)
 *     top
 *     middle_nw
 *     middle_ne
 *     middle_sw
 *     middle_se
 *     bottom
 *   region (visible)
 *     top
 *     middle_nw
 *     middle_ne
 *     middle_sw
 *     middle_se
 *     bottom
 *
 * @type {string}
 * @const
 */
cvox.ActiveIndicator.STYLE =
    '.cvox_indicator_container {' +
    '  position: absolute !important;' +
    '  left: 0 !important;' +
    '  top: 0 !important;' +
    '  z-index: 2147483647 !important;' +
    '  pointer-events: none !important;' +
    '  margin: 0px !important;' +
    '  padding: 0px !important;' +
    '}' +
    '.cvox_indicator_window_not_focused {' +
    '  visibility: hidden !important;' +
    '}' +
    '.cvox_indicator_pulsing {' +
    '  -webkit-animation: ' +
    // NOTE(deboer): This animation is 0 seconds long to work around
    // http://crbug.com/128993.  Revert it to 2s when the bug is fixed.
    '      cvox_indicator_pulsing_animation 0s 2 alternate !important;' +
    '  -webkit-animation-timing-function: ease-in-out !important;' +
    '}' +
    '.cvox_indicator_region {' +
    '  opacity: 0 !important;' +
    '  -webkit-transition: opacity 1s !important;' +
    '}' +
    '.cvox_indicator_visible {' +
    '  opacity: 1 !important;' +
    '}' +
    '.cvox_indicator_container .cvox_indicator_region * {' +
    '  position:absolute !important;' +
    '  box-shadow: 0 0 4px 4px #f7983a !important;' +
    '  border-radius: 6px !important;' +
    '  margin: 0px !important;' +
    '  padding: 0px !important;' +
    '  -webkit-transition: none !important;' +
    '}' +
    '.cvox_indicator_animate_normal .cvox_indicator_region * {' +
    '  -webkit-transition: all 0.3s !important;' +
    '}' +
    '.cvox_indicator_animate_quick .cvox_indicator_region * {' +
    '  -webkit-transition: all 0.1s !important;' +
    '}' +
    '.cvox_indicator_top {' +
    '  border-radius: inherit inherit 0 0 !important;' +
    '}' +
    '.cvox_indicator_middle_nw {' +
    '  border-radius: inherit 0 0 0 !important;' +
    '}' +
    '.cvox_indicator_middle_ne {' +
    '  border-radius: 0 inherit 0 0 !important;' +
    '}' +
    '.cvox_indicator_middle_se {' +
    '  border-radius: 0 0 inherit 0 !important;' +
    '}' +
    '.cvox_indicator_middle_sw {' +
    '  border-radius: 0 0 0 inherit !important;' +
    '}' +
    '.cvox_indicator_bottom {' +
    '  border-radius: 0 0 inherit inherit !important;' +
    '}' +
    '@-webkit-keyframes cvox_indicator_pulsing_animation {' +
    '   0% {opacity: 1.0}' +
    '  50% {opacity: 0.5}' +
    ' 100% {opacity: 1.0}' +
    '}';

/**
 * The minimum number of milliseconds that must have elapsed
 * since the last navigation for a quick animation to be allowed.
 * @type {number}
 * @const
 */
cvox.ActiveIndicator.QUICK_ANIM_DELAY_MS = 100;

/**
 * The minimum number of milliseconds that must have elapsed
 * since the last navigation for a normal (slower) animation
 * to be allowed.
 * @type {number}
 * @const
 */
cvox.ActiveIndicator.NORMAL_ANIM_DELAY_MS = 300;

/**
 * Margin between the active object's rect and the indicator border.
 * @type {number}
 * @const
 */
cvox.ActiveIndicator.MARGIN = 8;

/**
 * Remove the indicator from the DOM.
 */
cvox.ActiveIndicator.prototype.removeFromDom = function() {
  if (this.container_ && this.container_.parentElement) {
    this.container_.parentElement.removeChild(this.container_);
  }
};

/**
 * Move the indicator to surround the given node.
 * @param {Node} node The new target of the indicator.
 */
cvox.ActiveIndicator.prototype.syncToNode = function(node) {
  if (!node) {
    return;
  }
  // In the navigation manager, and specifically the node walkers, focusing
  // on the body means we are before the beginning of the document.  In
  // that case, we simply hide the active indicator.
  if (node == document.body) {
    this.removeFromDom();
    return;
  }
  this.syncToNodes([node]);
};

/**
 * Move the indicator to surround the given nodes.
 * @param {Array<Node>} nodes The new targets of the indicator.
 */
cvox.ActiveIndicator.prototype.syncToNodes = function(nodes) {
  var clientRects = this.clientRectsFromNodes_(nodes);
  this.moveIndicator_(clientRects, cvox.ActiveIndicator.MARGIN);
  this.lastSyncTarget_ = nodes;
  this.lastClientRects_ = clientRects;
  if (this.updateIndicatorTimeoutId_ != null) {
    window.clearTimeout(this.updateIndicatorTimeoutId_);
    this.updateIndicatorTimeoutId_ = null;
  }
};

/**
 * Move the indicator to surround the given range.
 * @param {Range} range The range.
 */
cvox.ActiveIndicator.prototype.syncToRange = function(range) {
  var margin = cvox.ActiveIndicator.MARGIN;
  if (range.startContainer == range.endContainer &&
      range.startOffset + 1 == range.endOffset) {
    margin = 1;
  }

  var clientRects = range.getClientRects();
  this.moveIndicator_(clientRects, margin);
  this.lastSyncTarget_ = range;
  this.lastClientRects_ = clientRects;
  if (this.updateIndicatorTimeoutId_ != null) {
    window.clearTimeout(this.updateIndicatorTimeoutId_);
    this.updateIndicatorTimeoutId_ = null;
  }
};

/**
 * Move the indicator to surround the given cursor range.
 * @param {!cvox.CursorSelection} sel The start cursor position.
 */
cvox.ActiveIndicator.prototype.syncToCursorSelection = function(sel) {
  if (sel.start.node == sel.end.node && sel.start.index == sel.end.index) {
    this.syncToNode(sel.start.node);
  } else {
    var range = document.createRange();
    range.setStart(sel.start.node, sel.start.index);
    range.setEnd(sel.end.node, sel.end.index);
    this.syncToRange(range);
  }
};

/**
 * Called when we should check to see if the indicator target has moved.
 * Schedule it after a short delay so that we don't waste a lot of time
 * updating.
 */
cvox.ActiveIndicator.prototype.updateIndicatorIfChanged = function() {
  if (this.updateIndicatorTimeoutId_) {
    return;
  }
  this.updateIndicatorTimeoutId_ = window.setTimeout(goog.bind(function() {
    this.handleUpdateIndicatorIfChanged_();
  }, this), 100);
};

/**
 * Called when we should check to see if the indicator target has moved.
 * Schedule it after a short delay so that we don't waste a lot of time
 * updating.
 * @private
 */
cvox.ActiveIndicator.prototype.handleUpdateIndicatorIfChanged_ = function() {
  this.updateIndicatorTimeoutId_ = null;
  if (!this.lastSyncTarget_) {
    return;
  }

  var newClientRects;
  if (this.lastSyncTarget_ instanceof Array) {
    newClientRects = this.clientRectsFromNodes_(this.lastSyncTarget_);
  } else {
    newClientRects = this.lastSyncTarget_.getClientRects();
  }
  if (!newClientRects || newClientRects.length == 0) {
    this.syncToNode(document.body);
    return;
  }

  var needsUpdate = false;
  if (newClientRects.length != this.lastClientRects_.length) {
    needsUpdate = true;
  } else {
    for (var i = 0; i < this.lastClientRects_.length; ++i) {
      var last = this.lastClientRects_[i];
      var current = newClientRects[i];
      if (last.top != current.top ||
          last.right != current.right ||
          last.bottom != current.bottom ||
          last.left != last.left) {
        needsUpdate = true;
        break;
      }
    }
  }
  if (needsUpdate) {
    this.moveIndicator_(newClientRects, cvox.ActiveIndicator.MARGIN);
    this.lastClientRects_ = newClientRects;
  }
};

/**
 * @param {Array<Node>} nodes An array of nodes.
 * @return {Array<ClientRect>} An array of client rects corresponding to
 *     those nodes.
 * @private
 */
cvox.ActiveIndicator.prototype.clientRectsFromNodes_ = function(nodes) {
  var clientRects = [];
  for (var i = 0; i < nodes.length; ++i) {
    var node = nodes[i];
    if (node.constructor == Text) {
      var range = document.createRange();
      range.selectNode(node);
      var rangeRects = range.getClientRects();
      for (var j = 0; j < rangeRects.length; ++j)
        clientRects.push(rangeRects[j]);
    } else {
      while (node && !node.getClientRects) {
        node = node.parentElement;
      }
      if (!node) {
        return [];
      }
      var nodeRects = node.getClientRects();
      for (var j = 0; j < nodeRects.length; ++j)
        clientRects.push(nodeRects[j]);
    }
  }
  return clientRects;
};

/**
 * Move the indicator from its current location, if any, to surround
 * the given set of rectanges.
 *
 * The rectangles need not be contiguous - they're automatically
 * grouped into contiguous regions. The first region is "primary" - it
 * gets animated smoothly from the previous location to the new location.
 * Any other region (like, for example, a text range
 * that continues on a second column) gets a temporary outline that
 * disappears as soon as the indicator moves again.
 *
 * A single region does not have to be rectangular - a region outline
 * is designed to handle the slightly non-rectangular shape of a typical
 * text paragraph, but not anything more complicated than that.
 *
 * @param {ClientRectList|Array<ClientRect>} immutableRects The object
 *     rectangles.
 * @param {number} margin Margin in pixels.
 * @private
 */
cvox.ActiveIndicator.prototype.moveIndicator_ = function(
    immutableRects, margin) {
  // Never put the active indicator into the DOM when the whole page is
  // contentEditable; it will end up part of content that the user may
  // be trying to edit.
  if (document.body.isContentEditable) {
    this.removeFromDom();
    return;
  }

  var n = immutableRects.length;
  if (n == 0) {
    return;
  }

  // Offset the rects by documentElement, body, and/or scroll offsets,
  // while copying them into a new mutable array.
  var offsetX;
  var offsetY;
  if (window.getComputedStyle(document.body, null).position != 'static') {
    offsetX = -document.body.getBoundingClientRect().left;
    offsetY = -document.body.getBoundingClientRect().top;
  } else if (window.getComputedStyle(document.documentElement, null).position !=
                 'static') {
    offsetX = -document.documentElement.getBoundingClientRect().left;
    offsetY = -document.documentElement.getBoundingClientRect().top;
  } else {
    offsetX = window.pageXOffset;
    offsetY = window.pageYOffset;
  }

  var rects = [];
  for (var i = 0; i < n; i++) {
    rects.push(
        this.inset_(immutableRects[i], offsetX, offsetY, -offsetX, -offsetY));
  }

  // Create and attach the container if it doesn't exist or if it was detached.
  if (!this.container_ || !this.container_.parentElement) {
    // In case there are any detached containers around, clean them up. One case
    // that requires clean up like this is when users download a file on Chrome
    // on Android.
    var oldContainers =
        document.getElementsByClassName('cvox_indicator_container');
    for (var j = 0, oldContainer; oldContainer = oldContainers[j]; j++) {
      if (oldContainer.parentNode) {
        oldContainer.parentNode.removeChild(oldContainer);
      }
    }
    this.container_ = this.createDiv_(
        document.body, 'cvox_indicator_container', document.body.firstChild);
  }

  // Add the CSS style to the page if it's not already there.
  var style = document.createElement('style');
  style.id = 'cvox_indicator_style';
  style.innerHTML = cvox.ActiveIndicator.STYLE;
  cvox.DomUtil.addNodeToHead(style, style.id);

  // Decide on the animation speed. By default we do a medium-speed
  // animation between the previous and new location. If the user is
  // moving rapidly, we do a fast animation, or no animation.
  var now = new Date().getTime();
  var delta = now - this.lastMoveTime_;
  this.container_.className = 'cvox_indicator_container';
  if (!cvox.ChromeVox.documentHasFocus() || this.blurred_) {
    this.container_.classList.add('cvox_indicator_window_not_focused');
  }
  if (delta > cvox.ActiveIndicator.NORMAL_ANIM_DELAY_MS) {
    this.container_.classList.add('cvox_indicator_animate_normal');
  } else if (delta > cvox.ActiveIndicator.QUICK_ANIM_DELAY_MS) {
    this.container_.classList.add('cvox_indicator_animate_quick');
  }
  this.lastMoveTime_ = now;

  // Compute the zoom level of the browser - this is needed to avoid
  // roundoff errors when placing the various pieces of the region
  // outline.
  this.computeZoomLevel_();

  // Make it start pulsing after it's drawn the first frame - this is so
  // that the opacity is always 100% when the indicator appears, and only
  // starts pulsing afterwards.
  window.setTimeout(goog.bind(function() {
    this.container_.classList.add('cvox_indicator_pulsing');
  }, this), 0);

  // If there was more than one region previously, delete all except
  // the first one.
  while (this.container_.childElementCount > 1) {
    this.container_.removeChild(this.container_.lastElementChild);
  }

  // Split the rects into contiguous regions.
  var regions = [[rects[0]]];
  var regionRects = [rects[0]];
  for (i = 1; i < rects.length; i++) {
    var found = false;
    for (var j = 0; j < regions.length && !found; j++) {
      if (this.intersects_(rects[i], regionRects[j])) {
        regions[j].push(rects[i]);
        regionRects[j] = this.union_(regionRects[j], rects[i]);
        found = true;
      }
    }
    if (!found) {
      regions.push([rects[i]]);
      regionRects.push(rects[i]);
    }
  }

  // Keep merging regions that intersect.
  // TODO(dmazzoni): reduce the worst-case complexity! This appears like
  // it could be O(n^3), make sure it's not in practice.
  do {
    var merged = false;
    for (i = 0; i < regions.length - 1 && !merged; i++) {
      for (j = i + 1; j < regions.length && !merged; j++) {
        if (this.intersects_(regionRects[i], regionRects[j])) {
          regions[i] = regions[i].concat(regions[j]);
          regionRects[i] = this.union_(regionRects[i], regionRects[j]);
          regions.splice(j, 1);
          regionRects.splice(j, 1);
          merged = true;
        }
      }
    }
  } while (merged);

  // Sort rects within each region by y and then x position.
  for (i = 0; i < regions.length; i++) {
    regions[i].sort(function(r1, r2) {
      if (r1.top != r2.top) {
        return r1.top - r2.top;
      } else {
        return r1.left - r2.left;
      }
    });
  }

  // Draw each indicator region. The first region attempts to re-use the
  // existing elements (which results in animating the transition).
  for (i = 0; i < regions.length; i++) {
    var parent = null;
    if (i == 0 &&
        this.container_.childElementCount == 1 &&
        this.container_.children[0].childElementCount == 6) {
      parent = this.container_.children[0];
    }
    this.updateIndicatorRegion_(regions[i], parent, margin);
  }
};

/**
 * Update one indicator region - a set of contiguous rectangles on the
 * page.
 *
 * A region is made up of six pieces, designed to handle the shape of a
 * typical text paragraph:
 *
 *              TOP TOP TOP
 *              TOP     TOP
 *  NW NW NW NW NW      NE NE NE NE NE NE NE NE NE
 *  NW                                          NE
 *  NW                                          NE
 *  SW                                          SE
 *  SW                                          SE
 *  SW SW BOTTOM                      BOTTOM SE SE
 *        BOTTOM                      BOTTOM
 *        BOTTOM BOTTOM BOTTOM BOTTOM BOTTOM
 *
 * When there's only a single rectangle - like when outlining something
 * simple like a button, all six pieces are still used - this makes the
 * animation smooth when sliding from a paragraph to a rectangular object
 * and then to another paragraph, for example:
 *
 *       TOP TOP TOP TOP TOP TOP TOP
 *       TOP                     TOP
 *       NW                       NE
 *       NW                       NE
 *       SW                       SE
 *       SW                       SE
 *       BOTTOM               BOTTOM
 *       BOTTOM BOTTOM BOTTOM BOTTOM
 *
 * Each piece is just a div that uses CSS to absolutely position itself.
 * The outline effect is done using the 'box-shadow' property around the
 * whole box, with the 'clip' property used to make sure that only 2 - 3
 * sides of the box are actually shown.
 *
 * This code is very subtle! If you want to adjust something by a few
 * pixels, be prepared to do LOTS of testing!
 *
 * Tip: while debugging, comment out the clipping and make each rectangle
 * a different color. That will make it much easier to see where each piece
 * starts and ends.
 *
 * @param {Array<ClientRect>} rects The list of rects in the region.
 *     These should already be sorted (top to bottom and left to right).
 * @param {?Element} parent If present, try to reuse the existing element
 *     (and animate the transition).
 * @param {number} margin Margin in pixels.
 * @private
 */
cvox.ActiveIndicator.prototype.updateIndicatorRegion_ = function(
    rects, parent, margin) {
  if (parent) {
    // Reuse the existing element (so we animate to the new location).
    var regionTop = parent.children[0];
    var regionMiddleNW = parent.children[1];
    var regionMiddleNE = parent.children[2];
    var regionMiddleSW = parent.children[3];
    var regionMiddleSE = parent.children[4];
    var regionBottom = parent.children[5];
  } else {
    // Create a new region (when the indicator first appears, or when
    // this is a secondary region, like for text continuing on a second
    // column).
    parent = this.createDiv_(this.container_, 'cvox_indicator_region');
    window.setTimeout(function() {
      parent.classList.add('cvox_indicator_visible');
    }, 0);
    regionTop = this.createDiv_(parent, 'cvox_indicator_top');
    regionMiddleNW = this.createDiv_(parent, 'cvox_indicator_middle_nw');
    regionMiddleNE = this.createDiv_(parent, 'cvox_indicator_middle_ne');
    regionMiddleSW = this.createDiv_(parent, 'cvox_indicator_middle_sw');
    regionMiddleSE = this.createDiv_(parent, 'cvox_indicator_middle_se');
    regionBottom = this.createDiv_(parent, 'cvox_indicator_bottom');
  }

  // Grab all of the rectangles in the top row.
  var topRect = rects[0];
  var topMiddle = Math.floor((topRect.top + topRect.bottom) / 2);
  var topIndex = 1;
  var n = rects.length;
  while (topIndex < n && rects[topIndex].top < topMiddle) {
    topRect = this.union_(topRect, rects[topIndex]);
    topMiddle = Math.floor((topRect.top + topRect.bottom) / 2);
    topIndex++;
  }

  if (topIndex == n) {
    // Everything fits on one line, so use special case code to form
    // the region into a rectangle.
    var r = this.inset_(topRect, -margin, -margin, -margin, -margin);
    var q1 = Math.floor((3 * r.top + 1 * r.bottom) / 4);
    var q2 = Math.floor((2 * r.top + 2 * r.bottom) / 4);
    var q3 = Math.floor((1 * r.top + 3 * r.bottom) / 4);
    this.setElementCoords_(regionTop, r.left, r.top, r.right, q1,
                                      true, true, true, false);
    this.setElementCoords_(regionMiddleNW, r.left, q1, r.left, q2,
                                           true, true, false, false);
    this.setElementCoords_(regionMiddleSW, r.left, q2, r.left, q3,
                                           true, false, false, true);
    this.setElementCoords_(regionMiddleNE, r.right, q1, r.right, q2,
                                           false, true, true, false);
    this.setElementCoords_(regionMiddleSE, r.right, q2, r.right, q3,
                                           false, false, true, true);
    this.setElementCoords_(regionBottom, r.left, q3, r.right, r.bottom,
                                         true, false, true, true);
    return;
  }

  // Start from the end and grab all of the rectangles in the bottom row.
  var bottomRect = rects[n - 1];
  var bottomMiddle = Math.floor((bottomRect.top + bottomRect.bottom) / 2);
  var bottomIndex = n - 2;
  while (bottomIndex >= 0 && rects[bottomIndex].bottom > bottomMiddle) {
    bottomRect = this.union_(bottomRect, rects[bottomIndex]);
    bottomMiddle = Math.floor((bottomRect.top + bottomRect.bottom) / 2);
    bottomIndex--;
  }

  // Extend the top and bottom rectangles a bit.
  topRect = this.inset_(topRect, -margin, -margin, -margin, margin);
  bottomRect = this.inset_(bottomRect, -margin, margin, -margin, -margin);

  // Whatever's in-between the top and bottom is the "middle".
  var middleRect;
  if (topIndex > bottomIndex) {
    middleRect = this.union_(topRect, bottomRect);
    middleRect.top = topRect.bottom;
    middleRect.bottom = bottomRect.top;
    middleRect.height = Math.floor((middleRect.top + middleRect.bottom) / 2);
  } else {
    middleRect = rects[topIndex];
    var middleIndex = topIndex + 1;
    while (middleIndex <= bottomIndex) {
      middleRect = this.union_(middleRect, rects[middleIndex]);
      middleIndex++;
    }
    middleRect = this.inset_(middleRect, -margin, -margin, -margin, -margin);
    middleRect.left = Math.min(
        middleRect.left, topRect.left, bottomRect.left);
    middleRect.right = Math.max(
        middleRect.right, topRect.right, bottomRect.right);
    middleRect.width = middleRect.right - middleRect.left;
  }

  // If the top or bottom is pretty close to the edge of the middle box,
  // make them flush.
  if (topRect.right > middleRect.right - 40) {
    topRect.right = middleRect.right;
    topRect.width = topRect.right - topRect.left;
  }
  if (topRect.left < middleRect.left + 40) {
    topRect.left = middleRect.left;
    topRect.width = topRect.right - topRect.left;
  }
  if (bottomRect.right > middleRect.right - 40) {
    bottomRect.right = middleRect.right;
    bottomRect.width = bottomRect.right - bottomRect.left;
  }
  if (bottomRect.left < middleRect.left + 40) {
    bottomRect.left = middleRect.left;
    bottomRect.width = bottomRect.right - bottomRect.left;
  }

  var midline = Math.floor((middleRect.top + middleRect.bottom) / 2);

  this.setElementRect_(regionTop, topRect, true, true, true, false);
  this.setElementRect_(regionBottom, bottomRect, true, false, true, true);

  this.setElementCoords_(
      regionMiddleNW,
      middleRect.left, topRect.bottom, topRect.left, midline,
      true, true, false, false);
  this.setElementCoords_(
      regionMiddleNE,
      topRect.right, topRect.bottom,
      middleRect.right, midline,
      false, true, true, false);
  this.setElementCoords_(
      regionMiddleSW,
      middleRect.left, midline, bottomRect.left, bottomRect.top,
      true, false, false, true);
  this.setElementCoords_(
      regionMiddleSE,
      bottomRect.right, midline,
      middleRect.right, bottomRect.top,
      false, false, true, true);
};

/**
 * Given two rectangles, return whether or not they intersect
 * (including a bit of slop, so if they're almost touching, we
 * return true).
 * @param {ClientRect} r1 The first rect.
 * @param {ClientRect} r2 The second rect.
 * @return {boolean} Whether or not they intersect.
 * @private
 */
cvox.ActiveIndicator.prototype.intersects_ = function(r1, r2) {
  var slop = 2 * cvox.ActiveIndicator.MARGIN;
  return (r2.left <= r1.right + slop &&
          r2.right >= r1.left - slop &&
          r2.top <= r1.bottom + slop &&
          r2.bottom >= r1.top - slop);
};

/**
 * Given two rectangles, compute their union.
 * @param {ClientRect} r1 The first rect.
 * @param {ClientRect} r2 The second rect.
 * @return {ClientRect} The union of the two rectangles.
 * @private
 * @suppress {invalidCasts} invalid cast - must be a subtype or supertype
 * from: {bottom: number, height: number, left: number, right: number, ...}
 * to  : (ClientRect|null)
 */
cvox.ActiveIndicator.prototype.union_ = function(r1, r2) {
  var result = {
    left: Math.min(r1.left, r2.left),
    top: Math.min(r1.top, r2.top),
    right: Math.max(r1.right, r2.right),
    bottom: Math.max(r1.bottom, r2.bottom)
  };
  result.width = result.right - result.left;
  result.height = result.bottom - result.top;
  return /** @type {ClientRect} */(result);
};

/**
 * Given a rectangle and four offsets, return a new rectangle inset by
 * the given offsets.
 * @param {ClientRect} r The first rect.
 * @param {number} left The left inset.
 * @param {number} top The top inset.
 * @param {number} right The right inset.
 * @param {number} bottom The bottom inset.
 * @return {ClientRect} The new rectangle.
 * @private
 * @suppress {invalidCasts} invalid cast - must be a subtype or supertype
 * from: {bottom: number, height: number, left: number, right: number, ...}
 * to  : (ClientRect|null)
 */
cvox.ActiveIndicator.prototype.inset_ = function(r, left, top, right, bottom) {
  var result = {
    left: r.left + left,
    top: r.top + top,
    right: r.right - right,
    bottom: r.bottom - bottom
  };
  result.width = result.right - result.left;
  result.height = result.bottom - result.top;
  return /** @type {ClientRect} */(result);
};

/**
 * Convenience method to create an element of type DIV, give it
 * particular class name, and add it as a child of a given parent.
 * @param {Element} parent The parent element of the new div.
 * @param {string} className The class name of the new div.
 * @param {Node=} opt_before Will insert before this node, if present.
 * @return {Element} The new div.
 * @private
 */
cvox.ActiveIndicator.prototype.createDiv_ = function(
      parent, className, opt_before) {
  var elem = document.createElement('div');
  elem.setAttribute('aria-hidden', 'true');

  // This allows the MutationObserver used for live regions to quickly
  // ignore changes to this element rather than doing a lot of calculations
  // first.
  elem.setAttribute('cvoxIgnore', '');

  elem.className = className;
  if (opt_before) {
    parent.insertBefore(elem, opt_before);
  } else {
    parent.appendChild(elem);
  }
  return elem;
};

/**
 * In WebKit, when the user has zoomed the page, every CSS coordinate is
 * multiplied by the zoom level and rounded down. This can cause objects to
 * fail to line up; for example an object with left position 100 and width
 * 50 may not line up with an object with right position 150 pixels, if the
 * zoom is not equal to 1.0. To fix this, we compute the actual desired
 * coordinate when zoomed, then add a small fractional offset and divide
 * by the zoom factor, and use that value as the item's coordinate instead.
 *
 * @param {number} x A coordinate to be transformed.
 * @return {number} The new coordinate to use.
 * @private
 */
cvox.ActiveIndicator.prototype.fixZoom_ = function(x) {
  return (Math.round(x * this.zoom_) + 0.1) / this.zoom_;
};

/**
 * See fixZoom_, above. This method is the same except that it returns the
 * width such that right pos (x + width) is correct when multiplied by the
 * zoom factor.
 *
 * @param {number} x A coordinate to be transformed.
 * @param {number} width The width of the object.
 * @return {number} The new width to use.
 * @private
 */
cvox.ActiveIndicator.prototype.fixZoomSum_ = function(x, width) {
  var zoomedX = Math.round(x * this.zoom_);
  var zoomedRight = Math.round((x + width) * this.zoom_);
  var zoomedWidth = (zoomedRight - zoomedX);
  return (zoomedWidth + 0.1) / this.zoom_;
};

/**
 * Set the coordinates of an element to the given left, top, right, and
 * bottom pixel coordinates, taking the browser zoom level into account.
 * Also set the clipping rectangle to exclude some of the edges of the
 * rectangle, based on the value of showLeft, showTop, showRight, and
 * showBottom.
 *
 * @param {Element} element The element to move.
 * @param {number} left The new left coordinate.
 * @param {number} top The new top coordinate.
 * @param {number} right The new right coordinate.
 * @param {number} bottom The new bottom coordinate.
 * @param {boolean} showLeft Whether to show or clip at the left border.
 * @param {boolean} showTop Whether to show or clip at the top border.
 * @param {boolean} showRight Whether to show or clip at the right border.
 * @param {boolean} showBottom Whether to show or clip at the bottom border.
 * @private
 */
cvox.ActiveIndicator.prototype.setElementCoords_ = function(
      element,
      left, top, right, bottom,
      showLeft, showTop, showRight, showBottom) {
  var origWidth = right - left;
  var origHeight = bottom - top;

  var width = right - left;
  var height = bottom - top;
  var clipLeft = showLeft ? -20 : 0;
  var clipTop = showTop ? -20 : 0;
  var clipRight = showRight ? 20 : 0;
  var clipBottom = showBottom ? 20 : 0;
  if (width == 0) {
    if (showRight) {
      left -= 5;
      width += 5;
    } else if (showLeft) {
      width += 10;
    }
    clipTop = 10;
    clipBottom = 10;
    top -= 10;
    height += 20;
  }
  if (!showBottom)
    height += 5;
  if (!showTop) {
    top -= 5;
    height += 5;
    clipTop += 5;
    clipBottom += 5;
  }
  if (clipRight == 0 && origWidth == 0) {
    clipRight = 1;
  } else {
    clipRight = this.fixZoomSum_(left, clipRight + origWidth);
  }
  clipBottom = this.fixZoomSum_(top, clipBottom + origHeight);

  element.style.left = this.fixZoom_(left) + 'px';
  element.style.top = this.fixZoom_(top) + 'px';
  element.style.width = this.fixZoomSum_(left, width) + 'px';
  element.style.height = this.fixZoomSum_(top, height) + 'px';
  element.style.clip =
      'rect(' + [clipTop, clipRight, clipBottom, clipLeft].join('px ') + 'px)';
};

/**
 * Same as setElementCoords_, but takes a rect instead of coordinates.
 *
 * @param {Element} element The element to move.
 * @param {ClientRect} r The new coordinates.
 * @param {boolean} showLeft Whether to show or clip at the left border.
 * @param {boolean} showTop Whether to show or clip at the top border.
 * @param {boolean} showRight Whether to show or clip at the right border.
 * @param {boolean} showBottom Whether to show or clip at the bottom border.
 * @private
 */
cvox.ActiveIndicator.prototype.setElementRect_ = function(
      element, r, showLeft, showTop, showRight, showBottom) {
  this.setElementCoords_(element, r.left, r.top, r.right, r.bottom,
                         showLeft, showTop, showRight, showBottom);
};

/**
 * Compute an approximation of the current browser zoom level by
 * comparing the measurement of a large character of text
 * with the -webkit-text-size-adjust:none style to the expected
 * pixel coordinates if it was adjusted.
 * @private
 */
cvox.ActiveIndicator.prototype.computeZoomLevel_ = function() {
  if (window.innerHeight === this.innerHeight_ &&
      window.innerWidth === this.innerWidth_) {
    return;
  }

  this.innerHeight_ = window.innerHeight;
  this.innerWidth_ = window.innerWidth;

  var zoomMeasureElement = document.createElement('div');
  zoomMeasureElement.innerHTML = 'X';
  zoomMeasureElement.setAttribute(
      'style',
      'font: 5000px/1em sans-serif !important;' +
          ' -webkit-text-size-adjust:none !important;' +
          ' visibility:hidden !important;' +
          ' left: -10000px !important;' +
          ' top: -10000px !important;' +
          ' position:absolute !important;');
  document.body.appendChild(zoomMeasureElement);

  var zoomLevel = 5000 / zoomMeasureElement.clientHeight;
  var newZoom = Math.round(zoomLevel * 500) / 500;
  if (newZoom > 0.1 && newZoom < 10) {
    this.zoom_ = newZoom;
  }

  // TODO(dmazzoni): warn or log if the computed zoom is bad?
  zoomMeasureElement.parentNode.removeChild(zoomMeasureElement);
};
