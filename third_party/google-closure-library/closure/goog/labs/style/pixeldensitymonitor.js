/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Utility class that monitors pixel density ratio changes.
 *
 * @see ../demos/pixeldensitymonitor.html
 */

goog.provide('goog.labs.style.PixelDensityMonitor');
goog.provide('goog.labs.style.PixelDensityMonitor.Density');
goog.provide('goog.labs.style.PixelDensityMonitor.EventType');

goog.require('goog.events');
goog.require('goog.events.EventTarget');
goog.requireType('goog.dom.DomHelper');



/**
 * Monitors the window for changes to the ratio between device and screen
 * pixels, e.g. when the user moves the window from a high density screen to a
 * screen with normal density. Dispatches
 * goog.labs.style.PixelDensityMonitor.EventType.CHANGE events when the density
 * changes between the two predefined values NORMAL and HIGH.
 *
 * This class uses the window.devicePixelRatio value which is supported in
 * WebKit and FF18. If the value does not exist, it will always return a
 * NORMAL density. It requires support for MediaQueryList to detect changes to
 * the devicePixelRatio.
 *
 * @param {!goog.dom.DomHelper=} opt_domHelper The DomHelper which contains the
 *     document associated with the window to listen to. Defaults to the one in
 *     which this code is executing.
 * @constructor
 * @extends {goog.events.EventTarget}
 * @final
 */
goog.labs.style.PixelDensityMonitor = function(opt_domHelper) {
  'use strict';
  goog.labs.style.PixelDensityMonitor.base(this, 'constructor');

  /**
   * @type {!Window}
   * @private
   * @const
   */
  this.window_ = opt_domHelper ? opt_domHelper.getWindow() : window;

  /**
   * The last density that was reported so that changes can be detected.
   * @type {!goog.labs.style.PixelDensityMonitor.Density}
   * @private
   */
  this.lastDensity_ = this.getDensity();

  /**
   * @type {function ()}
   * @private
   * @const
   */
  this.listener_ = goog.bind(this.handleMediaQueryChange_, this);

  /**
   * Remove the internal event listener on mediaQueryList.
   * @type {?function ()}
   * @private
   */
  this.removeListener_ = null;

  /**
   * The media query list for a query that detects high density, if supported
   * by the browser. Because matchMedia returns a new object for every call, it
   * needs to be saved here so the listener can be removed when disposing.
   * @type {?MediaQueryList}
   * @private
   */
  this.mediaQueryList_ = this.window_.matchMedia ?
      this.window_.matchMedia(
          goog.labs.style.PixelDensityMonitor.HIGH_DENSITY_QUERY_) :
      null;

  /**
   * The Cobalt browser (https://cobalt.dev/) doesn't implement `addListener` or
   * `addEventListener`.
   */
  if (this.mediaQueryList_ &&
      typeof this.mediaQueryList_.addListener !== 'function' &&
      typeof this.mediaQueryList_.addEventListener !== 'function') {
    this.mediaQueryList_ = null;
  }
};
goog.inherits(goog.labs.style.PixelDensityMonitor, goog.events.EventTarget);


/**
 * The two different pixel density modes on which the various ratios between
 * physical and device pixels are mapped.
 * @enum {number}
 */
goog.labs.style.PixelDensityMonitor.Density = {
  /**
   * Mode for older portable devices and desktop screens, defined as having a
   * device pixel ratio of less than 1.5.
   */
  NORMAL: 1,

  /**
   * Mode for newer portable devices with a high resolution screen, defined as
   * having a device pixel ratio of more than 1.5.
   */
  HIGH: 2
};


/**
 * The events fired by the PixelDensityMonitor.
 * @enum {string}
 * @const
 */
goog.labs.style.PixelDensityMonitor.EventType = {
  /**
   * Dispatched when density changes between NORMAL and HIGH.
   */
  CHANGE: goog.events.getUniqueId('change')
};


/**
 * Minimum ratio between device and screen pixel needed for high density mode.
 * @type {number}
 * @private
 * @const
 */
goog.labs.style.PixelDensityMonitor.HIGH_DENSITY_RATIO_ = 1.5;


/**
 * Media query that matches for high density.
 * @type {string}
 * @private
 * @const
 */
goog.labs.style.PixelDensityMonitor.HIGH_DENSITY_QUERY_ =
    '(min-resolution: 1.5dppx), (-webkit-min-device-pixel-ratio: 1.5)';


/**
 * Starts monitoring for changes in pixel density.
 */
goog.labs.style.PixelDensityMonitor.prototype.start = function() {
  'use strict';
  if (this.mediaQueryList_) {
    if (typeof this.mediaQueryList_.addEventListener === 'function') {
      this.mediaQueryList_.addEventListener('change', this.listener_);
      this.removeListener_ = () => {
        this.mediaQueryList_.removeEventListener('change', this.listener_);
      };
    } else {
      this.mediaQueryList_.addListener(this.listener_);
      this.removeListener_ = () => {
        this.mediaQueryList_.removeListener(this.listener_);
      };
    }
  }
};


/**
 * @return {!goog.labs.style.PixelDensityMonitor.Density} The density for the
 *     window.
 */
goog.labs.style.PixelDensityMonitor.prototype.getDensity = function() {
  'use strict';
  if (this.window_.devicePixelRatio >=
      goog.labs.style.PixelDensityMonitor.HIGH_DENSITY_RATIO_) {
    return goog.labs.style.PixelDensityMonitor.Density.HIGH;
  } else {
    return goog.labs.style.PixelDensityMonitor.Density.NORMAL;
  }
};


/**
 * Handles a change to the media query and checks whether the density has
 * changed since the last call.
 * @private
 */
goog.labs.style.PixelDensityMonitor.prototype.handleMediaQueryChange_ =
    function() {
  'use strict';
  const newDensity = this.getDensity();
  if (this.lastDensity_ != newDensity) {
    this.lastDensity_ = newDensity;
    this.dispatchEvent(goog.labs.style.PixelDensityMonitor.EventType.CHANGE);
  }
};


/** @override */
goog.labs.style.PixelDensityMonitor.prototype.disposeInternal = function() {
  'use strict';
  if (this.removeListener_) {
    this.removeListener_();
  }
  goog.labs.style.PixelDensityMonitor.base(this, 'disposeInternal');
};
