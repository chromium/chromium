/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.fs.DirectoryEntryTest');
goog.setTestOnly();

const FsDirectoryEntry = goog.require('goog.fs.DirectoryEntry');
const FsError = goog.require('goog.fs.Error');
const FsFileSystem = goog.require('goog.testing.fs.FileSystem');
const MockClock = goog.require('goog.testing.MockClock');
const TestCase = goog.require('goog.testing.TestCase');
const googArray = goog.require('goog.array');
const testSuite = goog.require('goog.testing.testSuite');

const Behavior = FsDirectoryEntry.Behavior;
let dir;
let fs;
let mockClock;

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
    dir = fs.getRoot().createDirectorySync('foo');
    dir.createDirectorySync('subdir').createFileSync('subfile');
    dir.createFileSync('file');
    mockClock.uninstall();
  },

  testIsFile() {
    assertFalse(dir.isFile());
  },

  testIsDirectory() {
    assertTrue(dir.isDirectory());
  },

  testRemoveWithChildren() {
    dir.getFileSync('bar', Behavior.CREATE);
    return dir.remove().then(fail, (e) => {
      assertEquals(FsError.ErrorCode.INVALID_MODIFICATION, e.code);
    });
  },

  testRemoveWithoutChildren() {
    const emptyDir = dir.getDirectorySync('empty', Behavior.CREATE);
    return emptyDir.remove().then(() => {
      assertTrue(emptyDir.deleted);
      assertFalse(fs.getRoot().hasChild('empty'));
    });
  },

  testRemoveRootRecursively() {
    const root = fs.getRoot();
    return root.removeRecursively().then(() => {
      assertTrue(dir.deleted);
      assertFalse(fs.getRoot().deleted);
    });
  },

  testGetFile() {
    return dir.getFile('file')
        .then((file) => {
          assertEquals(dir.getFileSync('file'), file);
          assertEquals('file', file.getName());
          assertEquals('/foo/file', file.getFullPath());
          assertTrue(file.isFile());

          return dir.getLastModified();
        })
        .then((date) => {
          assertEquals(
              'Reading a file should not update the modification date.', 0,
              date.getTime());
          return dir.getMetadata();
        })
        .then((metadata) => {
          assertEquals(
              'Reading a file should not update the metadata.', 0,
              metadata.modificationTime.getTime());
        });
  },

  testGetFileFromSubdir() {
    return dir.getFile('subdir/subfile').then((file) => {
      assertEquals(dir.getDirectorySync('subdir').getFileSync('subfile'), file);
      assertEquals('subfile', file.getName());
      assertEquals('/foo/subdir/subfile', file.getFullPath());
      assertTrue(file.isFile());
    });
  },

  testGetAbsolutePaths() {
    return fs.getRoot()
        .getFile('/foo/subdir/subfile')
        .then((subfile) => {
          assertEquals('/foo/subdir/subfile', subfile.getFullPath());
          return fs.getRoot().getDirectory('//foo////');
        })
        .then((foo) => {
          assertEquals('/foo', foo.getFullPath());
          return foo.getDirectory('/');
        })
        .then((root) => {
          assertEquals('/', root.getFullPath());
          return root.getDirectory('/////');
        })
        .then((root) => {
          assertEquals('/', root.getFullPath());
        });
  },

  testCreateFile() {
    // Advance the clock to an arbitrary, known time.
    mockClock.install();
    mockClock.tick(43);
    const promise = dir.getLastModified()
                        .then((date) => {
                          assertEquals(0, date.getTime());
                        })
                        .then(() => dir.getFile('bar', Behavior.CREATE))
                        .then((file) => {
                          mockClock.tick();
                          assertEquals('bar', file.getName());
                          assertEquals('/foo/bar', file.getFullPath());
                          assertEquals(dir, file.parent);
                          assertTrue(file.isFile());

                          return dir.getLastModified();
                        })
                        .then((date) => {
                          assertEquals(43, date.getTime());
                          return dir.getMetadata();
                        })
                        .then((metadata) => {
                          assertEquals(43, metadata.modificationTime.getTime());
                        })
                        .thenAlways(() => {
                          mockClock.uninstall();
                        });
    mockClock.tick();
    return promise;
  },

  testCreateFileThatAlreadyExists() {
    mockClock.install();
    mockClock.tick(47);
    const existingFile = dir.getFileSync('file');
    const promise = dir.getFile('file', Behavior.CREATE)
                        .then((file) => {
                          assertEquals('file', file.getName());
                          assertEquals('/foo/file', file.getFullPath());
                          assertEquals(dir, file.parent);
                          assertEquals(existingFile, file);
                          assertTrue(file.isFile());

                          return dir.getLastModified();
                        })
                        .then((date) => {
                          assertEquals(47, date.getTime());
                          return dir.getMetadata();
                        })
                        .then((metadata) => {
                          assertEquals(47, metadata.modificationTime.getTime());
                        })
                        .thenAlways(() => {
                          mockClock.uninstall();
                        });
    mockClock.tick();
    return promise;
  },

  testCreateFileInSubdir() {
    return dir.getFile('subdir/bar', Behavior.CREATE).then((file) => {
      assertEquals('bar', file.getName());
      assertEquals('/foo/subdir/bar', file.getFullPath());
      assertEquals(dir.getDirectorySync('subdir'), file.parent);
      assertTrue(file.isFile());
    });
  },

  testCreateFileExclusive() {
    return dir.getFile('bar', Behavior.CREATE_EXCLUSIVE).then((file) => {
      assertEquals('bar', file.getName());
      assertEquals('/foo/bar', file.getFullPath());
      assertEquals(dir, file.parent);
      assertTrue(file.isFile());
    });
  },

  testGetNonExistentFile() {
    return dir.getFile('bar').then(fail, (e) => {
      assertEquals(FsError.ErrorCode.NOT_FOUND, e.code);
    });
  },

  testGetNonExistentFileInSubdir() {
    return dir.getFile('subdir/bar').then(fail, (e) => {
      assertEquals(FsError.ErrorCode.NOT_FOUND, e.code);
    });
  },

  testGetFileInNonExistentSubdir() {
    return dir.getFile('bar/subfile').then(fail, (e) => {
      assertEquals(FsError.ErrorCode.NOT_FOUND, e.code);
    });
  },

  testGetFileThatsActuallyADirectory() {
    return dir.getFile('subdir').then(fail, (e) => {
      assertEquals(FsError.ErrorCode.TYPE_MISMATCH, e.code);
    });
  },

  testCreateFileInNonExistentSubdir() {
    return dir.getFile('bar/newfile', Behavior.CREATE).then(fail, (e) => {
      assertEquals(FsError.ErrorCode.NOT_FOUND, e.code);
    });
  },

  testCreateFileThatsActuallyADirectory() {
    return dir.getFile('subdir', Behavior.CREATE).then(fail, (e) => {
      assertEquals(FsError.ErrorCode.TYPE_MISMATCH, e.code);
    });
  },

  testCreateExclusiveExistingFile() {
    return dir.getFile('file', Behavior.CREATE_EXCLUSIVE).then(fail, (e) => {
      assertEquals(FsError.ErrorCode.INVALID_MODIFICATION, e.code);
    });
  },

  testListEmptyDirectory() {
    const emptyDir = fs.getRoot().getDirectorySync('empty', Behavior.CREATE);

    return emptyDir.listDirectory().then((entryList) => {
      assertSameElements([], entryList);
    });
  },

  testListDirectory() {
    const root = fs.getRoot();
    root.getDirectorySync('dir1', Behavior.CREATE);
    root.getDirectorySync('dir2', Behavior.CREATE);
    root.getFileSync('file1', Behavior.CREATE);
    root.getFileSync('file2', Behavior.CREATE);

    return fs.getRoot().listDirectory().then((entryList) => {
      assertSameElements(
          ['dir1', 'dir2', 'file1', 'file2', 'foo'],
          googArray.map(entryList, (entry) => entry.getName()));
    });
  },

  testCreatePath() {
    return dir.createPath('baz/bat')
        .then((batDir) => {
          assertEquals('/foo/baz/bat', batDir.getFullPath());
          return batDir.createPath('../zazzle');
        })
        .then((zazzleDir) => {
          assertEquals('/foo/baz/zazzle', zazzleDir.getFullPath());
          return zazzleDir.createPath('/elements/actinides/neptunium/');
        })
        .then((elDir) => {
          assertEquals('/elements/actinides/neptunium', elDir.getFullPath());
        });
  },
});
