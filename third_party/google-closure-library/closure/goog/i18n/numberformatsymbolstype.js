/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provide a type definition for the goog.i18n.NumberFormatSymbols
 * objects.
 */

goog.module('goog.i18n.NumberFormatSymbolsType');

/**
 * Number formatting symbols for locale.
 * @record
 */
const Type = class {
  constructor() {
    /** @type {string} */ this.DECIMAL_SEP;  //
    /** @type {string} */ this.GROUP_SEP;    //
    /** @type {string} */ this.PERCENT;      //
    /** @type {string} */ this.ZERO_DIGIT;   //
    /** @type {string} */ this.PLUS_SIGN;
    /** @type {string} */ this.MINUS_SIGN;       //
    /** @type {string} */ this.EXP_SYMBOL;       //
    /** @type {string} */ this.PERMILL;          //
    /** @type {string} */ this.INFINITY;         //
    /** @type {string} */ this.NAN;              //
    /** @type {string} */ this.DECIMAL_PATTERN;  //
    /** @type {string} */ this.SCIENTIFIC_PATTERN;
    /** @type {string} */ this.PERCENT_PATTERN;  //
    /** @type {string} */ this.CURRENCY_PATTERN;
    /** @type {string} */ this.DEF_CURRENCY_CODE;
  }
};

exports.Type = Type;
