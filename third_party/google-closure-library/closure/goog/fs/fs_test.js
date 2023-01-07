/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.fsTest');
goog.setTestOnly();

const FsDirectoryEntry = goog.require('goog.fs.DirectoryEntry');
const FsError = goog.require('goog.fs.Error');
const FsFileReader = goog.require('goog.fs.FileReader');
const FsFileSaver = goog.require('goog.fs.FileSaver');
const GoogPromise = goog.require('goog.Promise');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const googArray = goog.require('goog.array');
const googFs = goog.require('goog.fs');
const googFsBlob = goog.require('goog.fs.blob');
const googString = goog.require('goog.string');
const testSuite = goog.require('goog.testing.testSuite');

const TEST_DIR = 'goog-fs-test-dir';

const fsExists = (globalThis.requestFileSystem !== undefined) ||
    globalThis.webkitRequestFileSystem !== undefined;
/** @suppress {checkTypes} suppression added to enable type checking */
const deferredFs = fsExists ? googFs.getTemporary() : null;
const stubs = new PropertyReplacer();

function loadTestDir() {
  return deferredFs.then(
      (fs) => fs.getRoot().getDirectory(
          TEST_DIR, FsDirectoryEntry.Behavior.CREATE));
}

function loadFile(filename, behavior) {
  return loadTestDir().then((dir) => dir.getFile(filename, behavior));
}

function loadDirectory(filename, behavior) {
  return loadTestDir().then((dir) => dir.getDirectory(filename, behavior));
}

function startWrite(content, file) {
  return file.createWriter()
      .then(goog.partial(checkReadyState, FsFileSaver.ReadyState.INIT))
      .then((writer) => {
        writer.write(googFsBlob.getBlob(content));
        return writer;
      })
      .then(goog.partial(checkReadyState, FsFileSaver.ReadyState.WRITING));
}

function waitForEvent(type, target) {
  let done;
  const promise = new GoogPromise((_done) => {
    done = _done;
  });
  events.listenOnce(target, type, done);
  return promise;
}

function writeToFile(content, file) {
  return startWrite(content, file)
      .then(goog.partial(waitForEvent, FsFileSaver.EventType.WRITE))
      .then(() => file);
}

function checkFileContent(content, file) {
  return checkFileContentAs(content, 'Text', undefined, file);
}

function checkFileContentWithEncoding(content, encoding, file) {
  return checkFileContentAs(content, 'Text', encoding, file);
}

function checkFileContentAs(content, filetype, encoding, file) {
  return file.file()
      .then((blob) => FsFileReader[`readAs${filetype}`](blob, encoding))
      .then(goog.partial(checkEquals, content));
}

function checkEquals(a, b) {
  if (a instanceof ArrayBuffer && b instanceof ArrayBuffer) {
    assertEquals(a.byteLength, b.byteLength);
    const viewA = new DataView(a);
    const viewB = new DataView(b);
    for (let i = 0; i < a.byteLength; i++) {
      assertEquals(viewA.getUint8(i), viewB.getUint8(i));
    }
  } else {
    assertEquals(a, b);
  }
}

/** @suppress {checkTypes} suppression added to enable type checking */
function checkFileRemoved(filename) {
  return loadFile(filename).then(
      goog.partial(fail, 'expected file to be removed'), (err) => {
        assertEquals(err.code, FsError.ErrorCode.NOT_FOUND);
      });
}

function checkReadyState(expectedState, writer) {
  assertEquals(expectedState, writer.getReadyState());
  return writer;
}

function splitArgs(fn) {
  return (args) => fn(args[0], args[1]);
}
testSuite({
  setUpPage() {
    if (!fsExists) {
      return;
    }

    return loadTestDir().then(null, (err) => {
      let msg;
      if (err.code == FsError.ErrorCode.QUOTA_EXCEEDED) {
        msg = err.message + '. If you\'re using Chrome, you probably need to ' +
            'pass --unlimited-quota-for-files on the command line.';
      } else if (
          err.code == FsError.ErrorCode.SECURITY &&
          window.location.href.match(/^file:/)) {
        msg = err.message + '. file:// URLs can\'t access the filesystem API.';
      } else {
        msg = err.message;
      }
      const body = dom.getDocument().body;
      dom.insertSiblingBefore(
          dom.createDom(TagName.H1, {}, msg), body.childNodes[0]);
    });
  },

  tearDown() {
    if (!fsExists) {
      return;
    }

    return loadTestDir().then((dir) => dir.removeRecursively());
  },

  testUnavailableTemporaryFilesystem() {
    stubs.set(globalThis, 'requestFileSystem', null);
    stubs.set(globalThis, 'webkitRequestFileSystem', null);

    return googFs.getTemporary(1024).then(
        fail, /**
                 @suppress {strictMissingProperties} suppression added to
                 enable type checking
               */
        (e) => {
          assertEquals('File API unsupported', e.message);
        });
  },

  testUnavailablePersistentFilesystem() {
    stubs.set(globalThis, 'requestFileSystem', null);
    stubs.set(globalThis, 'webkitRequestFileSystem', null);

    return googFs.getPersistent(2048).then(
        fail, /**
                 @suppress {strictMissingProperties} suppression added to
                 enable type checking
               */
        (e) => {
          assertEquals('File API unsupported', e.message);
        });
  },

  testIsFile() {
    if (!fsExists) {
      return;
    }

    return loadFile('test', FsDirectoryEntry.Behavior.CREATE)
        .then((fileEntry) => {
          assertFalse(fileEntry.isDirectory());
          assertTrue(fileEntry.isFile());
        });
  },

  testIsDirectory() {
    if (!fsExists) {
      return;
    }

    return loadDirectory('test', FsDirectoryEntry.Behavior.CREATE)
        .then((fileEntry) => {
          assertTrue(fileEntry.isDirectory());
          assertFalse(fileEntry.isFile());
        });
  },

  testReadFileUtf16() {
    if (!fsExists) {
      return;
    }
    const str = 'test content';
    const buf = new ArrayBuffer(str.length * 2);
    const arr = new Uint16Array(buf);
    for (let i = 0; i < str.length; i++) {
      arr[i] = str.charCodeAt(i);
    }

    return loadFile('test', FsDirectoryEntry.Behavior.CREATE)
        .then(goog.partial(writeToFile, arr.buffer))
        .then(goog.partial(checkFileContentWithEncoding, str, 'UTF-16'));
  },

  testReadFileUtf8() {
    if (!fsExists) {
      return;
    }
    const str = 'test content';
    const buf = new ArrayBuffer(str.length);
    const arr = new Uint8Array(buf);
    for (let i = 0; i < str.length; i++) {
      arr[i] = str.charCodeAt(i) & 0xff;
    }

    return loadFile('test', FsDirectoryEntry.Behavior.CREATE)
        .then(goog.partial(writeToFile, arr.buffer))
        .then(goog.partial(checkFileContentWithEncoding, str, 'UTF-8'));
  },

  testReadFileAsArrayBuffer() {
    if (!fsExists) {
      return;
    }
    const str = 'test content';
    const buf = new ArrayBuffer(str.length);
    const arr = new Uint8Array(buf);
    for (let i = 0; i < str.length; i++) {
      arr[i] = str.charCodeAt(i) & 0xff;
    }

    return loadFile('test', FsDirectoryEntry.Behavior.CREATE)
        .then(goog.partial(writeToFile, arr.buffer))
        .then(goog.partial(
            checkFileContentAs, arr.buffer, 'ArrayBuffer', undefined));
  },

  testReadFileAsBinaryString() {
    if (!fsExists) {
      return;
    }
    const str = 'test content';
    const buf = new ArrayBuffer(str.length);
    const arr = new Uint8Array(buf);
    for (let i = 0; i < str.length; i++) {
      arr[i] = str.charCodeAt(i);
    }

    return loadFile('test', FsDirectoryEntry.Behavior.CREATE)
        .then(goog.partial(writeToFile, arr.buffer))
        .then(goog.partial(checkFileContentAs, str, 'BinaryString', undefined));
  },

  testWriteFile() {
    if (!fsExists) {
      return;
    }

    return loadFile('test', FsDirectoryEntry.Behavior.CREATE)
        .then(goog.partial(writeToFile, 'test content'))
        .then(goog.partial(checkFileContent, 'test content'));
  },

  testRemoveFile() {
    if (!fsExists) {
      return;
    }

    return loadFile('test', FsDirectoryEntry.Behavior.CREATE)
        .then(goog.partial(writeToFile, 'test content'))
        .then((file) => file.remove())
        .then(goog.partial(checkFileRemoved, 'test'));
  },

  testMoveFile() {
    if (!fsExists) {
      return;
    }

    const deferredSubdir =
        loadDirectory('subdir', FsDirectoryEntry.Behavior.CREATE);
    const deferredWrittenFile =
        loadFile('test', FsDirectoryEntry.Behavior.CREATE)
            .then(goog.partial(writeToFile, 'test content'));
    return GoogPromise.all([deferredSubdir, deferredWrittenFile])
        .then(splitArgs((dir, file) => file.moveTo(dir)))
        .then(goog.partial(checkFileContent, 'test content'))
        .then(goog.partial(checkFileRemoved, 'test'));
  },

  testCopyFile() {
    if (!fsExists) {
      return;
    }

    const deferredFile = loadFile('test', FsDirectoryEntry.Behavior.CREATE);
    const deferredSubdir =
        loadDirectory('subdir', FsDirectoryEntry.Behavior.CREATE);
    const deferredWrittenFile =
        deferredFile.then(goog.partial(writeToFile, 'test content'));
    return GoogPromise.all([deferredSubdir, deferredWrittenFile])
        .then(splitArgs((dir, file) => file.copyTo(dir)))
        .then(goog.partial(checkFileContent, 'test content'))
        .then(() => deferredFile)
        .then(goog.partial(checkFileContent, 'test content'));
  },

  /** @suppress {uselessCode} suppression added to enable type checking */
  testAbortWrite() {
    // TODO(nicksantos): This test is broken in newer versions of chrome.
    // We don't know why yet.
    if (true) return;

    if (!fsExists) {
      return;
    }

    const deferredFile = loadFile('test', FsDirectoryEntry.Behavior.CREATE);
    return deferredFile.then(goog.partial(startWrite, 'test content'))
        .then((writer) => new GoogPromise((resolve) => {
                events.listenOnce(writer, FsFileSaver.EventType.ABORT, resolve);
                writer.abort();
              }))
        .then(/**
                 @suppress {checkTypes} suppression added to enable type
                 checking
               */
              () => loadFile('test'))
        .then(goog.partial(checkFileContent, ''));
  },

  testSeek() {
    if (!fsExists) {
      return;
    }

    const deferredFile = loadFile('test', FsDirectoryEntry.Behavior.CREATE);
    return deferredFile.then(goog.partial(writeToFile, 'test content'))
        .then((file) => file.createWriter())
        .then(goog.partial(checkReadyState, FsFileSaver.ReadyState.INIT))
        .then((writer) => {
          writer.seek(5);
          writer.write(googFsBlob.getBlob('stuff and things'));
          return writer;
        })
        .then(goog.partial(checkReadyState, FsFileSaver.ReadyState.WRITING))
        .then(goog.partial(waitForEvent, FsFileSaver.EventType.WRITE))
        .then(() => deferredFile)
        .then(goog.partial(checkFileContent, 'test stuff and things'));
  },

  testTruncate() {
    if (!fsExists) {
      return;
    }

    const deferredFile = loadFile('test', FsDirectoryEntry.Behavior.CREATE);
    return deferredFile.then(goog.partial(writeToFile, 'test content'))
        .then((file) => file.createWriter())
        .then(goog.partial(checkReadyState, FsFileSaver.ReadyState.INIT))
        .then((writer) => {
          writer.truncate(4);
          return writer;
        })
        .then(goog.partial(checkReadyState, FsFileSaver.ReadyState.WRITING))
        .then(goog.partial(waitForEvent, FsFileSaver.EventType.WRITE))
        .then(() => deferredFile)
        .then(goog.partial(checkFileContent, 'test'));
  },

  testGetLastModified() {
    if (!fsExists) {
      return;
    }
    const now = Date.now();
    return loadFile('test', FsDirectoryEntry.Behavior.CREATE)
        .then((entry) => entry.getLastModified())
        .then((date) => {
          assertRoughlyEquals(
              'Expected the last modified date to be within ' +
                  'a few milliseconds of the test start time.',
              now, date.getTime(), 2000);
        });
  },

  testCreatePath() {
    if (!fsExists) {
      return;
    }

    return loadTestDir()
        .then((testDir) => testDir.createPath('foo'))
        .then((fooDir) => {
          assertEquals('/goog-fs-test-dir/foo', fooDir.getFullPath());
          return fooDir.createPath('bar/baz/bat');
        })
        .then((batDir) => {
          assertEquals(
              '/goog-fs-test-dir/foo/bar/baz/bat', batDir.getFullPath());
        });
  },

  testCreateAbsolutePath() {
    if (!fsExists) {
      return;
    }

    return loadTestDir()
        .then((testDir) => testDir.createPath(`/${TEST_DIR}/fee/fi/fo/fum`))
        .then((absDir) => {
          assertEquals('/goog-fs-test-dir/fee/fi/fo/fum', absDir.getFullPath());
        });
  },

  testCreateRelativePath() {
    if (!fsExists) {
      return;
    }

    return loadTestDir()
        .then((dir) => dir.createPath(`../${TEST_DIR}/dir`))
        .then((relDir) => {
          assertEquals('/goog-fs-test-dir/dir', relDir.getFullPath());
          return relDir.createPath('.');
        })
        .then((sameDir) => {
          assertEquals('/goog-fs-test-dir/dir', sameDir.getFullPath());
          return sameDir.createPath('./././.');
        })
        .then((reallySameDir) => {
          assertEquals('/goog-fs-test-dir/dir', reallySameDir.getFullPath());
          return reallySameDir.createPath('./new/../..//dir/./new////.');
        })
        .then((newDir) => {
          assertEquals('/goog-fs-test-dir/dir/new', newDir.getFullPath());
        });
  },

  testCreateBadPath() {
    if (!fsExists) {
      return;
    }

    return loadTestDir()
        .then(() => loadTestDir())
        .then((dir) => {
          // There is only one layer of parent directory from the test dir.
          return dir.createPath(`../../../../${TEST_DIR}/baz/bat`);
        })
        .then((batDir) => {
          assertEquals(
              'The parent directory of the root directory should ' +
                  'point back to the root directory.',
              '/goog-fs-test-dir/baz/bat', batDir.getFullPath());
        })
        .

        then(() => loadTestDir())
        .then((dir) => {
          // An empty path should return the same as the input directory.
          return dir.createPath('');
        })
        .then((testDir) => {
          assertEquals('/goog-fs-test-dir', testDir.getFullPath());
        });
  },

  testGetAbsolutePaths() {
    if (!fsExists) {
      return;
    }

    return loadFile('foo', FsDirectoryEntry.Behavior.CREATE)
        .then(() => loadTestDir())
        .then((testDir) => testDir.getDirectory('/'))
        .then((root) => {
          assertEquals('/', root.getFullPath());
          return root.getDirectory(`/${TEST_DIR}`);
        })
        .then((testDir) => {
          assertEquals('/goog-fs-test-dir', testDir.getFullPath());
          return testDir.getDirectory(`//${TEST_DIR}////`);
        })
        .then((testDir) => {
          assertEquals('/goog-fs-test-dir', testDir.getFullPath());
          return testDir.getDirectory('////');
        })
        .then((testDir) => {
          assertEquals('/', testDir.getFullPath());
        });
  },

  testListEmptyDirectory() {
    if (!fsExists) {
      return;
    }

    return loadTestDir().then((dir) => dir.listDirectory()).then((entries) => {
      assertArrayEquals([], entries);
    });
  },

  testListDirectory() {
    if (!fsExists) {
      return;
    }

    return loadDirectory('testDir', FsDirectoryEntry.Behavior.CREATE)
        .then(() => loadFile('testFile', FsDirectoryEntry.Behavior.CREATE))
        .then(() => loadTestDir())
        .then((testDir) => testDir.listDirectory())
        .then((entries) => {
          // Verify the contents of the directory listing.
          assertEquals(2, entries.length);

          const dir =
              googArray.find(entries, (entry) => entry.getName() == 'testDir');
          assertNotNull(dir);
          assertTrue(dir.isDirectory());

          const file =
              googArray.find(entries, (entry) => entry.getName() == 'testFile');
          assertNotNull(file);
          assertTrue(file.isFile());
        });
  },

  /** @suppress {uselessCode} suppression added to enable type checking */
  testListBigDirectory() {
    // TODO(nicksantos): This test is broken in newer versions of chrome.
    // We don't know why yet.
    if (true) return;

    if (!fsExists) {
      return;
    }

    function getFileName(i) {
      return 'file' + googString.padNumber(i, String(count).length);
    }

    // NOTE: This was intended to verify that the results from repeated
    // DirectoryReader.readEntries() callbacks are appropriately concatenated.
    // In current versions of Chrome (March 2011), all results are returned in
    // the first callback regardless of directory size. The count can be
    // increased in the future to test batched result lists once they are
    // implemented.
    const count = 100;

    const expectedNames = [];

    const def = GoogPromise.resolve();
    for (let i = 0; i < count; i++) {
      const name = getFileName(i);
      expectedNames.push(name);

      def.then(() => loadFile(name, FsDirectoryEntry.Behavior.CREATE));
    }

    return def.then(() => loadTestDir())
        .then((testDir) => testDir.listDirectory())
        .then((entries) => {
          assertEquals(count, entries.length);

          assertSameElements(
              expectedNames,
              googArray.map(entries, (entry) => entry.getName()));
          assertTrue(googArray.every(entries, (entry) => entry.isFile()));
        });
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSliceBlob() {
    // A mock blob object whose slice returns the parameters it was called with.
    const blob = {
      'size': 10,
      'slice': function(start, end) {
        return [start, end];
      },
    };

    // Expect slice to be called with no change to parameters
    assertArrayEquals([2, 10], googFs.sliceBlob(blob, 2));
    assertArrayEquals([-2, 10], googFs.sliceBlob(blob, -2));
    assertArrayEquals([3, 6], googFs.sliceBlob(blob, 3, 6));
    assertArrayEquals([3, -6], googFs.sliceBlob(blob, 3, -6));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetBlobThrowsError() {
    stubs.remove(globalThis, 'BlobBuilder');
    stubs.remove(globalThis, 'WebKitBlobBuilder');
    stubs.remove(globalThis, 'Blob');

    try {
      googFsBlob.getBlob();
      fail();
    } catch (e) {
      assertEquals(
          'This browser doesn\'t seem to support creating Blobs', e.message);
    }

    stubs.reset();
  },

  testGetBlobWithProperties() {
    // Skip test if browser doesn't support Blob API.
    if (typeof (globalThis.Blob) != 'function') {
      return;
    }

    const blob =
        googFsBlob.getBlobWithProperties(['test'], 'text/test', 'native');
    assertEquals('text/test', blob.type);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetBlobWithPropertiesThrowsError() {
    stubs.remove(globalThis, 'BlobBuilder');
    stubs.remove(globalThis, 'WebKitBlobBuilder');
    stubs.remove(globalThis, 'Blob');

    try {
      googFsBlob.getBlobWithProperties();
      fail();
    } catch (e) {
      assertEquals(
          'This browser doesn\'t seem to support creating Blobs', e.message);
    }

    stubs.reset();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testGetBlobWithPropertiesUsingBlobBuilder() {
    function BlobBuilder() {
      this.parts = [];
      this.append = function(value, endings) {
        this.parts.push({value: value, endings: endings});
      };
      this.getBlob = function(type) {
        return {type: type, builder: this};
      };
    }
    stubs.set(globalThis, 'BlobBuilder', BlobBuilder);

    const blob =
        googFsBlob.getBlobWithProperties(['test'], 'text/test', 'native');
    assertEquals('text/test', blob.type);
    assertEquals('test', blob.builder.parts[0].value);
    assertEquals('native', blob.builder.parts[0].endings);

    stubs.reset();
  },
});
