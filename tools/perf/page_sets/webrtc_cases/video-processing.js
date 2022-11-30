/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

'use strict';

/* global VideoMirrorHelper */ // defined in video-mirror-helper.js

/**
 * Opens the device's camera with getUserMedia.
 * @implements {MediaStreamSource} in pipeline.js
 */
class CameraSource { // eslint-disable-line no-unused-vars
  constructor() {
    /**
     * @private @const {!VideoMirrorHelper} manages displaying the video stream
     *     in the page
     */
    this.videoMirrorHelper_ = new VideoMirrorHelper();
    /** @private {?MediaStream} camera stream, initialized in getMediaStream */
    this.stream_ = null;
    /** @private {string} */
    this.debugPath_ = '<unknown>';
  }
  /** @override */
  setDebugPath(path) {
    this.debugPath_ = path;
    this.videoMirrorHelper_.setDebugPath(`${path}.videoMirrorHelper_`);
  }
  /** @override */
  setVisibility(visible) {
    this.videoMirrorHelper_.setVisibility(visible);
  }
  /** @override */
  async getMediaStream() {
    if (this.stream_) return this.stream_;
    console.log('[CameraSource] Requesting camera.');
    this.stream_ =
        await navigator.mediaDevices.getUserMedia({audio: false, video: true});
    console.log(
        '[CameraSource] Received camera stream.',
        `${this.debugPath_}.stream_ =`, this.stream_);
    this.videoMirrorHelper_.setStream(this.stream_);
    return this.stream_;
  }
  /** @override */
  destroy() {
    console.log('[CameraSource] Stopping camera');
    this.videoMirrorHelper_.destroy();
    if (this.stream_) {
      this.stream_.getTracks().forEach(t => t.stop());
    }
  }
}

/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

'use strict';

const TEXT_SOURCE =
    'https://raw.githubusercontent.com/w3c/mediacapture-insertable-streams/main/explainer.md';
const CANVAS_ASPECT_RATIO = 16 / 9;

/**
 * @param {number} x
 * @return {number} x rounded to the nearest even integer
 */
function roundToEven(x) {
  return 2 * Math.round(x / 2);
}

/**
 * Draws text on a Canvas.
 * @implements {MediaStreamSource} in pipeline.js
 */
class CanvasSource { // eslint-disable-line no-unused-vars
  constructor() {
    /** @private {boolean} */
    this.visibility_ = false;
    /**
     * @private {?HTMLCanvasElement} canvas element providing the MediaStream.
     */
    this.canvas_ = null;
    /**
     * @private {?CanvasRenderingContext2D} the 2D context used to draw the
     *     animation.
     */
    this.ctx_ = null;
    /**
     * @private {?MediaStream} the MediaStream from captureStream.
     */
    this.stream_ = null;
    /**
     * @private {?CanvasCaptureMediaStreamTrack} the capture track from
     *     canvas_, obtained from stream_. We manually request new animation
     *     frames on this track.
     */
    this.captureTrack_ = null;
    /** @private {number} requestAnimationFrame handle */
    this.requestAnimationFrameHandle_ = 0;
    /** @private {!Array<string>} text to render */
    this.text_ = ['WebRTC samples'];
    /** @private {string} */
    this.debugPath_ = '<unknown>';
    fetch(TEXT_SOURCE)
        .then(response => {
          if (response.ok) {
            return response.text();
          }
          throw new Error(`Request completed with status ${response.status}.`);
        })
        .then(text => {
          this.text_ = text.trim().split('\n');
        })
        .catch((e) => {
          console.log(`[CanvasSource] The request to retrieve ${
            TEXT_SOURCE} encountered an error: ${e}.`);
        });
  }
  /** @override */
  setDebugPath(path) {
    this.debugPath_ = path;
  }
  /** @override */
  setVisibility(visible) {
    this.visibility_ = visible;
    if (this.canvas_) {
      this.updateCanvasVisibility();
    }
  }
  /** @private */
  updateCanvasVisibility() {
    if (this.canvas_.parentNode && !this.visibility_) {
      this.canvas_.parentNode.removeChild(this.canvas_);
    } else if (!this.canvas_.parentNode && this.visibility_) {
      console.log('[CanvasSource] Adding source canvas to page.');
      const outputVideoContainer =
          document.getElementById('outputVideoContainer');
      outputVideoContainer.parentNode.insertBefore(
          this.canvas_, outputVideoContainer);
    }
  }
  /** @private */
  requestAnimationFrame() {
    this.requestAnimationFrameHandle_ =
        requestAnimationFrame(now => this.animate(now));
  }
  /**
   * @private
   * @param {number} now current animation timestamp
   */
  animate(now) {
    this.requestAnimationFrame();
    const ctx = this.ctx_;
    if (!this.canvas_ || !ctx || !this.captureTrack_) {
      return;
    }

    // Resize canvas based on displayed size; or if not visible, based on the
    // output video size.
    // VideoFrame prefers to have dimensions that are even numbers.
    if (this.visibility_) {
      this.canvas_.width = roundToEven(this.canvas_.clientWidth);
    } else {
      const outputVideoContainer =
          document.getElementById('outputVideoContainer');
      const outputVideo = outputVideoContainer.firstElementChild;
      if (outputVideo) {
        this.canvas_.width = roundToEven(outputVideo.clientWidth);
      }
    }
    this.canvas_.height = roundToEven(this.canvas_.width / CANVAS_ASPECT_RATIO);

    ctx.fillStyle = '#fff';
    ctx.fillRect(0, 0, this.canvas_.width, this.canvas_.height);

    const linesShown = 20;
    const millisecondsPerLine = 1000;
    const linesIncludingExtraBlank = this.text_.length + linesShown;
    const totalAnimationLength = linesIncludingExtraBlank * millisecondsPerLine;
    const currentFrame = now % totalAnimationLength;
    const firstLineIdx = Math.floor(
        linesIncludingExtraBlank * (currentFrame / totalAnimationLength) -
        linesShown);
    const lineFraction = (now % millisecondsPerLine) / millisecondsPerLine;

    const border = 20;
    const fontSize = (this.canvas_.height - 2 * border) / (linesShown + 1);
    ctx.font = `${fontSize}px sansserif`;

    const textWidth = this.canvas_.width - 2 * border;

    // first line
    if (firstLineIdx >= 0) {
      const fade = Math.floor(256 * lineFraction);
      ctx.fillStyle = `rgb(${fade},${fade},${fade})`;
      const position = (2 - lineFraction) * fontSize;
      ctx.fillText(this.text_[firstLineIdx], border, position, textWidth);
    }

    // middle lines
    for (let line = 2; line <= linesShown - 1; line++) {
      const lineIdx = firstLineIdx + line - 1;
      if (lineIdx >= 0 && lineIdx < this.text_.length) {
        ctx.fillStyle = 'black';
        const position = (line + 1 - lineFraction) * fontSize;
        ctx.fillText(this.text_[lineIdx], border, position, textWidth);
      }
    }

    // last line
    const lastLineIdx = firstLineIdx + linesShown - 1;
    if (lastLineIdx >= 0 && lastLineIdx < this.text_.length) {
      const fade = Math.floor(256 * (1 - lineFraction));
      ctx.fillStyle = `rgb(${fade},${fade},${fade})`;
      const position = (linesShown + 1 - lineFraction) * fontSize;
      ctx.fillText(this.text_[lastLineIdx], border, position, textWidth);
    }

    this.captureTrack_.requestFrame();
  }
  /** @override */
  async getMediaStream() {
    if (this.stream_) return this.stream_;

    console.log('[CanvasSource] Initializing 2D context for source animation.');
    this.canvas_ =
      /** @type {!HTMLCanvasElement} */ (document.createElement('canvas'));
    this.canvas_.classList.add('video', 'sourceVideo');
    // Generally video frames do not have an alpha channel. Even if the browser
    // supports it, there may be a performance cost, so we disable alpha.
    this.ctx_ = /** @type {?CanvasRenderingContext2D} */ (
      this.canvas_.getContext('2d', {alpha: false}));
    if (!this.ctx_) {
      throw new Error('Unable to create CanvasRenderingContext2D');
    }
    this.updateCanvasVisibility();
    this.stream_ = this.canvas_.captureStream(0);
    this.captureTrack_ = /** @type {!CanvasCaptureMediaStreamTrack} */ (
      this.stream_.getTracks()[0]);
    this.requestAnimationFrame();
    console.log(
        '[CanvasSource] Initialized canvas, context, and capture stream.',
        `${this.debugPath_}.canvas_ =`, this.canvas_,
        `${this.debugPath_}.ctx_ =`, this.ctx_, `${this.debugPath_}.stream_ =`,
        this.stream_, `${this.debugPath_}.captureTrack_ =`, this.captureTrack_);

    return this.stream_;
  }
  /** @override */
  destroy() {
    console.log('[CanvasSource] Stopping source animation');
    if (this.requestAnimationFrameHandle_) {
      cancelAnimationFrame(this.requestAnimationFrameHandle_);
    }
    if (this.canvas_) {
      if (this.canvas_.parentNode) {
        this.canvas_.parentNode.removeChild(this.canvas_);
      }
    }
  }
}

/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

'use strict';

/**
 * Applies a picture-frame effect using CanvasRenderingContext2D.
 * @implements {FrameTransform} in pipeline.js
 */
class CanvasTransform { // eslint-disable-line no-unused-vars
  constructor() {
    /**
     * @private {?OffscreenCanvas} canvas used to create the 2D context.
     *     Initialized in init.
     */
    this.canvas_ = null;
    /**
     * @private {?CanvasRenderingContext2D} the 2D context used to draw the
     *     effect. Initialized in init.
     */
    this.ctx_ = null;
    /**
     * @private {boolean} If false, pass VideoFrame directly to
     * CanvasRenderingContext2D.drawImage and create VideoFrame directly from
     * this.canvas_. If either of these operations fail (it's not supported in
     * Chrome <90 and broken in Chrome 90: https://crbug.com/1184128), we set
     * this field to true; in that case we create an ImageBitmap from the
     * VideoFrame and pass the ImageBitmap to drawImage on the input side and
     * create the VideoFrame using an ImageBitmap of the canvas on the output
     * side.
     */
    this.use_image_bitmap_ = false;
    /** @private {string} */
    this.debugPath_ = 'debug.pipeline.frameTransform_';
  }
  /** @override */
  async init() {
    console.log('[CanvasTransform] Initializing 2D context for transform');
    this.canvas_ = new OffscreenCanvas(1, 1);
    this.ctx_ = /** @type {?CanvasRenderingContext2D} */ (
      this.canvas_.getContext('2d', {alpha: false, desynchronized: true}));
    if (!this.ctx_) {
      throw new Error('Unable to create CanvasRenderingContext2D');
    }
    console.log(
        '[CanvasTransform] CanvasRenderingContext2D initialized.',
        `${this.debugPath_}.canvas_ =`, this.canvas_,
        `${this.debugPath_}.ctx_ =`, this.ctx_);
  }

  /** @override */
  async transform(frame, controller) {
    const ctx = this.ctx_;
    if (!this.canvas_ || !ctx) {
      frame.close();
      return;
    }
    const width = frame.displayWidth;
    const height = frame.displayHeight;
    this.canvas_.width = width;
    this.canvas_.height = height;
    const timestamp = frame.timestamp;

    if (!this.use_image_bitmap_) {
      try {
        // Supported for Chrome 90+.
        ctx.drawImage(frame, 0, 0);
      } catch (e) {
        // This should only happen on Chrome <90.
        console.log(
            '[CanvasTransform] Failed to draw VideoFrame directly. Falling ' +
                'back to ImageBitmap.',
            e);
        this.use_image_bitmap_ = true;
      }
    }
    if (this.use_image_bitmap_) {
      // Supported for Chrome <92.
      const inputBitmap = await frame.createImageBitmap();
      ctx.drawImage(inputBitmap, 0, 0);
      inputBitmap.close();
    }
    frame.close();

    ctx.shadowColor = '#000';
    ctx.shadowBlur = 20;
    ctx.lineWidth = 50;
    ctx.strokeStyle = '#000';
    ctx.strokeRect(0, 0, width, height);

    if (!this.use_image_bitmap_) {
      try {
        // alpha: 'discard' is needed in order to send frames to a PeerConnection.
        controller.enqueue(new VideoFrame(this.canvas_, {timestamp, alpha: 'discard'}));
      } catch (e) {
        // This should only happen on Chrome <91.
        console.log(
            '[CanvasTransform] Failed to create VideoFrame from ' +
                'OffscreenCanvas directly. Falling back to ImageBitmap.',
            e);
        this.use_image_bitmap_ = true;
      }
    }
    if (this.use_image_bitmap_) {
      const outputBitmap = await createImageBitmap(this.canvas_);
      const outputFrame = new VideoFrame(outputBitmap, {timestamp});
      outputBitmap.close();
      controller.enqueue(outputFrame);
    }
  }

  /** @override */
  destroy() {}
}

/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

'use strict';

/* global MediaStreamTrackProcessor, MediaStreamTrackGenerator */
if (typeof MediaStreamTrackProcessor === 'undefined' ||
    typeof MediaStreamTrackGenerator === 'undefined') {
  alert(
      'Your browser does not support the experimental MediaStreamTrack API ' +
      'for Insertable Streams of Media. See the note at the bottom of the ' +
      'page.');
}

// In Chrome 88, VideoFrame.close() was called VideoFrame.destroy()
if (VideoFrame.prototype.close === undefined) {
  VideoFrame.prototype.close = VideoFrame.prototype.destroy;
}

/* global CameraSource */ // defined in camera-source.js
/* global CanvasSource */ // defined in canvas-source.js
/* global CanvasTransform */ // defined in canvas-transform.js
/* global PeerConnectionSink */ // defined in peer-connection-sink.js
/* global PeerConnectionSource */ // defined in peer-connection-source.js
/* global Pipeline */ // defined in pipeline.js
/* global NullTransform, DropTransform, DelayTransform */ // defined in simple-transforms.js
/* global VideoSink */ // defined in video-sink.js
/* global VideoSource */ // defined in video-source.js
/* global WebGLTransform */ // defined in webgl-transform.js
/* global WebCodecTransform */ // defined in webcodec-transform.js

/**
 * Allows inspecting objects in the console. See console log messages for
 * attributes added to this debug object.
 * @type {!Object<string,*>}
 */
let debug = {};

/**
 * FrameTransformFn applies a transform to a frame and queues the output frame
 * (if any) using the controller. The first argument is the input frame and the
 * second argument is the stream controller.
 * The VideoFrame should be closed as soon as it is no longer needed to free
 * resources and maintain good performance.
 * @typedef {function(
 *     !VideoFrame,
 *     !TransformStreamDefaultController<!VideoFrame>): !Promise<undefined>}
 */
let FrameTransformFn; // eslint-disable-line no-unused-vars

/**
 * Creates a pair of MediaStreamTrackProcessor and MediaStreamTrackGenerator
 * that applies transform to sourceTrack. This function is the core part of the
 * sample, demonstrating how to use the new API.
 * @param {!MediaStreamTrack} sourceTrack the video track to be transformed. The
 *     track can be from any source, e.g. getUserMedia, RTCTrackEvent, or
 *     captureStream on HTMLMediaElement or HTMLCanvasElement.
 * @param {!FrameTransformFn} transform the transform to apply to sourceTrack;
 *     the transformed frames are available on the returned track. See the
 *     implementations of FrameTransform.transform later in this file for
 *     examples.
 * @param {!AbortSignal} signal can be used to stop processing
 * @return {!MediaStreamTrack} the result of sourceTrack transformed using
 *     transform.
 */
// eslint-disable-next-line no-unused-vars
function createProcessedMediaStreamTrack(sourceTrack, transform, signal) {
  // Create the MediaStreamTrackProcessor.
  /** @type {?MediaStreamTrackProcessor<!VideoFrame>} */
  let processor;
  try {
    processor = new MediaStreamTrackProcessor(sourceTrack);
  } catch (e) {
    alert(`MediaStreamTrackProcessor failed: ${e}`);
    throw e;
  }

  // Create the MediaStreamTrackGenerator.
  /** @type {?MediaStreamTrackGenerator<!VideoFrame>} */
  let generator;
  try {
    generator = new MediaStreamTrackGenerator('video');
  } catch (e) {
    alert(`MediaStreamTrackGenerator failed: ${e}`);
    throw e;
  }

  const source = processor.readable;
  const sink = generator.writable;

  // Create a TransformStream using our FrameTransformFn. (Note that the
  // "Stream" in TransformStream refers to the Streams API, specified by
  // https://streams.spec.whatwg.org/, not the Media Capture and Streams API,
  // specified by https://w3c.github.io/mediacapture-main/.)
  /** @type {!TransformStream<!VideoFrame, !VideoFrame>} */
  const transformer = new TransformStream({transform});

  // Apply the transform to the processor's stream and send it to the
  // generator's stream.
  const promise = source.pipeThrough(transformer, {signal}).pipeTo(sink);

  promise.catch((e) => {
    if (signal.aborted) {
      console.log(
          '[createProcessedMediaStreamTrack] Shutting down streams after abort.');
    } else {
      console.error(
          '[createProcessedMediaStreamTrack] Error from stream transform:', e);
    }
    source.cancel(e);
    sink.abort(e);
  });

  debug['processor'] = processor;
  debug['generator'] = generator;
  debug['transformStream'] = transformer;
  console.log(
      '[createProcessedMediaStreamTrack] Created MediaStreamTrackProcessor, ' +
          'MediaStreamTrackGenerator, and TransformStream.',
      'debug.processor =', processor, 'debug.generator =', generator,
      'debug.transformStream =', transformer);

  return generator;
}

/**
 * The current video pipeline. Initialized by initPipeline().
 * @type {?Pipeline}
 */
let pipeline;

/**
 * Sets up handlers for interacting with the UI elements on the page.
 */
function initUI() {
  const sourceSelector = /** @type {!HTMLSelectElement} */ (
    document.getElementById('sourceSelector'));
  const sourceVisibleCheckbox = (/** @type {!HTMLInputElement} */ (
    document.getElementById('sourceVisible')));
  /**
   * Updates the pipeline based on the current settings of the sourceSelector
   * and sourceVisible UI elements. Unlike updatePipelineSource(), never
   * re-initializes the pipeline.
   */
  function updatePipelineSourceIfSet() {
    const sourceType =
        sourceSelector.options[sourceSelector.selectedIndex].value;
    if (!sourceType) return;
    console.log(`[UI] Selected source: ${sourceType}`);
    let source;
    switch (sourceType) {
      case 'camera':
        source = new CameraSource();
        break;
      case 'video':
        source = new VideoSource();
        break;
      case 'canvas':
        source = new CanvasSource();
        break;
      case 'pc':
        source = new PeerConnectionSource(new CameraSource());
        break;
      default:
        alert(`unknown source ${sourceType}`);
        return;
    }
    source.setVisibility(sourceVisibleCheckbox.checked);
    pipeline.updateSource(source);
  }
  /**
   * Updates the pipeline based on the current settings of the sourceSelector
   * and sourceVisible UI elements. If the "stopped" option is selected,
   * reinitializes the pipeline instead.
   */
  function updatePipelineSource() {
    const sourceType =
        sourceSelector.options[sourceSelector.selectedIndex].value;
    if (!sourceType || !pipeline) {
      initPipeline();
    } else {
      updatePipelineSourceIfSet();
    }
  }
  sourceSelector.oninput = updatePipelineSource;
  sourceSelector.disabled = false;

  /**
   * Updates the source visibility, if the source is already started.
   */
  function updatePipelineSourceVisibility() {
    console.log(`[UI] Changed source visibility: ${
        sourceVisibleCheckbox.checked ? 'added' : 'removed'}`);
    if (pipeline) {
      const source = pipeline.getSource();
      if (source) {
        source.setVisibility(sourceVisibleCheckbox.checked);
      }
    }
  }
  sourceVisibleCheckbox.oninput = updatePipelineSourceVisibility;
  sourceVisibleCheckbox.disabled = false;

  const transformSelector = /** @type {!HTMLSelectElement} */ (
    document.getElementById('transformSelector'));
  /**
   * Updates the pipeline based on the current settings of the transformSelector
   * UI element.
   */
  function updatePipelineTransform() {
    if (!pipeline) {
      return;
    }
    const transformType =
        transformSelector.options[transformSelector.selectedIndex].value;
    console.log(`[UI] Selected transform: ${transformType}`);
    switch (transformType) {
      case 'webgl':
        pipeline.updateTransform(new WebGLTransform());
        break;
      case 'canvas2d':
        pipeline.updateTransform(new CanvasTransform());
        break;
      case 'drop':
        // Defined in simple-transforms.js.
        pipeline.updateTransform(new DropTransform());
        break;
      case 'noop':
        // Defined in simple-transforms.js.
        pipeline.updateTransform(new NullTransform());
        break;
      case 'delay':
        // Defined in simple-transforms.js.
        pipeline.updateTransform(new DelayTransform());
        break;
      case 'webcodec':
        // Defined in webcodec-transform.js
        pipeline.updateTransform(new WebCodecTransform());
        break;
      default:
        alert(`unknown transform ${transformType}`);
        break;
    }
  }
  transformSelector.oninput = updatePipelineTransform;
  transformSelector.disabled = false;

  const sinkSelector = (/** @type {!HTMLSelectElement} */ (
    document.getElementById('sinkSelector')));
  /**
   * Updates the pipeline based on the current settings of the sinkSelector UI
   * element.
   */
  function updatePipelineSink() {
    const sinkType = sinkSelector.options[sinkSelector.selectedIndex].value;
    console.log(`[UI] Selected sink: ${sinkType}`);
    switch (sinkType) {
      case 'video':
        pipeline.updateSink(new VideoSink());
        break;
      case 'pc':
        pipeline.updateSink(new PeerConnectionSink());
        break;
      default:
        alert(`unknown sink ${sinkType}`);
        break;
    }
  }
  sinkSelector.oninput = updatePipelineSink;
  sinkSelector.disabled = false;

  /**
   * Initializes/reinitializes the pipeline. Called on page load and after the
   * user chooses to stop the video source.
   */
  function initPipeline() {
    if (pipeline) pipeline.destroy();
    pipeline = new Pipeline();
    debug = {pipeline};
    updatePipelineSourceIfSet();
    updatePipelineTransform();
    updatePipelineSink();
    console.log(
        '[initPipeline] Created new Pipeline.', 'debug.pipeline =', pipeline);
  }
}

window.onload = initUI;

/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

'use strict';

/**
 * Sends a MediaStream to one end of an RTCPeerConnection and provides the
 * remote end as the resulting MediaStream.
 * In an actual video calling app, the two RTCPeerConnection objects would be
 * instantiated on different devices. However, in this sample, both sides of the
 * peer connection are local to allow the sample to be self-contained.
 * For more detailed samples using RTCPeerConnection, take a look at
 * https://webrtc.github.io/samples/.
 */
class PeerConnectionPipe { // eslint-disable-line no-unused-vars
  /**
   * @param {!MediaStream} inputStream stream to pipe over the peer connection
   * @param {string} debugPath the path to this object from the debug global var
   */
  constructor(inputStream, debugPath) {
    /**
     * @private @const {!RTCPeerConnection} the calling side of the peer
     *     connection, connected to inputStream_.
     */
    this.caller_ = new RTCPeerConnection(null);
    /**
     * @private @const {!RTCPeerConnection} the answering side of the peer
     *     connection, providing the stream returned by getMediaStream.
     */
    this.callee_ = new RTCPeerConnection(null);
    /** @private {string} */
    this.debugPath_ = debugPath;
    /**
     * @private @const {!Promise<!MediaStream>} the stream containing tracks
     *     from callee_, returned by getMediaStream.
     */
    this.outputStreamPromise_ = this.init_(inputStream);
  }
  /**
   * Sets the path to this object from the debug global var.
   * @param {string} path
   */
  setDebugPath(path) {
    this.debugPath_ = path;
  }
  /**
   * @param {!MediaStream} inputStream stream to pipe over the peer connection
   * @return {!Promise<!MediaStream>}
   * @private
   */
  async init_(inputStream) {
    console.log(
        '[PeerConnectionPipe] Initiating peer connection.',
        `${this.debugPath_} =`, this);
    this.caller_.onicecandidate = (/** !RTCPeerConnectionIceEvent*/ event) => {
      if (event.candidate) this.callee_.addIceCandidate(event.candidate);
    };
    this.callee_.onicecandidate = (/** !RTCPeerConnectionIceEvent */ event) => {
      if (event.candidate) this.caller_.addIceCandidate(event.candidate);
    };
    const outputStream = new MediaStream();
    const receiverStreamPromise = new Promise(resolve => {
      this.callee_.ontrack = (/** !RTCTrackEvent */ event) => {
        outputStream.addTrack(event.track);
        if (outputStream.getTracks().length == inputStream.getTracks().length) {
          resolve(outputStream);
        }
      };
    });
    inputStream.getTracks().forEach(track => {
      this.caller_.addTransceiver(track, {direction: 'sendonly'});
    });
    await this.caller_.setLocalDescription();
    await this.callee_.setRemoteDescription(
        /** @type {!RTCSessionDescription} */ (this.caller_.localDescription));
    await this.callee_.setLocalDescription();
    await this.caller_.setRemoteDescription(
        /** @type {!RTCSessionDescription} */ (this.callee_.localDescription));
    await receiverStreamPromise;
    console.log(
        '[PeerConnectionPipe] Peer connection established.',
        `${this.debugPath_}.caller_ =`, this.caller_,
        `${this.debugPath_}.callee_ =`, this.callee_);
    return receiverStreamPromise;
  }

  /**
   * Provides the MediaStream that has been piped through a peer connection.
   * @return {!Promise<!MediaStream>}
   */
  getOutputStream() {
    return this.outputStreamPromise_;
  }

  /** Frees any resources used by this object. */
  destroy() {
    console.log('[PeerConnectionPipe] Closing peer connection.');
    this.caller_.close();
    this.callee_.close();
  }
}

/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

'use strict';

/* global PeerConnectionPipe */ // defined in peer-connection-pipe.js
/* global VideoSink */ // defined in video-sink.js

/**
 * Sends the transformed video to one end of an RTCPeerConnection and displays
 * the remote end in a video element. In this sample, a PeerConnectionSink
 * represents processing the local user's camera input using a
 * MediaStreamTrackProcessor before sending it to a remote video call
 * participant. Contrast with a PeerConnectionSource.
 * @implements {MediaStreamSink} in pipeline.js
 */
class PeerConnectionSink { // eslint-disable-line no-unused-vars
  constructor() {
    /**
     * @private @const {!VideoSink} manages displaying the video stream in the
     *     page
     */
    this.videoSink_ = new VideoSink();
    /**
     * @private {?PeerConnectionPipe} handles piping the MediaStream through an
     *     RTCPeerConnection
     */
    this.pipe_ = null;
    /** @private {string} */
    this.debugPath_ = 'debug.pipeline.sink_';
    this.videoSink_.setDebugPath(`${this.debugPath_}.videoSink_`);
  }

  /** @override */
  async setMediaStream(stream) {
    console.log(
        '[PeerConnectionSink] Setting peer connection sink stream.', stream);
    if (this.pipe_) this.pipe_.destroy();
    this.pipe_ = new PeerConnectionPipe(stream, `${this.debugPath_}.pipe_`);
    const pipedStream = await this.pipe_.getOutputStream();
    console.log(
        '[PeerConnectionSink] Received callee peer connection stream.',
        pipedStream);
    await this.videoSink_.setMediaStream(pipedStream);
  }

  /** @override */
  destroy() {
    this.videoSink_.destroy();
    if (this.pipe_) this.pipe_.destroy();
  }
}

/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

'use strict';

/* global PeerConnectionPipe */ // defined in peer-connection-pipe.js
/* global VideoMirrorHelper */ // defined in video-mirror-helper.js

/**
 * Sends the original source video to one end of an RTCPeerConnection and
 * provides the remote end as the final source.
 * In this sample, a PeerConnectionSource represents receiving video from a
 * remote participant and locally processing it using a
 * MediaStreamTrackProcessor before displaying it on the screen. Contrast with a
 * PeerConnectionSink.
 * @implements {MediaStreamSource} in pipeline.js
 */
class PeerConnectionSource { // eslint-disable-line no-unused-vars
  /**
   * @param {!MediaStreamSource} originalSource original stream source, whose
   *     output is sent over the peer connection
   */
  constructor(originalSource) {
    /**
     * @private @const {!VideoMirrorHelper} manages displaying the video stream
     *     in the page
     */
    this.videoMirrorHelper_ = new VideoMirrorHelper();
    /**
     * @private @const {!MediaStreamSource} original stream source, whose output
     *     is sent on the sender peer connection. In an actual video calling
     *     app, this stream would be generated from the remote participant's
     *     camera. However, in this sample, both sides of the peer connection
     *     are local to allow the sample to be self-contained.
     */
    this.originalStreamSource_ = originalSource;
    /**
     * @private {?PeerConnectionPipe} handles piping the MediaStream through an
     *     RTCPeerConnection
     */
    this.pipe_ = null;
    /** @private {string} */
    this.debugPath_ = '<unknown>';
  }
  /** @override */
  setDebugPath(path) {
    this.debugPath_ = path;
    this.videoMirrorHelper_.setDebugPath(`${path}.videoMirrorHelper_`);
    this.originalStreamSource_.setDebugPath(`${path}.originalStreamSource_`);
    if (this.pipe_) this.pipe_.setDebugPath(`${path}.pipe_`);
  }
  /** @override */
  setVisibility(visible) {
    this.videoMirrorHelper_.setVisibility(visible);
  }

  /** @override */
  async getMediaStream() {
    if (this.pipe_) return this.pipe_.getOutputStream();

    console.log(
        '[PeerConnectionSource] Obtaining original source media stream.',
        `${this.debugPath_}.originalStreamSource_ =`,
        this.originalStreamSource_);
    const originalStream = await this.originalStreamSource_.getMediaStream();
    this.pipe_ =
        new PeerConnectionPipe(originalStream, `${this.debugPath_}.pipe_`);
    const outputStream = await this.pipe_.getOutputStream();
    console.log(
        '[PeerConnectionSource] Received callee peer connection stream.',
        outputStream);
    this.videoMirrorHelper_.setStream(outputStream);
    return outputStream;
  }

  /** @override */
  destroy() {
    this.videoMirrorHelper_.destroy();
    if (this.pipe_) this.pipe_.destroy();
    this.originalStreamSource_.destroy();
  }
}

/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

'use strict';

/* global createProcessedMediaStreamTrack */ // defined in main.js

/**
 * Wrapper around createProcessedMediaStreamTrack to apply transform to a
 * MediaStream.
 * @param {!MediaStream} sourceStream the video stream to be transformed. The
 *     first video track will be used.
 * @param {!FrameTransformFn} transform the transform to apply to the
 *     sourceStream.
 * @param {!AbortSignal} signal can be used to stop processing
 * @return {!MediaStream} holds a single video track of the transformed video
 *     frames
 */
function createProcessedMediaStream(sourceStream, transform, signal) {
  // For this sample, we're only dealing with video tracks.
  /** @type {!MediaStreamTrack} */
  const sourceTrack = sourceStream.getVideoTracks()[0];

  const processedTrack =
      createProcessedMediaStreamTrack(sourceTrack, transform, signal);

  // Create a new MediaStream to hold our processed track.
  const processedStream = new MediaStream();
  processedStream.addTrack(processedTrack);

  return processedStream;
}

/**
 * Interface implemented by all video sources the user can select. A common
 * interface allows the user to choose a source independently of the transform
 * and sink.
 * @interface
 */
class MediaStreamSource { // eslint-disable-line no-unused-vars
  /**
   * Sets the path to this object from the debug global var.
   * @param {string} path
   */
  setDebugPath(path) {}
  /**
   * Indicates if the source video should be mirrored/displayed on the page. If
   * false (the default), any element producing frames will not be a child of
   * the document.
   * @param {boolean} visible whether to add the raw source video to the page
   */
  setVisibility(visible) {}
  /**
   * Initializes and returns the MediaStream for this source.
   * @return {!Promise<!MediaStream>}
   */
  async getMediaStream() {}
  /** Frees any resources used by this object. */
  destroy() {}
}

/**
 * Interface implemented by all video transforms that the user can select. A
 * common interface allows the user to choose a transform independently of the
 * source and sink.
 * @interface
 */
class FrameTransform { // eslint-disable-line no-unused-vars
  /** Initializes state that is reused across frames. */
  async init() {}
  /**
   * Applies the transform to frame. Queues the output frame (if any) using the
   * controller.
   * @param {!VideoFrame} frame the input frame
   * @param {!TransformStreamDefaultController<!VideoFrame>} controller
   */
  async transform(frame, controller) {}
  /** Frees any resources used by this object. */
  destroy() {}
}

/**
 * Interface implemented by all video sinks that the user can select. A common
 * interface allows the user to choose a sink independently of the source and
 * transform.
 * @interface
 */
class MediaStreamSink { // eslint-disable-line no-unused-vars
  /**
   * @param {!MediaStream} stream
   */
  async setMediaStream(stream) {}
  /** Frees any resources used by this object. */
  destroy() {}
}

/**
 * Assembles a MediaStreamSource, FrameTransform, and MediaStreamSink together.
 */
class Pipeline { // eslint-disable-line no-unused-vars
  constructor() {
    /** @private {?MediaStreamSource} set by updateSource*/
    this.source_ = null;
    /** @private {?FrameTransform} set by updateTransform */
    this.frameTransform_ = null;
    /** @private {?MediaStreamSink} set by updateSink */
    this.sink_ = null;
    /** @private {!AbortController} may used to stop all processing */
    this.abortController_ = new AbortController();
    /**
     * @private {?MediaStream} set in maybeStartPipeline_ after all of source_,
     *     frameTransform_, and sink_ are set
     */
    this.processedStream_ = null;
  }

  /** @return {?MediaStreamSource} */
  getSource() {
    return this.source_;
  }

  /**
   * Sets a new source for the pipeline.
   * @param {!MediaStreamSource} mediaStreamSource
   */
  async updateSource(mediaStreamSource) {
    if (this.source_) {
      this.abortController_.abort();
      this.abortController_ = new AbortController();
      this.source_.destroy();
      this.processedStream_ = null;
    }
    this.source_ = mediaStreamSource;
    this.source_.setDebugPath('debug.pipeline.source_');
    console.log(
        '[Pipeline] Updated source.',
        'debug.pipeline.source_ = ', this.source_);
    await this.maybeStartPipeline_();
  }

  /** @private */
  async maybeStartPipeline_() {
    if (this.processedStream_ || !this.source_ || !this.frameTransform_ ||
        !this.sink_) {
      return;
    }
    const sourceStream = await this.source_.getMediaStream();
    await this.frameTransform_.init();
    try {
      this.processedStream_ = createProcessedMediaStream(
          sourceStream, async (frame, controller) => {
            if (this.frameTransform_) {
              await this.frameTransform_.transform(frame, controller);
            }
          }, this.abortController_.signal);
    } catch (e) {
      this.destroy();
      return;
    }
    await this.sink_.setMediaStream(this.processedStream_);
    console.log(
        '[Pipeline] Pipeline started.',
        'debug.pipeline.abortController_ =', this.abortController_);
  }

  /**
   * Sets a new transform for the pipeline.
   * @param {!FrameTransform} frameTransform
   */
  async updateTransform(frameTransform) {
    if (this.frameTransform_) this.frameTransform_.destroy();
    this.frameTransform_ = frameTransform;
    console.log(
        '[Pipeline] Updated frame transform.',
        'debug.pipeline.frameTransform_ = ', this.frameTransform_);
    if (this.processedStream_) {
      await this.frameTransform_.init();
    } else {
      await this.maybeStartPipeline_();
    }
  }

  /**
   * Sets a new sink for the pipeline.
   * @param {!MediaStreamSink} mediaStreamSink
   */
  async updateSink(mediaStreamSink) {
    if (this.sink_) this.sink_.destroy();
    this.sink_ = mediaStreamSink;
    console.log(
        '[Pipeline] Updated sink.', 'debug.pipeline.sink_ = ', this.sink_);
    if (this.processedStream_) {
      await this.sink_.setMediaStream(this.processedStream_);
    } else {
      await this.maybeStartPipeline_();
    }
  }

  /** Frees any resources used by this object. */
  destroy() {
    console.log('[Pipeline] Destroying Pipeline');
    this.abortController_.abort();
    if (this.source_) this.source_.destroy();
    if (this.frameTransform_) this.frameTransform_.destroy();
    if (this.sink_) this.sink_.destroy();
  }
}

/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

'use strict';

/**
 * Does nothing.
 * @implements {FrameTransform} in pipeline.js
 */
class NullTransform { // eslint-disable-line no-unused-vars
  /** @override */
  async init() {}
  /** @override */
  async transform(frame, controller) {
    controller.enqueue(frame);
  }
  /** @override */
  destroy() {}
}

/**
 * Drops frames at random.
 * @implements {FrameTransform} in pipeline.js
 */
class DropTransform { // eslint-disable-line no-unused-vars
  /** @override */
  async init() {}
  /** @override */
  async transform(frame, controller) {
    if (Math.random() < 0.5) {
      controller.enqueue(frame);
    } else {
      frame.close();
    }
  }
  /** @override */
  destroy() {}
}

/**
 * Delays all frames by 100ms.
 * @implements {FrameTransform} in pipeline.js
 */
class DelayTransform { // eslint-disable-line no-unused-vars
  /** @override */
  async init() {}
  /** @override */
  async transform(frame, controller) {
    await new Promise(resolve => setTimeout(resolve, 100));
    controller.enqueue(frame);
  }
  /** @override */
  destroy() {}
}

/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

'use strict';

/**
 * Helper to display a MediaStream in an HTMLVideoElement, based on the
 * visibility setting.
 */
class VideoMirrorHelper { // eslint-disable-line no-unused-vars
  constructor() {
    /** @private {boolean} */
    this.visibility_ = false;
    /** @private {?MediaStream} the stream to display */
    this.stream_ = null;
    /**
     * @private {?HTMLVideoElement} video element mirroring the camera stream.
     *    Set if visibility_ is true and stream_ is set.
     */
    this.video_ = null;
    /** @private {string} */
    this.debugPath_ = '<unknown>';
  }
  /**
   * Sets the path to this object from the debug global var.
   * @param {string} path
   */
  setDebugPath(path) {
    this.debugPath_ = path;
  }
  /**
   * Indicates if the video should be mirrored/displayed on the page.
   * @param {boolean} visible whether to add the video from the source stream to
   *     the page
   */
  setVisibility(visible) {
    this.visibility_ = visible;
    if (this.video_ && !this.visibility_) {
      this.video_.parentNode.removeChild(this.video_);
      this.video_ = null;
    }
    this.maybeAddVideoElement_();
  }

  /**
   * @param {!MediaStream} stream
   */
  setStream(stream) {
    this.stream_ = stream;
    this.maybeAddVideoElement_();
  }

  /** @private */
  maybeAddVideoElement_() {
    if (!this.video_ && this.visibility_ && this.stream_) {
      this.video_ =
        /** @type {!HTMLVideoElement} */ (document.createElement('video'));
      console.log(
          '[VideoMirrorHelper] Adding source video mirror.',
          `${this.debugPath_}.video_ =`, this.video_);
      this.video_.classList.add('video', 'sourceVideo');
      this.video_.srcObject = this.stream_;
      const outputVideoContainer =
          document.getElementById('outputVideoContainer');
      outputVideoContainer.parentNode.insertBefore(
          this.video_, outputVideoContainer);
      this.video_.play();
    }
  }

  /** Frees any resources used by this object. */
  destroy() {
    if (this.video_) {
      this.video_.pause();
      this.video_.srcObject = null;
      this.video_.parentNode.removeChild(this.video_);
    }
  }
}

/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

'use strict';

/**
 * Displays the output stream in a video element.
 * @implements {MediaStreamSink} in pipeline.js
 */
class VideoSink { // eslint-disable-line no-unused-vars
  constructor() {
    /**
     * @private {?HTMLVideoElement} output video element
     */
    this.video_ = null;
    /** @private {string} */
    this.debugPath_ = 'debug.pipeline.sink_';
  }
  /**
   * Sets the path to this object from the debug global var.
   * @param {string} path
   */
  setDebugPath(path) {
    this.debugPath_ = path;
  }
  /** @override */
  async setMediaStream(stream) {
    console.log('[VideoSink] Setting sink stream.', stream);
    if (!this.video_) {
      this.video_ =
        /** @type {!HTMLVideoElement} */ (document.createElement('video'));
      this.video_.classList.add('video', 'sinkVideo');
      document.getElementById('outputVideoContainer').appendChild(this.video_);
      console.log(
          '[VideoSink] Added video element to page.',
          `${this.debugPath_}.video_ =`, this.video_);
    }
    this.video_.srcObject = stream;
    this.video_.play();
  }
  /** @override */
  destroy() {
    if (this.video_) {
      console.log('[VideoSink] Stopping sink video');
      this.video_.pause();
      this.video_.srcObject = null;
      this.video_.parentNode.removeChild(this.video_);
    }
  }
}

/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

'use strict';

/**
 * Decodes and plays a video.
 * @implements {MediaStreamSource} in pipeline.js
 */
class VideoSource { // eslint-disable-line no-unused-vars
  constructor() {
    /** @private {boolean} */
    this.visibility_ = false;
    /** @private {?HTMLVideoElement} video element providing the MediaStream */
    this.video_ = null;
    /**
     * @private {?Promise<!MediaStream>} a Promise that resolves to the
     *     MediaStream from captureStream. Set iff video_ is set.
     */
    this.stream_ = null;
    /** @private {string} */
    this.debugPath_ = '<unknown>';
  }
  /** @override */
  setDebugPath(path) {
    this.debugPath_ = path;
  }
  /** @override */
  setVisibility(visible) {
    this.visibility_ = visible;
    if (this.video_) {
      this.updateVideoVisibility();
    }
  }
  /** @private */
  updateVideoVisibility() {
    if (this.video_.parentNode && !this.visibility_) {
      if (!this.video_.paused) {
        // Video playback is automatically paused when the element is removed
        // from the DOM. That is not the behavior we want.
        this.video_.onpause = async () => {
          this.video_.onpause = null;
          await this.video_.play();
        };
      }
      this.video_.parentNode.removeChild(this.video_);
    } else if (!this.video_.parentNode && this.visibility_) {
      console.log(
          '[VideoSource] Adding source video element to page.',
          `${this.debugPath_}.video_ =`, this.video_);
      const outputVideoContainer =
          document.getElementById('outputVideoContainer');
      outputVideoContainer.parentNode.insertBefore(
          this.video_, outputVideoContainer);
    }
  }
  /** @override */
  async getMediaStream() {
    if (this.stream_) return this.stream_;

    console.log('[VideoSource] Loading video');

    this.video_ =
      /** @type {!HTMLVideoElement} */ (document.createElement('video'));
    this.video_.classList.add('video', 'sourceVideo');
    this.video_.controls = true;
    this.video_.loop = true;
    this.video_.muted = true;
    // All browsers that support insertable streams also support WebM/VP8.
    this.video_.src = 'road_trip_640_480.mp4';
    this.video_.load();
    this.video_.play();
    this.updateVideoVisibility();
    this.stream_ = new Promise((resolve, reject) => {
      this.video_.oncanplay = () => {
        if (!resolve || !reject) return;
        console.log('[VideoSource] Obtaining video capture stream');
        if (this.video_.captureStream) {
          resolve(this.video_.captureStream());
        } else if (this.video_.mozCaptureStream) {
          resolve(this.video_.mozCaptureStream());
        } else {
          const e = new Error('Stream capture is not supported');
          console.error(e);
          reject(e);
        }
        resolve = null;
        reject = null;
      };
    });
    await this.stream_;
    console.log(
        '[VideoSource] Received source video stream.',
        `${this.debugPath_}.stream_ =`, this.stream_);
    return this.stream_;
  }
  /** @override */
  destroy() {
    if (this.video_) {
      console.log('[VideoSource] Stopping source video');
      this.video_.pause();
      if (this.video_.parentNode) {
        this.video_.parentNode.removeChild(this.video_);
      }
    }
  }
}

/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

'use strict';

/**
 * Encodes and decodes frames using the WebCodec API.
 * @implements {FrameTransform} in pipeline.js
 */
class WebCodecTransform { // eslint-disable-line no-unused-vars
  constructor() {
    // Encoder and decoder are initialized in init()
    this.decoder_ = null;
    this.encoder_ = null;
    this.controller_ = null;
  }
  /** @override */
  async init() {
    console.log('[WebCodecTransform] Initializing encoder and decoder');
    this.decoder_ = new VideoDecoder({
      output: frame => this.handleDecodedFrame(frame),
      error: this.error
    });
    this.encoder_ = new VideoEncoder({
      output: frame => this.handleEncodedFrame(frame),
      error: this.error
    });
    this.encoder_.configure({codec: 'vp8', width: 640, height: 480});
    this.decoder_.configure({codec: 'vp8', width: 640, height: 480});
  }

  /** @override */
  async transform(frame, controller) {
    if (!this.encoder_) {
      frame.close();
      return;
    }
    this.controller_ = controller;
    this.encoder_.encode(frame);
  }

  /** @override */
  destroy() {}

  /* Helper functions */
  handleEncodedFrame(encodedFrame) {
    this.decoder_.decode(encodedFrame);
  }

  handleDecodedFrame(videoFrame) {
    if (!this.controller_) {
      videoFrame.close();
      return;
    }
    this.controller_.enqueue(videoFrame);
  }

  error(e) {
    console.log('[WebCodecTransform] Bad stuff happened: ' + e);
  }
}

/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

'use strict';

/**
 * Applies a warp effect using WebGL.
 * @implements {FrameTransform} in pipeline.js
 */
class WebGLTransform { // eslint-disable-line no-unused-vars
  constructor() {
    // All fields are initialized in init()
    /** @private {?OffscreenCanvas} canvas used to create the WebGL context */
    this.canvas_ = null;
    /** @private {?WebGLRenderingContext} */
    this.gl_ = null;
    /** @private {?WebGLUniformLocation} location of inSampler */
    this.sampler_ = null;
    /** @private {?WebGLProgram} */
    this.program_ = null;
    /** @private {?WebGLTexture} input texture */
    this.texture_ = null;
    /**
     * @private {boolean} If false, pass VideoFrame directly to
     * WebGLRenderingContext.texImage2D and create VideoFrame directly from
     * this.canvas_. If either of these operations fail (it's not supported in
     * Chrome <90 and broken in Chrome 90: https://crbug.com/1184128), we set
     * this field to true; in that case we create an ImageBitmap from the
     * VideoFrame and pass the ImageBitmap to texImage2D on the input side and
     * create the VideoFrame using an ImageBitmap of the canvas on the output
     * side.
     */
    this.use_image_bitmap_ = false;
    /** @private {string} */
    this.debugPath_ = 'debug.pipeline.frameTransform_';
  }
  /** @override */
  async init() {
    console.log('[WebGLTransform] Initializing WebGL.');
    this.canvas_ = new OffscreenCanvas(1, 1);
    const gl = /** @type {?WebGLRenderingContext} */ (
      this.canvas_.getContext('webgl'));
    if (!gl) {
      alert(
          'Failed to create WebGL context. Check that WebGL is supported ' +
          'by your browser and hardware.');
      return;
    }
    this.gl_ = gl;
    const vertexShader = this.loadShader_(gl.VERTEX_SHADER, `
      precision mediump float;
      attribute vec3 g_Position;
      attribute vec2 g_TexCoord;
      varying vec2 texCoord;
      void main() {
        gl_Position = vec4(g_Position, 1.0);
        texCoord = g_TexCoord;
      }`);
    const fragmentShader = this.loadShader_(gl.FRAGMENT_SHADER, `
      precision mediump float;
      varying vec2 texCoord;
      uniform sampler2D inSampler;
      void main(void) {
        float boundary = distance(texCoord, vec2(0.5)) - 0.2;
        if (boundary < 0.0) {
          gl_FragColor = texture2D(inSampler, texCoord);
        } else {
          // Rotate the position
          float angle = 2.0 * boundary;
          vec2 rotation = vec2(sin(angle), cos(angle));
          vec2 fromCenter = texCoord - vec2(0.5);
          vec2 rotatedPosition = vec2(
            fromCenter.x * rotation.y + fromCenter.y * rotation.x,
            fromCenter.y * rotation.y - fromCenter.x * rotation.x) + vec2(0.5);
          gl_FragColor = texture2D(inSampler, rotatedPosition);
        }
      }`);
    if (!vertexShader || !fragmentShader) return;
    // Create the program object
    const programObject = gl.createProgram();
    gl.attachShader(programObject, vertexShader);
    gl.attachShader(programObject, fragmentShader);
    // Link the program
    gl.linkProgram(programObject);
    // Check the link status
    const linked = gl.getProgramParameter(programObject, gl.LINK_STATUS);
    if (!linked) {
      const infoLog = gl.getProgramInfoLog(programObject);
      gl.deleteProgram(programObject);
      throw new Error(`Error linking program:\n${infoLog}`);
    }
    gl.deleteShader(vertexShader);
    gl.deleteShader(fragmentShader);
    this.sampler_ = gl.getUniformLocation(programObject, 'inSampler');
    this.program_ = programObject;
    // Bind attributes
    const vertices = [1.0, -1.0, -1.0, -1.0, 1.0, 1.0, -1.0, 1.0];
    // Pass-through.
    const txtcoords = [1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 1.0];
    // Mirror horizonally.
    // const txtcoords = [0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0];
    this.attributeSetFloats_('g_Position', 2, vertices);
    this.attributeSetFloats_('g_TexCoord', 2, txtcoords);
    // Initialize input texture
    this.texture_ = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, this.texture_);
    const pixel = new Uint8Array([0, 0, 255, 255]); // opaque blue
    gl.texImage2D(
        gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, pixel);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    console.log(
        '[WebGLTransform] WebGL initialized.', `${this.debugPath_}.canvas_ =`,
        this.canvas_, `${this.debugPath_}.gl_ =`, this.gl_);
  }

  /**
   * Creates and compiles a WebGLShader from the provided source code.
   * @param {number} type either VERTEX_SHADER or FRAGMENT_SHADER
   * @param {string} shaderSrc
   * @return {!WebGLShader}
   * @private
   */
  loadShader_(type, shaderSrc) {
    const gl = this.gl_;
    const shader = gl.createShader(type);
    // Load the shader source
    gl.shaderSource(shader, shaderSrc);
    // Compile the shader
    gl.compileShader(shader);
    // Check the compile status
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
      const infoLog = gl.getShaderInfoLog(shader);
      gl.deleteShader(shader);
      throw new Error(`Error compiling shader:\n${infoLog}`);
    }
    return shader;
  }

  /**
   * Sets a floating point shader attribute to the values in arr.
   * @param {string} attrName the name of the shader attribute to set
   * @param {number} vsize the number of components of the shader attribute's
   *   type
   * @param {!Array<number>} arr the values to set
   * @private
   */
  attributeSetFloats_(attrName, vsize, arr) {
    const gl = this.gl_;
    gl.bindBuffer(gl.ARRAY_BUFFER, gl.createBuffer());
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(arr), gl.STATIC_DRAW);
    const attr = gl.getAttribLocation(this.program_, attrName);
    gl.enableVertexAttribArray(attr);
    gl.vertexAttribPointer(attr, vsize, gl.FLOAT, false, 0, 0);
  }

  /** @override */
  async transform(frame, controller) {
    const gl = this.gl_;
    if (!gl || !this.canvas_) {
      frame.close();
      return;
    }
    const width = frame.displayWidth;
    const height = frame.displayHeight;
    if (this.canvas_.width !== width || this.canvas_.height !== height) {
      this.canvas_.width = width;
      this.canvas_.height = height;
      gl.viewport(0, 0, width, height);
    }
    const timestamp = frame.timestamp;
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, this.texture_);
    gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);
    if (!this.use_image_bitmap_) {
      try {
        // Supported for Chrome 90+.
        gl.texImage2D(
            gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, frame);
      } catch (e) {
        // This should only happen on Chrome <90.
        console.log(
            '[WebGLTransform] Failed to upload VideoFrame directly. Falling ' +
                'back to ImageBitmap.',
            e);
        this.use_image_bitmap_ = true;
      }
    }
    if (this.use_image_bitmap_) {
      // Supported for Chrome <92.
      const inputBitmap =
            await frame.createImageBitmap({imageOrientation: 'flipY'});
      gl.texImage2D(
          gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, inputBitmap);
      inputBitmap.close();
    }
    frame.close();
    gl.useProgram(this.program_);
    gl.uniform1i(this.sampler_, 0);
    gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
    gl.bindTexture(gl.TEXTURE_2D, null);
    if (!this.use_image_bitmap_) {
      try {
        // alpha: 'discard' is needed in order to send frames to a PeerConnection.
        controller.enqueue(new VideoFrame(this.canvas_, {timestamp, alpha: 'discard'}));
      } catch (e) {
        // This should only happen on Chrome <91.
        console.log(
            '[WebGLTransform] Failed to create VideoFrame from ' +
                'OffscreenCanvas directly. Falling back to ImageBitmap.',
            e);
        this.use_image_bitmap_ = true;
      }
    }
    if (this.use_image_bitmap_) {
      const outputBitmap = await createImageBitmap(this.canvas_);
      const outputFrame = new VideoFrame(outputBitmap, {timestamp});
      outputBitmap.close();
      controller.enqueue(outputFrame);
    }
  }

  /** @override */
  destroy() {
    if (this.gl_) {
      console.log('[WebGLTransform] Forcing WebGL context to be lost.');
      /** @type {!WEBGL_lose_context} */ (
        this.gl_.getExtension('WEBGL_lose_context'))
          .loseContext();
    }
  }
}
