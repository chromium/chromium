/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.fs.FileReaderTest');
goog.setTestOnly();

const EventObserver = goog.require('goog.testing.events.EventObserver');
const FsError = goog.require('goog.fs.Error');
const FsFile = goog.requireType('goog.testing.fs.File');
const FsFileReader = goog.require('goog.fs.FileReader');
const FsFileSystem = goog.require('goog.testing.fs.FileSystem');
const GoogPromise = goog.require('goog.Promise');
const TestingFsFileReader = goog.require('goog.testing.fs.FileReader');
const dispose = goog.require('goog.dispose');
const events = goog.require('goog.events');
const googObject = goog.require('goog.object');
const testSuite = goog.require('goog.testing.testSuite');

const EventType = FsFileReader.EventType;
const ReadyState = FsFileReader.ReadyState;

/** @type {!FsFile} */
let file;

/** @type {!TestingFsFileReader} */
let reader;

/** @type {!EventObserver} */
let observer;

/** @const */
const hasArrayBuffer = (globalThis.ArrayBuffer !== undefined);

testSuite({
  setUp() {
    const observedEvents = [];
    const fs = new FsFileSystem();
    const fileEntry =
        fs.getRoot().createDirectorySync('foo').createFileSync('bar');

    file = fileEntry.fileSync();
    file.setDataInternal('test content');

    reader = new TestingFsFileReader();

    // Observe all file events fired by the FileReader.
    observer = new EventObserver();
    events.listen(reader, googObject.getValues(EventType), observer);
  },

  tearDown() {
    dispose(reader);
  },

  testRead() {
    assertEquals(ReadyState.INIT, reader.getReadyState());
    assertUndefined(reader.getResult());

    return new GoogPromise((resolve, reject) => {
             events.listen(reader, EventType.LOAD_END, resolve);
             reader.readAsText(file);
             assertEquals(ReadyState.LOADING, reader.getReadyState());
           })
        .then((result) => {
          assertEquals(file.toString(), reader.getResult());

          assertEquals(ReadyState.DONE, reader.getReadyState());
          assertArrayEquals(
              [
                EventType.LOAD_START,
                EventType.LOAD,
                EventType.LOAD,
                EventType.LOAD,
                EventType.LOAD_END,
              ],
              observer.getEvents().map(e => e.type));
        });
  },

  testReadAsArrayBuffer() {
    if (!hasArrayBuffer) {
      // Skip if array buffer is not supported
      return;
    }

    return new GoogPromise((resolve, reject) => {
             events.listen(reader, EventType.LOAD_END, resolve);
             reader.readAsArrayBuffer(file);
             assertEquals(ReadyState.LOADING, reader.getReadyState());
           })
        .then(/**
                 @suppress {checkTypes} suppression added to enable type
                 checking
               */
              (result) => {
                assertElementsEquals(file.toArrayBuffer(), reader.getResult());

                assertEquals(ReadyState.DONE, reader.getReadyState());
                assertArrayEquals(
                    [
                      EventType.LOAD_START,
                      EventType.LOAD,
                      EventType.LOAD,
                      EventType.LOAD,
                      EventType.LOAD_END,
                    ],
                    observer.getEvents().map(e => e.type));
              });
  },

  testReadAsDataUrl() {
    return new GoogPromise((resolve, reject) => {
             events.listen(reader, EventType.LOAD_END, resolve);
             reader.readAsDataUrl(file);
             assertEquals(ReadyState.LOADING, reader.getReadyState());
           })
        .then((result) => {
          assertEquals(file.toDataUrl(), reader.getResult());

          assertEquals(ReadyState.DONE, reader.getReadyState());
          assertArrayEquals(
              [
                EventType.LOAD_START,
                EventType.LOAD,
                EventType.LOAD,
                EventType.LOAD,
                EventType.LOAD_END,
              ],
              observer.getEvents().map(e => e.type));
        });
  },

  testAbort() {
    return new GoogPromise((resolve, reject) => {
             events.listen(reader, EventType.LOAD_END, resolve);
             reader.readAsText(file);
             assertEquals(ReadyState.LOADING, reader.getReadyState());
             reader.abort();
           })
        .then((result) => {
          assertUndefined(reader.getResult());

          assertEquals(ReadyState.DONE, reader.getReadyState());
          assertArrayEquals(
              [EventType.ERROR, EventType.ABORT, EventType.LOAD_END],
              observer.getEvents().map(e => e.type));
        });
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testAbortBeforeRead() {
    const err = assertThrows(() => {
      reader.abort();
    });
    assertEquals(FsError.ErrorCode.INVALID_STATE, err.code);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testReadDuringRead() {
    const err = assertThrows(() => {
      reader.readAsText(file);
      reader.readAsText(file);
    });
    assertEquals(FsError.ErrorCode.INVALID_STATE, err.code);
  },
});
