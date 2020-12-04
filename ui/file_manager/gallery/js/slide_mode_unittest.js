// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @suppress {checkTypes, constantProperty}
 */
chrome.fileManagerPrivate = {
  FormatFileSystemType: {
    VFAT: 'vfat',
    EXFAT: 'exfat',
    NTFS: 'ntfs',
  },
};

/**
 * Mock implementation of strf function.
 */
function strf(id, var_args) {
  return `${id}-${Array.from(arguments).slice(1).join("-")}`;
}

const fallbackDir = /** @type{!DirectoryEntry} */ ({fullPath: '/fallback'});

/**
 * Test case for writable format and writable volume.
 */
function testGetEditorWarningMessageWritableFormatAndVolumeCase(callback) {
  const item = /** @type{!GalleryItem} */ ({isWritableFormat: () => true});

  reportPromise(SlideMode.getEditorWarningMessage(
      item, '', fallbackDir).then(function(message) {
    assertEquals(message, null);
  }), callback);
}

/**
 * Test case for writable format and read only volume.
 */
function testGetEditorWarningMessageWritableFormatReadOnlyCase(callback) {
  const item = /** @type{!GalleryItem} */ ({isWritableFormat: () => true});

  reportPromise(SlideMode.getEditorWarningMessage(
      item, 'NON_WRITABLE_VOLUME', fallbackDir).then(function(message) {
    assertEquals(message, 'GALLERY_READONLY_WARNING-NON_WRITABLE_VOLUME');
  }), callback);
}

/**
 * Test case for non-writable format and writable volume.
 */
function testGetEditorWarningMessageNonWritableFormatAndWritableVolumeCase(
    callback) {
  const item = /** @type{!GalleryItem} */ ({
    isWritableFormat: function() {
      return false;
    },
    getEntry: function() {
      return {
        fullPath: '/parent/test.png',
        getParent: function(callback) {
          callback({ fullPath: '/parent' });
        }
      };
    },
    getCopyName: function(dirEntry) {
      assertEquals(dirEntry.fullPath, '/parent');
      return Promise.resolve('test - Edited.png');
    }
  });

  reportPromise(SlideMode.getEditorWarningMessage(
      item, '', fallbackDir).then(function(message) {
    assertEquals(message,
        'GALLERY_NON_WRITABLE_FORMAT_WARNING-test - Edited.png');
  }), callback);
}

/**
 * Test case for non-writable format and read only volume.
 */
function testGetEditorWarningMessageNonWritableFormatAndReadOnlyCase(callback) {
  const item = /** @type{!GalleryItem} */ ({
    isWritableFormat: function() {
      return false;
    },
    getCopyName: function(dirEntry) {
      assertEquals(dirEntry.fullPath, '/fallback');
      return Promise.resolve('test - Edited.png');
    }
  });

  reportPromise(SlideMode.getEditorWarningMessage(
      item, 'NON_WRITABLE_VOLUME', fallbackDir).then(function(message) {
    assertEquals(message,
        "GALLERY_READONLY_AND_NON_WRITABLE_FORMAT_WARNING-" +
        "NON_WRITABLE_VOLUME-test - Edited.png");
  }), callback);
}
