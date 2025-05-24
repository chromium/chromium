/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.provide('goog.ui.KeyboardEventData');

goog.require('goog.asserts');
goog.require('goog.events.BrowserEvent');



/**
 * Data object that contains all the necessary information from a keyboard event
 * that is required to process it in `KeyboardShortcutHandler`.
 *
 * Prefer using `goog.ui.KeyboardEventData.Builder` over using this constructor.
 * @param {number} keyCode
 * @param {string} key
 * @param {boolean} shiftKey
 * @param {boolean} altKey
 * @param {boolean} ctrlKey
 * @param {boolean} metaKey
 * @param {!Node} target
 * @param {!EventTarget} rootTarget
 * @param {function(): void} preventDefaultFn
 * @param {function(): void} stopPropagationFn
 * @constructor @struct @final
 * @package
 */
goog.ui.KeyboardEventData = function(
    keyCode, key, shiftKey, altKey, ctrlKey, metaKey, target, rootTarget,
    preventDefaultFn, stopPropagationFn) {
  'use strict';
  /** @private @const {number} */
  this.keyCode_ = keyCode;

  /** @private @const {string} */
  this.key_ = key;

  /** @private @const {boolean} */
  this.shiftKey_ = shiftKey;

  /** @private @const {boolean} */
  this.altKey_ = altKey;

  /** @private @const {boolean} */
  this.ctrlKey_ = ctrlKey;

  /** @private @const {boolean} */
  this.metaKey_ = metaKey;

  /** @private @const {!Node} */
  this.target_ = target;

  /**
   * For events fired from inside `open` Shadow DOM elements, the root event
   * target (i.e. the first `EventTarget` in the composed path). For all other
   * events, the original target.
   * @private @const {!EventTarget}
   */
  this.rootTarget_ = rootTarget;

  /** @private @const {function(): void} */
  this.preventDefaultFn_ = preventDefaultFn;

  /** @private @const {function(): void} */
  this.stopPropagationFn_ = stopPropagationFn;
};


/** @return {number} The keyCode of the event. */
goog.ui.KeyboardEventData.prototype.getKeyCode = function() {
  'use strict';
  return this.keyCode_;
};


/** @return {string} The key of the event, or `''` if not one. */
goog.ui.KeyboardEventData.prototype.getKey = function() {
  'use strict';
  return this.key_;
};


/** @return {boolean} If the shift key was pressed. */
goog.ui.KeyboardEventData.prototype.getShiftKey = function() {
  'use strict';
  return this.shiftKey_;
};


/** @return {boolean} If the alt key was pressed. */
goog.ui.KeyboardEventData.prototype.getAltKey = function() {
  'use strict';
  return this.altKey_;
};


/** @return {boolean} If the ctrl key was pressed. */
goog.ui.KeyboardEventData.prototype.getCtrlKey = function() {
  'use strict';
  return this.ctrlKey_;
};


/** @return {boolean} If the meta key was pressed. */
goog.ui.KeyboardEventData.prototype.getMetaKey = function() {
  'use strict';
  return this.metaKey_;
};


/** @return {!Node} The target of the event. */
goog.ui.KeyboardEventData.prototype.getTarget = function() {
  'use strict';
  return this.target_;
};


/** @return {!EventTarget} The rootTarget of the event. */
goog.ui.KeyboardEventData.prototype.getRootTarget = function() {
  'use strict';
  return this.rootTarget_;
};


/** @return {function(): void} Callback to prevent default. */
goog.ui.KeyboardEventData.prototype.getPreventDefaultFn = function() {
  'use strict';
  return this.preventDefaultFn_;
};


/** @return {function(): void} Callback to stop propagation. */
goog.ui.KeyboardEventData.prototype.getStopPropagationFn = function() {
  'use strict';
  return this.stopPropagationFn_;
};


/**
 * @param {!goog.events.BrowserEvent} event
 * @return {!goog.ui.KeyboardEventData}
 * @suppress {strictMissingProperties} path is a union type
 */
goog.ui.KeyboardEventData.fromBrowserEvent = function(event) {
  'use strict';
  var e = event.getBrowserEvent();
  // Check existence to prevent classic FF reference error in strict mode.
  var hasComposed = e && 'composed' in e;
  var hasComposedPath = e && 'composedPath' in e;
  // EventTarget is updated, when browser supports shadow dom and event is
  // triggered inside `open` shadow root.
  var path = hasComposed && hasComposedPath && e.composed && e.composedPath();
  var rootTarget = (path && path.length > 0) ? path[0] : event.target;

  return new goog.ui.KeyboardEventData.Builder()
      .keyCode(event.keyCode || 0)
      .key(event.key || '')
      .shiftKey(!!event.shiftKey)
      .altKey(!!event.altKey)
      .ctrlKey(!!event.ctrlKey)
      .metaKey(!!event.metaKey)
      .target(event.target)
      .rootTarget(rootTarget)
      .preventDefaultFn(() => event.preventDefault())
      .stopPropagationFn(() => event.stopPropagation())
      .build();
};



/**
 * Builder for `KeyboardEventData`. All fields are required except `key`, which
 * defaults to `''`.
 * @constructor @struct @final
 */
goog.ui.KeyboardEventData.Builder = function() {
  'use strict';
  /** @private {?number} */
  this.keyCode_ = null;

  /** @private {string} */
  this.key_ = '';

  /** @private {?boolean} */
  this.shiftKey_ = null;

  /** @private {?boolean} */
  this.altKey_ = null;

  /** @private {?boolean} */
  this.ctrlKey_ = null;

  /** @private {?boolean} */
  this.metaKey_ = null;

  /** @private {?Node} */
  this.target_ = null;

  /** @private {?EventTarget} */
  this.rootTarget_ = null;

  /** @private {?function(): void} */
  this.preventDefaultFn_ = null;

  /** @private {?function(): void} */
  this.stopPropagationFn_ = null;
};


/**
 * @param {number} keyCode
 * @return {!goog.ui.KeyboardEventData.Builder}
 */
goog.ui.KeyboardEventData.Builder.prototype.keyCode = function(keyCode) {
  'use strict';
  this.keyCode_ = keyCode;
  return this;
};


/**
 * @param {string} key
 * @return {!goog.ui.KeyboardEventData.Builder}
 */
goog.ui.KeyboardEventData.Builder.prototype.key = function(key) {
  'use strict';
  this.key_ = key;
  return this;
};


/**
 * @param {boolean} shiftKey
 * @return {!goog.ui.KeyboardEventData.Builder}
 */
goog.ui.KeyboardEventData.Builder.prototype.shiftKey = function(shiftKey) {
  'use strict';
  this.shiftKey_ = shiftKey;
  return this;
};


/**
 * @param {boolean} altKey
 * @return {!goog.ui.KeyboardEventData.Builder}
 */
goog.ui.KeyboardEventData.Builder.prototype.altKey = function(altKey) {
  'use strict';
  this.altKey_ = altKey;
  return this;
};


/**
 * @param {boolean} ctrlKey
 * @return {!goog.ui.KeyboardEventData.Builder}
 */
goog.ui.KeyboardEventData.Builder.prototype.ctrlKey = function(ctrlKey) {
  'use strict';
  this.ctrlKey_ = ctrlKey;
  return this;
};


/**
 * @param {boolean} metaKey
 * @return {!goog.ui.KeyboardEventData.Builder}
 */
goog.ui.KeyboardEventData.Builder.prototype.metaKey = function(metaKey) {
  'use strict';
  this.metaKey_ = metaKey;
  return this;
};


/**
 * @param {?Node} target
 * @return {!goog.ui.KeyboardEventData.Builder}
 */
goog.ui.KeyboardEventData.Builder.prototype.target = function(target) {
  'use strict';
  this.target_ = target;
  return this;
};


/**
 * @param {?EventTarget} rootTarget
 * @return {!goog.ui.KeyboardEventData.Builder}
 */
goog.ui.KeyboardEventData.Builder.prototype.rootTarget = function(rootTarget) {
  'use strict';
  this.rootTarget_ = rootTarget;
  return this;
};


/**
 * @param {function(): void} preventDefaultFn
 * @return {!goog.ui.KeyboardEventData.Builder}
 */
goog.ui.KeyboardEventData.Builder.prototype.preventDefaultFn = function(
    preventDefaultFn) {
  'use strict';
  this.preventDefaultFn_ = preventDefaultFn;
  return this;
};


/**
 * @param {function(): void} stopPropagationFn
 * @return {!goog.ui.KeyboardEventData.Builder}
 */
goog.ui.KeyboardEventData.Builder.prototype.stopPropagationFn = function(
    stopPropagationFn) {
  'use strict';
  this.stopPropagationFn_ = stopPropagationFn;
  return this;
};


/** @return {!goog.ui.KeyboardEventData} */
goog.ui.KeyboardEventData.Builder.prototype.build = function() {
  'use strict';
  return new goog.ui.KeyboardEventData(
      goog.asserts.assertNumber(this.keyCode_), this.key_,
      goog.asserts.assertBoolean(this.shiftKey_),
      goog.asserts.assertBoolean(this.altKey_),
      goog.asserts.assertBoolean(this.ctrlKey_),
      goog.asserts.assertBoolean(this.metaKey_),
      goog.asserts.assert(this.target_), goog.asserts.assert(this.rootTarget_),
      goog.asserts.assertFunction(this.preventDefaultFn_),
      goog.asserts.assertFunction(this.stopPropagationFn_));
};
