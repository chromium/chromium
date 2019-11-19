// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Mock of ImageUtil and metrics.
 */
var ImageUtil = {
  getMetricName: function() {},
};
var metrics = {
  recordEnum: function() {},
  recordInterval: function() {},
  startInterval: function() {}
};

/**
 * Load time data.
 */
loadTimeData.data = {
  DRIVE_DIRECTORY_LABEL: '',
  DOWNLOADS_DIRECTORY_LABEL: ''
};

function setUp() {
  // Replace the real ImageEncoder with a mock.
  ImageEncoder = /** @lends{ImageEncoder} */ (
      {encodeMetadata: function() {}, getBlob: function() {}});
}

function getMockMetadataModel() {
  return new MockMetadataModel({size: 200});
}

/**
 * Creates a mock result for GalleryItem#saveToFile.
 *
 * @param{!GalleryItem} item
 * @param{!MetadataModel} metadataModel
 * @param{boolean} overwrite
 * @return {Promise}
 */
function makeMockSavePromise(item, metadataModel, overwrite) {
  let canvas =
      assertInstanceof(document.createElement('canvas'), HTMLCanvasElement);
  let mockVolumeManager = /**@type{!VolumeManager} */ ({
    getLocationInfo: function() {
      return {};
    },
    getVolumeInfo: function() {
      return {};
    }
  });
  return new Promise(item.saveToFile.bind(
      item, mockVolumeManager, metadataModel,
      /** @type{!DirectoryEntry} */ ({}),  // fallbackDir.
      canvas, overwrite /* overwrite */));
}

/**
 * Tests for GalleryItem#saveToFile.
 */
function testSaveToFile(callback) {
  var fileSystem = new MockFileSystem('volumeId');
  fileSystem.populate(['/test.jpg']);
  var entry = /** @type {FileEntry} */ (fileSystem.entries['/test.jpg']);
  entry.createWriter = function(callback) {
    let mockWriter = /**@lends {FileWriter} */ ({
      write: function() {
        Promise.resolve().then(function() {
          mockWriter.onwriteend();
        });
      },
      truncate: function() {
        mockWriter.write();
      }
    });
    callback(mockWriter);
  };
  var entryChanged = false;
  var metadataModel = getMockMetadataModel();
  metadataModel.notifyEntriesChanged = function() {
    entryChanged = true;
  };


  var item =
      new MockGalleryItem(entry, null, {size: 100}, null, true /*original */);
  assertEquals(100, item.getMetadataItem().size);
  assertFalse(entryChanged);
  reportPromise(
      makeMockSavePromise(item, metadataModel, true).then(function() {
        assertEquals(200, item.getMetadataItem().size);
        assertTrue(entryChanged);
      }),
      callback);
}

/**
 * Tests for GalleryItem#saveToFile. In this test case, fileWriter.write fails
 * with an error.
 */
function testSaveToFileWriteFailCase(callback) {
  var fileSystem = new MockFileSystem('volumeId');
  fileSystem.populate(['/test.jpg']);
  var entry = /** @type {FileEntry} */ (fileSystem.entries['/test.jpg']);

  entry.createWriter = function(callback) {
    let mockWriter = /**@lends {FileWriter} */ ({
      write: function() {
        Promise.resolve().then(function() {
          mockWriter.onerror(new Error());
        });
      },
      truncate: function() {
        Promise.resolve().then(function() {
          mockWriter.onwriteend();
        });
      }
    });
    callback(mockWriter);
  };

  var item =
      new MockGalleryItem(entry, null, {size: 100}, null, true /*original */);
  reportPromise(
      makeMockSavePromise(item, getMockMetadataModel(), true)
          .then(function(result) {
            assertFalse(result);
          }),
      callback);
}

/**
 * Tests for GalleryItem#saveToFile. In this test case, ImageEncoder.getBlob
 * fails with an error. This test case confirms that no write operation runs
 * when it fails to get a blob of new image.
 */
function testSaveToFileGetBlobFailCase(callback) {
  ImageEncoder.getBlob = function() {
    throw new Error();
  };

  var fileSystem = new MockFileSystem('volumeId');
  fileSystem.populate(['/test.jpg']);
  var entry = /** @type {FileEntry} */ (fileSystem.entries['/test.jpg']);

  var writeOperationRun = false;
  entry.createWriter = function(callback) {
    let mockWriter = /**@lends {FileWriter} */ ({
      write: function() {
        Promise.resolve().then(function() {
          writeOperationRun = true;
          mockWriter.onwriteend();
        });
      },
      truncate: function() {
        Promise.resolve().then(function() {
          writeOperationRun = true;
          mockWriter.onwriteend();
        });
      }
    });
    callback(mockWriter);
  };

  var item =
      new MockGalleryItem(entry, null, {size: 100}, null, true /*original */);
  reportPromise(
      makeMockSavePromise(item, getMockMetadataModel(), true)
          .then(function(result) {
            assertFalse(result);
            assertFalse(writeOperationRun);
          }),
      callback);
}

function testSaveToFileRaw(callback) {
  var fileSystem = new MockFileSystem('volumeId');
  fileSystem.populate(['/test.arw']);
  fileSystem.entries['/'].getFile = function(name, options, success, error) {
    if (options.create) {
      assertEquals('test - Edited.jpg', name);
      fileSystem.populate(['/test - Edited.jpg']);
      var entry = fileSystem.entries['/test - Edited.jpg'];
      entry.createWriter = function(callback) {
        let mockWriter = /**@lends {FileWriter} */ ({
          write: function() {
            Promise.resolve().then(function() {
              mockWriter.onwriteend();
            });
          },
          truncate: function() {
            mockWriter.write();
          }
        });
        callback(mockWriter);
      };
    }
    MockDirectoryEntry.prototype.getFile.apply(this, arguments);
  };
  var entryChanged = false;
  var metadataModel = getMockMetadataModel();
  metadataModel.notifyEntriesChanged = function() {
    entryChanged = true;
  };

  var entry = /** @type {!FileEntry} */ (fileSystem.entries['/test.arw']);
  var item =
      new MockGalleryItem(entry, null, {size: 100}, null, true /*original */);
  assertEquals(100, item.getMetadataItem().size);
  assertFalse(entryChanged);
  reportPromise(
      makeMockSavePromise(item, metadataModel, false).then(function(success) {
        assertTrue(success);
        assertEquals(200, item.getMetadataItem().size);
        assertTrue(entryChanged);
        assertFalse(item.isOriginal());
      }),
      callback);
}

function testIsWritableFile() {
  var downloads = new MockFileSystem('downloads');
  var removable = new MockFileSystem('removable');
  var mtp = new MockFileSystem('mtp');

  var volumeTypes = {
    downloads: VolumeManagerCommon.VolumeType.DOWNLOADS,
    removable: VolumeManagerCommon.VolumeType.REMOVABLE,
    mtp: VolumeManagerCommon.VolumeType.MTP
  };

  // Mock volume manager.
  var volumeManager = {
    getVolumeInfo: function(entry) {
      return {
        volumeType: volumeTypes[entry.filesystem.name]
      };
    }
  };

  // Jpeg file on downloads.
  assertTrue(
      MockGalleryItem
          .makeWithPath('/test.jpg', downloads, false /* not read only */)
          .isWritableFile(volumeManager));

  // Png file on downloads.
  assertTrue(
      MockGalleryItem
          .makeWithPath('/test.png', downloads, false /* not read only */)
          .isWritableFile(volumeManager));

  // Webp file on downloads.
  assertFalse(
      MockGalleryItem
          .makeWithPath('/test.webp', downloads, false /* not read only */)
          .isWritableFile(volumeManager));

  // Jpeg file on non-writable volume.
  assertFalse(
      MockGalleryItem.makeWithPath('/test.jpg', removable, true /* read only */)
          .isWritableFile(volumeManager));

  // Jpeg file on mtp volume.
  assertFalse(
      MockGalleryItem.makeWithPath('/test.jpg', mtp, false /* not read only */)
          .isWritableFile(volumeManager));
}

function testIsEditableFile() {
  var downloads = new MockFileSystem('downloads');
  var getGalleryItem = function(path, isReadOnly) {
    return MockGalleryItem.makeWithPath(path, downloads, isReadOnly);
  };

  // Images and raw files are editable, even if read-only (a copy is made).
  assertTrue(getGalleryItem('/test.jpg', false).isEditable());
  assertTrue(getGalleryItem('/test.png', false).isEditable());
  assertTrue(getGalleryItem('/test.webp', false).isEditable());
  assertTrue(getGalleryItem('/test.arw', false).isEditable());
  assertTrue(getGalleryItem('/test.jpg', true).isEditable());

  // Video files are not editable.
  assertFalse(getGalleryItem('/test.avi', false).isEditable());
  assertFalse(getGalleryItem('/test.mkv', false).isEditable());
  assertFalse(getGalleryItem('/test.mp4', false).isEditable());
  assertFalse(getGalleryItem('/test.mov', false).isEditable());
  assertFalse(getGalleryItem('/test.webm', false).isEditable());
}
