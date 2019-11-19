// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-lottie' is a wrapper around the player for lottie
 * animations.
 * Fires a 'cr-lottie-initialized' event when the animation was successfully
 * initialized.
 * Fires a 'cr-lottie-playing' event when the animation starts playing.
 * Fires a 'cr-lottie-paused' event when the animation has paused.
 * Fires a 'cr-lottie-resized' event when the canvas the animation is being
 * drawn on is resized.
 */

/**
 * The resource url for the lottier web worker script.
 * @const {string}
 */
const LOTTIE_JS_URL = 'chrome://resources/lottie/lottie_worker.min.js';

Polymer({
  is: 'cr-lottie',

  properties: {
    animationUrl: {
      type: String,
      value: '',
    },
    autoplay: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {?HTMLCanvasElement} */
  canvasElement_: null,

  /** @private {boolean} True if the animation has loaded successfully */
  isAnimationLoaded_: false,

  /** @private {?OffscreenCanvas} */
  offscreenCanvas_: null,

  /** @private {?ResizeObserver} */
  resizeObserver_: null,

  /** @private {?Worker} */
  worker_: null,

  /** @override */
  attached: function() {
    // CORS blocks loading worker script from a different origin but
    // loading scripts as blob and then instantiating it as web worker
    // is possible.
    this.sendXmlHttpRequest_(LOTTIE_JS_URL, 'blob', function(response) {
      if (this.isAttached) {
        this.worker_ = new Worker(URL.createObjectURL(response));
        this.worker_.onmessage = this.onMessage_.bind(this);
        this.initialize_();
      }
    }.bind(this));
  },

  /** @override */
  detached: function() {
    if (this.resizeObserver_) {
      this.resizeObserver_.disconnect();
    }
    if (this.worker_) {
      this.worker_.terminate();
      this.worker_ = null;
    }
  },

  /**
   * Controls the animation based on the value of |shouldPlay|.
   * @param {boolean} shouldPlay Will play the animation if true else pauses it.
   */
  setPlay: function(shouldPlay) {
    if (this.isAnimationLoaded_) {
      this.worker_.postMessage({control: {play: shouldPlay}});
    } else {
      this.autoplay = shouldPlay;
    }
  },

  /**
   * Initializes all the members of this polymer element.
   * @private
   */
  initialize_: function() {
    // Generate an offscreen canvas.
    this.canvasElement_ =
        /** @type {HTMLCanvasElement} */ (this.$.canvas);
    this.offscreenCanvas_ = this.canvasElement_.transferControlToOffscreen();

    this.resizeObserver_ =
        new ResizeObserver(this.onCanvasElementResized_.bind(this));
    this.resizeObserver_.observe(this.canvasElement_);

    if (this.isAnimationLoaded_) {
      return;
    }

    // Open animation file and start playing the animation.
    this.sendXmlHttpRequest_(
        this.animationUrl, 'json', this.initAnimation_.bind(this));
  },

  /**
   * Computes the draw buffer size for the canvas. This ensures that the
   * rasterization is crisp and sharp rather than blurry.
   * @return {Object} Size of the canvas draw buffer
   * @private
   */
  getCanvasDrawBufferSize_: function() {
    const canvasElement = this.$.canvas;
    const devicePixelRatio = window.devicePixelRatio;
    const clientRect = canvasElement.getBoundingClientRect();
    const drawSize = {
      width: clientRect.width * devicePixelRatio,
      height: clientRect.height * devicePixelRatio
    };
    return drawSize;
  },

  /**
   * Returns true if the |maybeValidUrl| provided is safe to use in an
   * XMLHTTPRequest.
   * @param {string} maybeValidUrl The url string to check for validity.
   * @return {boolean}
   * @private
   */
  isValidUrl_: function(maybeValidUrl) {
    const url = new URL(maybeValidUrl, document.location.href);
    return url.protocol === 'chrome:' ||
        (url.protocol == 'data:' &&
         url.pathname.startsWith('application/json;'));
  },

  /**
   * Sends an XMLHTTPRequest to load a resource and runs the callback on
   * getting a successful response.
   * @param {string} url The URL to load the resource.
   * @param {string} responseType The type of response the request would
   *     give on success.
   * @param {function((Object|null|string))} successCallback The callback to run
   *     when a successful response is received.
   * @private
   */
  sendXmlHttpRequest_: function(url, responseType, successCallback) {
    assert(this.isValidUrl_(url), 'Invalid scheme or data url used.');
    const xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.responseType = responseType;
    xhr.send();
    xhr.onreadystatechange = function() {
      if (xhr.readyState == 4 && xhr.status == 200) {
        successCallback(xhr.response);
      }
    };
  },

  /**
   * Handles the canvas element resize event. This informs the offscreen
   * canvas worker of the new canvas size.
   * @private
   */
  onCanvasElementResized_: function() {
    if (this.isAnimationLoaded_) {
      this.worker_.postMessage({drawSize: this.getCanvasDrawBufferSize_()});
    }
  },

  /**
   * Initializes the the animation on the web worker with the data provided.
   * @param {Object|null|string} animationData The animation that will be
   * played.
   * @private
   */
  initAnimation_: function(animationData) {
    this.worker_.postMessage(
        {
          canvas: this.offscreenCanvas_,
          animationData: animationData,
          drawSize: this.getCanvasDrawBufferSize_(),
          params: {loop: true, autoplay: this.autoplay}
        },
        [this.offscreenCanvas_]);
  },

  /**
   * Handles the messages sent from the web worker to its parent thread.
   * @param {Event} event Event sent by the web worker.
   * @private
   */
  onMessage_: function(event) {
    if (event.data.name == 'initialized' && event.data.success) {
      this.isAnimationLoaded_ = true;
      this.fire('cr-lottie-initialized');
    } else if (event.data.name == 'playing') {
      this.fire('cr-lottie-playing');
    } else if (event.data.name == 'paused') {
      this.fire('cr-lottie-paused');
    } else if (event.data.name == 'resized') {
      this.fire('cr-lottie-resized', event.data.size);
    }
  },
});
