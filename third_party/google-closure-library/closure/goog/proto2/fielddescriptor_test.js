/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.proto2.FieldDescriptorTest');
goog.setTestOnly();

const FieldDescriptor = goog.require('goog.proto2.FieldDescriptor');
const Message = goog.require('goog.proto2.Message');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testFieldDescriptorConstruction() {
    const messageType = {};
    /** @suppress {checkTypes} suppression added to enable type checking */
    const fieldDescriptor = new FieldDescriptor(messageType, 10, {
      name: 'test',
      repeated: true,
      packed: true,
      fieldType: FieldDescriptor.FieldType.INT32,
      type: Number,
    });

    assertEquals(10, fieldDescriptor.getTag());
    assertEquals('test', fieldDescriptor.getName());

    assertEquals(true, fieldDescriptor.isRepeated());

    assertEquals(true, fieldDescriptor.isPacked());

    assertEquals(
        FieldDescriptor.FieldType.INT32, fieldDescriptor.getFieldType());
    assertEquals(Number, fieldDescriptor.getNativeType());
    assertEquals(0, fieldDescriptor.getDefaultValue());
  },

  testGetDefaultValueOfString() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const fieldDescriptor = new FieldDescriptor({}, 10, {
      name: 'test',
      fieldType: FieldDescriptor.FieldType.STRING,
      type: String,
    });

    assertEquals('', fieldDescriptor.getDefaultValue());
  },

  testGetDefaultValueOfBool() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const fieldDescriptor = new FieldDescriptor({}, 10, {
      name: 'test',
      fieldType: FieldDescriptor.FieldType.BOOL,
      type: Boolean,
    });

    assertEquals(false, fieldDescriptor.getDefaultValue());
  },

  testGetDefaultValueOfInt64() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const fieldDescriptor = new FieldDescriptor({}, 10, {
      name: 'test',
      fieldType: FieldDescriptor.FieldType.INT64,
      type: String,
    });

    assertEquals('0', fieldDescriptor.getDefaultValue());
  },

  testRepeatedField() {
    const messageType = {};
    /** @suppress {checkTypes} suppression added to enable type checking */
    const fieldDescriptor = new FieldDescriptor(
        messageType, 10,
        {name: 'test', repeated: true, fieldType: 7, type: Number});

    assertEquals(true, fieldDescriptor.isRepeated());
    assertEquals(false, fieldDescriptor.isRequired());
    assertEquals(false, fieldDescriptor.isOptional());
  },

  testRequiredField() {
    const messageType = {};
    /** @suppress {checkTypes} suppression added to enable type checking */
    const fieldDescriptor = new FieldDescriptor(
        messageType, 10,
        {name: 'test', required: true, fieldType: 7, type: Number});

    assertEquals(false, fieldDescriptor.isRepeated());
    assertEquals(true, fieldDescriptor.isRequired());
    assertEquals(false, fieldDescriptor.isOptional());
  },

  testOptionalField() {
    const messageType = {};
    /** @suppress {checkTypes} suppression added to enable type checking */
    const fieldDescriptor = new FieldDescriptor(
        messageType, 10, {name: 'test', fieldType: 7, type: Number});

    assertEquals(false, fieldDescriptor.isRepeated());
    assertEquals(false, fieldDescriptor.isRequired());
    assertEquals(true, fieldDescriptor.isOptional());
  },

  testContaingType() {
    const MessageType = function() {
      MessageType.base(this, 'constructor');
    };
    goog.inherits(MessageType, Message);

    MessageType.getDescriptor = () => {
      if (!MessageType.descriptor_) {
        // The descriptor is created lazily when we instantiate a new instance.
        const descriptorObj = {
          0: {name: 'test_message', fullName: 'this.is.a.test_message'},
          10: {name: 'test', fieldType: 7, type: Number},
        };
        MessageType.descriptor_ =
            Message.createDescriptor(MessageType, descriptorObj);
      }
      return MessageType.descriptor_;
    };
    MessageType.prototype.getDescriptor = MessageType.getDescriptor;

    const descriptor = MessageType.getDescriptor();
    const fieldDescriptor = descriptor.getFields()[0];
    assertEquals('10', fieldDescriptor.getTag());
    assertEquals(descriptor, fieldDescriptor.getContainingType());
  },
});
