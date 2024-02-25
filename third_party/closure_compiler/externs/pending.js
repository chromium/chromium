// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs for stuff not added to the Closure compiler yet, but
 * should get added.
 * @externs
 */

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

/**
 * @see https://drafts.css-houdini.org/css-typed-om/#stylepropertymapreadonly
 */
class StylePropertyMapReadOnly {
  /** @param {string} property */
  get(property) {}
}

/** @return {!StylePropertyMapReadOnly} */
HTMLElement.prototype.computedStyleMap = function() {};

/** @return {!AnimationEffectTimingProperties} */
AnimationEffect.prototype.getTiming = function() {};

/** @return {!Array<!Object>} */
AnimationEffect.prototype.getKeyframes = function() {};

/**
 * https://developer.mozilla.org/en-US/docs/Web/API/structuredClone
 * @param {T} obj
 * @return {T}
 * @template T
 */
function structuredClone(obj) {}
