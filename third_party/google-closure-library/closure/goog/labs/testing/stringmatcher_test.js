/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.labs.testing.stringMatcherTest');
goog.setTestOnly();

const MatcherError = goog.require('goog.labs.testing.MatcherError');
const assertThat = goog.require('goog.labs.testing.assertThat');
/** @suppress {extraRequire} */
const matchers = goog.require('goog.labs.testing');
const testSuite = goog.require('goog.testing.testSuite');

function assertMatcherError(callable, errorString) {
  const e = assertThrows(errorString || 'callable throws exception', callable);
  assertTrue(e instanceof MatcherError);
}
testSuite({
  testAnyString() {
    assertThat('foo', anyString(), 'typeof "foo" == "string"');
    assertMatcherError(() => {
      assertThat(1, anyString());
    }, 'typeof 1 == "string"');
  },

  testContainsString() {
    assertThat('hello', containsString('ell'), 'hello contains ell');

    assertMatcherError(() => {
      assertThat('hello', containsString('world!'));
    }, 'containsString should throw exception when it fails');
  },

  testEndsWith() {
    assertThat('hello', endsWith('llo'), 'hello ends with llo');

    assertMatcherError(() => {
      assertThat('minutes', endsWith('midnight'));
    }, 'endsWith should throw exception when it fails');
  },

  testEqualToIgnoringWhitespace() {
    assertThat(
        '    h\n   EL L\tO', equalToIgnoringWhitespace('h el l o'),
        '"   h   EL L\tO   " is equal to "h el l o"');

    assertMatcherError(() => {
      assertThat('hybrid', equalToIgnoringWhitespace('theory'));
    }, 'equalToIgnoringWhitespace should throw exception when it fails');
  },

  testEquals() {
    assertThat('hello', equals('hello'), 'hello equals hello');

    assertMatcherError(() => {
      assertThat('thousand', equals('suns'));
    }, 'equals should throw exception when it fails');
  },

  testStartsWith() {
    assertThat('hello', startsWith('hel'), 'hello starts with hel');

    assertMatcherError(() => {
      assertThat('linkin', startsWith('park'));
    }, 'startsWith should throw exception when it fails');
  },

  testStringContainsInOrder() {
    assertThat(
        'hello', stringContainsInOrder(['h', 'el', 'el', 'l', 'o']),
        'hello contains in order: [h, el, l, o]');

    assertMatcherError(() => {
      assertThat('hybrid', stringContainsInOrder(['hy', 'brid', 'theory']));
    }, 'stringContainsInOrder should throw exception when it fails');
  },

  testMatchesRegex() {
    assertThat('foobar', matchesRegex(/foobar/));
    assertThat('foobar', matchesRegex(/oobar/));

    assertMatcherError(() => {
      assertThat('foo', matchesRegex(/^foobar$/));
    }, 'matchesRegex should throw exception when it fails');
  },
});
