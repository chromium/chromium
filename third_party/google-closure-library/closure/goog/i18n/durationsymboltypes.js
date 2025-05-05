/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview DurationFormatSymbolsTypes supports the duration symbol types
 * to be referenced in the CLDR-version specific data file.
 */
goog.module('goog.i18n.DurationSymbolTypes');

/**
 * A collection of formatting patterns describing how to format each time unit
 * in different styles for a locale. There is one of these per locale in
 * durationsysmbols.js or durationsu,bolsext.js.
 * @typedef {{
 *   YEAR:    !DurationSymbolsFormatStyles,
 *   MONTH:   !DurationSymbolsFormatStyles,
 *   WEEK:    !DurationSymbolsFormatStyles,
 *   DAY:     !DurationSymbolsFormatStyles,
 *   HOUR:    !DurationSymbolsFormatStyles,
 *   MINUTE:  !DurationSymbolsFormatStyles,
 *   SECOND:  !DurationSymbolsFormatStyles,
 * }}
 */
let DurationSymbols;

/** @typedef {!DurationSymbols} */
exports.DurationSymbols;

/**
 * A collection of duration formatting display styles.
 * @typedef {{
 *   LONG:   (string|undefined),
 *   SHORT:  string,
 *   NARROW: (string|undefined),
 * }}
 */
let DurationSymbolsFormatStyles;

/** @typedef {!DurationSymbolsFormatStyles} */
exports.DurationSymbolsFormatStyles;