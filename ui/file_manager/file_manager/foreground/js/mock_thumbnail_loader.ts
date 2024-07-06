// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {LoadTarget} from './thumbnail_loader.js';

/**
 * Mock thumbnail loader.
 */
export class MockThumbnailLoader {
  static testImageDataUrl: string|null = null;
  static testImageWidth = 0;
  static testImageHeight = 0;
  static errorUrls: string[] = [];

  /**
   * @param entry An entry.
   * @param metadata Metadata.
   * @param mediaType Media type.
   * @param loadTargets Load targets.
   * @param priority Priority.
   */
  constructor(
      private entry_: Entry, _metadata?: Object, _mediaType?: string,
      _loadTargets?: LoadTarget[], _priority?: number) {}

  /**
   * Loads thumbnail as data url.
   *
   * @return A promise which is resolved with data url.
   */
  async loadAsDataUrl():
      Promise<{data: null | string, width: number, height: number}> {
    if (MockThumbnailLoader.errorUrls.indexOf(this.entry_.toURL()) !== -1) {
      throw new Error('Failed to load thumbnail.');
    }

    return Promise.resolve({
      data: MockThumbnailLoader.testImageDataUrl,
      width: MockThumbnailLoader.testImageWidth,
      height: MockThumbnailLoader.testImageHeight,
    });
  }
}
