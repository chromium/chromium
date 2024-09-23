// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ImageLoaderClient} from 'chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/image_loader_client.js';
import type {ImageOrientation, ImageTransformParam} from 'chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/image_orientation.js';
import type {LoadImageResponse} from 'chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/load_image_request.js';
import {type LoadImageRequest} from 'chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/load_image_request.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockEntry, MockFileSystem} from '../../common/js/mock_entry.js';

import type {ThumbnailMetadataItem} from './metadata/thumbnail_model.js';
import {FillMode, LoadTarget, ThumbnailLoader} from './thumbnail_loader.js';

type MockLoad =
    (request: LoadImageRequest,
     callback: (response: LoadImageResponse) => void) => void;

function getLoadTarget(
    entry: Entry, metadata: Partial<ThumbnailMetadataItem>|undefined) {
  return new ThumbnailLoader(entry, metadata as ThumbnailMetadataItem)
      .getLoadTarget();
}

function getRotate90(from: ImageOrientation|ImageTransformParam|
                     undefined): number|null {
  if (from && 'rotate90' in from) {
    return from.rotate90;
  }
  return null;
}

/**
 * Generates a data url of a sample image for testing.
 *
 * @param width Width.
 * @param height Height.
 * @return Data url of a sample image.
 */
function generateSampleImageDataUrl(width: number, height: number): string {
  const canvas = document.createElement('canvas');
  canvas.width = width;
  canvas.height = height;

  const context = canvas.getContext('2d');
  if (!context) {
    console.error('failed to get the canvas context');
    return '';
  }
  context.fillStyle = 'black';
  context.fillRect(0, 0, width / 2, height / 2);
  context.fillRect(width / 2, height / 2, width / 2, height / 2);

  return canvas.toDataURL('image/png');
}

/**
 * Installs a mock ImageLoader with a compatible load method.
 *
 */
function installMockLoad(mockLoad: MockLoad) {
  ImageLoaderClient.getInstance = () => {
    return {load: mockLoad} as ImageLoaderClient;
  };
}

export function testShouldUseMetadataThumbnail() {
  const mockFileSystem = new MockFileSystem('volumeId');
  const imageEntry = new MockEntry(mockFileSystem, '/test.jpg');
  const pdfEntry = new MockEntry(mockFileSystem, '/test.pdf');

  // Embed thumbnail is provided.
  assertEquals(LoadTarget.CONTENT_METADATA, getLoadTarget(imageEntry, {
                 thumbnail: {url: 'url'},
               }));

  // Drive thumbnail is provided and the file is not cached locally.
  assertEquals(LoadTarget.EXTERNAL_METADATA, getLoadTarget(imageEntry, {
                 external: {thumbnailUrl: 'url'},
               }));

  // Drive thumbnail is provided but the file is cached locally.
  assertEquals(LoadTarget.FILE_ENTRY, getLoadTarget(imageEntry, {
                 external: {thumbnailUrl: 'url', present: true},
               }));

  // Drive thumbnail is provided and it is not an image file.
  assertEquals(LoadTarget.EXTERNAL_METADATA, getLoadTarget(pdfEntry, {
                 external: {thumbnailUrl: 'url', present: true},
               }));
}

export async function testLoadAsDataUrlFromImageClient() {
  installMockLoad((_, callback) => {
    callback(
        {status: 'success', data: 'imageDataUrl', width: 32, height: 32} as
        LoadImageResponse);
  });

  const fileSystem = new MockFileSystem('volume-id');
  const entry = new MockEntry(fileSystem, '/Test1.jpg');
  const thumbnailLoader = new ThumbnailLoader(entry);
  const result = await thumbnailLoader.loadAsDataUrl(FillMode.OVER_FILL);
  assertEquals('imageDataUrl', result.data);
}

export async function testLoadAsDataUrlFromExifThumbnail() {
  installMockLoad((request, callback) => {
    // Assert that data url is passed.
    assertTrue(/^data:/i.test(request.url ?? ''));
    callback(
        {status: 'success', data: request.url, width: 32, height: 32} as
        LoadImageResponse);
  });

  const metadata = {
    filesystem: {},
    external: {},
    media: {},
    thumbnail: {
      url: generateSampleImageDataUrl(32, 32),
    },
  };

  const fileSystem = new MockFileSystem('volume-id');
  const entry = new MockEntry(fileSystem, '/Test1.jpg');
  const thumbnailLoader = new ThumbnailLoader(entry, metadata);
  const result = await thumbnailLoader.loadAsDataUrl(FillMode.OVER_FILL);
  assertEquals(metadata.thumbnail.url, result.data);
}

export async function testLoadAsDataUrlFromExifThumbnailPropagatesTransform() {
  installMockLoad((request, callback) => {
    // Assert that data url and transform info is passed.
    assertTrue(/^data:/i.test(request.url ?? ''));
    assertEquals(1, getRotate90(request.orientation));
    callback({
      status: 'success',
      data: generateSampleImageDataUrl(32, 64),
      width: 32,
      height: 64,
    } as LoadImageResponse);
  });

  const metadata = {
    filesystem: {},
    external: {},
    media: {},
    thumbnail: {
      url: generateSampleImageDataUrl(64, 32),
      transform: {
        rotate90: 1,
        scaleX: 1,
        scaleY: -1,
      },
    },
  };

  const fileSystem = new MockFileSystem('volume-id');
  const entry = new MockEntry(fileSystem, '/Test1.jpg');
  const thumbnailLoader = new ThumbnailLoader(entry, metadata);
  const result = await thumbnailLoader.loadAsDataUrl(FillMode.OVER_FILL);
  assertEquals(32, result.width);
  assertEquals(64, result.height);
  assertEquals(generateSampleImageDataUrl(32, 64), result.data);
}

export async function testLoadAsDataUrlFromExternal() {
  const externalThumbnailUrl = 'https://external-thumbnail-url/';
  const externalCroppedThumbnailUrl = 'https://external-cropped-thumbnail-url/';
  const externalThumbnailDataUrl = generateSampleImageDataUrl(32, 32);

  installMockLoad((request, callback) => {
    assertEquals(externalCroppedThumbnailUrl, request.url);
    callback({
      status: 'success',
      data: externalThumbnailDataUrl,
      width: 32,
      height: 32,
    } as LoadImageResponse);
  });

  const metadata = {
    filesystem: {},
    thumbnail: {},
    media: {},
    external: {
      thumbnailUrl: externalThumbnailUrl,
      croppedThumbnailUrl: externalCroppedThumbnailUrl,
    },
  };

  const fileSystem = new MockFileSystem('volume-id');
  const entry = new MockEntry(fileSystem, '/Test1.jpg');
  const thumbnailLoader = new ThumbnailLoader(entry, metadata);
  const result = await thumbnailLoader.loadAsDataUrl(FillMode.OVER_FILL);
  assertEquals(externalThumbnailDataUrl, result.data);
}
