// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'I18nBehavior' is a behavior to mix in loading of internationalization
 * strings. Typically it is used as [[i18n('someString')]] computed bindings or
 * for this.i18n('foo'). It is not needed for HTML $i18n{otherString}, which is
 * handled by a C++ templatizer.
 */

// #import {parseHtmlSubset} from './parse_html_subset.m.js';
// #import {loadTimeData, SanitizeInnerHtmlOpts} from './load_time_data.m.js';

/** @polymerBehavior */
/* #export */ const I18nBehavior = {
  properties: {
    /**
     * The language the UI is presented in. Used to signal dynamic language
     * change.
     */
    locale: {
      type: String,
      value: '',
    },
  },

  /**
   * Returns a translated string where $1 to $9 are replaced by the given
   * values.
   * @param {string} id The ID of the string to translate.
   * @param {...string} var_args Values to replace the placeholders $1 to $9
   *     in the string.
   * @return {string} A translated, substituted string.
   * @private
   */
  i18nRaw_: function(id, var_args) {
    return arguments.length == 1 ?
        loadTimeData.getString(id) :
        loadTimeData.getStringF.apply(loadTimeData, arguments);
  },

  /**
   * Returns a translated string where $1 to $9 are replaced by the given
   * values. Also sanitizes the output to filter out dangerous HTML/JS.
   * Use with Polymer bindings that are *not* inner-h-t-m-l.
   * NOTE: This is not related to $i18n{foo} in HTML, see file overview.
   * @param {string} id The ID of the string to translate.
   * @param {...string|number} var_args Values to replace the placeholders $1
   *     to $9 in the string.
   * @return {string} A translated, sanitized, substituted string.
   */
  i18n: function(id, var_args) {
    const rawString = this.i18nRaw_.apply(this, arguments);
    return parseHtmlSubset('<b>' + rawString + '</b>').firstChild.textContent;
  },

  /**
   * Similar to 'i18n', returns a translated, sanitized, substituted string.
   * It receives the string ID and a dictionary containing the substitutions
   * as well as optional additional allowed tags and attributes. Use with
   * Polymer bindings that are inner-h-t-m-l, for example.
   * @param {string} id The ID of the string to translate.
   * @param {SanitizeInnerHtmlOpts=} opts
   * @return {string}
   */
  i18nAdvanced: function(id, opts) {
    opts = opts || {};
    const args = [id].concat(opts.substitutions || []);
    const rawString = this.i18nRaw_.apply(this, args);
    return loadTimeData.sanitizeInnerHtml(rawString, opts);
  },

  /**
   * Similar to 'i18n', with an unused |locale| parameter used to trigger
   * updates when |this.locale| changes.
   * @param {string} locale The UI language used.
   * @param {string} id The ID of the string to translate.
   * @param {...string} var_args Values to replace the placeholders $1 to $9
   *     in the string.
   * @return {string} A translated, sanitized, substituted string.
   */
  i18nDynamic: function(locale, id, var_args) {
    return this.i18n.apply(this, Array.prototype.slice.call(arguments, 1));
  },

  /**
   * Similar to 'i18nDynamic', but var_args valus are interpreted as keys in
   * loadTimeData. This allows generation of strings that take other localized
   * strings as parameters.
   * @param {string} locale The UI language used.
   * @param {string} id The ID of the string to translate.
   * @param {...string} var_args Values to replace the placeholders $1 to $9
   *     in the string. Values are interpreted as strings IDs if found in the
   *     list of localized strings.
   * @return {string} A translated, sanitized, substituted string.
   */
  i18nRecursive: function(locale, id, var_args) {
    let args = Array.prototype.slice.call(arguments, 2);
    if (args.length > 0) {
      // Try to replace IDs with localized values.
      const self = this;
      args = args.map(function(str) {
        return self.i18nExists(str) ? loadTimeData.getString(str) : str;
      });
    }
    return this.i18nDynamic.apply(this, [locale, id].concat(args));
  },

  /**
   * Returns true if a translation exists for |id|.
   * @param {string} id
   * @return {boolean}
   */
  i18nExists: function(id) {
    return loadTimeData.valueExists(id);
  },

  /**
   * Call this when UI strings may have changed. This will send an update to
   * any data bindings to i18nDynamic(locale, ...).
   * @suppress {checkTypes}
   */
  i18nUpdateLocale: function() {
    // Force reload.
    this.locale = undefined;
    this.locale = loadTimeData.getString('language');
  },
};

/**
 * TODO(stevenjb): Replace with an interface. b/24294625
 * @typedef {{
 *   i18n: function(string, ...string): string,
 *   i18nAdvanced: function(string, SanitizeInnerHtmlOpts=): string,
 *   i18nDynamic: function(string, string, ...string): string,
 *   i18nExists: function(string),
 *   i18nUpdateLocale: function()
 * }}
 */
I18nBehavior.Proto;
