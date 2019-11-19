// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  context.fillStyle = 'black';
  context.fillRect(0, 0, width / 2, height / 2);
  context.fillRect(width / 2, height / 2, width / 2, height / 2);

  return canvas.toDataURL('image/png');
}

/**
 * Installs a mock ImageLoader with a compatible load method.
 *
 * @param {function(!LoadImageRequest, function(!Object))} mockLoad
 */
function installMockLoad(mockLoad) {
  ImageLoaderClient.getInstance = () => {
    return /** @type {!ImageLoaderClient} */ ({load: mockLoad});
  };
}

function testShouldUseMetadataThumbnail() {
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

function testLoadAsDataUrlFromImageClient(callback) {
  installMockLoad((request, callback) => {
    callback({status: 'success', data: 'imageDataUrl', width: 32, height: 32});
  });

  const fileSystem = new MockFileSystem('volume-id');
  const entry = new MockEntry(fileSystem, '/Test1.jpg');
  const thumbnailLoader = new ThumbnailLoader(entry);
  reportPromise(
      thumbnailLoader.loadAsDataUrl(ThumbnailLoader.FillMode.OVER_FILL)
          .then(result => {
            assertEquals('imageDataUrl', result.data);
          }),
      callback);
}

function testLoadAsDataUrlFromExifThumbnail(callback) {
  installMockLoad((request, callback) => {
    // Assert that data url is passed.
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

function testLoadAsDataUrlFromExifThumbnailPropagatesTransform(callback) {
  installMockLoad((request, callback) => {
    // Assert that data url and transform info is passed.
    assertTrue(/^data:/i.test(request.url));
    assertEquals(1, request.orientation.rotate90);
    callback({
      status: 'success',
      data: generateSampleImageDataUrl(32, 64),
      width: 32,
      height: 64
    });
  });

  const metadata = {
    thumbnail: {
      url: generateSampleImageDataUrl(64, 32),
      transform: {
        rotate90: 1,
        scaleX: 1,
        scaleY: -1,
      }
    }
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

function testLoadAsDataUrlFromExternal(callback) {
  const externalThumbnailUrl = 'https://external-thumbnail-url/';
  const externalCroppedThumbnailUrl = 'https://external-cropped-thumbnail-url/';
  const externalThumbnailDataUrl = generateSampleImageDataUrl(32, 32);

  installMockLoad((request, callback) => {
    assertEquals(externalCroppedThumbnailUrl, request.url);
    callback({
      status: 'success',
      data: externalThumbnailDataUrl,
      width: 32,
      height: 32
    });
  });

  const metadata = {
    external: {
      thumbnailUrl: externalThumbnailUrl,
      croppedThumbnailUrl: externalCroppedThumbnailUrl
    }
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

function testLoadDetachedFromExifInCavnasModeThumbnailDoesNotRotate(callback) {
  installMockLoad((request, callback) => {
    // Assert that data url is passed.
    assertTrue(/^data:/i.test(request.url));
    // Assert that the rotation is propagated to ImageLoader.
    assertEquals(1, request.orientation.rotate90);
    // ImageLoader returns rotated image.
    callback({
      status: 'success',
      data: generateSampleImageDataUrl(32, 64),
      width: 32,
      height: 64
    });
  });

  const metadata = {
    thumbnail: {
      url: generateSampleImageDataUrl(64, 32),
      transform: {
        rotate90: 1,
        scaleX: 1,
        scaleY: -1,
      }
    }
  };

  const fileSystem = new MockFileSystem('volume-id');
  const entry = new MockEntry(fileSystem, '/Test1.jpg');
  const thumbnailLoader =
      new ThumbnailLoader(entry, ThumbnailLoader.LoaderType.CANVAS, metadata);

  reportPromise(
      new Promise((resolve, reject) => {
        thumbnailLoader.loadDetachedImage(resolve);
      }).then(() => {
        const image = thumbnailLoader.getImage();
        // No need to rotate by loadDetachedImage() as it's already done.
        assertEquals(32, image.width);
        assertEquals(64, image.height);
      }),
      callback);
}
