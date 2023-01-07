/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.fs.FileEntryTest');
goog.setTestOnly();

const FsFileEntry = goog.require('goog.testing.fs.FileEntry');
const FsFileSystem = goog.require('goog.testing.fs.FileSystem');
const MockClock = goog.require('goog.testing.MockClock');
const testSuite = goog.require('goog.testing.testSuite');

let currentTime;
let file;
let fileEntry;
let fs;
let mockClock;

testSuite({
  setUp() {
    // Temporarily install the MockClock for predictable timestamps on new
    // files.
    mockClock = new MockClock(true);
    fs = new FsFileSystem();
    fileEntry = fs.getRoot().createDirectorySync('foo').createFileSync('bar');

    // Uninstall the MockClock since it interferes with goog.Promise execution.
    // Tests that require specific timing may reinstall the MockClock and
    // manually advance promises using mockClock.tick().
    mockClock.uninstall();
  },

  testIsFile() {
    assertTrue(fileEntry.isFile());
  },

  testIsDirectory() {
    assertFalse(fileEntry.isDirectory());
  },

  testFile() {
    const testFile = new FsFileEntry(fs, fs.getRoot(), 'test', 'hello world');
    return testFile.file().then((f) => {
      assertEquals('test', f.name);
      assertEquals('hello world', f.toString());
    });
  },

  testGetLastModified() {
    // Advance the clock to a known time.
    mockClock.install();
    mockClock.tick(53);
    const testFile =
        new FsFileEntry(fs, fs.getRoot(), 'timeTest', 'hello world');
    const promise = testFile.getLastModified()
                        .then((date) => {
                          assertEquals(53, date.getTime());
                        })
                        .thenAlways(() => {
                          mockClock.uninstall();
                        });
    mockClock.tick();
    return promise;
  },

  testGetMetadata() {
    // Advance the clock to a known time.
    mockClock.install();
    mockClock.tick(54);
    const testFile =
        new FsFileEntry(fs, fs.getRoot(), 'timeTest', 'hello world');
    const promise = testFile.getMetadata()
                        .then((metadata) => {
                          assertEquals(54, metadata.modificationTime.getTime());
                        })
                        .thenAlways(() => {
                          mockClock.uninstall();
                        });
    mockClock.tick();
    return promise;
  },
});
