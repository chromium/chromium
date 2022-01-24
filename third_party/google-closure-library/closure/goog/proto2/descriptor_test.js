/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.proto2.DescriptorTest');
goog.setTestOnly();

const Descriptor = goog.require('goog.proto2.Descriptor');
const Message = goog.require('goog.proto2.Message');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testDescriptorConstruction() {
    const messageType = () => {};
    const descriptor = new Descriptor(
        messageType, {name: 'test', fullName: 'this.is.a.test'}, []);

    assertEquals('test', descriptor.getName());
    assertEquals('this.is.a.test', descriptor.getFullName());
    assertEquals(null, descriptor.getContainingType());
  },

  testParentDescriptor() {
    const parentType = () => {};
    const messageType = () => {};

    const parentDescriptor = new Descriptor(
        parentType, {name: 'parent', fullName: 'this.is.a.parent'}, []);

    parentType.getDescriptor = () => parentDescriptor;

    /** @suppress {checkTypes} suppression added to enable type checking */
    const descriptor = new Descriptor(
        messageType,
        {name: 'test', fullName: 'this.is.a.test', containingType: parentType},
        []);

    assertEquals(parentDescriptor, descriptor.getContainingType());
  },

  testStaticGetDescriptorCachesResults() {
    const messageType = function() {};

    // This method would be provided by proto_library() BUILD rule.
    messageType.prototype.getDescriptor = () => {
      if (!messageType.descriptor_) {
        // The descriptor is created lazily when we instantiate a new instance.
        const descriptorObj = {0: {name: 'test', fullName: 'this.is.a.test'}};
        messageType.descriptor_ =
            Message.createDescriptor(messageType, descriptorObj);
      }
      return messageType.descriptor_;
    };
    messageType.getDescriptor = messageType.prototype.getDescriptor;

    const descriptor = messageType.getDescriptor();
    assertEquals(descriptor, messageType.getDescriptor());  // same instance
  },
});
