// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file defines a singleton which provides access to all data
 * that is available as soon as the page's resources are loaded (before DOM
 * content has finished loading). This data includes both localized strings and
 * any data that is important to have ready from a very early stage (e.g. things
 * that must be displayed right away).
 *
 * Note that loadTimeData is not guaranteed to be consistent between page
 * refreshes (https://crbug.com/740629) and should not contain values that might
 * change if the page is re-opened later.
 */

import {assert} from './assert.js';

interface LoadTimeDataRaw {
  [key: string]: any;
}

class LoadTimeData {
  private data_: LoadTimeDataRaw|null = null;

  /**
   * Sets the backing object.
   *
   * Note that there is no getter for |data_| to discourage abuse of the form:
   *
   *     var value = loadTimeData.data()['key'];
   */
  set data(value: LoadTimeDataRaw) {
    assert(!this.data_, 'Re-setting data.');
    this.data_ = value;
  }

  /**
   * @param id An ID of a value that might exist.
   * @return True if |id| is a key in the dictionary.
   */
  valueExists(id: string): boolean {
    assert(this.data_, 'No data. Did you remember to include strings.js?');
    return id in this.data_;
  }

  /**
   * Fetches a value, expecting that it exists.
   * @param id The key that identifies the desired value.
   * @return The corresponding value.
   */
  getValue(id: string): any {
    assert(this.data_, 'No data. Did you remember to include strings.js?');
    const value = this.data_[id];
    assert(typeof value !== 'undefined', 'Could not find value for ' + id);
    return value;
  }

  /**
   * As above, but also makes sure that the value is a string.
   * @param id The key that identifies the desired string.
   * @return The corresponding string value.
   */
  getString(id: string): string {
    const value = this.getValue(id);
    assert(typeof value === 'string', `[${value}] (${id}) is not a string`);
    return value;
  }

  /**
   * Returns a formatted localized string where $1 to $9 are replaced by the
   * second to the tenth argument.
   * @param id The ID of the string we want.
   * @param args The extra values to include in the formatted output.
   * @return The formatted string.
   */
  getStringF(id: string, ...args: Array<string|number>): string {
    const value = this.getString(id);
    if (!value) {
      return '';
    }

    return this.substituteString(value, ...args);
  }

  /**
   * Returns a formatted localized string where $1 to $9 are replaced by the
   * second to the tenth argument. Any standalone $ signs must be escaped as
   * $$.
   * @param label The label to substitute through. This is not an resource ID.
   * @param args The extra values to include in the formatted output.
   * @return The formatted string.
   */
  substituteString(label: string, ...args: Array<string|number>): string {
    return label.replace(/\$(.|$|\n)/g, function(m) {
      assert(m.match(/\$[$1-9]/), 'Unescaped $ found in localized string.');
      if (m === '$$') {
        return '$';
      }

      const substitute = args[Number(m[1]) - 1];
      if (substitute === undefined || substitute === null) {
        // Not all callers actually provide values for all substitutes. Return
        // an empty value for this case.
        return '';
      }
      return substitute.toString();
    });
  }

  /**
   * Returns a formatted string where $1 to $9 are replaced by the second to
   * tenth argument, split apart into a list of pieces describing how the
   * substitution was performed. Any standalone $ signs must be escaped as $$.
   * @param label A localized string to substitute through.
   *     This is not an resource ID.
   * @param args The extra values to include in the formatted output.
   * @return The formatted string pieces.
   */
  getSubstitutedStringPieces(label: string, ...args: Array<string|number>):
      Array<{value: string, arg: (string|null)}> {
    // Split the string by separately matching all occurrences of $1-9 and of
    // non $1-9 pieces.
    const pieces = (label.match(/(\$[1-9])|(([^$]|\$([^1-9]|$))+)/g) ||
                    []).map(function(p) {
      // Pieces that are not $1-9 should be returned after replacing $$
      // with $.
      if (!p.match(/^\$[1-9]$/)) {
        assert(
            (p.match(/\$/g) || []).length % 2 === 0,
            'Unescaped $ found in localized string.');
        return {value: p.replace(/\$\$/g, '$'), arg: null};
      }

      // Otherwise, return the substitution value.
      const substitute = args[Number(p[1]) - 1];
      if (substitute === undefined || substitute === null) {
        // Not all callers actually provide values for all substitutes. Return
        // an empty value for this case.
        return {value: '', arg: p};
      }
      return {value: substitute.toString(), arg: p};
    });

    return pieces;
  }

  /**
   * As above, but also makes sure that the value is a boolean.
   * @param id The key that identifies the desired boolean.
   * @return The corresponding boolean value.
   */
  getBoolean(id: string): boolean {
    const value = this.getValue(id);
    assert(typeof value === 'boolean', `[${value}] (${id}) is not a boolean`);
    return value;
  }

  /**
   * As above, but also makes sure that the value is an integer.
   * @param id The key that identifies the desired number.
   * @return The corresponding number value.
   */
  getInteger(id: string): number {
    const value = this.getValue(id);
    assert(typeof value === 'number', `[${value}] (${id}) is not a number`);
    assert(value === Math.floor(value), 'Number isn\'t integer: ' + value);
    return value;
  }

  /**
   * Override values in loadTimeData with the values found in |replacements|.
   * @param replacements The dictionary object of keys to replace.
   */
  overrideValues(replacements: LoadTimeDataRaw) {
    assert(
        typeof replacements === 'object',
        'Replacements must be a dictionary object.');
    assert(this.data_, 'Data must exist before being overridden');
    for (const key in replacements) {
      this.data_[key] = replacements[key];
    }
  }

  /**
   * Reset loadTimeData's data. Should only be used in tests.
   * @param newData The data to restore to, when null restores to unset state.
   */
  resetForTesting(newData: LoadTimeDataRaw|null = null) {
    this.data_ = newData;
  }

  /**
   * @return Whether loadTimeData.data has been set.
   */
  isInitialized(): boolean {
    return this.data_ !== null;
  }
}

export const loadTimeData = new LoadTimeData();
