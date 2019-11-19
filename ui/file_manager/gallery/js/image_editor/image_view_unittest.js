// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testImageView() {
  const mockFileSystem = new MockFileSystem('volumeId');
  const mockEntry = MockFileEntry.create(mockFileSystem, '/test.jpg');

  // Item has full size cache.
  const itemWithFullCache = new MockGalleryItem(mockEntry, null, {});
  itemWithFullCache.contentImage =
      assertInstanceof(document.createElement('canvas'), HTMLCanvasElement);
  assertEquals(
      ImageView.LoadTarget.CACHED_MAIN_IMAGE,
      ImageView.getLoadTarget(itemWithFullCache, new ImageView.Effect.None()));

  // Item with content thumbnail.
  const itemWithContentThumbnail =
      new MockGalleryItem(mockEntry, null, {thumbnail: {url: 'url'}});
  assertEquals(
      ImageView.LoadTarget.THUMBNAIL,
      ImageView.getLoadTarget(
          itemWithContentThumbnail, new ImageView.Effect.None()));

  // Item with external thumbnail.
  const itemWithExternalThumbnail =
      new MockGalleryItem(mockEntry, null, {external: {thumbnailUrl: 'url'}});
  assertEquals(
      ImageView.LoadTarget.THUMBNAIL,
      ImageView.getLoadTarget(
          itemWithExternalThumbnail, new ImageView.Effect.None()));

  // Item with external thumbnail but present locally.
  const itemWithExternalThumbnailPresent = new MockGalleryItem(
      mockEntry, null, {external: {thumbnailUrl: 'url', present: true}});
  assertEquals(
      ImageView.LoadTarget.MAIN_IMAGE,
      ImageView.getLoadTarget(
          itemWithExternalThumbnailPresent, new ImageView.Effect.None()));

  // Item with external thumbnail shown by slide effect.
  const itemWithExternalThumbnailSlide =
      new MockGalleryItem(mockEntry, null, {external: {thumbnailUrl: 'url'}});
  assertEquals(
      ImageView.LoadTarget.THUMBNAIL,
      ImageView.getLoadTarget(
          itemWithExternalThumbnailSlide, new ImageView.Effect.Slide(1)));

  // Item with external thumbnail shown by zoom to screen effect.
  const itemWithExternalThumbnailZoomToScreen =
      new MockGalleryItem(mockEntry, null, {external: {thumbnailUrl: 'url'}});
  assertEquals(
      ImageView.LoadTarget.THUMBNAIL,
      ImageView.getLoadTarget(
          itemWithExternalThumbnailZoomToScreen,
          new ImageView.Effect.ZoomToScreen(new ImageRect(0, 0, 100, 100))));

  // Item with external thumbnail shown by zoom effect.
  const itemWithExternalThumbnailZoom =
      new MockGalleryItem(mockEntry, null, {external: {thumbnailUrl: 'url'}});
  assertEquals(
      ImageView.LoadTarget.MAIN_IMAGE,
      ImageView.getLoadTarget(
          itemWithExternalThumbnailZoom,
          new ImageView.Effect.Zoom(0, 0, new ImageRect(0, 0, 1, 1))));

  // Item without cache/thumbnail.
  const itemWithoutCacheOrThumbnail = new MockGalleryItem(mockEntry, null, {});
  assertEquals(
      ImageView.LoadTarget.MAIN_IMAGE,
      ImageView.getLoadTarget(
          itemWithoutCacheOrThumbnail, new ImageView.Effect.None));
}

function testLoadVideo(callback) {
  const container =
      assertInstanceof(document.createElement('div'), HTMLElement);
  // We re-use the image-container for video, it starts with this class.
  container.classList.add('image-container');

  const viewport = new Viewport(window);

  class MockMetadataProvider extends MetadataProvider {
    constructor() {
      super([]);
    }

    /** @override */
    async get() {
      return [];
    }
  }

  const metadataModel = new MetadataModel(new MockMetadataProvider());
  const imageView = new ImageView(container, viewport, metadataModel);

  const downloads = new MockFileSystem('file:///downloads');
  const getGalleryItem = function(path) {
    return new MockGalleryItem(
        MockFileEntry.create(downloads, path), null, {size: 100});
  };
  const item = getGalleryItem('/test.webm');
  const effect = new ImageView.Effect.None();
  const displayDone = function() {
    assertTrue(container.classList.contains('image-container'));
    assertTrue(container.classList.contains('video-container'));
    const video = container.firstElementChild;
    assertTrue(video instanceof HTMLVideoElement);
    const source = video.firstElementChild;
    assertTrue(source instanceof HTMLSourceElement);
    assertTrue(source.src.startsWith(
        'filesystem:file:///downloads/test.webm?nocache='));
    callback();
  };
  const loadDone = function() {};
  imageView.load(item, effect, displayDone, loadDone);
}
