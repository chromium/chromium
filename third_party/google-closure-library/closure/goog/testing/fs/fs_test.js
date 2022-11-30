/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.fsTest');
goog.setTestOnly();

const FsBlob = goog.require('goog.testing.fs.Blob');
const fs = goog.require('goog.testing.fs');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testObjectUrls() {
    const blob = fs.getBlob('foo');
    const url = fs.createObjectUrl(blob);
    assertTrue(fs.isObjectUrlGranted(blob));
    fs.revokeObjectUrl(url);
    assertFalse(fs.isObjectUrlGranted(blob));
  },

  testGetBlob() {
    assertEquals(
        new FsBlob('foobarbaz').toString(),
        fs.getBlob('foo', 'bar', 'baz').toString());
    assertEquals(
        new FsBlob('foobarbaz').toString(),
        fs.getBlob('foo', new FsBlob('bar'), 'baz').toString());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetBlobWithProperties() {
    assertEquals(
        'data:spam/eggs;base64,Zm9vYmFy',
        new fs.getBlobWithProperties(['foo', new FsBlob('bar')], 'spam/eggs')
            .toDataUrl());
  },

  testSliceBlob() {
    let myBlob = new FsBlob('0123456789');
    /** @suppress {checkTypes} suppression added to enable type checking */
    let actual = new fs.sliceBlob(myBlob, 1, 3);
    let expected = new FsBlob('12');
    assertEquals(expected.toString(), actual.toString());

    myBlob = new FsBlob('0123456789');
    /** @suppress {checkTypes} suppression added to enable type checking */
    actual = new fs.sliceBlob(myBlob, 0, -1);
    expected = new FsBlob('012345678');
    assertEquals(expected.toString(), actual.toString());
  },
});
