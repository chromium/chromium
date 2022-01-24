/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.fs.BlobTest');
goog.setTestOnly();

const FsBlob = goog.require('goog.testing.fs.Blob');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

const hasArrayBuffer = (globalThis.ArrayBuffer !== undefined);

testSuite({
  /** @suppress {visibility} suppression added to enable type checking */
  testInput() {
    let blob = new FsBlob();
    assertEquals('', blob.toString());
    assertEquals(0, blob.size);

    // input is a string
    blob = new FsBlob('資');
    assertEquals('資', blob.toString());
    assertEquals(3, blob.size);

    if (!hasArrayBuffer) {
      return;
    }

    // input is an Array of Arraybuffer.
    // decimal code for e8 b3 87, that's 資 in UTF-8
    blob = new FsBlob([new Uint8Array([232, 179, 135])]);
    assertEquals('資', blob.toString());
    assertEquals(3, blob.size);

    // input is an Array of Arraybuffer with control characters.
    const data = dom.getWindow().atob('iVBORw0KGgo=');
    const arr = [];
    for (let i = 0; i < data.length; i++) {
      arr.push(data.charCodeAt(i));
    }
    blob = new FsBlob([new Uint8Array(arr)]);
    assertArrayEquals([137, 80, 78, 71, 13, 10, 26, 10], blob.data_);
    assertEquals(8, blob.size);

    // input is an Array of strings
    blob = new FsBlob(['資', 'é']);
    assertEquals('資é', blob.toString());
    assertEquals(5, blob.size);

    // input is an Array of Arraybuffer + string
    blob = new FsBlob([new Uint8Array([232, 179, 135]), 'é']);
    assertEquals('資é', blob.toString());
    assertEquals(5, blob.size);
  },

  testType() {
    let blob = new FsBlob();
    assertEquals('', blob.type);

    blob = new FsBlob('foo bar baz', 'text/plain');
    assertEquals('text/plain', blob.type);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSlice() {
    let blob = new FsBlob('abcdef');
    assertEquals('bc', blob.slice(1, 3).toString());
    assertEquals('def', blob.slice(3, 10).toString());
    assertEquals('abcd', blob.slice(0, -2).toString());
    assertEquals('', blob.slice(10, 1).toString());
    assertEquals('', blob.slice(10, 30).toString());
    assertEquals('b', blob.slice(-5, 2).toString());

    assertEquals('abcdef', blob.slice().toString());
    assertEquals('abc', blob.slice(/* opt_start */ undefined, 3).toString());
    assertEquals('def', blob.slice(3).toString());

    assertEquals('text/plain', blob.slice(1, 2, 'text/plain').type);

    blob = new FsBlob('ab資cd');
    assertEquals('ab資', blob.slice(0, 5).toString());  // 資 is 3-bytes long.
    assertEquals('資', blob.slice(2, 5).toString());
    assertEquals('資cd', blob.slice(2, 10).toString());
    assertEquals('ab', blob.slice(0, -5).toString());
    assertEquals('c', blob.slice(-2, -1).toString());
    assertEquals('資c', blob.slice(-5, -1).toString());

    assertEquals('ab資cd', blob.slice().toString());
    assertEquals('ab資', blob.slice(/* opt_start */ undefined, 5).toString());
    assertEquals('cd', blob.slice(5).toString());
    assertArrayEquals([232], blob.slice(2, 3).data_);  // first byte of 資.
  },

  testToArrayBuffer() {
    if (!hasArrayBuffer) {
      return;
    }
    const blob = new FsBlob('資');
    const buf = new ArrayBuffer(this.size);
    let arr = new Uint8Array(buf);
    arr = [232, 179, 135];
    assertElementsEquals(buf, blob.toArrayBuffer());
  },

  testToDataUrl() {
    const blob = new FsBlob('資', 'text');
    assertEquals('data:text;base64,6LOH', blob.toDataUrl());
  },
});
