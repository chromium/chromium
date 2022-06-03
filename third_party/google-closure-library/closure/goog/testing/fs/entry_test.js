/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.fs.EntryTest');
goog.setTestOnly();

const FsDirectoryEntry = goog.require('goog.fs.DirectoryEntry');
const FsError = goog.require('goog.fs.Error');
const FsFileSystem = goog.require('goog.testing.fs.FileSystem');
const MockClock = goog.require('goog.testing.MockClock');
const TestCase = goog.require('goog.testing.TestCase');
const testSuite = goog.require('goog.testing.testSuite');

let file;
let fs;
let mockClock;

function assertFailsWhenDeleted(fn) {
  return file.remove().then(fn).then(
      () => {
        fail('Expected an error');
      },
      (err) => {
        assertEquals(FsError.ErrorCode.NOT_FOUND, err.code);
      });
}
testSuite({
  setUpPage() {
    // This test has a tendency to timeout on external Travis testing
    // infrastructure. Up to 5s from 1s.
    TestCase.getActiveTestCase().promiseTimeout = 5000;
  },

  setUp() {
    // Install the MockClock to create predictable timestamps for new files.
    mockClock = new MockClock(true);

    fs = new FsFileSystem();
    file = fs.getRoot()
               .getDirectorySync('foo', FsDirectoryEntry.Behavior.CREATE)
               .getFileSync('bar', FsDirectoryEntry.Behavior.CREATE);

    // Uninstall the MockClock since it interferes with goog.Promise execution.
    // Tests that require specific timing may reinstall the MockClock and
    // manually advance promises using mockClock.tick().
    mockClock.uninstall();
  },

  testGetName() {
    assertEquals('bar', file.getName());
  },

  testGetFullPath() {
    assertEquals('/foo/bar', file.getFullPath());
    assertEquals('/', fs.getRoot().getFullPath());
  },

  testGetFileSystem() {
    assertEquals(fs, file.getFileSystem());
  },

  testMoveTo() {
    return file.moveTo(fs.getRoot()).then((newFile) => {
      assertTrue(file.deleted);
      assertFalse(newFile.deleted);
      assertEquals('/bar', newFile.getFullPath());
      assertEquals(fs.getRoot(), newFile.parent);
      assertEquals(newFile, fs.getRoot().getFileSync('bar'));
      assertFalse(fs.getRoot().getDirectorySync('foo').hasChild('bar'));
    });
  },

  testMoveToNewName() {
    // Advance the clock to an arbitrary, known time.
    mockClock.install();
    mockClock.tick(71);
    const promise =
        file.moveTo(fs.getRoot(), 'baz')
            .then((newFile) => {
              mockClock.tick();
              assertTrue(file.deleted);
              assertFalse(newFile.deleted);
              assertEquals('/baz', newFile.getFullPath());
              assertEquals(fs.getRoot(), newFile.parent);
              assertEquals(newFile, fs.getRoot().getFileSync('baz'));

              const oldParentDir = fs.getRoot().getDirectorySync('foo');
              assertFalse(oldParentDir.hasChild('bar'));
              assertFalse(oldParentDir.hasChild('baz'));

              return oldParentDir.getLastModified();
            })
            .then((lastModifiedDate) => {
              assertEquals(71, lastModifiedDate.getTime());
              const oldParentDir = fs.getRoot().getDirectorySync('foo');
              return oldParentDir.getMetadata();
            })
            .then((metadata) => {
              assertEquals(71, metadata.modificationTime.getTime());
              return fs.getRoot().getLastModified();
            })
            .then((rootLastModifiedDate) => {
              assertEquals(71, rootLastModifiedDate.getTime());
              return fs.getRoot().getMetadata();
            })
            .then((rootMetadata) => {
              assertEquals(71, rootMetadata.modificationTime.getTime());
            })
            .thenAlways(() => {
              mockClock.uninstall();
            });
    mockClock.tick();
    return promise;
  },

  testMoveDeletedFile() {
    return assertFailsWhenDeleted(() => file.moveTo(fs.getRoot()));
  },

  testCopyTo() {
    mockClock.install();
    mockClock.tick(61);
    const promise =
        file.copyTo(fs.getRoot())
            .then((newFile) => {
              assertFalse(file.deleted);
              assertFalse(newFile.deleted);
              assertEquals('/bar', newFile.getFullPath());
              assertEquals(fs.getRoot(), newFile.parent);
              assertEquals(newFile, fs.getRoot().getFileSync('bar'));

              const oldParentDir = fs.getRoot().getDirectorySync('foo');
              assertEquals(file, oldParentDir.getFileSync('bar'));
              return oldParentDir.getLastModified();
            })
            .then((lastModifiedDate) => {
              assertEquals(
                  'The original parent directory was not modified.', 0,
                  lastModifiedDate.getTime());
              const oldParentDir = fs.getRoot().getDirectorySync('foo');
              return oldParentDir.getMetadata();
            })
            .then((metadata) => {
              assertEquals(
                  'The original parent directory was not modified.', 0,
                  metadata.modificationTime.getTime());
              return fs.getRoot().getLastModified();
            })
            .then((rootLastModifiedDate) => {
              assertEquals(61, rootLastModifiedDate.getTime());
              return fs.getRoot().getMetadata();
            })
            .then((rootMetadata) => {
              assertEquals(61, rootMetadata.modificationTime.getTime());
            })
            .thenAlways(() => {
              mockClock.uninstall();
            });
    mockClock.tick();
    return promise;
  },

  testCopyToNewName() {
    return file.copyTo(fs.getRoot(), 'baz').addCallback((newFile) => {
      assertFalse(file.deleted);
      assertFalse(newFile.deleted);
      assertEquals('/baz', newFile.getFullPath());
      assertEquals(fs.getRoot(), newFile.parent);
      assertEquals(newFile, fs.getRoot().getFileSync('baz'));
      assertEquals(
          file, fs.getRoot().getDirectorySync('foo').getFileSync('bar'));
      assertFalse(fs.getRoot().getDirectorySync('foo').hasChild('baz'));
    });
  },

  testCopyDeletedFile() {
    return assertFailsWhenDeleted(() => file.copyTo(fs.getRoot()));
  },

  testRemove() {
    mockClock.install();
    mockClock.tick(57);
    const promise =
        file.remove()
            .then(() => {
              mockClock.tick();
              const parentDir = fs.getRoot().getDirectorySync('foo');

              assertTrue(file.deleted);
              assertFalse(parentDir.hasChild('bar'));

              return parentDir.getLastModified();
            })
            .then((date) => {
              assertEquals(57, date.getTime());
              const parentDir = fs.getRoot().getDirectorySync('foo');
              return parentDir.getMetadata();
            })
            .then((metadata) => {
              assertEquals(57, metadata.modificationTime.getTime());
            })
            .thenAlways(() => {
              mockClock.uninstall();
            });
    mockClock.tick();
    return promise;
  },

  testRemoveDeletedFile() {
    return assertFailsWhenDeleted(() => file.remove());
  },

  testGetParent() {
    return file.getParent().then((p) => {
      assertEquals(file.parent, p);
      assertEquals(fs.getRoot().getDirectorySync('foo'), p);
      assertEquals('/foo', p.getFullPath());
    });
  },

  testGetDeletedFileParent() {
    return assertFailsWhenDeleted(() => file.getParent());
  },
});
