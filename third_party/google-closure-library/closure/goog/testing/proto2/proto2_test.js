/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.proto2Test');
goog.setTestOnly();

const TestAllTypes = goog.require('proto2.TestAllTypes');
const proto2 = goog.require('goog.testing.proto2');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testAssertEquals() {
    const assertProto2Equals = proto2.assertEquals;
    assertProto2Equals(new TestAllTypes, new TestAllTypes);
    assertProto2Equals(new TestAllTypes, new TestAllTypes, 'oops');

    let ex = assertThrowsJsUnitException(goog.partial(
        assertProto2Equals, new TestAllTypes, new TestAllTypes.NestedMessage));
    assertEquals(
        'Message type mismatch: TestAllTypes != TestAllTypes.NestedMessage',
        ex.message);

    const message = new TestAllTypes;
    message.setOptionalInt32(1);
    ex = assertThrowsJsUnitException(
        goog.partial(assertProto2Equals, new TestAllTypes, message));
    assertEquals('optional_int32 should not be present', ex.message);

    ex = assertThrowsJsUnitException(
        goog.partial(assertProto2Equals, new TestAllTypes, message, 'oops'));
    assertEquals('oops\noptional_int32 should not be present', ex.message);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testFindDifferences_EmptyMessages() {
    assertEquals(
        '', proto2.findDifferences_(new TestAllTypes, new TestAllTypes, ''));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testFindDifferences_FieldNotPresent() {
    const message = new TestAllTypes;
    message.setOptionalInt32(0);
    const empty = new TestAllTypes;
    assertEquals(
        'optional_int32 should not be present',
        proto2.findDifferences_(empty, message, ''));
    assertEquals(
        'optional_int32 should be present',
        proto2.findDifferences_(message, empty, ''));
    assertEquals(
        'path/optional_int32 should be present',
        proto2.findDifferences_(message, empty, 'path'));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testFindDifferences_IntFieldDiffers() {
    const message1 = new TestAllTypes;
    message1.setOptionalInt32(1);
    const message2 = new TestAllTypes;
    message2.setOptionalInt32(2);
    assertEquals(
        'optional_int32 should be 1, but was 2',
        proto2.findDifferences_(message1, message2, ''));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testFindDifferences_NestedIntFieldDiffers() {
    const message1 = new TestAllTypes;
    const nested1 = new TestAllTypes.NestedMessage();
    nested1.setB(1);
    message1.setOptionalNestedMessage(nested1);
    const message2 = new TestAllTypes;
    const nested2 = new TestAllTypes.NestedMessage();
    nested2.setB(2);
    message2.setOptionalNestedMessage(nested2);
    assertEquals(
        'optional_nested_message/b should be 1, but was 2',
        proto2.findDifferences_(message1, message2, ''));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testFindDifferences_RepeatedFieldLengthDiffers() {
    const message1 = new TestAllTypes;
    message1.addRepeatedInt32(1);
    const message2 = new TestAllTypes;
    message2.addRepeatedInt32(1);
    message2.addRepeatedInt32(2);
    assertEquals(
        'repeated_int32 should have 1 items, but has 2',
        proto2.findDifferences_(message1, message2, ''));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testFindDifferences_RepeatedFieldItemDiffers() {
    const message1 = new TestAllTypes;
    message1.addRepeatedInt32(1);
    const message2 = new TestAllTypes;
    message2.addRepeatedInt32(2);
    assertEquals(
        'repeated_int32[0] should be 1, but was 2',
        proto2.findDifferences_(message1, message2, ''));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testFindDifferences_RepeatedNestedMessageDiffers() {
    const message1 = new TestAllTypes;
    const nested1 = new TestAllTypes.NestedMessage();
    nested1.setB(1);
    message1.addRepeatedNestedMessage(nested1);
    const message2 = new TestAllTypes;
    const nested2 = new TestAllTypes.NestedMessage();
    nested2.setB(2);
    message2.addRepeatedNestedMessage(nested2);
    assertEquals(
        'repeated_nested_message[0]/b should be 1, but was 2',
        proto2.findDifferences_(message1, message2, ''));
  },

  testFromObject() {
    const nested = new TestAllTypes.NestedMessage();
    nested.setB(1);
    const message = new TestAllTypes;
    message.addRepeatedNestedMessage(nested);
    message.setOptionalInt32(2);
    // Successfully deserializes simple as well as message fields.
    assertObjectEquals(
        message,
        proto2.fromObject(
            TestAllTypes,
            {'optional_int32': 2, 'repeated_nested_message': [{'b': 1}]}));
    // Fails if the field name is not recognized.
    assertThrows(() => {
      proto2.fromObject(TestAllTypes, {'unknown': 1});
    });
    // Fails if the value type is wrong in the JSON object.
    assertThrows(() => {
      proto2.fromObject(TestAllTypes, {'optional_int32': '1'});
    });
  },
});
