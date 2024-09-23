// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {ImageLoaderClient} from './image_loader_client.js';
import {createForUrl, LoadImageResponse, LoadImageResponseStatus} from './load_image_request.js';

/**
 * Lets the client to load URL and returns the local cache (not caches in the
 * image loader extension) is used or not.
 *
 * @param url URL
 * @param cache Whether to request caching on the request.
 * @return True if the local cache is used.
 */
function loadAndCheckCacheUsed(
    client: ImageLoaderClient, url: string, cache: boolean): Promise<boolean> {
  let cacheUsed = true;

  chrome.runtime.sendMessage = (_id, request, _options, callback) => {
    cacheUsed = false;
    callback?.(new LoadImageResponse(
        LoadImageResponseStatus.SUCCESS, request.taskId || -1,
        {width: 100, height: 100, ifd: undefined, data: 'ImageData'}));
  };

  const request = createForUrl(url);
  request.cache = cache;

  return new Promise((fulfill) => {
    client.load(request, () => {
      fulfill(cacheUsed);
    });
  });
}

export async function testCache() {
  const client = new ImageLoaderClient();

  const cacheUsed =
      await loadAndCheckCacheUsed(client, 'http://example.com/image.jpg', true);
  assertFalse(!!cacheUsed);
  const cacheUsed2 =
      await loadAndCheckCacheUsed(client, 'http://example.com/image.jpg', true);
  assertTrue(!!cacheUsed2);
}

export async function testNoCache() {
  const client = new ImageLoaderClient();
  const cacheUsed = await loadAndCheckCacheUsed(
      client, 'http://example.com/image.jpg', false);
  assertFalse(!!cacheUsed);
  const cacheUsed2 = await loadAndCheckCacheUsed(
      client, 'http://example.com/image.jpg', false);
  assertFalse(!!cacheUsed2);
}

export async function testDataURLCache() {
  const client = new ImageLoaderClient();
  const cacheUsed = await loadAndCheckCacheUsed(client, 'data:URI', true);
  assertFalse(!!cacheUsed);
  const cacheUsed2 = await loadAndCheckCacheUsed(client, 'data:URI', true);
  assertFalse(!!cacheUsed2);
}
