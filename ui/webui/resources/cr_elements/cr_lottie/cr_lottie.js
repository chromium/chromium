// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-lottie' is a wrapper around the player for lottie
 * animations. Since the player runs on a worker thread, 'cr-lottie' requires
 * the document CSP to be set to "worker-src blob: 'self';".
 * Fires a 'cr-lottie-initialized' event when the animation was successfully
 * initialized.
 * Fires a 'cr-lottie-playing' event when the animation starts playing.
 * Fires a 'cr-lottie-paused' event when the animation has paused.
 * Fires a 'cr-lottie-stopped' event when animation has stopped.
 * Fires a 'cr-lottie-resized' event when the canvas the animation is being
 * drawn on is resized.
 */

/**
 * The resource url for the lottier web worker script.
 * @const {string}
 */
/* #export */ const LOTTIE_JS_URL =
    'chrome://resources/lottie/lottie_worker.min.js';

Polymer({
  is: 'cr-lottie',

  properties: {
    animationUrl: {
      type: String,
      value: '',
      observer: 'animationUrlChanged_',
    },

    autoplay: {
      type: Boolean,
      value: false,
    },

    hidden: {
      type: Boolean,
      value: false,
    },

    singleLoop: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {?HTMLCanvasElement} */
  canvasElement_: null,

  /** @private {boolean} Whether the animation has loaded successfully */
  isAnimationLoaded_: false,

  /** @private {?OffscreenCanvas} */
  offscreenCanvas_: null,

  /**
   * @private {boolean} Whether the canvas has been transferred to the worker
   * thread.
   */
  hasTransferredCanvas_: false,

  /** @private {?ResizeObserver} */
  resizeObserver_: null,

  /** @private {?Worker} */
  worker_: null,

  /** @private {?XMLHttpRequest} The current in-flight request. */
  xhr_: null,

  /** @override */
  attached() {
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
  detached() {
    if (this.resizeObserver_) {
      this.resizeObserver_.disconnect();
    }
    if (this.worker_) {
      this.worker_.terminate();
      this.worker_ = null;
    }
    if (this.xhr_) {
      this.xhr_.abort();
      this.xhr_ = null;
    }
  },

  /**
   * Controls the animation based on the value of |shouldPlay|.
   * @param {boolean} shouldPlay Will play the animation if true else pauses it.
   */
  setPlay(shouldPlay) {
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
  initialize_() {
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
   * Updates the animation that is being displayed.
   * @param {string} animationUrl the new animation URL.
   * @param {string} oldAnimationUrl the previous animation URL.
   * @private
   */
  animationUrlChanged_(animationUrl, oldAnimationUrl) {
    if (!this.worker_) {
      // The worker hasn't loaded yet. We will load the new animation once the
      // worker loads.
      return;
    }
    if (this.xhr_) {
      // There is an in-flight request to load the previous animation. Abort it
      // before loading a new image.
      this.xhr_.abort();
      this.xhr_ = null;
    }
    if (this.isAnimationLoaded_) {
      this.worker_.postMessage({control: {stop: true}});
      this.isAnimationLoaded_ = false;
    }
    this.sendXmlHttpRequest_(
        this.animationUrl, 'json', this.initAnimation_.bind(this));
  },

  /**
   * Computes the draw buffer size for the canvas. This ensures that the
   * rasterization is crisp and sharp rather than blurry.
   * @return {Object} Size of the canvas draw buffer
   * @private
   */
  getCanvasDrawBufferSize_() {
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
  isValidUrl_(maybeValidUrl) {
    const url = new URL(maybeValidUrl, document.location.href);
    return url.protocol === 'chrome:' ||
        (url.protocol === 'data:' &&
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
  sendXmlHttpRequest_(url, responseType, successCallback) {
    assert(this.isValidUrl_(url), 'Invalid scheme or data url used.');
    assert(!this.xhr_);

    this.xhr_ = new XMLHttpRequest();
    this.xhr_.open('GET', url, true);
    this.xhr_.responseType = responseType;
    this.xhr_.send();
    this.xhr_.onreadystatechange = () => {
      if (this.xhr_.readyState === 4 && this.xhr_.status === 200) {
        // |successCallback| might trigger another xhr, so we set to null before
        // calling it.
        const response = this.xhr_.response;
        this.xhr_ = null;
        successCallback(response);
      }
    };
  },

  /**
   * Handles the canvas element resize event. This informs the offscreen
   * canvas worker of the new canvas size.
   * @private
   */
  onCanvasElementResized_() {
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
  initAnimation_(animationData) {
    const message = [{
      animationData,
      drawSize: this.getCanvasDrawBufferSize_(),
      params: {loop: !this.singleLoop, autoplay: this.autoplay}
    }];
    if (!this.hasTransferredCanvas_) {
      message[0].canvas = this.offscreenCanvas_;
      message.push([this.offscreenCanvas_]);
      this.hasTransferredCanvas_ = true;
    }
    this.worker_.postMessage(...message);
  },

  /**
   * Handles the messages sent from the web worker to its parent thread.
   * @param {Event} event Event sent by the web worker.
   * @private
   */
  onMessage_(event) {
    if (event.data.name === 'initialized' && event.data.success) {
      this.isAnimationLoaded_ = true;
      this.fire('cr-lottie-initialized');
    } else if (event.data.name === 'playing') {
      this.fire('cr-lottie-playing');
    } else if (event.data.name === 'paused') {
      this.fire('cr-lottie-paused');
    } else if (event.data.name === 'stopped') {
      this.fire('cr-lottie-stopped');
    } else if (event.data.name === 'resized') {
      this.fire('cr-lottie-resized', event.data.size);
    }
  },
});
/* #ignore */ console.warn('crbug/1173575, non-JS module files deprecated.');
