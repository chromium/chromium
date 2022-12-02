// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * NOTE: This file is deprecated, and provides only the minimal LoadTimeData
 * functions for places in the code still not using JS modules. Use
 * load_time_data.ts in all new code.
 *
 * This file defines a singleton which provides access to all data
 * that is available as soon as the page's resources are loaded (before DOM
 * content has finished loading). This data includes both localized strings and
 * any data that is important to have ready from a very early stage (e.g. things
 * that must be displayed right away).
 *
 * Note that loadTimeData is not guaranteed to be consistent between page
 * refreshes (https://crbug.com/740629) and should not contain values that might
 * change if the page is re-opened later.
 */

/** @type {!LoadTimeData} */
// eslint-disable-next-line no-var
var loadTimeData;

class LoadTimeData {
  constructor() {
    /** @type {?Object} */
    this.data_ = null;
  }

  /**
   * Sets the backing object.
   *
   * Note that there is no getter for |data_| to discourage abuse of the form:
   *
   *     var value = loadTimeData.data()['key'];
   *
   * @param {Object} value The de-serialized page data.
   */
  set data(value) {
    expect(!this.data_, 'Re-setting data.');
    this.data_ = value;
  }

  /**
   * @param {string} id An ID of a value that might exist.
   * @return {boolean} True if |id| is a key in the dictionary.
   */
  valueExists(id) {
    return id in this.data_;
  }

  /**
   * Fetches a value, expecting that it exists.
   * @param {string} id The key that identifies the desired value.
   * @return {*} The corresponding value.
   */
  getValue(id) {
    expect(this.data_, 'No data. Did you remember to include strings.js?');
    const value = this.data_[id];
    expect(typeof value !== 'undefined', 'Could not find value for ' + id);
    return value;
  }

  /**
   * As above, but also makes sure that the value is a string.
   * @param {string} id The key that identifies the desired string.
   * @return {string} The corresponding string value.
   */
  getString(id) {
    const value = this.getValue(id);
    expectIsType(id, value, 'string');
    return /** @type {string} */ (value);
  }

  /**
   * Returns a formatted localized string where $1 to $9 are replaced by the
   * second to the tenth argument.
   * @param {string} id The ID of the string we want.
   * @param {...(string|number)} var_args The extra values to include in the
   *     formatted output.
   * @return {string} The formatted string.
   */
  getStringF(id, var_args) {
    const value = this.getString(id);
    if (!value) {
      return '';
    }

    const args = Array.prototype.slice.call(arguments);
    args[0] = value;
    return this.substituteString.apply(this, args);
  }

  /**
   * Returns a formatted localized string where $1 to $9 are replaced by the
   * second to the tenth argument. Any standalone $ signs must be escaped as
   * $$.
   * @param {string} label The label to substitute through.
   *     This is not an resource ID.
   * @param {...(string|number)} var_args The extra values to include in the
   *     formatted output.
   * @return {string} The formatted string.
   */
  substituteString(label, var_args) {
    const varArgs = arguments;
    return label.replace(/\$(.|$|\n)/g, function(m) {
      expect(m.match(/\$[$1-9]/), 'Unescaped $ found in localized string.');
      return m === '$$' ? '$' : varArgs[m[1]];
    });
  }

  /**
   * As above, but also makes sure that the value is a boolean.
   * @param {string} id The key that identifies the desired boolean.
   * @return {boolean} The corresponding boolean value.
   */
  getBoolean(id) {
    const value = this.getValue(id);
    expectIsType(id, value, 'boolean');
    return /** @type {boolean} */ (value);
  }

  /**
   * As above, but also makes sure that the value is an integer.
   * @param {string} id The key that identifies the desired number.
   * @return {number} The corresponding number value.
   */
  getInteger(id) {
    const value = this.getValue(id);
    expectIsType(id, value, 'number');
    expect(value === Math.floor(value), 'Number isn\'t integer: ' + value);
    return /** @type {number} */ (value);
  }

  /**
   * Override values in loadTimeData with the values found in |replacements|.
   * @param {Object} replacements The dictionary object of keys to replace.
   */
  overrideValues(replacements) {
    expect(
        typeof replacements === 'object',
        'Replacements must be a dictionary object.');
    for (const key in replacements) {
      this.data_[key] = replacements[key];
    }
  }
}

/**
 * Checks condition, throws error message if expectation fails.
 * @param {*} condition The condition to check for truthiness.
 * @param {string} message The message to display if the check fails.
 */
function expect(condition, message) {
  if (!condition) {
    throw new Error(
        'Unexpected condition on ' + document.location.href + ': ' + message);
  }
}

/**
 * Checks that the given value has the given type.
 * @param {string} id The id of the value (only used for error message).
 * @param {*} value The value to check the type on.
 * @param {string} type The type we expect |value| to be.
 */
function expectIsType(id, value, type) {
  expect(
      typeof value === type, '[' + value + '] (' + id + ') is not a ' + type);
}

expect(!loadTimeData, 'should only include this file once');
loadTimeData = new LoadTimeData();

// Expose |loadTimeData| directly on |window|, since within a JS module the
// scope is local and not all files have been updated to import the exported
// |loadTimeData| explicitly.
window.loadTimeData = loadTimeData;

console.warn('crbug/1173575, non-JS module files deprecated.');
