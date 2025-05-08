/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.i18n.messages_test');
goog.setTestOnly();

const testSuite = goog.require('goog.testing.testSuite');
const {declareIcuTemplate} = goog.require('goog.i18n.messages');

testSuite({
  testClosureStylePlaceholdersForbidden() {
    const actualError = assertThrows(
        () => declareIcuTemplate(
            '{$GREETING}, {NAME}.', {description: 'test description'}));
    assertContains(
        'closure-style placeholder: "{$GREETING}" found in ICU template',
        actualError.toString());
  },

  testUnknownOptionName() {
    const actualError =
        assertThrows(() => declareIcuTemplate('Hello, {NAME}.', {
                       description: 'test description',
                       bad_option_name: {
                         'GREETING': 'Hello',
                         'NAME': 'George',
                       },
                     }));
    assertContains(
        'unknown option name: "bad_option_name"', actualError.toString());
  },

  testExampleForUnknownPlaceholderName() {
    const actualError =
        assertThrows(() => declareIcuTemplate('Hello, {NAME}.', {
                       description: 'test description',
                       example: {
                         'GREETING': 'Hello',
                         'NAME': 'George',
                       },
                     }));
    assertContains(
        'example: unknown placeholder: "GREETING"', actualError.toString());
  },

  testOriginalCodeForUnknownPlaceholderName() {
    const actualError =
        assertThrows(() => declareIcuTemplate('Hello, {NAME}.', {
                       description: 'test description',
                       original_code: {
                         'GREETING': 'Hello',
                         'NAME': 'George',
                       },
                     }));
    assertContains(
        'original_code: unknown placeholder: "GREETING"',
        actualError.toString());
  },

  testReturnValueDescriptionOnly() {
    const templateString = '{GREETING}, {NAME}.';
    assertEquals(
        declareIcuTemplate(templateString, {description: 'test description'}),
        templateString);
  },

  testReturnValueAllOptions() {
    const templateString = '{GREETING}, {NAME}.';
    assertEquals(
        declareIcuTemplate(templateString, {
          description: 'test description',
          meaning: 'nihilism',
          example: {
            'GREETING': 'Hello',
            'NAME': 'George',
          },
          original_code: {
            'GREETING': 'getGreeting()',
            'NAME': 'getName()',
          }
        }),
        templateString);
  },
});
