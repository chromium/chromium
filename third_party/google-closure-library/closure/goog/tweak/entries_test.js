/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.tweak.BaseEntryTest');
goog.setTestOnly();

const MockControl = goog.require('goog.testing.MockControl');
const testSuite = goog.require('goog.testing.testSuite');
/** @suppress {extraRequire} needed for createRegistryEntries. */
const testhelpers = goog.require('goog.tweak.testhelpers');
const tweak = goog.require('goog.tweak');

let mockControl;

testSuite({
  setUp() {
    mockControl = new MockControl();
  },

  tearDown() {
    /** @suppress {visibility} suppression added to enable type checking */
    tweak.registry_ = null;
    mockControl.$verifyAll();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testGetValue_defaultValues() {
    createRegistryEntries('');
    assertFalse('wrong initial value for bool', boolEntry.getValue());
    assertEquals('wrong initial value for enum', 'A', strEnumEntry.getValue());
    assertEquals('wrong initial value for str', '', strEntry.getValue());

    assertEquals('wrong initial value for bool2', true, boolEntry2.getValue());
    assertEquals('wrong initial value for enum2', 1, numEnumEntry.getValue());
    assertEquals('wrong initial value for str2', 'foo', strEntry2.getValue());

    assertFalse('wrong initial value for BoolOne', boolOneEntry.getValue());
    assertTrue('wrong initial value for BoolTwo', boolTwoEntry.getValue());
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testGetValue_nonDefaultValues() {
    createRegistryEntries('?bool=1&enum=C');
    // These have the restartRequired option set.
    boolEntry.setValue(false);
    strEntry.setValue('foo');
    numEntry.setValue(5);
    assertTrue('wrong value for boolean', boolEntry.getValue());
    assertEquals(
        'wrong value for string', strEntry.getDefaultValue(),
        strEntry.getValue());
    assertEquals(
        'wrong value for num', numEntry.getDefaultValue(), numEntry.getValue());

    // These do not have the restartRequired option set.
    strEnumEntry.setValue('B');
    boolOneEntry.setValue(true);
    assertEquals('wrong value for strEnum', 'B', strEnumEntry.getValue());
    assertEquals('wrong value for boolOne', true, boolOneEntry.getValue());
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testCallbacks() {
    createRegistryEntries('');
    const mockCallback = mockControl.createFunctionMock();
    boolEntry.addCallback(mockCallback);
    boolOneEntry.addCallback(mockCallback);
    strEnumEntry.addCallback(mockCallback);
    numEnumEntry.addCallback(mockCallback);

    mockCallback(boolEntry);
    mockCallback(boolOneEntry);
    mockCallback(strEnumEntry);
    mockCallback(numEnumEntry);
    mockControl.$replayAll();

    boolEntry.setValue(true);
    boolOneEntry.setValue(true);
    strEnumEntry.setValue('C');
    numEnumEntry.setValue(3);
  },
});
