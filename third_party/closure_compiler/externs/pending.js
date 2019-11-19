// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs for stuff not added to the Closure compiler yet, but
 * should get added.
 * @externs
 */

/**
 * TODO(dstockwell): Remove this once it is added to Closure Compiler itself.
 * @see https://drafts.fxtf.org/geometry/#DOMMatrix
 */
class DOMMatrix {
  /**
   * @param {number} x
   * @param {number} y
   */
  translateSelf(x, y) {}
  /**
   * @param {number} x
   * @param {number} y
   * @param {number} z
   */
  rotateSelf(x, y, z) {}
  /**
   * @param {number} x
   * @param {number} y
   */
  scaleSelf(x, y) {}
  /**
   * @param {{x: number, y: number}} point
   * @return {{x: number, y: number}}
   */
  transformPoint(point) {}
}

/**
 * @see https://wicg.github.io/ResizeObserver/#resizeobserverentry
 * @typedef {{contentRect: DOMRectReadOnly,
 *            target: Element}}
 * TODO(dpapad): Remove this once it is added to Closure Compiler itself.
 */
let ResizeObserverEntry;

/**
 * @see https://wicg.github.io/ResizeObserver/#api
 * TODO(dpapad): Remove this once it is added to Closure Compiler itself.
 */
class ResizeObserver {
  /**
   * @param {!function(Array<ResizeObserverEntry>, ResizeObserver)} callback
   */
  constructor(callback) {}

  disconnect() {}

  /** @param {Element} target */
  observe(target) {}

  /** @param {Element} target */
  unobserve(target) {}
}

/**
 * @see
 * https://github.com/tc39/proposal-bigint
 * This supports wrapping and operating on arbitrarily large integers.
 *
 * @param {number} value
 */
let BigInt = function(value) {};

/** @const {!Clipboard} */
Navigator.prototype.clipboard;

/**
 * TODO(katie): Remove this once length is added to the Closure
 * chrome_extensions.js.
 * An event from the TTS engine to communicate the status of an utterance.
 * @constructor
 */
function TtsEvent() {}

/** @type {number} */
TtsEvent.prototype.length;



/**
 * @param {number|ArrayBufferView|Array.<number>|ArrayBuffer} length or array
 *     or buffer
 * @param {number=} opt_byteOffset
 * @param {number=} opt_length
 * @extends {ArrayBufferView}
 * @constructor
 * @throws {Error}
 * @modifies {arguments}
 */
function BigInt64Array(length, opt_byteOffset, opt_length) {}

/** @type {number} */
BigInt64Array.BYTES_PER_ELEMENT;

/** @type {number} */
BigInt64Array.prototype.BYTES_PER_ELEMENT;

/** @type {number} */
BigInt64Array.prototype.length;

/**
 * @param {ArrayBufferView|Array.<number>} array
 * @param {number=} opt_offset
 */
BigInt64Array.prototype.set = function(array, opt_offset) {};

/**
 * @param {number} begin
 * @param {number=} opt_end
 * @return {!BigInt64Array}
 * @nosideeffects
 */
BigInt64Array.prototype.subarray = function(begin, opt_end) {};

/**
 * @see https://drafts.css-houdini.org/css-typed-om/#stylepropertymap
 * @typedef {{set: function(string, *):void,
 *            append: function(string, *):void,
 *            delete: function(string):void,
 *            clear: function():void }}
 * TODO(rbpotter): Remove this once it is added to Closure Compiler itself.
 */
class StylePropertyMap {
  /**
   * @param {string} property
   * @param {*} values
   */
  set(property, values) {}

  /**
   * @param {string} property
   * @param {*} values
   */
  append(property, values) {}

  /** @param {string} property */
  delete(property) {}

  clear() {}
}

/** @type {!StylePropertyMap} */
HTMLElement.prototype.attributeStyleMap;
