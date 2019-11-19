// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const TEST_METADATA = {
  'a.txt': {
    contentMimeType: 'text/plain',
    size: 1023,
    modificationTime: new Date(2016, 1, 1, 0, 0, 2),
  },
  'b.html': {
    contentMimeType: 'text/html',
    size: 206,
    modificationTime: new Date(2016, 1, 1, 0, 0, 1),
  },
  'c.jpg': {
    contentMimeType: 'image/jpeg',
    size: 342134,
    modificationTime: new Date(2016, 1, 1, 0, 0, 0),
  },
};

function assertFileListModelElementNames(fileListModel, names) {
  assertEquals(fileListModel.length, names.length);
  for (let i = 0; i < fileListModel.length; i++) {
    assertEquals(fileListModel.item(i).name, names[i]);
  }
}

function assertEntryArrayEquals(entryArray, names) {
  assertEquals(entryArray.length, names.length);
  assertArrayEquals(entryArray.map((e) => e.name), names);
}

function makeSimpleFileListModel(names) {
  const fileListModel = new FileListModel(createFakeMetadataModel({}));
  for (let i = 0; i < names.length; i++) {
    fileListModel.push({name: names[i], isDirectory: false});
  }
  return fileListModel;
}

/**
 * Returns a fake MetadataModel, used to provide metadata from the given |data|
 * object (usually TEST_METADATA) to the FileListModel.
 * @param {!Object} data
 * @return {!MetadataModel}
 */
function createFakeMetadataModel(data) {
  return /** @type {!MetadataModel} */ ({
    getCache: (entries, names) => {
      let result = [];
      for (let i = 0; i < entries.length; i++) {
        let metadata = {};
        if (!entries[i].isDirectory && data[entries[i].name]) {
          for (let j = 0; j < names.length; j++) {
            metadata[names[j]] = data[entries[i].name][names[j]];
          }
        }
        result.push(metadata);
      }
      return result;
    },
  });
}

function testIsImageDominant() {
  const fileListModel =
      new FileListModel(createFakeMetadataModel(TEST_METADATA));

  assertEquals(fileListModel.isImageDominant(), false);

  // Adding one image. Image should be dominant in this directory (100%).
  fileListModel.push({name: 'c.jpg', isDirectory: false});
  assertEquals(fileListModel.isImageDominant(), true);

  // Adding a directory shouldn't affect how the image is dominant (still 100%).
  fileListModel.push({name: 'tmp_folder', isDirectory: true});
  assertEquals(fileListModel.isImageDominant(), true);

  // Adding a non-image file, which will make the images not dominant (50%);
  fileListModel.push({name: 'a.txt', isDirectory: false});
  assertEquals(fileListModel.isImageDominant(), false);

  // Adding two image. Now 75%(3/4) files are images. Still not dominant.
  fileListModel.push({name: 'c.jpg', isDirectory: false});
  fileListModel.push({name: 'c.jpg', isDirectory: false});
  assertEquals(fileListModel.isImageDominant(), false);

  // Adding one more. Now 80%(4/5) files are images. Reached the threshold.
  fileListModel.push({name: 'c.jpg', isDirectory: false});
  assertEquals(fileListModel.isImageDominant(), true);
}

function testSortWithFolders() {
  const fileListModel =
      new FileListModel(createFakeMetadataModel(TEST_METADATA));
  fileListModel.push({name: 'dirA', isDirectory: true});
  fileListModel.push({name: 'dirB', isDirectory: true});
  fileListModel.push({name: 'a.txt', isDirectory: false});
  fileListModel.push({name: 'b.html', isDirectory: false});
  fileListModel.push({name: 'c.jpg', isDirectory: false});

  // In following sort tests, note that folders should always be prior to files.
  fileListModel.sort('name', 'asc');
  assertFileListModelElementNames(
      fileListModel, ['dirA', 'dirB', 'a.txt', 'b.html', 'c.jpg']);
  fileListModel.sort('name', 'desc');
  assertFileListModelElementNames(
      fileListModel, ['dirB', 'dirA', 'c.jpg', 'b.html', 'a.txt']);
  // Sort files by size. Folders should be sorted by their names.
  fileListModel.sort('size', 'asc');
  assertFileListModelElementNames(
      fileListModel, ['dirA', 'dirB', 'b.html', 'a.txt', 'c.jpg']);
  fileListModel.sort('size', 'desc');
  assertFileListModelElementNames(
      fileListModel, ['dirB', 'dirA', 'c.jpg', 'a.txt', 'b.html']);
  // Sort files by modification. Folders should be sorted by their names.
  fileListModel.sort('modificationTime', 'asc');
  assertFileListModelElementNames(
      fileListModel, ['dirA', 'dirB', 'c.jpg', 'b.html', 'a.txt']);
  fileListModel.sort('modificationTime', 'desc');
  assertFileListModelElementNames(
      fileListModel, ['dirB', 'dirA', 'a.txt', 'b.html', 'c.jpg']);
}

function testSplice() {
  const fileListModel = makeSimpleFileListModel(['d', 'a', 'x', 'n']);
  fileListModel.sort('name', 'asc');

  fileListModel.addEventListener('splice', event => {
    assertEntryArrayEquals(event.added, ['p', 'b']);
    assertEntryArrayEquals(event.removed, ['n']);
    // The first inserted item, 'p', should be at index:3 after splice.
    assertEquals(event.index, 3);
  });

  fileListModel.addEventListener('permuted', event => {
    assertArrayEquals(event.permutation, [0, 2, -1, 4]);
    assertEquals(event.newLength, 5);
  });

  fileListModel.splice(
      2, 1, {name: 'p', isDirectory: false}, {name: 'b', isDirectory: false});
  assertFileListModelElementNames(fileListModel, ['a', 'b', 'd', 'p', 'x']);
}

function testSpliceWithoutSortStatus() {
  const fileListModel = makeSimpleFileListModel(['d', 'a', 'x', 'n']);

  fileListModel.addEventListener('splice', event => {
    assertEntryArrayEquals(event.added, ['p', 'b']);
    assertEntryArrayEquals(event.removed, ['x']);
    // The first inserted item, 'p', should be at index:2 after splice.
    assertEquals(event.index, 2);
  });

  fileListModel.addEventListener('permuted', event => {
    assertArrayEquals(event.permutation, [0, 1, -1, 4]);
    assertEquals(event.newLength, 5);
  });

  fileListModel.splice(
      2, 1, {name: 'p', isDirectory: false}, {name: 'b', isDirectory: false});
  // If the sort status is not specified, the original order should be kept.
  // i.e. the 2nd element in the original array, 'x', should be removed, and
  // 'p' and 'b' should be inserted at the position without changing the order.
  assertFileListModelElementNames(fileListModel, ['d', 'a', 'p', 'b', 'n']);
}

function testSpliceWithoutAddingNewItems() {
  const fileListModel = makeSimpleFileListModel(['d', 'a', 'x', 'n']);
  fileListModel.sort('name', 'asc');

  fileListModel.addEventListener('splice', event => {
    assertEntryArrayEquals(event.added, []);
    assertEntryArrayEquals(event.removed, ['n']);
    // The first item after insertion/deletion point is 'x', which should be at
    // 2nd position after the sort.
    assertEquals(event.index, 2);
  });

  fileListModel.addEventListener('permuted', event => {
    assertArrayEquals(event.permutation, [0, 1, -1, 2]);
    assertEquals(event.newLength, 3);
  });

  fileListModel.splice(2, 1);
  assertFileListModelElementNames(fileListModel, ['a', 'd', 'x']);
}

function testSpliceWithoutDeletingItems() {
  const fileListModel = makeSimpleFileListModel(['d', 'a', 'x', 'n']);
  fileListModel.sort('name', 'asc');

  fileListModel.addEventListener('splice', event => {
    assertEntryArrayEquals(event.added, ['p', 'b']);
    assertEntryArrayEquals(event.removed, []);
    assertEquals(event.index, 4);
  });

  fileListModel.addEventListener('permuted', event => {
    assertArrayEquals(event.permutation, [0, 2, 3, 5]);
    assertEquals(event.newLength, 6);
  });

  fileListModel.splice(
      2, 0, {name: 'p', isDirectory: false}, {name: 'b', isDirectory: false});
  assertFileListModelElementNames(
      fileListModel, ['a', 'b', 'd', 'n', 'p', 'x']);
}
