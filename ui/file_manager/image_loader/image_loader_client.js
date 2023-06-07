// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LRUCache} from 'chrome://file-manager/common/js/lru_cache.js';

import {LoadImageRequest, LoadImageResponse, LoadImageResponseStatus} from './load_image_request.js';

/**
 * Client used to connect to the remote ImageLoader extension. Client class runs
 * in the extension, where the client.js is included (eg. Files app).
 * It sends remote requests using IPC to the ImageLoader class and forwards
 * its responses.
 *
 * Implements cache, which is stored in the calling extension.
 *
 * @constructor
 */
export function ImageLoaderClient() {
  /**
   * @type {number}
   * @private
   */
  this.lastTaskId_ = 0;

  /**
   * LRU cache for images.
   * @type {!LRUCache.<{
   *   timestamp: ?number,
   *   width: number,
   *   height: number,
   *   ifd: ?string,
   *   data: string
   * }>}
   * @private
   */
  this.cache_ = new LRUCache(ImageLoaderClient.CACHE_MEMORY_LIMIT);
}

/**
 * Image loader's extension id.
 * @const
 * @type {string}
 */
ImageLoaderClient.EXTENSION_ID = 'pmfjbimdmchhbnneeidfognadeopoehp';

/**
 * Returns a singleton instance.
 * @return {ImageLoaderClient} Client instance.
 */
ImageLoaderClient.getInstance = function() {
  if (!ImageLoaderClient.instance_) {
    ImageLoaderClient.instance_ = new ImageLoaderClient();
  }
  return ImageLoaderClient.instance_;
};

/**
 * Records binary metrics. Counts for true and false are stored as a histogram.
 * @param {string} name Histogram's name.
 * @param {boolean} value True or false.
 */
ImageLoaderClient.recordBinary = function(name, value) {
  chrome.metricsPrivate.recordValue(
      { metricName: 'ImageLoader.Client.' + name,
        type: chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LINEAR,
        min: 1,  // According to histogram.h, this should be 1 for enums.
        max: 2,  // Maximum should be exclusive.
        buckets: 3 },  // Number of buckets: 0, 1 and overflowing 2.
      value ? 1 : 0);
};

/**
 * Records percent metrics, stored as a histogram.
 * @param {string} name Histogram's name.
 * @param {number} value Value (0..100).
 */
ImageLoaderClient.recordPercentage = function(name, value) {
  chrome.metricsPrivate.recordPercentage('ImageLoader.Client.' + name,
                                         Math.round(value));
};

/**
 * Sends a message to the Image Loader extension.
 * @param {!LoadImageRequest} request The image request.
 * @param {!function(!LoadImageResponse)=} callback Response handling callback.
 *     The response is passed as a hash array.
 * @private
 */
ImageLoaderClient.sendMessage_ = function(request, callback) {
  chrome.runtime.sendMessage(ImageLoaderClient.EXTENSION_ID, request, callback);
};

/**
 * Image loader client extension request URL matcher.
 * @const {!RegExp}
 */
ImageLoaderClient.CLIENT_URL_REGEX = /filesystem:chrome-extension:\/\/[a-z]+/;

/**
 * Image loader client chrome://file-manager request URL matcher.
 * @const {!RegExp}
 */
ImageLoaderClient.CLIENT_SWA_REGEX = /filesystem:chrome:\/\/file-manager/;

/**
 * All client request URL match ImageLoaderClient.CLIENT_URL_REGEX and all are
 * rewritten: the client extension id part of the request URL is replaced with
 * the image loader extension id.
 * @const {string}
 */
ImageLoaderClient.IMAGE_LOADER_URL =
    'filesystem:chrome-extension://' + ImageLoaderClient.EXTENSION_ID;

/**
 * Loads and resizes and image.
 *
 * @param {!LoadImageRequest} request
 * @param {!function(!LoadImageResponse)=} callback Response handling callback.
 * @return {?number} Remote task id or null if loaded from cache.
 */
ImageLoaderClient.prototype.load = function(request, callback) {
  // Record cache usage.
  ImageLoaderClient.recordPercentage('Cache.Usage',
      this.cache_.size() / ImageLoaderClient.CACHE_MEMORY_LIMIT * 100.0);

  // Replace the client origin with the image loader extension origin.
  request.url = request.url.replace(
      ImageLoaderClient.CLIENT_URL_REGEX, ImageLoaderClient.IMAGE_LOADER_URL);
  request.url = request.url.replace(
      ImageLoaderClient.CLIENT_SWA_REGEX, ImageLoaderClient.IMAGE_LOADER_URL);

  // Try to load from cache, if available.
  const cacheKey = LoadImageRequest.cacheKey(request);
  if (cacheKey) {
    if (request.cache) {
      // Load from cache.
      ImageLoaderClient.recordBinary('Cached', true);
      let cachedValue = this.cache_.get(cacheKey);
      // Check if the image in cache is up to date. If not, then remove it.
      if (cachedValue && cachedValue.timestamp != request.timestamp) {
        this.cache_.remove(cacheKey);
        cachedValue = null;
      }
      if (cachedValue && cachedValue.data &&
          cachedValue.width && cachedValue.height) {
        ImageLoaderClient.recordBinary('Cache.HitMiss', true);
        callback(new LoadImageResponse(LoadImageResponseStatus.SUCCESS, null, {
          width: cachedValue.width,
          height: cachedValue.height,
          ifd: cachedValue.ifd,
          data: cachedValue.data,
        }));
        return null;
      } else {
        ImageLoaderClient.recordBinary('Cache.HitMiss', false);
      }
    } else {
      // Remove from cache.
      ImageLoaderClient.recordBinary('Cached', false);
      this.cache_.remove(cacheKey);
    }
  }

  // Not available in cache, performing a request to a remote extension.
  this.lastTaskId_++;
  request.taskId = this.lastTaskId_;

  ImageLoaderClient.sendMessage_(request, function(result_data) {
    if (chrome.runtime.lastError) {
      console.warn(chrome.runtime.lastError.message);
      callback(new LoadImageResponse(
          LoadImageResponseStatus.ERROR,
          /** @type {number} */ (request.taskId)));
      return;
    }
    // TODO(tapted): Move to a prototype for better type checking.
    const result = /** @type {!LoadImageResponse} */ (result_data);
    // Save to cache.
    if (cacheKey && request.cache) {
      const value = LoadImageResponse.cacheValue(result, request.timestamp);
      if (value) {
        this.cache_.put(cacheKey, value, value.data.length);
      }
    }
    callback(result);
  }.bind(this));
  return request.taskId;
};

/**
 * Cancels the request. Note the original callback may still be invoked if this
 * message doesn't reach the ImageLoader before it starts processing.
 * @param {number} taskId Task id returned by ImageLoaderClient.load().
 */
ImageLoaderClient.prototype.cancel = function(taskId) {
  ImageLoaderClient.sendMessage_(
      LoadImageRequest.createCancel(taskId), function(result) {});
};

/**
 * Memory limit for images data in bytes.
 *
 * @const
 * @type {number}
 */
ImageLoaderClient.CACHE_MEMORY_LIMIT = 20 * 1024 * 1024;  // 20 MB.

// Helper functions.

/**
 * Loads and resizes and image.
 *
 * @param {!LoadImageRequest} request
 * @param {HTMLImageElement} image Image node to load the requested picture
 *     into.
 * @param {function()} onSuccess Callback for success.
 * @param {function()} onError Callback for failure.
 * @return {?number} Remote task id or null if loaded from cache.
 */
ImageLoaderClient.loadToImage = function(request, image, onSuccess, onError) {
  const callback = function(result) {
    if (!result || result.status == LoadImageResponseStatus.ERROR) {
      onError();
      return;
    }
    image.src = result.data;
    onSuccess();
  };

  return ImageLoaderClient.getInstance().load(request, callback);
};
