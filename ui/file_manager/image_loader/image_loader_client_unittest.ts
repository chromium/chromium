// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

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

/**
 * Helper to set up mocks for console.warn,
 * chrome.fileManagerPrivate.grantAccess, and chrome.runtime.sendMessage, and
 * return their state and a restore function.
 *
 * @param grantAccessMock The mock implementation for grantAccess.
 * @param sendMessageMock The mock implementation for sendMessage.
 */
function setUpGrantAccessMocks(
    grantAccessMock: (...args: any[]) => void,
    sendMessageMock: (id: any, request: any, options: any, callback?: any) =>
        void) {
  // Ensure fileManagerPrivate is defined on chrome.
  chrome.fileManagerPrivate = chrome.fileManagerPrivate || {};

  const originalWarn = console.warn;
  const originalGrantAccess = chrome.fileManagerPrivate.grantAccess;
  const originalSendMessage = chrome.runtime.sendMessage;
  const originalLastError = chrome.runtime.lastError;

  const mocks = {
    warnMessage: null as any,
    grantAccessCalled: false,
    grantAccessUrls: [] as string[],
    grantAccessOptions: null as any,
    sendMessageCalled: false,
    restore() {
      console.warn = originalWarn;
      chrome.fileManagerPrivate.grantAccess = originalGrantAccess;
      chrome.runtime.sendMessage = originalSendMessage;
      chrome.runtime.lastError = originalLastError;
    },
  };

  console.warn = (message: any) => {
    mocks.warnMessage = message;
  };

  chrome.fileManagerPrivate.grantAccess = (...args: any[]) => {
    mocks.grantAccessCalled = true;
    mocks.grantAccessUrls = args[0] as string[];
    if (args.length === 3) {
      mocks.grantAccessOptions = args[1];
    }
    grantAccessMock(...args);
  };

  chrome.runtime.sendMessage = (id, request, options, callback) => {
    mocks.sendMessageCalled = true;
    sendMessageMock(id, request, options, callback);
  };

  chrome.runtime.lastError = undefined;

  return mocks;
}

export async function testGrantAccessSuccess() {
  const client = new ImageLoaderClient();
  const imageUrl =
      'filesystem:chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/foo.jpg';

  const mocks = setUpGrantAccessMocks(
      (...args) => {
        const callback = args[args.length - 1];
        callback();
      },
      (_id, request, _options, callback) => {
        callback?.(new LoadImageResponse(
            LoadImageResponseStatus.SUCCESS, request.taskId || -1,
            {width: 100, height: 100, ifd: undefined, data: 'ImageData'}));
      });

  const request = createForUrl(imageUrl);

  try {
    const resultPromise = new Promise<LoadImageResponse>((resolve) => {
      client.load(request, (response) => {
        resolve(response);
      });
    });

    const response = await resultPromise;

    assertTrue(mocks.grantAccessCalled);
    assertTrue(mocks.sendMessageCalled);
    assertTrue(mocks.grantAccessUrls.includes(imageUrl));
    assertEquals(true, mocks.grantAccessOptions?.forThumbnailing);
    assertTrue(response.status === LoadImageResponseStatus.SUCCESS);
    assertEquals(null, mocks.warnMessage);
  } finally {
    mocks.restore();
  }
}

export async function testGrantAccessFailure() {
  const client = new ImageLoaderClient();
  const imageUrl =
      'filesystem:chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/foo.jpg';

  const failureMessage = 'Grant access failed';

  const mocks = setUpGrantAccessMocks((..._args) => {
    chrome.runtime.lastError = {message: failureMessage};
  }, (_id, _request, _options, _callback) => {});

  const request = createForUrl(imageUrl);

  try {
    const resultPromise = new Promise<LoadImageResponse>((resolve) => {
      const taskId = client.load(request, (response) => {
        resolve(response);
      });
      // Due to how mocks are set up, as the grantAccess function sets the
      // chrome.runtime.lastError property immediately and hasHandledError also
      // executes its code synchronously and returns false, the load method
      // must return null taskId.
      assertTrue(taskId === null);
    });

    const response = await resultPromise;

    assertFalse(mocks.sendMessageCalled);
    assertTrue(response.status === LoadImageResponseStatus.ERROR);
    assertEquals(failureMessage, mocks.warnMessage);
  } finally {
    mocks.restore();
  }
}
