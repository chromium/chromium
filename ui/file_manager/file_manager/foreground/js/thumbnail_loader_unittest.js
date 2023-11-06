// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ImageLoaderClient} from 'chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/image_loader_client.js';
import {LoadImageRequest} from 'chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/load_image_request.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockEntry, MockFileSystem} from '../../common/js/mock_entry.js';
import {reportPromise} from '../../common/js/test_error_reporting.js';

import {ThumbnailLoader} from './thumbnail_loader.js';

/**
 * @param {Entry} entry
 * @param {?Object} metadata
 */
function getLoadTarget(entry, metadata) {
  return new ThumbnailLoader(entry, ThumbnailLoader.LoaderType.CANVAS, metadata)
      .getLoadTarget();
}

/**
 * Generates a data url of a sample image for testing.
 *
 * @param {number} width Width.
 * @param {number} height Height.
 * @return {string} Data url of a sample image.
 */
function generateSampleImageDataUrl(width, height) {
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
 * @param {function(!LoadImageRequest, function(!Object):void ):void} mockLoad
 */
function installMockLoad(mockLoad) {
  ImageLoaderClient.getInstance = () => {
    return /** @type {!ImageLoaderClient} */ ({load: mockLoad});
  };
}

export function testShouldUseMetadataThumbnail() {
  const mockFileSystem = new MockFileSystem('volumeId');
  const imageEntry = new MockEntry(mockFileSystem, '/test.jpg');
  const pdfEntry = new MockEntry(mockFileSystem, '/test.pdf');

  // Embed thumbnail is provided.
  assertEquals(
      ThumbnailLoader.LoadTarget.CONTENT_METADATA,
      getLoadTarget(imageEntry, {thumbnail: {url: 'url'}}));

  // Drive thumbnail is provided and the file is not cached locally.
  assertEquals(
      ThumbnailLoader.LoadTarget.EXTERNAL_METADATA,
      getLoadTarget(imageEntry, {external: {thumbnailUrl: 'url'}}));

  // Drive thumbnail is provided but the file is cached locally.
  assertEquals(
      ThumbnailLoader.LoadTarget.FILE_ENTRY,
      getLoadTarget(
          imageEntry, {external: {thumbnailUrl: 'url', present: true}}));

  // Drive thumbnail is provided and it is not an image file.
  assertEquals(
      ThumbnailLoader.LoadTarget.EXTERNAL_METADATA,
      getLoadTarget(
          pdfEntry, {external: {thumbnailUrl: 'url', present: true}}));
}

/** @param {()=>void} callback */
export function testLoadAsDataUrlFromImageClient(callback) {
  installMockLoad((_, callback) => {
    callback({status: 'success', data: 'imageDataUrl', width: 32, height: 32});
  });

  const fileSystem = new MockFileSystem('volume-id');
  const entry = /** @type {Entry}*/ (new MockEntry(fileSystem, '/Test1.jpg'));
  const thumbnailLoader = new ThumbnailLoader(entry);
  reportPromise(
      thumbnailLoader.loadAsDataUrl(ThumbnailLoader.FillMode.OVER_FILL)
          .then(result => {
            assertEquals('imageDataUrl', result.data);
          }),
      callback);
}

/** @param {()=>void} callback */
export function testLoadAsDataUrlFromExifThumbnail(callback) {
  installMockLoad((request, callback) => {
    // Assert that data url is passed.
    // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
    // assignable to parameter of type 'string'.
    assertTrue(/^data:/i.test(request.url));
    callback({status: 'success', data: request.url, width: 32, height: 32});
  });

  const metadata = {
    thumbnail: {
      url: generateSampleImageDataUrl(32, 32),
    },
  };

  const fileSystem = new MockFileSystem('volume-id');
  const entry = new MockEntry(fileSystem, '/Test1.jpg');
  const thumbnailLoader = new ThumbnailLoader(entry, undefined, metadata);
  reportPromise(
      thumbnailLoader.loadAsDataUrl(ThumbnailLoader.FillMode.OVER_FILL)
          .then(result => {
            assertEquals(metadata.thumbnail.url, result.data);
          }),
      callback);
}

/** @param {()=>void} callback */
export function testLoadAsDataUrlFromExifThumbnailPropagatesTransform(
    callback) {
  installMockLoad((request, callback) => {
    // Assert that data url and transform info is passed.
    // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
    // assignable to parameter of type 'string'.
    assertTrue(/^data:/i.test(request.url));
    // @ts-ignore: error TS2339: Property 'rotate90' does not exist on type
    // 'ImageOrientation | ImageTransformParam'.
    assertEquals(1, request.orientation.rotate90);
    callback({
      status: 'success',
      data: generateSampleImageDataUrl(32, 64),
      width: 32,
      height: 64,
    });
  });

  const metadata = {
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
  const thumbnailLoader = new ThumbnailLoader(entry, undefined, metadata);
  reportPromise(
      thumbnailLoader.loadAsDataUrl(ThumbnailLoader.FillMode.OVER_FILL)
          .then(result => {
            assertEquals(32, result.width);
            assertEquals(64, result.height);
            assertEquals(generateSampleImageDataUrl(32, 64), result.data);
          }),
      callback);
}

/** @param {()=>void} callback */
export function testLoadAsDataUrlFromExternal(callback) {
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
    });
  });

  const metadata = {
    external: {
      thumbnailUrl: externalThumbnailUrl,
      croppedThumbnailUrl: externalCroppedThumbnailUrl,
    },
  };

  const fileSystem = new MockFileSystem('volume-id');
  const entry = new MockEntry(fileSystem, '/Test1.jpg');
  const thumbnailLoader = new ThumbnailLoader(entry, undefined, metadata);
  reportPromise(
      thumbnailLoader.loadAsDataUrl(ThumbnailLoader.FillMode.OVER_FILL)
          .then(result => {
            assertEquals(externalThumbnailDataUrl, result.data);
          }),
      callback);
}

/** @param {()=>void} callback */
export function testLoadDetachedFromExifInCavnasModeThumbnailDoesNotRotate(
    callback) {
  installMockLoad((request, callback) => {
    // Assert that data url is passed.
    // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
    // assignable to parameter of type 'string'.
    assertTrue(/^data:/i.test(request.url));
    // Assert that the rotation is propagated to ImageLoader.
    // @ts-ignore: error TS2339: Property 'rotate90' does not exist on type
    // 'ImageOrientation | ImageTransformParam'.
    assertEquals(1, request.orientation.rotate90);
    // ImageLoader returns rotated image.
    callback({
      status: 'success',
      data: generateSampleImageDataUrl(32, 64),
      width: 32,
      height: 64,
    });
  });

  const metadata = {
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
  const thumbnailLoader =
      new ThumbnailLoader(entry, ThumbnailLoader.LoaderType.CANVAS, metadata);

  reportPromise(
      // @ts-ignore: error TS6133: 'reject' is declared but its value is never
      // read.
      new Promise((resolve, reject) => {
        thumbnailLoader.loadDetachedImage(resolve);
      }).then(() => {
        const image = thumbnailLoader.getImage();
        // No need to rotate by loadDetachedImage() as it's already done.
        // @ts-ignore: error TS2339: Property 'width' does not exist on type
        // 'HTMLCanvasElement | (new (width?: number | undefined, height?:
        // number | undefined) => HTMLImageElement)'.
        assertEquals(32, image.width);
        // @ts-ignore: error TS2339: Property 'height' does not exist on type
        // 'HTMLCanvasElement | (new (width?: number | undefined, height?:
        // number | undefined) => HTMLImageElement)'.
        assertEquals(64, image.height);
      }),
      callback);
}
