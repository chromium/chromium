// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Helper to construct testing GalleryItem objects.
 *
 * @param {!FileEntry} entry
 * @param {EntryLocation} locationInfo
 * @param {Object} metadataItem
 * @param {ThumbnailMetadataItem=} opt_thumbnailMetadataItem
 * @param {boolean=} opt_original Whether the entry is original or edited.
 * @constructor
 * @extends GalleryItem
 */
function MockGalleryItem(
    entry, locationInfo, metadataItem, opt_thumbnailMetadataItem,
    opt_original) {
  let entryLocation = locationInfo || /** @type {!EntryLocation} */ ({});
  GalleryItem.call(
      this, entry, entryLocation, /** @type {MetadataItem} */ (metadataItem),
      opt_thumbnailMetadataItem || null, opt_original || false);
}

/**
 * Helper to construct a MockGalleryItem with a given |path| and dummy metadata.
 *
 * @param {!string} path
 * @param {!FileSystem} fileSystem
 * @param {boolean} isReadOnly
 * @return MockGalleryItem
 */
MockGalleryItem.makeWithPath = function(path, fileSystem, isReadOnly) {
  return new MockGalleryItem(
      MockFileEntry.create(fileSystem, path),
      /** @type {EntryLocation} */ ({isReadOnly: isReadOnly}), {size: 100},
      null, true /* original */);
};

MockGalleryItem.prototype = {
  __proto__: GalleryItem.prototype
};
