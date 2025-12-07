// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {ImageCache} from './cache.js';
import type {ImageTransformParam} from './image_orientation.js';
import {ImageOrientation} from './image_orientation.js';
import {ImageRequestTask} from './image_request_task.js';
import type {LoadImageRequest, LoadImageResponse} from './load_image_request.js';
import {Scheduler} from './scheduler.js';

let instance: ImageLoader|null = null;

/**
 * Loads and resizes an image.
 */
export class ImageLoader {
  /**
   * Persistent cache object.
   */
  private cache_: ImageCache = new ImageCache();

  /**
   * Manages pending requests and runs them in order of priorities.
   */
  private scheduler_: Scheduler = new Scheduler();

  constructor() {
    // Initialize the cache and then start the scheduler.
    this.cache_.initialize(() => this.scheduler_.start());
  }

  /**
   * Handles a request. Depending on type of the request, starts or stops
   * an image task.
   * @return True if the message channel should stay alive until the
   *     sendResponse callback is called.
   */
  handle(
      request: LoadImageRequest,
      sendResponse: (r: LoadImageResponse) => void): boolean {
    assert(request.imageLoaderRequestId);
    if (request.cancel) {
      this.scheduler_.remove(request.imageLoaderRequestId);
      return false;
    }

    // When manually debugging the Image Loader extension, you can reply with
    // a placeholder image here by patching in https://crrev.com/c/5796592

    // Sending a response may fail if the receiver already went offline.
    // This is not an error, but a normal and quite common situation.
    const failSafeSendResponse = function(response: LoadImageResponse) {
      try {
        sendResponse(response);
      } catch (e) {
        // Ignore the error.
      }
    };
    // Incoming requests won't have the full type.
    assert(!(request.orientation instanceof ImageOrientation));
    assert(typeof request.orientation !== 'number');

    if (request.orientation) {
      request.orientation = ImageOrientation.fromRotationAndScale(
          request.orientation as ImageTransformParam);
    } else {
      request.orientation = new ImageOrientation(1, 0, 0, 1);
    }

    // Add a new request task to the scheduler (queue).
    this.scheduler_.add(new ImageRequestTask(
        request.imageLoaderRequestId, this.cache_, request,
        failSafeSendResponse));
    return true;
  }

  /**
   * Returns a singleton instance.
   */
  static getInstance(): ImageLoader {
    if (!instance) {
      instance = new ImageLoader();
    }
    return instance;
  }
}
