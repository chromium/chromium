// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var model;
var fileSystem;
var item;

function setUp() {
  // Avoid creating the default EntryListWatcher, since it needs access to
  // fileManagerPrivate, which is not in the unit test environment.
  let mockEntryListWatcher = /** @type{!EntryListWatcher} */ ({});
  model = new GalleryDataModel(new MockMetadataModel({}), mockEntryListWatcher);

  fileSystem = new MockFileSystem('volumeId');
  model.fallbackSaveDirectory = fileSystem.root;
}

function testSaveItemOverwrite(callback) {
  var item = new MockGalleryItem(
      MockFileEntry.create(fileSystem, '/test.jpg'), null, {}, null,
      false /* isOriginal */);

  // Mocking the saveToFile method.
  item.saveToFile = function(
      volumeManager,
      metadataModel,
      fallbackDir,
      canvas,
      overwrite,
      callback) {
    callback(true);
  };
  model.push(item);
  reportPromise(
      model
          .saveItem(
              {}, item, document.createElement('canvas'), true /* overwrite */)
          .then(function() {
            assertEquals(1, model.length);
          }),
      callback);
}

function testSaveItemToNewFile(callback) {
  var item = new MockGalleryItem(
      MockFileEntry.create(fileSystem, '/test.webp'), null, {}, null,
      true /* isOriginal */);

  // Mocking the saveToFile method. In this case, Gallery saves to a new file
  // since it cannot overwrite to webp image file.
  /** @suppress {accessControls} */
  item.saveToFile = function(
      volumeManager,
      metadataModel,
      fallbackDir,
      canvas,
      overwrite,
      callback) {
    // Gallery item track new file.
    item.entry_ = MockFileEntry.create(fileSystem, '/test (1).png');
    item.original_ = false;
    callback(true);
  };
  model.push(item);
  reportPromise(
      model.saveItem({}, item, document.createElement('canvas'),
          false /* not overwrite */).
          then(function() {
            assertEquals(2, model.length);
            assertEquals('test (1).png', model.item(0).getFileName());
            assertFalse(model.item(0).isOriginal());
            assertEquals('test.webp', model.item(1).getFileName());
            assertTrue(model.item(1).isOriginal());
          }),
      callback);
}
