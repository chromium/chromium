/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Utility functions that make testing code using i18n-specific
 * Unicode whitespace characters simpler.
 */

goog.module('goog.testing.i18n.whitespace');
goog.module.declareLegacyNamespace();
goog.setTestOnly('goog.testing.i18n.whitespace');

/**
 * A regular expression for identifying all horizontal white space
 * characters. Same as \h in a Java regex Pattern.
 * @const {!RegExp}
 */
exports.HORIZONTAL_WHITE_SPACE_REGEX = new RegExp(
    '[' +
        // Unicode space
        ' ' +
        // tab character
        '\t' +
        // No-break space
        '\xA0' +
        // Ogham Space Mark
        '\u1680' +
        // Mongolian Vowel Separator
        '\u180e' +
        // En Quad (00)
        // Em Quad (01)
        // En Space (02)
        // Em space (03)
        // Three-per-Em space (04)
        // Four-per-Em space (05)
        // Six-per-Em space (06)
        // Figure space (07)
        // Punctuation space (08)
        // Thin Space (09)
        // Hair Space
        '\u2000-\u200a' +
        // Narrow no-break space
        '\u202f' +
        // Medium Mathematical Space
        '\u205f' +
        // Ideographic space
        '\u3000' +
        ']',
    'g');

/**
 * Normalizes whitespace characters within the input, removing various
 * whitespace characters used for presentational purposes that make testing
 * difficult.
 *
 * @param {string} input
 * @return {string}
 */
exports.removeWhitespace = (input) => {
  return input.replace(exports.HORIZONTAL_WHITE_SPACE_REGEX, '');
};
