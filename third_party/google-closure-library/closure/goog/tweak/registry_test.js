/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.tweak.RegistryTest');
goog.setTestOnly();

const AssertionError = goog.require('goog.asserts.AssertionError');
const testSuite = goog.require('goog.testing.testSuite');
/** @suppress {extraRequire} needed for createRegistryEntries. */
const testhelpers = goog.require('goog.tweak.testhelpers');
const tweak = goog.require('goog.tweak');

let registry;

testSuite({
  setUp() {
    createRegistryEntries('');
    registry = tweak.getRegistry();
  },

  tearDown() {
    /** @suppress {visibility} suppression added to enable type checking */
    tweak.registry_ = null;
  },

  testGetBaseEntry() {
    // Initial values
    assertEquals(
        'wrong bool1 object', boolEntry, registry.getBooleanSetting('Bool'));
    assertEquals(
        'wrong string object', strEntry, registry.getStringSetting('Str'));
    assertEquals(
        'wrong numeric object', numEntry, registry.getNumericSetting('Num'));
    assertEquals(
        'wrong button object', buttonEntry, registry.getEntry('Button'));
    assertEquals(
        'wrong button object', boolGroup, registry.getEntry('BoolGroup'));
  },

  testInitializeFromQueryParams() {
    const testCase = 0;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    function assertQuery(
        queryStr, boolValue, enumValue, strValue, subBoolValue, subBoolValue2) {
      createRegistryEntries(queryStr);
      assertEquals(
          `Wrong bool value for query: ${queryStr}`, boolValue,
          boolEntry.getValue());
      assertEquals(
          `Wrong enum value for query: ${queryStr}`, enumValue,
          strEnumEntry.getValue());
      assertEquals(
          `Wrong str value for query: ${queryStr}`, strValue,
          strEntry.getValue());
      assertEquals(
          `Wrong BoolOne value for query: ${queryStr}`, subBoolValue,
          boolOneEntry.getValue());
      assertEquals(
          `Wrong BoolTwo value for query: ${queryStr}`, subBoolValue2,
          boolTwoEntry.getValue());
    }
    assertQuery('?dummy=1&bool=&enum=&s=', false, '', '', false, true);
    assertQuery('?bool=0&enum=A&s=a', false, 'A', 'a', false, true);
    assertQuery('?bool=1&enum=A', true, 'A', '', false, true);
    assertQuery('?bool=true&enum=B&s=as+df', true, 'B', 'as df', false, true);
    assertQuery('?enum=C', false, 'C', '', false, true);
    assertQuery('?enum=C&boolgroup=-booltwo', false, 'C', '', false, false);
    assertQuery('?enum=C&boolgroup=b1,-booltwo', false, 'C', '', true, false);
    assertQuery('?enum=C&boolgroup=b1', false, 'C', '', true, true);
    assertQuery('?s=a+b%20c%26', false, 'A', 'a b c&', false, true);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testMakeUrlQuery() {
    assertEquals('All values are default.', '', registry.makeUrlQuery(''));
    assertEquals(
        'All values are default - with existing params.', '?super=pudu',
        registry.makeUrlQuery('?super=pudu'));

    boolEntry.setValue(true);
    numEnumEntry.setValue(2);
    strEntry.setValue('f o&o');
    assertEquals(
        'Wrong query string 1.', '?bool=1&enum2=2&s=f+o%26o',
        registry.makeUrlQuery('?bool=1'));
    assertEquals(
        'Wrong query string 1 - with existing params.',
        '?super=pudu&bool=1&enum2=2&s=f+o%26o',
        registry.makeUrlQuery('?bool=0&s=g&super=pudu'));

    boolOneEntry.setValue(true);
    assertEquals(
        'Wrong query string 2.', '?bool=1&boolgroup=B1&enum2=2&s=f+o%26o',
        registry.makeUrlQuery(''));

    boolTwoEntry.setValue(false);
    assertEquals(
        'Wrong query string 3.',
        '?bool=1&boolgroup=B1,-booltwo&enum2=2&s=f+o%26o',
        registry.makeUrlQuery(''));
  },
});
