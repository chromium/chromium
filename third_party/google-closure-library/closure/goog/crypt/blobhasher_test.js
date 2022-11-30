/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.crypt.BlobHasherTest');
goog.setTestOnly();

const BlobHasher = goog.require('goog.crypt.BlobHasher');
const Md5 = goog.require('goog.crypt.Md5');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const crypt = goog.require('goog.crypt');
const events = goog.require('goog.events');
const fs = goog.require('goog.fs');
const testSuite = goog.require('goog.testing.testSuite');

/**
 * A browser-independent mock of goog.fs.sliceBlob. The actual implementation
 * calls the underlying slice method differently based on browser version.
 * This mock does not support negative opt_end.
 */
const fsSliceBlobMock = (blob, start, end = undefined) => {
  if (typeof end !== 'number') {
    end = blob.size;
  }
  return blob.slice(start, end);
};

// Mock out the Blob using a string.
class BlobMock {
  /** @param {string} string */
  constructor(string) {
    this.data = string;
    this.size = this.data.length;
  }

  slice(start, end) {
    return new BlobMock(this.data.substr(start, end - start));
  }
}

// Mock out the FileReader to have control over the flow.
class FileReaderMock {
  constructor() {
    this.array_ = [];
    this.result = null;
    this.readyState = this.EMPTY;

    this.onload = null;
    this.onabort = null;
    this.onerror = null;

    this.EMPTY = 0;
    this.LOADING = 1;
    this.DONE = 2;
  }

  mockLoad() {
    this.readyState = this.DONE;
    this.result = this.array_;
    if (this.onload) {
      this.onload.call();
    }
  }

  abort() {
    this.readyState = this.DONE;
    if (this.onabort) {
      this.onabort.call();
    }
  }

  mockError() {
    this.readyState = this.DONE;
    if (this.onerror) {
      this.onerror.call();
    }
  }

  readAsArrayBuffer(blobMock) {
    this.readyState = this.LOADING;
    this.array_ = [];
    for (let i = 0; i < blobMock.size; ++i) {
      this.array_[i] = blobMock.data.charCodeAt(i);
    }
  }

  isLoading() {
    return this.readyState == this.LOADING;
  }
}

const stubs = new PropertyReplacer();

/**
 * Makes the blobHasher read chunks from the blob and hash it. The number of
 * reads shall not exceed a pre-determined number (typically blob size / chunk
 * size) for computing hash. This function fails fast (after maxReads is
 * reached), assuming that the hasher failed to generate hashes. This prevents
 * the test suite from going into infinite loop.
 * @param {!BlobHasher} blobHasher Hasher in action.
 * @param {number} maxReads Max number of read attempts.
 * @suppress {visibility,missingProperties} suppression added to enable type
 * checking
 */
function readFromBlob(blobHasher, maxReads) {
  let counter = 0;
  while (blobHasher.fileReader_ && blobHasher.fileReader_.isLoading() &&
         counter <= maxReads) {
    blobHasher.fileReader_.mockLoad();
    counter++;
  }
  assertTrue(counter <= maxReads);
  return counter;
}

testSuite({
  setUp() {
    stubs.set(globalThis, 'FileReader', FileReaderMock);
    stubs.set(fs, 'sliceBlob', fsSliceBlobMock);
  },

  tearDown() {
    stubs.reset();
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testBasicOperations() {
    if (!window.Blob) {
      return;
    }

    // Test hashing with one chunk.
    const hashFn = new Md5();
    let blobHasher = new BlobHasher(hashFn);
    let blob = new BlobMock('The quick brown fox jumps over the lazy dog');
    blobHasher.hash(blob);
    readFromBlob(blobHasher, 1);
    assertEquals(
        '9e107d9d372bb6826bd81d3542a419d6',
        crypt.byteArrayToHex(blobHasher.getHash()));

    // Test hashing with multiple chunks.
    blobHasher = new BlobHasher(hashFn, 7);
    blobHasher.hash(blob);
    readFromBlob(blobHasher, Math.ceil(blob.size / 7));
    assertEquals(
        '9e107d9d372bb6826bd81d3542a419d6',
        crypt.byteArrayToHex(blobHasher.getHash()));

    // Test hashing with no chunks.
    blob = new BlobMock('');
    blobHasher.hash(blob);
    readFromBlob(blobHasher, 1);
    assertEquals(
        'd41d8cd98f00b204e9800998ecf8427e',
        crypt.byteArrayToHex(blobHasher.getHash()));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testNormalFlow() {
    if (!window.Blob) {
      return;
    }

    // Test the flow with one chunk.
    const hashFn = new Md5();
    const blobHasher = new BlobHasher(hashFn, 13);
    let blob = new BlobMock('short');
    let startedEvents = 0;
    let progressEvents = 0;
    let completeEvents = 0;
    events.listen(blobHasher, BlobHasher.EventType.STARTED, () => {
      ++startedEvents;
    });
    events.listen(blobHasher, BlobHasher.EventType.PROGRESS, () => {
      ++progressEvents;
    });
    events.listen(blobHasher, BlobHasher.EventType.COMPLETE, () => {
      ++completeEvents;
    });
    blobHasher.hash(blob);
    assertEquals(1, startedEvents);
    assertEquals(0, progressEvents);
    assertEquals(0, completeEvents);
    readFromBlob(blobHasher, 1);
    assertEquals(1, startedEvents);
    assertEquals(1, progressEvents);
    assertEquals(1, completeEvents);

    // Test the flow with multiple chunks.
    blob = new BlobMock('The quick brown fox jumps over the lazy dog');
    startedEvents = 0;
    progressEvents = 0;
    completeEvents = 0;
    let progressLoops = 0;
    blobHasher.hash(blob);
    assertEquals(1, startedEvents);
    assertEquals(0, progressEvents);
    assertEquals(0, completeEvents);
    progressLoops = readFromBlob(blobHasher, Math.ceil(blob.size / 13));
    assertEquals(1, startedEvents);
    assertEquals(progressLoops, progressEvents);
    assertEquals(1, completeEvents);
  },

  /**
     @suppress {checkTypes,visibility,missingProperties} suppression added to
     enable type checking
   */
  testAbortsAndErrors() {
    if (!window.Blob) {
      return;
    }

    const hashFn = new Md5();
    const blobHasher = new BlobHasher(hashFn, 13);
    const blob = new BlobMock('The quick brown fox jumps over the lazy dog');
    let abortEvents = 0;
    let errorEvents = 0;
    let completeEvents = 0;
    events.listen(blobHasher, BlobHasher.EventType.ABORT, () => {
      ++abortEvents;
    });
    events.listen(blobHasher, BlobHasher.EventType.ERROR, () => {
      ++errorEvents;
    });
    events.listen(blobHasher, BlobHasher.EventType.COMPLETE, () => {
      ++completeEvents;
    });

    // Immediate abort.
    blobHasher.hash(blob);
    assertEquals(0, abortEvents);
    assertEquals(0, errorEvents);
    assertEquals(0, completeEvents);
    blobHasher.abort();
    blobHasher.abort();
    assertEquals(1, abortEvents);
    assertEquals(0, errorEvents);
    assertEquals(0, completeEvents);
    abortEvents = 0;

    // Delayed abort.
    blobHasher.hash(blob);
    blobHasher.fileReader_.mockLoad();
    assertEquals(0, abortEvents);
    assertEquals(0, errorEvents);
    assertEquals(0, completeEvents);
    blobHasher.abort();
    blobHasher.abort();
    assertEquals(1, abortEvents);
    assertEquals(0, errorEvents);
    assertEquals(0, completeEvents);
    abortEvents = 0;

    // Immediate error.
    blobHasher.hash(blob);
    blobHasher.fileReader_.mockError();
    assertEquals(0, abortEvents);
    assertEquals(1, errorEvents);
    assertEquals(0, completeEvents);
    errorEvents = 0;

    // Delayed error.
    blobHasher.hash(blob);
    blobHasher.fileReader_.mockLoad();
    blobHasher.fileReader_.mockError();
    assertEquals(0, abortEvents);
    assertEquals(1, errorEvents);
    assertEquals(0, completeEvents);
    abortEvents = 0;
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testBasicThrottling() {
    if (!window.Blob) {
      return;
    }

    const hashFn = new Md5();
    const blobHasher = new BlobHasher(hashFn, 5);
    const blob = new BlobMock('The quick brown fox jumps over the lazy dog');
    let throttledEvents = 0;
    let completeEvents = 0;
    events.listen(blobHasher, BlobHasher.EventType.THROTTLED, () => {
      ++throttledEvents;
    });
    events.listen(blobHasher, BlobHasher.EventType.COMPLETE, () => {
      ++completeEvents;
    });

    // Start a throttled hash. No chunks should be processed yet.
    blobHasher.setHashingLimit(0);
    assertEquals(0, throttledEvents);
    blobHasher.hash(blob);
    assertEquals(1, throttledEvents);
    assertEquals(0, blobHasher.getBytesProcessed());
    assertNull(blobHasher.fileReader_);

    // One chunk should be processed.
    blobHasher.setHashingLimit(4);
    assertEquals(1, throttledEvents);
    assertEquals(1, readFromBlob(blobHasher, 1));
    assertEquals(2, throttledEvents);
    assertEquals(4, blobHasher.getBytesProcessed());

    // One more chunk should be processed.
    blobHasher.setHashingLimit(5);
    assertEquals(2, throttledEvents);
    assertEquals(1, readFromBlob(blobHasher, 1));
    assertEquals(3, throttledEvents);
    assertEquals(5, blobHasher.getBytesProcessed());

    // Two more chunks should be processed.
    blobHasher.setHashingLimit(15);
    assertEquals(3, throttledEvents);
    assertEquals(2, readFromBlob(blobHasher, 2));
    assertEquals(4, throttledEvents);
    assertEquals(15, blobHasher.getBytesProcessed());

    // The entire blob should be processed.
    blobHasher.setHashingLimit(Infinity);
    const expectedChunks = Math.ceil(blob.size / 5) - 3;
    assertEquals(expectedChunks, readFromBlob(blobHasher, expectedChunks));
    assertEquals(4, throttledEvents);
    assertEquals(1, completeEvents);
    assertEquals(
        '9e107d9d372bb6826bd81d3542a419d6',
        crypt.byteArrayToHex(blobHasher.getHash()));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testLengthZeroThrottling() {
    if (!window.Blob) {
      return;
    }

    const hashFn = new Md5();
    const blobHasher = new BlobHasher(hashFn);
    let throttledEvents = 0;
    let completeEvents = 0;
    events.listen(blobHasher, BlobHasher.EventType.THROTTLED, () => {
      ++throttledEvents;
    });
    events.listen(blobHasher, BlobHasher.EventType.COMPLETE, () => {
      ++completeEvents;
    });

    // Test throttling with length 0 blob.
    const blob = new BlobMock('');
    blobHasher.setHashingLimit(0);
    blobHasher.hash(blob);
    assertEquals(0, throttledEvents);
    assertEquals(1, completeEvents);
    assertEquals(
        'd41d8cd98f00b204e9800998ecf8427e',
        crypt.byteArrayToHex(blobHasher.getHash()));
  },

  /**
     @suppress {checkTypes,visibility,missingProperties} suppression added to
     enable type checking
   */
  testAbortsAndErrorsWhileThrottling() {
    if (!window.Blob) {
      return;
    }

    const hashFn = new Md5();
    const blobHasher = new BlobHasher(hashFn, 5);
    const blob = new BlobMock('The quick brown fox jumps over the lazy dog');
    let abortEvents = 0;
    let errorEvents = 0;
    let throttledEvents = 0;
    let completeEvents = 0;
    events.listen(blobHasher, BlobHasher.EventType.ABORT, () => {
      ++abortEvents;
    });
    events.listen(blobHasher, BlobHasher.EventType.ERROR, () => {
      ++errorEvents;
    });
    events.listen(blobHasher, BlobHasher.EventType.THROTTLED, () => {
      ++throttledEvents;
    });
    events.listen(blobHasher, BlobHasher.EventType.COMPLETE, () => {
      ++completeEvents;
    });

    // Test that processing cannot be continued after abort.
    blobHasher.setHashingLimit(0);
    blobHasher.hash(blob);
    assertEquals(1, throttledEvents);
    blobHasher.abort();
    assertEquals(1, abortEvents);
    blobHasher.setHashingLimit(10);
    assertNull(blobHasher.fileReader_);
    assertEquals(1, throttledEvents);
    assertEquals(0, completeEvents);
    assertNull(blobHasher.getHash());

    // Test that processing cannot be continued after error.
    blobHasher.hash(blob);
    assertEquals(1, throttledEvents);
    blobHasher.fileReader_.mockError();
    assertEquals(1, errorEvents);
    blobHasher.setHashingLimit(100);
    assertNull(blobHasher.fileReader_);
    assertEquals(1, throttledEvents);
    assertEquals(0, completeEvents);
    assertNull(blobHasher.getHash());
  },
});
