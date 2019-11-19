// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const imageEntry = {
  name: 'image.jpg',
  toURL: function() {
    return 'filesystem://A';
  }
};

const nonImageEntry = {
  name: 'note.txt',
  toURL: function() {
    return 'filesystem://B';
  }
};

const contentThumbnailTransform = {
  scaleX: 0,
  scaleY: 0,
  rotate90: 0,
};

const imageTransformation = {
  scaleX: 1,
  scaleY: 1,
  rotate90: 2,
};

let metadata;
let contentMetadata;
let thumbnailModel;

function setUp() {
  metadata = new MetadataItem();
  metadata.modificationTime = new Date(2015, 0, 1);
  metadata.present = true;
  metadata.thumbnailUrl = 'EXTERNAL_THUMBNAIL_URL';
  metadata.customIconUrl = 'CUSTOM_ICON_URL';
  metadata.contentThumbnailUrl = 'CONTENT_THUMBNAIL_URL';
  metadata.contentThumbnailTransform = contentThumbnailTransform;
  metadata.contentImageTransform = imageTransformation;

  thumbnailModel = new ThumbnailModel(/** @type {!MetadataModel} */ ({
    get: function(entries, names) {
      const result = new MetadataItem();
      for (let i = 0; i < names.length; i++) {
        const name = names[i];
        result[name] = metadata[name];
      }
      return Promise.resolve([result]);
    }
  }));
}

function testThumbnailModelGetBasic(callback) {
  reportPromise(
      thumbnailModel.get([imageEntry]).then(results => {
        assertEquals(1, results.length);
        assertEquals(
            new Date(2015, 0, 1).toString(),
            results[0].filesystem.modificationTime.toString());
        assertEquals(
            'EXTERNAL_THUMBNAIL_URL', results[0].external.thumbnailUrl);
        assertEquals('CUSTOM_ICON_URL', results[0].external.customIconUrl);
        assertTrue(results[0].external.present);
        assertEquals('CONTENT_THUMBNAIL_URL', results[0].thumbnail.url);
        assertEquals(contentThumbnailTransform, results[0].thumbnail.transform);
        assertEquals(imageTransformation, results[0].media.imageTransform);
      }),
      callback);
}

function testThumbnailModelGetNotPresent(callback) {
  metadata.present = false;
  reportPromise(
      thumbnailModel.get([imageEntry]).then(results => {
        assertEquals(1, results.length);
        assertEquals(
            new Date(2015, 0, 1).toString(),
            results[0].filesystem.modificationTime.toString());
        assertEquals(
            'EXTERNAL_THUMBNAIL_URL', results[0].external.thumbnailUrl);
        assertEquals('CUSTOM_ICON_URL', results[0].external.customIconUrl);
        assertFalse(results[0].external.present);
        assertEquals(undefined, results[0].thumbnail.url);
        assertEquals(undefined, results[0].thumbnail.transform);
        assertEquals(undefined, results[0].media.imageTransform);
      }),
      callback);
}

function testThumbnailModelGetNonImage(callback) {
  reportPromise(
      thumbnailModel.get([nonImageEntry]).then(results => {
        assertEquals(1, results.length);
        assertEquals(
            new Date(2015, 0, 1).toString(),
            results[0].filesystem.modificationTime.toString());
        assertEquals(
            'EXTERNAL_THUMBNAIL_URL', results[0].external.thumbnailUrl);
        assertEquals('CUSTOM_ICON_URL', results[0].external.customIconUrl);
        assertTrue(results[0].external.present);
        assertEquals(undefined, results[0].thumbnail.url);
        assertEquals(undefined, results[0].thumbnail.transform);
        assertEquals(undefined, results[0].media.imageTransform);
      }),
      callback);
}
