// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Set true if we should use wasm for raw preview image extraction (PIEX),
 * by the fileManagerPrivate.isPiexLoaderEnabled() code below.
 * @type {boolean}
 */
let useWasm = false;

/**
 * Call FMP.isPiexLoaderEnabled() to get the piex feature flag, and use it
 * to select nacl or wasm for PIEX work.
 */
if (chrome && chrome.fileManagerPrivate) {
  chrome.fileManagerPrivate.isPiexLoaderEnabled((piex_nacl_enabled) => {
    useWasm = !piex_nacl_enabled;
    console.log('[PiexLoader] wasm mode ' + useWasm);
  });
}

/**
 * Declares the piex-wasm Module interface. The Module has many interfaces
 * but only declare the parts required for PIEX work.
 * @typedef {{
 *   calledRun: boolean,
 *   onAbort: function((!Error|string)):undefined,
 *   HEAP8: !Uint8Array,
 *   _malloc: function(number):number,
 *   _free: function(number):undefined,
 *   image: function(number, number):PiexWasmImageResult
 * }}
 */
var PiexWasmModule;

/**
 * |window| var Module defined in page <script src='piex/piex.js.wasm'>.
 * @type {PiexWasmModule}
 */
var Module = window['Module'] || {};

/**
 * Set true only if the wasm Module.onAbort() handler is called.
 * @type {boolean}
 */
let wasmFailed = false;

/**
 * Installs an (Emscripten) wasm Module.onAbort handler, that records that
 * the Module has failed and re-throws the error.
 * @throws {!Error|string}
 */
Module.onAbort = (error) => {
  wasmFailed = true;
  throw error;
};

/**
 * Module failure recovery: if wasmFailed is set via onAbort due to OOM in
 * the C++ for example, or the Module failed to load or call run, then the
 * wasm Module is in a broken, non-functional state.
 *
 * Re-loading the page is the only reliable way to attempt to recover from
 * broken Module state.
 */
function wasmModuleFailed() {
  if (wasmFailed || !Module.calledRun) {
    console.error('[PiexLoader] wasmModuleFailed');
    setTimeout(chrome.runtime.reload, 0);
    return true;
  }
}

/**
 * @typedef {{
 *   fulfill: function(PiexLoaderResponse):undefined,
 *   reject: function(string):undefined}
 * }}
 */
var PiexRequestCallbacks;

/**
 * @param {{id:number, thumbnail:!ArrayBuffer, orientation:number,
 *          colorSpace: ColorSpace, ifd:?string}}
 *     data Data directly returned from NaCl module.
 * @constructor
 * @struct
 */
function PiexLoaderResponse(data) {
  /**
   * @public {number}
   * @const
   */
  this.id = data.id;

  /**
   * @public {!ArrayBuffer}
   * @const
   */
  this.thumbnail = data.thumbnail;

  /**
   * @public {!ImageOrientation}
   * @const
   */
  this.orientation =
      ImageOrientation.fromExifOrientation(data.orientation);

  /**
   * @public {ColorSpace}
   * @const
   */
  this.colorSpace = data.colorSpace;

  /**
   * JSON encoded RAW image photographic details (Piex Wasm module only).
   * @public {?string}
   * @const
   */
  this.ifd = data.ifd || null;
}

/**
 * Creates a PiexLoader for loading RAW files using a Piex NaCl module.
 *
 * All of the arguments are optional and used for tests only. If not passed,
 * then default implementations and values will be used.
 *
 * @param {function()=} opt_createModule Creates a NaCl module.
 * @param {function(!Element)=} opt_destroyModule Destroys a NaCl module.
 * @param {number=} opt_idleTimeout Idle timeout to destroy NaCl module.
 * @constructor
 * @struct
 */
function PiexLoader(opt_createModule, opt_destroyModule, opt_idleTimeout) {
  /**
   * @private {function():!HTMLEmbedElement}
   */
  this.createModule_ = opt_createModule || this.defaultCreateModule_.bind(this);

  /**
   * @private {function():!Element}
   */
  this.destroyModule_ =
      opt_destroyModule || this.defaultDestroyModule_.bind(this);

  this.idleTimeoutMs_ = opt_idleTimeout !== undefined ?
      opt_idleTimeout :
      PiexLoader.DEFAULT_IDLE_TIMEOUT_MS;

  /**
   * @private {HTMLEmbedElement}
   */
  this.naclModule_ = null;

  /**
   * @private {Element}
   */
  this.containerElement_ = null;

  /**
   * @private {number}
   */
  this.unloadTimer_ = 0;

  /**
   * @private {Promise<boolean>}
   */
  this.naclPromise_ = null;

  /**
   * @private {?function(boolean)}
   */
  this.naclPromiseFulfill_ = null;

  /**
   * @private {?function(string=)}
   */
  this.naclPromiseReject_ = null;

  /**
   * @private {!Object<number, ?PiexRequestCallbacks>}
   * @const
   */
  this.requests_ = {};

  /**
   * @private {number}
   */
  this.requestIdCount_ = 0;

  // Bound function so the listeners can be unregistered.
  this.onNaclLoadBound_ = this.onNaclLoad_.bind(this);
  this.onNaclMessageBound_ = this.onNaclMessage_.bind(this);
  this.onNaclErrorBound_ = this.onNaclError_.bind(this);
  this.onNaclCrashBound_ = this.onNaclCrash_.bind(this);
}

/**
 * Idling time before the NaCl module is unloaded. This lets the image loader
 * extension close when inactive.
 *
 * @const {number}
 */
PiexLoader.DEFAULT_IDLE_TIMEOUT_MS = 3000;  // 3 seconds.

/**
 * Creates a NaCl module element.
 *
 * Do not call directly. Use this.loadModule_ instead to support
 * tests.
 *
 * @return {!HTMLEmbedElement}
 * @private
 */
PiexLoader.prototype.defaultCreateModule_ = function() {
  var embed =
      assertInstanceof(document.createElement('embed'), HTMLEmbedElement);
  embed.setAttribute('type', 'application/x-pnacl');
  // The extension nmf is not allowed to load. We uses .nmf.js instead.
  embed.setAttribute('src', '/piex/piex.nmf.txt');
  embed.width = '0';
  embed.height = '0';
  return embed;
};

PiexLoader.prototype.defaultDestroyModule_ = function(module) {
  // The module is destroyed by removing it from DOM in loadNaclModule_().
};

/**
 * @return {!Promise<boolean>}
 * @private
 */
PiexLoader.prototype.loadNaclModule_ = function() {
  if (this.naclPromise_) {
    return this.naclPromise_;
  }

  this.naclPromise_ =
      new Promise(function(fulfill) {
        const useNacl = !useWasm;
        fulfill(useNacl);
      })
          .then(function(enabled) {
            if (!enabled) {
              return false;
            }
            return new Promise(function(fulfill, reject) {
              this.naclPromiseFulfill_ = fulfill;
              this.naclPromiseReject_ = reject;
              this.naclModule_ = this.createModule_();

              // The <EMBED> element is wrapped inside a <DIV>, which has both a
              // 'load' and a 'message' event listener attached.  This wrapping
              // method is used instead of attaching the event listeners
              // directly to the <EMBED> element to ensure that the listeners
              // are active before the NaCl module 'load' event fires.
              var listenerContainer = assertInstanceof(
                  document.createElement('div'), HTMLDivElement);
              listenerContainer.appendChild(this.naclModule_);
              listenerContainer.addEventListener(
                  'load', this.onNaclLoadBound_, true);
              listenerContainer.addEventListener(
                  'message', this.onNaclMessageBound_, true);
              listenerContainer.addEventListener(
                  'error', this.onNaclErrorBound_, true);
              listenerContainer.addEventListener(
                  'crash', this.onNaclCrashBound_, true);
              listenerContainer.style.height = '0px';
              this.containerElement_ = listenerContainer;
              document.body.appendChild(listenerContainer);

              // Force a relayout. Workaround for load event not being called on
              // <embed> for a NaCl module. crbug.com/699930
              /** @suppress {suspiciousCode} */ this.naclModule_.offsetTop;
            }.bind(this));
          }.bind(this))
          .catch(function(error) {
            console.error(error);
            return false;
          });

  return this.naclPromise_;
};

/**
 * @private
 */
PiexLoader.prototype.unloadNaclModule_ = function() {
  this.containerElement_.removeEventListener('load', this.onNaclLoadBound_);
  this.containerElement_.removeEventListener(
      'message', this.onNaclMessageBound_);
  this.containerElement_.removeEventListener('error', this.onNaclErrorBound_);
  this.containerElement_.removeEventListener('crash', this.onNaclCrashBound_);
  this.containerElement_.parentNode.removeChild(this.containerElement_);
  this.containerElement_ = null;

  this.destroyModule_();
  this.naclModule_ = null;
  this.naclPromise_ = null;
  this.naclPromiseFulfill_ = null;
  this.naclPromiseReject_ = null;
};

/**
 * @param {Event} event
 * @private
 */
PiexLoader.prototype.onNaclLoad_ = function(event) {
  console.assert(this.naclPromiseFulfill_);
  this.naclPromiseFulfill_(true);
};

/**
 * @param {Event} listener_event
 * @private
 */
PiexLoader.prototype.onNaclMessage_ = function(listener_event) {
  let event = /** @type{MessageEvent} */ (listener_event);
  var id = event.data.id;
  if (!event.data.error) {
    var response = new PiexLoaderResponse(event.data);
    console.assert(this.requests_[id]);
    this.requests_[id].fulfill(response);
  } else {
    console.assert(this.requests_[id]);
    this.requests_[id].reject(event.data.error);
  }
  delete this.requests_[id];
  if (Object.keys(this.requests_).length === 0) {
    this.scheduleUnloadOnIdle_();
  }
};

/**
 * @param {Event} event
 * @private
 */
PiexLoader.prototype.onNaclError_ = function(event) {
  console.assert(this.naclPromiseReject_);
  this.naclPromiseReject_(this.naclModule_['lastError']);
};

/**
 * @param {Event} event
 * @private
 */
PiexLoader.prototype.onNaclCrash_ = function(event) {
  console.assert(this.naclPromiseReject_);
  this.naclPromiseReject_('PiexLoader crashed.');
};

/**
 * Schedules unloading the NaCl module after IDLE_TIMEOUT_MS passes.
 * @private
 */
PiexLoader.prototype.scheduleUnloadOnIdle_ = function() {
  if (this.unloadTimer_) {
    clearTimeout(this.unloadTimer_);
  }
  this.unloadTimer_ =
      setTimeout(this.onIdleTimeout_.bind(this), this.idleTimeoutMs_);
};

/**
 * @private
 */
PiexLoader.prototype.onIdleTimeout_ = function() {
  this.unloadNaclModule_();
};

/**
 * Simulates time passed required to fire the closure enqueued with setTimeout.
 *
 * Note, that if there is no active timer set with setTimeout earlier, then
 * nothing will happen.
 *
 * This method is used to avoid waiting for DEFAULT_IDLE_TIMEOUT_MS in tests.
 * Also, it allows to avoid flakyness by effectively removing any dependency
 * on execution speed of the test (tests set the timeout to a very large value
 * and only rely on this method to simulate passed time).
 */
PiexLoader.prototype.simulateIdleTimeoutPassedForTests = function() {
  if (this.unloadTimer_) {
    clearTimeout(this.unloadTimer_);
    this.onIdleTimeout_();
  }
};

/**
 * Resolves the file entry associated with DOM filesystem |url| and returns
 * the file content in an ArrayBuffer.
 * @param {string} url - DOM filesystem URL of the file.
 * @returns {!Promise<!ArrayBuffer>}
 */
function readFromFileSystem(url) {
  return new Promise((resolve, reject) => {
    /**
     * Reject the Promise on fileEntry URL resolve or file read failures.
     */
    function failure(error) {
      reject(new Error('Reading file system: ' + error));
    }

    /**
     * Returns true if the fileEntry file size is within sensible limits.
     * @param {number} size - file size.
     * @return {boolean}
     */
    function valid(size) {
      return size > 0 && size < Math.pow(2, 30);
    }

    /**
     * Reads the fileEntry's content into an ArrayBuffer: resolve Promise
     * with the ArrayBuffer result or reject the Promise on failure.
     * @param {!Entry} entry - file system entry of |url|.
     */
    function readEntry(entry) {
      const fileEntry = /** @type {!FileEntry} */ (entry);
      fileEntry.file((file) => {
        if (valid(file.size)) {
          const reader = new FileReader();
          reader.onerror = failure;
          reader.onload = (_) => resolve(reader.result);
          reader.readAsArrayBuffer(file);
        } else {
          failure('invalid file size: ' + file.size);
        }
      }, failure);
    }

    window.webkitResolveLocalFileSystemURL(url, readEntry, failure);
  });
}

/**
 * Piex wasm extacts the preview image metadata from a raw image. The preview
 * image |format| is either 0 (JPEG) or 1 (RGB), and has a |colorSpace| (sRGB
 * or AdobeRGB1998) and a JEITA EXIF image |orientation|.
 *
 * An RGB format preview image has both |width| and |height|, but JPEG format
 * previews have neither (piex wasm C++ does not parse/decode JPEG).
 *
 * The |offset| to, and |length| of, the preview image relative to the source
 * data is indicated by those fields. They are positive > 0. Note: the values
 * are controlled by a third-party and are untrustworthy (Security).
 *
 * @typedef {{
 *  format:number,
 *  colorSpace:ColorSpace,
 *  orientation:number,
 *  width:?number,
 *  height:?number,
 *  offset:number,
 *  length:number
 * }}
 */
var PiexWasmPreviewImageMetadata;

/**
 * The piex wasm Module.image(<raw image source>,...) API returns |error|, or
 * else the source |preview| and/or |thumbnail| image metadata along with the
 * photographic |details| derived from the RAW image EXIF.
 *
 * FilesApp (and related) only use |preview| images. Preview images are JPEG.
 * The |thumbnail| images are small, lower-quality, JPEG or RGB format images
 * and are not currently used in FilesApp.
 *
 * @typedef {{
 *  error:?string,
 *  preview:?PiexWasmPreviewImageMetadata,
 *  thumbnail:?PiexWasmPreviewImageMetadata,
 *  details:?Object
 * }}
 */
var PiexWasmImageResult;

/**
 * Piex wasm raw image preview image extractor.
 */
class ImageBuffer {
  /**
   * @param {!ArrayBuffer} buffer - raw image source data.
   * @param {number} id - caller-defined id.
   */
  constructor(buffer, id) {
    /**
     * @type {number}
     * @const
     * @private
     */
    this.id = id;

    /**
     * @type {!Uint8Array}
     * @const
     * @private
     */
    this.source = new Uint8Array(buffer);

    /**
     * @type {number}
     * @const
     * @private
     */
    this.length = buffer.byteLength;

    /**
     * @type {number}
     * @private
     */
    this.memory = 0;
  }

  /**
   * Calls Module.image() to process |this.source| and return the result.
   *
   * @return {!PiexWasmImageResult}
   * @throws {!Error}
   */
  process() {
    this.memory = Module._malloc(this.length);
    if (!this.memory) {
      throw new Error('Image malloc failed: ' + this.length + ' bytes');
    }

    Module.HEAP8.set(this.source, this.memory);
    const result = Module.image(this.memory, this.length);
    if (result.error) {
      throw new Error(result.error);
    }

    return result;
  }

  /**
   * Returns the preview image data. If no preview image was found, returns
   * an empty preview image.
   *
   * @param {!PiexWasmImageResult} result
   *
   * @throws {!Error} Data access security error.
   *
   * @return {{id:number, thumbnail:!ArrayBuffer, orientation:number,
   *          colorSpace: ColorSpace, ifd:?string}}
   */
  preview(result) {
    const preview = result.preview;
    if (!preview) {
      return {
        thumbnail: new ArrayBuffer(0),
        colorSpace: ColorSpace.SRGB,
        orientation: 1,
        id: this.id,
        ifd: null,
      };
    }

    const offset = preview.offset;
    const length = preview.length;
    if (offset > this.length || (this.length - offset) < length) {
      throw new Error('Preview image access failed');
    }

    const view = new Uint8Array(this.source.buffer, offset, length);
    return {
      thumbnail: new Uint8Array(view).buffer,
      orientation: preview.orientation,
      colorSpace: preview.colorSpace,
      ifd: this.details(result),
      id: this.id,
    };
  }

  /**
   * Returns the RAW image photographic |details| in a JSON-encoded string.
   * Only number and string values are retained, and they are formatted for
   * presentation to the user.
   *
   * @private
   * @param {!PiexWasmImageResult} result
   * @return {?string}
   */
  details(result) {
    const details = result.details;
    if (!details) {
      return null;
    }

    let format = {};
    for (const [key, value] of Object.entries(details)) {
      if (typeof value === 'string') {
        format[key] = value.replace(/\0+$/, '').trim();
      } else if (typeof value === 'number') {
        if (!Number.isInteger(value)) {
          format[key] = Number(value.toFixed(3).replace(/0+$/, ''));
        } else {
          format[key] = value;
        }
      }
    }

    return JSON.stringify(format);
  }

  /**
   * Release resources.
   */
  close() {
    Module._free(this.memory);
  }
}

/**
 * Starts to load RAW image.
 * @param {string} url
 * @return {!Promise<!PiexLoaderResponse>}
 */
PiexLoader.prototype.load = function(url) {
  var requestId = this.requestIdCount_++;

  if (this.unloadTimer_) {
    clearTimeout(this.unloadTimer_);
    this.unloadTimer_ = 0;
  }

  if (useWasm) {
    let imageBuffer;
    return readFromFileSystem(url)
        .then((buffer) => {
          if (wasmModuleFailed() === true) {
            return Promise.reject('piex wasm module failed');
          }
          imageBuffer = new ImageBuffer(buffer, requestId);
          return imageBuffer.process();
        })
        .then((result) => {
          imageBuffer.close();
          return new PiexLoaderResponse(imageBuffer.preview(result));
        })
        .catch((error) => {
          if (wasmModuleFailed() === true) {
            return Promise.reject('piex wasm module failed');
          }
          imageBuffer && imageBuffer.close();
          console.error('[PiexLoader] ' + error);
          return Promise.reject(error);
        });
  }

  // Prevents unloading the NaCl module during handling the promises below.
  this.requests_[requestId] = null;

  return this.loadNaclModule_().then(function(loaded) {
    if (!loaded) {
      return Promise.reject('Piex is not loaded');
    }
    var message = {id: requestId, name: 'loadThumbnail', url: url};
    this.naclModule_.postMessage(message);
    return new Promise(function(fulfill, reject) {
             delete this.requests_[requestId];
             this.requests_[message.id] = {fulfill: fulfill, reject: reject};
           }.bind(this))
        .catch(function(error) {
          delete this.requests_[requestId];
          console.error('PiexLoaderError: ', error);
          return Promise.reject(error);
        });
  }.bind(this));
};
