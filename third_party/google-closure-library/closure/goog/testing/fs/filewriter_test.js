/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.fs.FileWriterTest');
goog.setTestOnly();

const EventObserver = goog.require('goog.testing.events.EventObserver');
const FsBlob = goog.require('goog.testing.fs.Blob');
const FsError = goog.require('goog.fs.Error');
const FsFile = goog.requireType('goog.testing.fs.File');
const FsFileSaver = goog.require('goog.fs.FileSaver');
const FsFileSystem = goog.require('goog.testing.fs.FileSystem');
const FsFileWriter = goog.requireType('goog.testing.fs.FileWriter');
const GoogPromise = goog.require('goog.Promise');
const MockClock = goog.require('goog.testing.MockClock');
const dispose = goog.require('goog.dispose');
const events = goog.require('goog.events');
const googArray = goog.require('goog.array');
const googObject = goog.require('goog.object');
const testSuite = goog.require('goog.testing.testSuite');

const EventType = FsFileSaver.EventType;
const ReadyState = FsFileSaver.ReadyState;

/** @type {!FsFile} */
let file;

/** @type {!FsFileWriter} */
let writer;

/** @type {!MockClock} */
let mockClock;

function waitForEvent(target, type) {
  return new GoogPromise((resolve, reject) => {
    events.listenOnce(target, type, resolve);
  });
}

function assertPositionAndLength(expectedPosition, expectedLength, writer) {
  assertEquals(expectedPosition, writer.getPosition());
  assertEquals(expectedLength, writer.getLength());
}

function assertLastModified(expectedTime, file) {
  assertEquals(expectedTime, file.lastModifiedDate.getTime());
}

function writeString(writer, str) {
  const promise = waitForEvent(writer, FsFileSaver.EventType.WRITE_END);
  writer.write(new FsBlob(str));
  return promise;
}

function createObserver(writer) {
  // Observe all file events fired by the FileWriter.
  const observer = new EventObserver();
  events.listen(writer, googObject.getValues(EventType), observer);
  return observer;
}
testSuite({
  setUp() {
    // Temporarily install the MockClock to get predictable file modified times.
    mockClock = new MockClock(true);
    const fs = new FsFileSystem();
    const fileEntry =
        fs.getRoot().createDirectorySync('foo').createFileSync('bar');
    mockClock.uninstall();

    file = fileEntry.fileSync();
    file.setDataInternal('');

    return fileEntry.createWriter().then((fileWriter) => {
      /** @suppress {checkTypes} suppression added to enable type checking */
      writer = fileWriter;
    });
  },

  tearDown() {
    dispose(writer);
  },

  testWrite() {
    const observer = createObserver(writer);

    mockClock.install();
    assertEquals(ReadyState.INIT, writer.getReadyState());
    assertPositionAndLength(0, 0, writer);
    assertLastModified(0, file);

    mockClock.tick(3);
    let promise = writeString(writer, 'hello');
    assertPositionAndLength(0, 0, writer);
    assertEquals(ReadyState.WRITING, writer.getReadyState());

    promise =
        promise
            .then(() => {
              assertEquals('hello', file.toString());
              assertPositionAndLength(5, 5, writer);
              assertLastModified(3, file);

              assertEquals(ReadyState.DONE, writer.getReadyState());
              assertArrayEquals(
                  [EventType.WRITE_START, EventType.WRITE, EventType.WRITE_END],
                  googArray.map(observer.getEvents(), (e) => e.type));

              const promise = writeString(writer, ' world');
              assertEquals(ReadyState.WRITING, writer.getReadyState());
              mockClock.tick();
              return promise;
            })
            .then(() => {
              assertEquals('hello world', file.toString());
              assertPositionAndLength(11, 11, writer);
              assertLastModified(4, file);

              assertEquals(ReadyState.DONE, writer.getReadyState());
              assertArrayEquals(
                  [
                    EventType.WRITE_START,
                    EventType.WRITE,
                    EventType.WRITE_END,
                    EventType.WRITE_START,
                    EventType.WRITE,
                    EventType.WRITE_END,
                  ],
                  googArray.map(observer.getEvents(), (e) => e.type));
            })
            .thenAlways(() => {
              mockClock.uninstall();
            });

    mockClock.tick();
    return promise;
  },

  testSeek() {
    mockClock.install();
    mockClock.tick(17);
    assertLastModified(0, file);

    const promise = writeString(writer, 'hello world')
                        .then(() => {
                          assertPositionAndLength(11, 11, writer);
                          assertLastModified(17, file);

                          writer.seek(6);
                          assertPositionAndLength(6, 11, writer);

                          const promise = writeString(writer, 'universe');
                          mockClock.tick();
                          return promise;
                        })
                        .then(() => {
                          assertEquals('hello universe', file.toString());
                          assertPositionAndLength(14, 14, writer);

                          writer.seek(500);
                          assertPositionAndLength(14, 14, writer);

                          const promise = writeString(writer, '!');
                          mockClock.tick();
                          return promise;
                        })
                        .then(() => {
                          assertEquals('hello universe!', file.toString());
                          assertPositionAndLength(15, 15, writer);

                          writer.seek(-9);
                          assertPositionAndLength(6, 15, writer);

                          const promise = writeString(writer, 'foo');
                          mockClock.tick();
                          return promise;
                        })
                        .then(() => {
                          assertEquals('hello fooverse!', file.toString());
                          assertPositionAndLength(9, 15, writer);

                          writer.seek(-500);
                          assertPositionAndLength(0, 15, writer);

                          const promise = writeString(writer, 'bye-o');
                          mockClock.tick();
                          return promise;
                        })
                        .then(() => {
                          assertEquals('bye-o fooverse!', file.toString());
                          assertPositionAndLength(5, 15, writer);
                          assertLastModified(21, file);
                        })
                        .thenAlways(() => {
                          mockClock.uninstall();
                        });

    mockClock.tick();
    return promise;
  },

  testAbort() {
    const observer = createObserver(writer);

    mockClock.install();
    mockClock.tick(13);

    let promise = writeString(writer, 'hello world');
    assertEquals(ReadyState.WRITING, writer.getReadyState());
    writer.abort();

    promise = promise
                  .then(() => {
                    assertEquals('', file.toString());

                    assertEquals(ReadyState.DONE, writer.getReadyState());
                    assertPositionAndLength(0, 0, writer);
                    assertLastModified(0, file);

                    assertArrayEquals(
                        [EventType.ERROR, EventType.ABORT, EventType.WRITE_END],
                        googArray.map(observer.getEvents(), (e) => e.type));
                  })
                  .thenAlways(() => {
                    mockClock.uninstall();
                  });

    mockClock.tick();
    return promise;
  },

  testTruncate() {
    // Create the event observer after the initial write is complete.
    let observer;

    mockClock.install();

    const promise =
        writeString(writer, 'hello world')
            .then(() => {
              observer = createObserver(writer);

              writer.truncate(5);
              assertEquals(ReadyState.WRITING, writer.getReadyState());
              assertPositionAndLength(11, 11, writer);
              assertLastModified(0, file);

              const promise = waitForEvent(writer, EventType.WRITE_END);
              mockClock.tick();
              return promise;
            })
            .then(() => {
              assertEquals('hello', file.toString());

              assertEquals(ReadyState.DONE, writer.getReadyState());
              assertPositionAndLength(5, 5, writer);
              assertLastModified(7, file);

              assertArrayEquals(
                  [EventType.WRITE_START, EventType.WRITE, EventType.WRITE_END],
                  googArray.map(observer.getEvents(), (e) => e.type));

              writer.truncate(10);
              const promise = waitForEvent(writer, EventType.WRITE_END);
              mockClock.tick(1);
              return promise;
            })
            .then(() => {
              assertEquals('hello\0\0\0\0\0', file.toString());
              assertLastModified(8, file);
            })
            .thenAlways(() => {
              mockClock.uninstall();
            });

    mockClock.tick(7);
    return promise;
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testAbortBeforeWrite() {
    const err = assertThrows(() => {
      writer.abort();
    });
    assertEquals(FsError.ErrorCode.INVALID_STATE, err.code);
  },

  testAbortAfterWrite() {
    return writeString(writer, 'hello world')
        .then(/**
                 @suppress {strictMissingProperties} suppression added to
                 enable type checking
               */
              () => {
                const err = assertThrows(() => {
                  writer.abort();
                });
                assertEquals(FsError.ErrorCode.INVALID_STATE, err.code);
              });
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testWriteDuringWrite() {
    writer.write(new FsBlob('hello'));
    const err = assertThrows(() => {
      writer.write(new FsBlob('world'));
    });
    assertEquals(FsError.ErrorCode.INVALID_STATE, err.code);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSeekDuringWrite() {
    writer.write(new FsBlob('hello world'));
    const err = assertThrows(() => {
      writer.seek(5);
    });
    assertEquals(FsError.ErrorCode.INVALID_STATE, err.code);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testTruncateDuringWrite() {
    writer.write(new FsBlob('hello world'));
    const err = assertThrows(() => {
      writer.truncate(5);
    });
    assertEquals(FsError.ErrorCode.INVALID_STATE, err.code);
  },
});
