// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import type {ImageOrientation, ImageTransformParam} from './image_orientation.js';

export interface CacheValue {
  timestamp: number|null;
  width: number;
  height: number;
  ifd?: string;
  data: string;
}

/**
 * Response status.
 */
export enum LoadImageResponseStatus {
  SUCCESS = 'success',
  ERROR = 'error',
}

/**
 * Structure of the response object passed to the LoadImageRequest callback.
 * All methods must be static since this is passed between isolated contexts.
 */
export class LoadImageResponse {
  status: LoadImageResponseStatus;
  taskId: number|null;
  width?: number;
  height?: number;
  ifd?: string;
  /** The (compressed) image data as a data URL.  */
  data?: string;

  /** @param taskId or null if fulfilled by the client-side cache.  */
  constructor(status: LoadImageResponseStatus, taskId: number|null, result?: {
    width: number,
    height: number,
    ifd?: string,
    data: string,
  }) {
    this.status = status;
    this.taskId = taskId;

    if (status === LoadImageResponseStatus.ERROR) {
      return;
    }

    // Response result defined only when status === SUCCESS.
    assert(result);

    this.width = result.width;
    this.height = result.height;
    this.ifd = result.ifd;

    this.data = result.data;
  }

  /**
   * Returns the cacheable result value for |response|, or null for an error.
   *
   * @param response Response data from the ImageLoader.
   * @param timestamp The request timestamp. If undefined, then null is used.
   *     Currently this disables any caching in the ImageLoader, but disables
   *     only *expiration* in the client unless a timestamp is presented on a
   *     later request.
   */
  static cacheValue(response: LoadImageResponse, timestamp: number|undefined):
      CacheValue|null {
    if (!response || response.status === LoadImageResponseStatus.ERROR) {
      return null;
    }

    // Response result defined only when status === SUCCESS.
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
 */
export interface LoadImageRequest {
  // Parts that uniquely identify the request.
  /** Url of the requested image. Undefined only for cancellations.  */
  url?: string;
  orientation?: ImageOrientation|ImageTransformParam;
  scale?: number;
  width?: number;
  height?: number;
  maxWidth?: number;
  maxHeight?: number;

  // Parts that control the request flow.
  taskId?: number;
  cancel?: boolean;
  crop?: boolean;
  timestamp?: number;
  cache?: boolean;
  priority?: number;
}

/**
 * Creates a cache key.
 *
 * @return Cache key. It may be null if the cache does not support the request.
 *     e.g. Data URI.
 */
export function cacheKey(request: LoadImageRequest): string|null {
  if (/^data:/i.test(request.url ?? '')) {
    return null;
  }
  return JSON.stringify({
    url: request.url,
    orientation: request.orientation,
    scale: request.scale,
    width: request.width,
    height: request.height,
    maxWidth: request.maxWidth,
    maxHeight: request.maxHeight,
  });
}

/**
 * Creates a cancel request.
 *
 * @param taskId The task to cancel.
 */
export function createCancel(taskId: number): LoadImageRequest {
  return {taskId: taskId, cancel: true};
}

/**
 * Creates a load request from an option map.
 * Only the timestamp may be undefined.
 *
 * @param params Request parameters.
 */
export function createRequest(params: {
  url: string,
  maxWidth: number,
  maxHeight: number,
  cache: boolean,
  priority: number,
  timestamp?: number,
  orientation?: ImageTransformParam,
}): LoadImageRequest {
  return params;
}

/**
 * Creates a request to load a full-sized image.
 * Only the timestamp may be undefined.
 *
 * @param params Request parameters.
 */
export function createFullImageRequest(params: {
  url: string,
  cache: boolean,
  priority: number,
  timestamp?: number,
}): LoadImageRequest {
  return params;
}

/** Creates a load request from a url string. All options are undefined. */
export function createForUrl(url: string): LoadImageRequest {
  return {url: url};
}
