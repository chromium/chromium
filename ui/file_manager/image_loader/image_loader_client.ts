// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LruCache} from 'chrome://file-manager/common/js/lru_cache.js';

import {cacheKey, type CacheValue, createCancel, type LoadImageRequest, LoadImageResponse, LoadImageResponseStatus} from './load_image_request.js';

let instance: ImageLoaderClient|null = null;

/**
 * Client used to connect to the remote ImageLoader extension. Client class runs
 * in the extension, where the client.js is included (eg. Files app).
 * It sends remote requests using IPC to the ImageLoader class and forwards
 * its responses.
 *
 * Implements cache, which is stored in the calling extension.
 */
export class ImageLoaderClient {
  private lastTaskId_: number = 0;
  /**
   * LRU cache for images.
   */
  private cache_ = new LruCache<CacheValue>(CACHE_MEMORY_LIMIT);

  /**
   * Returns a singleton instance.
   */
  static getInstance(): ImageLoaderClient {
    if (!instance) {
      instance = new ImageLoaderClient();
    }
    return instance;
  }

  /**
   * Sends a message to the Image Loader extension.
   * @param request The image request.
   * @param callback Response handling callback. The response is passed as a
   *     hash array.
   */
  private static sendMessage_(
      request: LoadImageRequest, callback?: (r: LoadImageResponse) => void) {
    chrome.runtime.sendMessage(EXTENSION_ID, request, {}, callback);
  }

  /**
   * Loads and resizes and image.
   *
   * @param callback Response handling callback.
   * @return Remote task id or null if loaded from cache.
   */
  load(request: LoadImageRequest, callback: (r: LoadImageResponse) => void):
      null|number {
    // Replace the client origin with the image loader extension origin.
    request.url = request.url ?? '';
    request.url = request.url.replace(CLIENT_URL_REGEX, IMAGE_LOADER_URL);
    request.url = request.url.replace(CLIENT_SWA_REGEX, IMAGE_LOADER_URL);

    // Try to load from cache, if available.
    const key = cacheKey(request);
    if (key) {
      if (request.cache) {
        // Load from cache.
        let cachedValue: CacheValue|null = this.cache_.get(key);
        // Check if the image in cache is up to date. If not, then remove it.
        // It relies on comparing `null` equals to `undefined`.
        // eslint-disable-next-line eqeqeq
        if (cachedValue && cachedValue.timestamp != request.timestamp) {
          this.cache_.remove(key);
          cachedValue = null;
        }
        if (cachedValue && cachedValue.data && cachedValue.width &&
            cachedValue.height) {
          callback(
              new LoadImageResponse(LoadImageResponseStatus.SUCCESS, null, {
                width: cachedValue.width,
                height: cachedValue.height,
                ifd: cachedValue.ifd,
                data: cachedValue.data,
              }));
          return null;
        }
      } else {
        // Remove from cache.
        this.cache_.remove(key);
      }
    }

    // Not available in cache, performing a request to a remote extension.
    this.lastTaskId_++;
    request.taskId = this.lastTaskId_;

    ImageLoaderClient.sendMessage_(request, (resultData) => {
      if (chrome.runtime.lastError) {
        console.warn(chrome.runtime.lastError.message);
        callback(new LoadImageResponse(
            LoadImageResponseStatus.ERROR, request.taskId!));
        return;
      }
      const result = resultData;
      // Save to cache.
      if (key && request.cache) {
        const value: CacheValue|null =
            LoadImageResponse.cacheValue(result, request.timestamp);
        if (value) {
          this.cache_.put(key, value, value.data.length);
        }
      }
      callback(result);
    });
    return request.taskId;
  }

  /**
   * Cancels the request. Note the original callback may still be invoked if
   * this message doesn't reach the ImageLoader before it starts processing.
   * @param taskId Task id returned by ImageLoaderClient.load().
   */
  cancel(taskId: number) {
    ImageLoaderClient.sendMessage_(createCancel(taskId), (_result) => {});
  }

  // Helper functions.

  /**
   * Loads and resizes and image.
   *
   * @param image Image node to load the requested picture into.
   * @param onSuccess Callback for success.
   * @param onError Callback for failure.
   * @return Remote task id or null if loaded from cache.
   */
  static loadToImage(
      request: LoadImageRequest, image: HTMLImageElement,
      onSuccess: VoidCallback, onError: VoidCallback): null|number {
    const callback = (result: LoadImageResponse) => {
      if (!result || result.status === LoadImageResponseStatus.ERROR) {
        onError();
        return;
      }
      image.src = result.data!;
      onSuccess();
    };

    return ImageLoaderClient.getInstance().load(request, callback);
  }
}

/**
 * Image loader's extension id.
 */
const EXTENSION_ID = 'pmfjbimdmchhbnneeidfognadeopoehp';

/**
 * Image loader client extension request URL matcher.
 */
const CLIENT_URL_REGEX = /filesystem:chrome-extension:\/\/[a-z]+/;

/**
 * Image loader client chrome://file-manager request URL matcher.
 */
const CLIENT_SWA_REGEX = /filesystem:chrome:\/\/file-manager/;

/**
 * All client request URL match CLIENT_URL_REGEX and all are
 * rewritten: the client extension id part of the request URL is replaced with
 * the image loader extension id.
 */
const IMAGE_LOADER_URL = 'filesystem:chrome-extension://' + EXTENSION_ID;

/**
 * Memory limit for images data in bytes.
 */
const CACHE_MEMORY_LIMIT = 20 * 1024 * 1024;  // 20 MB.
