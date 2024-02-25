// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MetadataItem, type MetadataKey} from './metadata_item.js';
import type {MetadataModel} from './metadata_model.js';
import {ThumbnailModel} from './thumbnail_model.js';

const imageEntry: Entry = {
  name: 'image.jpg',
  toURL: function() {
    return 'filesystem://A';
  },
} as Entry;

const nonImageEntry: Entry = {
  name: 'note.txt',
  toURL: function() {
    return 'filesystem://B';
  },
} as Entry;

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

let metadata: MetadataItem;
let thumbnailModel: ThumbnailModel;

export function setUp() {
  metadata = new MetadataItem();
  metadata.modificationTime = new Date(2015, 0, 1);
  metadata.present = true;
  metadata.thumbnailUrl = 'EXTERNAL_THUMBNAIL_URL';
  metadata.customIconUrl = 'CUSTOM_ICON_URL';
  metadata.contentThumbnailUrl = 'CONTENT_THUMBNAIL_URL';
  metadata.contentThumbnailTransform = contentThumbnailTransform;
  metadata.contentImageTransform = imageTransformation;

  thumbnailModel = new ThumbnailModel({
    get: function(_: Entry[], names: MetadataKey[]) {
      const result = new MetadataItem();
      // Assign all the properties from `metadata` with keys in `names` to the
      // same named property in `result`.
      (names as Array<keyof MetadataItem>)
          .forEach(
              <K extends keyof MetadataItem>(name: K) => result[name] =
                  metadata[name]);
      return Promise.resolve([result]);
    },
  } as MetadataModel);
}

export async function testThumbnailModelGetBasic() {
  const results = await thumbnailModel.get([imageEntry]);
  assertEquals(1, results.length);
  assertEquals(
      new Date(2015, 0, 1).toString(),
      results[0]!.filesystem!.modificationTime!.toString());
  assertEquals('EXTERNAL_THUMBNAIL_URL', results[0]!.external.thumbnailUrl);
  assertEquals('CUSTOM_ICON_URL', results[0]!.external.customIconUrl);
  assertTrue(!!results[0]!.external.present);
  assertEquals('CONTENT_THUMBNAIL_URL', results[0]!.thumbnail.url);
  assertEquals(contentThumbnailTransform, results[0]!.thumbnail.transform);
  assertEquals(imageTransformation, results[0]!.media.imageTransform);
}

export async function testThumbnailModelGetNotPresent() {
  metadata.present = false;
  const results = await thumbnailModel.get([imageEntry]);
  assertEquals(1, results.length);
  assertEquals(
      new Date(2015, 0, 1).toString(),
      results[0]!.filesystem!.modificationTime!.toString());
  assertEquals('EXTERNAL_THUMBNAIL_URL', results[0]!.external.thumbnailUrl);
  assertEquals('CUSTOM_ICON_URL', results[0]!.external.customIconUrl);
  assertFalse(!!results[0]!.external.present);
  assertEquals(undefined, results[0]!.thumbnail.url);
  assertEquals(undefined, results[0]!.thumbnail.transform);
  assertEquals(undefined, results[0]!.media.imageTransform);
}

export async function testThumbnailModelGetNonImage() {
  const results = await thumbnailModel.get([nonImageEntry]);
  assertEquals(1, results.length);
  assertEquals(
      new Date(2015, 0, 1).toString(),
      results[0]!.filesystem!.modificationTime!.toString());
  assertEquals('EXTERNAL_THUMBNAIL_URL', results[0]!.external.thumbnailUrl);
  assertEquals('CUSTOM_ICON_URL', results[0]!.external.customIconUrl);
  assertTrue(!!results[0]!.external.present);
  assertEquals(undefined, results[0]!.thumbnail.url);
  assertEquals(undefined, results[0]!.thumbnail.transform);
  assertEquals(undefined, results[0]!.media.imageTransform);
}
