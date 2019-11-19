// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Color space.
 *
 * @enum {string}
 */
const ColorSpace = {
  SRGB: 'sRgb',
  ADOBE_RGB: 'adobeRgb'
};

/**
 * Response status.
 *
 * @enum {string}
 */
const LoadImageResponseStatus = {
  SUCCESS: 'success',
  ERROR: 'error'
};

/**
 * Structure of the response object passed to the LoadImageRequest callback.
 * All methods must be static since this is passed between isolated contexts.
 *
 * @struct
 */
class LoadImageResponse {
  /**
   * @param {!LoadImageResponseStatus} status
   * @param {?number} taskId or null if fulfilled by the client-side cache.
   * @param {{width:number, height:number, ifd:?string, data:string}=}
   *    opt_result
   */
  constructor(status, taskId, opt_result) {
    /** @type {!LoadImageResponseStatus} */
    this.status = status;
    /** @type {?number} */
    this.taskId = taskId;

    if (status === LoadImageResponseStatus.ERROR) {
      return;
    }

    // Response result defined only when status == SUCCESS.
    assert(opt_result);

    /** @type {number|undefined} */
    this.width = opt_result.width;
    /** @type {number|undefined} */
    this.height = opt_result.height;
    /** @type {?string} */
    this.ifd = opt_result.ifd;

    /**
     * The (compressed) image data as a data URL.
     * @type {string|undefined}
     */
    this.data = opt_result.data;
  }

  /**
   * Returns the cacheable result value for |response|, or null for an error.
   *
   * @param {!LoadImageResponse} response Response data from the ImageLoader.
   * @param {number|undefined} timestamp The request timestamp. If undefined,
   *        then null is used. Currently this disables any caching in the
   *        ImageLoader, but disables only *expiration* in the client unless a
   *        timestamp is presented on a later request.
   * @return {?{
   *   timestamp: ?number,
   *   width: number,
   *   height: number,
   *   ifd: ?string,
   *   data: string
   * }}
   */
  static cacheValue(response, timestamp) {
    if (response.status === LoadImageResponseStatus.ERROR) {
      return null;
    }

    // Response result defined only when status == SUCCESS.
    assert(response.width);
    assert(response.height);
    assert(response.data);

    return {
      timestamp: timestamp || null,
      width: response.width,
      height: response.height,
      ifd: response.ifd,
      data: response.data,
    };
  }
}

/**
 * Encapsulates a request to load an image.
 * All methods must be static since this is passed between isolated contexts.
 *
 * @struct
 */
class LoadImageRequest {
  constructor() {
    // Parts that uniquely identify the request.

    /**
     * Url of the requested image. Undefined only for cancellations.
     * @type {string|undefined}
     */
    this.url;

    /** @type{ImageOrientation|ImageTransformParam|undefined} */
    this.orientation;
    /** @type {number|undefined} */
    this.scale;
    /** @type {number|undefined} */
    this.width;
    /** @type {number|undefined} */
    this.height;
    /** @type {number|undefined} */
    this.maxWidth;
    /** @type {number|undefined} */
    this.maxHeight;

    // Parts that control the request flow.

    /** @type {number|undefined} */
    this.taskId;
    /** @type {boolean|undefined} */
    this.cancel;
    /** @type {boolean|undefined} */
    this.crop;
    /** @type {number|undefined} */
    this.timestamp;
    /** @type {boolean|undefined} */
    this.cache;
    /** @type {number|undefined} */
    this.priority;

    /**
     * ColorSpace, only used for piex images.
     *
     * @type{ColorSpace|undefined}
     */
    this.colorSpace;
  }

  /**
   * Creates a cache key.
   *
   * @return {?string} Cache key. It may be null if the cache does not support
   *     the request. e.g. Data URI.
   */
  static cacheKey(request) {
    if (/^data:/i.test(request.url)) {
      return null;
    }
    return JSON.stringify({
      url: request.url,
      orientation: request.orientation,
      scale: request.scale,
      width: request.width,
      height: request.height,
      maxWidth: request.maxWidth,
      maxHeight: request.maxHeight
    });
  }

  /**
   * Creates a cancel request.
   *
   * @param{number} taskId The task to cancel.
   * @return {!LoadImageRequest}
   */
  static createCancel(taskId) {
    return /** @type {!LoadImageRequest} */ ({taskId: taskId, cancel: true});
  }

  /**
   * Creates a load request from an option map.
   * Only the timestamp may be undefined.
   *
   * @param {{
   *   url: !string,
   *   maxWidth: number,
   *   maxHeight: number,
   *   cache: boolean,
   *   priority: number,
   *   timestamp: (number|undefined),
   *   orientation: ?ImageTransformParam,
   * }} params Request parameters.
   * @return {!LoadImageRequest}
   */
  static createRequest(params) {
    return /** @type {!LoadImageRequest} */ (params);
  }

  /**
   * Creates a request to load a full-sized image.
   * Only the timestamp may be undefined.
   *
   * @param {{
   *   url: !string,
   *   cache: boolean,
   *   priority: number,
   *   timestamp: (?number|undefined),
   * }} params Request parameters.
   * @return {!LoadImageRequest}
   */
  static createFullImageRequest(params) {
    return /** @type {!LoadImageRequest} */ (params);
  }

  /**
   * Creates a load request from a url string. All options are undefined.
   *
   * @param {string} url
   * @return {!LoadImageRequest}
   */
  static createForUrl(url) {
    return /** @type {!LoadImageRequest} */ ({url: url});
  }
}
