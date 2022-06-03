/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.string.formatTest');
goog.setTestOnly();

const stringFormat = goog.require('goog.string.format');
const testSuite = goog.require('goog.testing.testSuite');

// The discussion on naming this functionality is going on.

testSuite({
  testImmediateFormatSpecifier() {
    assertEquals('Empty String', '', stringFormat(''));
    assertEquals(
        'Immediate Value', 'Immediate Value', stringFormat('Immediate Value'));
  },

  testPercentSign() {
    assertEquals('%', '%', stringFormat('%'));
    assertEquals('%%', '%', stringFormat('%%'));
    assertEquals('%%%', '%%', stringFormat('%%%'));
    assertEquals('%%%%', '%%', stringFormat('%%%%'));

    assertEquals(
        'width of the percent sign ???', '%%', stringFormat('%345%%-67.987%'));
  },

  testStringConversionSpecifier() {
    assertEquals('%s', 'abc', stringFormat('%s', 'abc'));
    assertEquals('%2s', 'abc', stringFormat('%2s', 'abc'));
    assertEquals('%6s', '   abc', stringFormat('%6s', 'abc'));
    assertEquals('%-6s', 'abc   ', stringFormat('%-6s', 'abc'));
  },

  testFloatConversionSpecifier() {
    assertEquals('%f', '123', stringFormat('%f', 123));
    assertEquals('%f', '0.1', stringFormat('%f', 0.1));
    assertEquals('%f', '123.456', stringFormat('%f', 123.456));

    // Precisions, paddings and other flags are handled on a flag to flag basis.
  },

  testAliasedConversionSpecifiers() {
    assertEquals('%i vs. %d', stringFormat('%i', 123), stringFormat('%d', 123));
    assertEquals('%u vs. %d', stringFormat('%u', 123), stringFormat('%d', 123));
  },

  testIntegerConversion() {
    assertEquals('%d', '0', stringFormat('%d', 0));

    assertEquals('%d', '123', stringFormat('%d', 123));
    assertEquals('%d', '0', stringFormat('%d', 0.1));
    assertEquals('%d', '0', stringFormat('%d', 0.9));
    assertEquals('%d', '123', stringFormat('%d', 123.456));

    assertEquals('%d', '-1', stringFormat('%d', -1));
    assertEquals('%d', '0', stringFormat('%d', -0.1));
    assertEquals('%d', '0', stringFormat('%d', -0.9));
    assertEquals('%d', '-123', stringFormat('%d', -123.456));

    // Precisions, paddings and other flags are handled on a flag to flag basis.
  },

  testSpaceFlag() {
    assertEquals('zero %+d ', ' 0', stringFormat('% d', 0));

    assertEquals('positive % d ', ' 123', stringFormat('% d', 123));
    assertEquals('negative % d ', '-123', stringFormat('% d', -123));

    assertEquals('positive % 3d', ' 123', stringFormat('% 3d', 123));
    assertEquals('negative % 3d', '-123', stringFormat('% 3d', -123));

    assertEquals('positive % 4d', ' 123', stringFormat('% 4d', 123));
    assertEquals('negative % 4d', '-123', stringFormat('% 4d', -123));

    assertEquals('positive % 6d', '   123', stringFormat('% 6d', 123));
    assertEquals('negative % 6d', '-  123', stringFormat('% 6d', -123));

    assertEquals('positive % f ', ' 123.456', stringFormat('% f', 123.456));
    assertEquals('negative % f ', '-123.456', stringFormat('% f', -123.456));

    assertEquals('positive % .2f ', ' 123.46', stringFormat('% .2f', 123.456));
    assertEquals('negative % .2f ', '-123.46', stringFormat('% .2f', -123.456));

    assertEquals('positive % 6.2f', ' 123.46', stringFormat('% 6.2f', 123.456));
    assertEquals(
        'negative % 6.2f', '-123.46', stringFormat('% 6.2f', -123.456));

    assertEquals('positive % 7.2f', ' 123.46', stringFormat('% 7.2f', 123.456));
    assertEquals(
        'negative % 7.2f', '-123.46', stringFormat('% 7.2f', -123.456));

    assertEquals(
        'positive % 10.2f', '    123.46', stringFormat('% 10.2f', 123.456));
    assertEquals(
        'negative % 10.2f', '-   123.46', stringFormat('% 10.2f', -123.456));

    assertEquals('string % s ', 'abc', stringFormat('% s', 'abc'));
    assertEquals('string % 3s', 'abc', stringFormat('% 3s', 'abc'));
    assertEquals('string % 4s', ' abc', stringFormat('% 4s', 'abc'));
    assertEquals('string % 6s', '   abc', stringFormat('% 6s', 'abc'));
  },

  testPlusFlag() {
    assertEquals('zero %+d ', '+0', stringFormat('%+d', 0));

    assertEquals('positive %+d ', '+123', stringFormat('%+d', 123));
    assertEquals('negative %+d ', '-123', stringFormat('%+d', -123));

    assertEquals('positive %+3d', '+123', stringFormat('%+3d', 123));
    assertEquals('negative %+3d', '-123', stringFormat('%+3d', -123));

    assertEquals('positive %+4d', '+123', stringFormat('%+4d', 123));
    assertEquals('negative %+4d', '-123', stringFormat('%+4d', -123));

    assertEquals('positive %+6d', '+  123', stringFormat('%+6d', 123));
    assertEquals('negative %+6d', '-  123', stringFormat('%+6d', -123));

    assertEquals('positive %+f ', '+123.456', stringFormat('%+f', 123.456));
    assertEquals('negative %+f ', '-123.456', stringFormat('%+f', -123.456));

    assertEquals('positive %+.2f ', '+123.46', stringFormat('%+.2f', 123.456));
    assertEquals('negative %+.2f ', '-123.46', stringFormat('%+.2f', -123.456));

    assertEquals('positive %+6.2f', '+123.46', stringFormat('%+6.2f', 123.456));
    assertEquals(
        'negative %+6.2f', '-123.46', stringFormat('%+6.2f', -123.456));

    assertEquals('positive %+7.2f', '+123.46', stringFormat('%+7.2f', 123.456));
    assertEquals(
        'negative %+7.2f', '-123.46', stringFormat('%+7.2f', -123.456));

    assertEquals(
        'positive %+10.2f', '+   123.46', stringFormat('%+10.2f', 123.456));
    assertEquals(
        'negative %+10.2f', '-   123.46', stringFormat('%+10.2f', -123.456));

    assertEquals('string %+s ', 'abc', stringFormat('%+s', 'abc'));
    assertEquals('string %+3s', 'abc', stringFormat('%+3s', 'abc'));
    assertEquals('string %+4s', ' abc', stringFormat('%+4s', 'abc'));
    assertEquals('string %+6s', '   abc', stringFormat('%+6s', 'abc'));
  },

  testPrecision() {
    assertEquals('%.5d', '0', stringFormat('%.5d', 0));

    assertEquals('%d', '123', stringFormat('%d', 123.456));
    assertEquals('%.2d', '123', stringFormat('%.2d', 123.456));

    assertEquals('%f', '123.456', stringFormat('%f', 123.456));
    assertEquals('%.2f', '123.46', stringFormat('%.2f', 123.456));

    assertEquals('%.3f', '123.456', stringFormat('%.3f', 123.456));
    assertEquals('%.6f', '123.456000', stringFormat('%.6f', 123.456));
    assertEquals('%1.2f', '123.46', stringFormat('%1.2f', 123.456));
    assertEquals('%7.2f', ' 123.46', stringFormat('%7.2f', 123.456));

    assertEquals('%5.6f', '123.456000', stringFormat('%5.6f', 123.456));
    assertEquals('%11.6f', ' 123.456000', stringFormat('%11.6f', 123.456));

    assertEquals('%07.2f', '0123.46', stringFormat('%07.2f', 123.456));
    assertEquals('%+7.2f', '+123.46', stringFormat('%+7.2f', 123.456));
  },

  testZeroFlag() {
    assertEquals('%0s', 'abc', stringFormat('%0s', 'abc'));
    assertEquals('%02s', 'abc', stringFormat('%02s', 'abc'));
    assertEquals('%06s', '   abc', stringFormat('%06s', 'abc'));

    assertEquals('%0d', '123', stringFormat('%0d', 123));
    assertEquals('%0d', '-123', stringFormat('%0d', -123));
    assertEquals('%06d', '000123', stringFormat('%06d', 123));
    assertEquals('%06d', '-00123', stringFormat('%06d', -123));
    assertEquals('%010d', '0000000123', stringFormat('%010d', 123));
    assertEquals('%010d', '-000000123', stringFormat('%010d', -123));
  },

  testFlagMinus() {
    assertEquals('%-s', 'abc', stringFormat('%-s', 'abc'));
    assertEquals('%-2s', 'abc', stringFormat('%-2s', 'abc'));
    assertEquals('%-6s', 'abc   ', stringFormat('%-6s', 'abc'));

    assertEquals('%-d', '123', stringFormat('%-d', 123));
    assertEquals('%-d', '-123', stringFormat('%-d', -123));
    assertEquals('%-2d', '123', stringFormat('%-2d', 123));
    assertEquals('%-2d', '-123', stringFormat('%-2d', -123));
    assertEquals('%-4d', '123 ', stringFormat('%-4d', 123));
    assertEquals('%-4d', '-123', stringFormat('%-4d', -123));

    assertEquals('%-d', '123', stringFormat('%-0d', 123));
    assertEquals('%-d', '-123', stringFormat('%-0d', -123));
    assertEquals('%-4d', '123 ', stringFormat('%-04d', 123));
    assertEquals('%-4d', '-123', stringFormat('%-04d', -123));
  },

  testExceptions() {
    let e = assertThrows(goog.partial(stringFormat, '%f%f', 123.456));
    assertEquals('[goog.string.format] Not enough arguments', e.message);

    e = assertThrows(stringFormat);
    assertEquals('[goog.string.format] Template required', e.message);
  },

  testNonParticipatingGroupHandling() {
    // Firefox supplies empty string instead of undefined for non-participating
    // capture groups. This can trigger bad behavior if a demuxer only checks
    // isNaN(val) and not also val == ''. Check for regressions.
    const format = '%s %d %i %u';
    const expected = '1 2 3 4';
    // Good types
    assertEquals(expected, stringFormat(format, 1, '2', '3', '4'));
    // Bad types
    assertEquals(expected, stringFormat(format, '1', 2, 3, 4));
  },

  testMinusString() {
    const format = '%0.1f%%';
    const expected = '-0.7%';
    assertEquals(expected, stringFormat(format, '-0.723'));
  },
});
