// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {ImageCache} from './cache.js';
import type {ImageTransformParam} from './image_orientation.js';
import {ImageOrientation} from './image_orientation.js';
import {ImageRequestTask} from './image_request_task.js';
import type {LoadImageResponse} from './load_image_request.js';
import {type LoadImageRequest} from './load_image_request.js';
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

    // Listen for incoming requests.
    chrome.runtime.onMessageExternal.addListener(
        (msg, sender, sendResponse) => {
          if (!sender.origin || !msg) {
            return;
          }

          if (ALLOWED_CLIENT_ORIGINS.indexOf(sender.origin) === -1) {
            return;
          }

          this.onIncomingRequest_(msg, sender.origin, sendResponse);
        });

    chrome.runtime.onConnectNative.addListener(port => {
      assert(port.sender);
      if (port.sender.nativeApplication !== 'com.google.ash_thumbnail_loader') {
        port.disconnect();
        return;
      }

      port.onMessage.addListener(msg => {
        // Each connection is expected to handle a single request only.
        const started = this.onIncomingRequest_(
            msg, port.sender!.nativeApplication!, response => {
              port.postMessage(response);
              port.disconnect();
            });

        if (!started) {
          port.disconnect();
        }
      });
    });
  }

  /**
   * Handler for incoming requests.
   */
  private onIncomingRequest_(
      request: LoadImageRequest, senderOrigin: string,
      sendResponse: (r: unknown) => void) {
    // Sending a response may fail if the receiver already went offline.
    // This is not an error, but a normal and quite common situation.
    const failSafeSendResponse = function(response: unknown) {
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
    return this.onMessage_(senderOrigin, request, failSafeSendResponse);
  }

  /**
   * Handles a request. Depending on type of the request, starts or stops
   * an image task.
   * @return True if the message channel should stay alive until the
   *     callback is called.
   */
  private onMessage_(
      senderOrigin: string, request: LoadImageRequest,
      callback: (r: LoadImageResponse) => void): boolean {
    const requestId = senderOrigin + ':' + request.taskId;
    if (request.cancel) {
      // Cancel a task.
      this.scheduler_.remove(requestId);
      return false;  // No callback calls.
    }
    // Create a request task and add it to the scheduler (queue).
    const requestTask =
        new ImageRequestTask(requestId, this.cache_, request, callback);
    this.scheduler_.add(requestTask);
    return true;  // Request will call the callback.
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

/**
 * List of extensions allowed to perform image requests.
 */
const ALLOWED_CLIENT_ORIGINS = [
  'chrome://file-manager',  // File Manager SWA
];
