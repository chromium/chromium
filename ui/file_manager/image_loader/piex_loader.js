// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.log('[PiexLoader] loaded');

/**
 * Declares the piex-wasm Module interface. The Module has many interfaces
 * but only declare the parts required for PIEX work.
 *
 * @typedef {{
 *  calledRun: boolean,
 *  onAbort: function((!Error|string)):undefined,
 *  HEAP8: !Uint8Array,
 *  _malloc: function(number):number,
 *  _free: function(number):undefined,
 *  image: function(number, number):!PiexWasmImageResult
 * }}
 */
let PiexWasmModule;

/**
 * Module defined by 'piex.js.wasm' script.
 * @type {!PiexWasmModule}
 */
const PiexModule = /** @type {!PiexWasmModule} */ (globalThis['Module']) || {};

/**
 * Set true if the Module.onAbort() handler is called.
 * @type {boolean}
 */
let piexFailed = false;

/**
 * Installs an (Emscripten) Module.onAbort handler. Record that the Module
 * has failed in piexFailed and re-throw the error.
 *
 * @param {!Error|string} error
 * @throws {!Error|string}
 */
PiexModule.onAbort = (error) => {
  piexFailed = true;
  throw error;
};

/**
 * Module failure recovery: if piexFailed is set via onAbort due to OOM in
 * the C++ for example, or the Module failed to load or call run, then the
 * Module is in a broken, non-functional state.
 *
 * Loading the entire page is the only reliable way to recover from broken
 * Module state. Log the error, and return true to tell caller to initiate
 * failure recovery steps.
 *
 * @return {boolean}
 */
function piexModuleFailed() {
  if (piexFailed || !PiexModule.calledRun) {
    console.error('[PiexLoader] piex wasm module failed');
    return true;
  }
  return false;
}

/**
 * @typedef {{
 *  thumbnail: !ArrayBuffer,
 *  mimeType: (string|undefined),
 *  orientation: number,
 *  colorSpace: string,
 *  ifd: ?string
 * }}
 */
let PiexPreviewImageData;

/**
 * @param {!PiexPreviewImageData} data The extracted preview image data.
 * @constructor
 * @struct
 */
function PiexLoaderResponse(data) {
  /**
   * @public {!ArrayBuffer}
   * @const
   */
  this.thumbnail = data.thumbnail;

  /**
   * @public {string}
   * @const
   */
  this.mimeType = data.mimeType || 'image/jpeg';

  /**
   * JEITA EXIF image orientation being an integer in [1..8].
   * @public {number}
   * @const
   */
  this.orientation = data.orientation;

  /**
   * JEITA EXIF image color space: 'sRgb' or 'adobeRgb'.
   * @public {string}
   * @const
   */
  this.colorSpace = data.colorSpace;

  /**
   * JSON encoded RAW image photographic details.
   * @public {?string}
   * @const
   */
  this.ifd = data.ifd || null;
}

/**
 * Returns the source data.
 *
 * If the source is an ArrayBuffer, return it. Otherwise assume the source is
 * is a File or DOM fileSystemURL and return its content in an ArrayBuffer.
 *
 * @param {!ArrayBuffer|!File|string} source
 * @return {!Promise<!ArrayBuffer>}
 */
function readSourceData(source) {
  if (source instanceof ArrayBuffer) {
    return Promise.resolve(source);
  }

  return new Promise((resolve, reject) => {
    /**
     * Reject the Promise on fileEntry URL resolve or file read failures.
     * @param {!Error|string|!ProgressEvent<!FileReader>|!FileError} error
     */
    function failure(error) {
      reject(new Error('Reading file system: ' + (error.message || error)));
    }

    /**
     * Returns true if the file size is within sensible limits.
     * @param {number} size - file size.
     * @return {boolean}
     */
    function valid(size) {
      return size > 0 && size < Math.pow(2, 30);
    }

    /**
     * Reads the file content to an ArrayBuffer. Resolve the Promise with
     * the ArrayBuffer result, or reject the Promise on failure.
     * @param {!File} file - file to read.
     */
    function readFile(file) {
      if (valid(file.size)) {
        const reader = new FileReader();
        reader.onerror = failure;
        reader.onload = (_) => {
          resolve(reader.result);
        };
        reader.readAsArrayBuffer(file);
      } else {
        failure('invalid file size: ' + file.size);
      }
    }

    /**
     * Resolve the fileEntry's file then read it with readFile, or reject
     * the Promise on failure.
     * @param {!Entry} entry - file system entry.
     */
    function readEntry(entry) {
      const fileEntry = /** @type {!FileEntry} */ (entry);
      fileEntry.file((file) => {
        readFile(file);
      }, failure);
    }

    if (source instanceof File) {
      readFile(/** @type {!File} */ (source));
      return;
    }

    const url = /** @type {string} fileSystemURL */ (source);
    globalThis.webkitResolveLocalFileSystemURL(url, readEntry, failure);
  });
}

/**
 * Piex-wasm extracts the "preview image" from a RAW image. The preview image
 * |format| is either 0 (JPEG), or 1 (RGB), and has a JEITA EXIF |colorSpace|
 * (sRGB or AdobeRGB1998) and a JEITA EXIF image |orientation|.
 *
 * An RGB format preview image has both |width| and |height|, but JPEG format
 * previews have neither (piex-wasm C++ does not parse/decode JPEG).
 *
 * The |offset| to, and |length| of, the preview image relative to the source
 * data is indicated by those fields. They are positive > 0. Note: the values
 * are controlled by a third-party and are untrustworthy (Security).
 *
 * @typedef {{
 *  format:number,
 *  colorSpace:string,
 *  orientation:number,
 *  width:?number,
 *  height:?number,
 *  offset:number,
 *  length:number
 * }}
 */
let PiexWasmPreviewImageMetadata;

/**
 * The piex-wasm Module.image(<RAW image source>,...) API returns |error|, or
 * else the source |preview| and/or |thumbnail| image metadata along with the
 * photographic |details| derived from the RAW image EXIF.
 *
 * The |preview| images are JPEG. The |thumbnail| images are smaller, lower-
 * quality, JPEG or RGB format images.
 *
 * @typedef {{
 *  error:?string,
 *  preview:?PiexWasmPreviewImageMetadata,
 *  thumbnail:?PiexWasmPreviewImageMetadata,
 *  details:?Object
 * }}
 */
let PiexWasmImageResult;

/**
 * Preview Image EXtractor (PIEX).
 */
class ImageBuffer {
  /**
   * @param {!ArrayBuffer} buffer - RAW image source data.
   */
  constructor(buffer) {
    /**
     * @const {!Uint8Array}
     * @private
     */
    this.source = new Uint8Array(buffer);

    /**
     * @const {number}
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
   * @throws {!Error} Memory allocation error.
   * @return {!PiexWasmImageResult}
   */
  process() {
    this.memory = PiexModule._malloc(this.length);
    if (!this.memory) {
      throw new Error('Image malloc failed: ' + this.length + ' bytes');
    }

    PiexModule.HEAP8.set(this.source, this.memory);
    const result = PiexModule.image(this.memory, this.length);
    if (result.error) {
      throw new Error(result.error);
    }

    return result;
  }

  /**
   * Returns the preview image data. If no preview image was found, returns
   * the thumbnail image.
   *
   * @param {!PiexWasmImageResult} result
   * @throws {!Error} Data access security error.
   * @return {!PiexPreviewImageData}
   */
  preview(result) {
    const preview = result.preview;
    if (!preview) {
      return this.thumbnail_(result);
    }

    const offset = preview.offset;
    const length = preview.length;
    if (offset > this.length || (this.length - offset) < length) {
      throw new Error('Preview image access failed');
    }

    const view = new Uint8Array(this.source.buffer, offset, length);
    return {
      thumbnail: new Uint8Array(view).buffer,
      mimeType: 'image/jpeg',
      ifd: this.details_(result, preview.orientation),
      orientation: preview.orientation,
      colorSpace: preview.colorSpace,
    };
  }

  /**
   * Returns the thumbnail image. If no thumbnail image was found, returns
   * an empty thumbnail image.
   *
   * @private
   * @param {!PiexWasmImageResult} result
   * @throws {!Error} Data access security error.
   * @return {!PiexPreviewImageData}
   */
  thumbnail_(result) {
    const thumbnail = result.thumbnail;
    if (!thumbnail) {
      return {
        thumbnail: new ArrayBuffer(0),
        colorSpace: 'sRgb',
        orientation: 1,
        ifd: null,
      };
    }

    if (thumbnail.format) {
      return this.rgb_(result);
    }

    const offset = thumbnail.offset;
    const length = thumbnail.length;
    if (offset > this.length || (this.length - offset) < length) {
      throw new Error('Thumbnail image access failed');
    }

    const view = new Uint8Array(this.source.buffer, offset, length);
    return {
      thumbnail: new Uint8Array(view).buffer,
      mimeType: 'image/jpeg',
      ifd: this.details_(result, thumbnail.orientation),
      orientation: thumbnail.orientation,
      colorSpace: thumbnail.colorSpace,
    };
  }

  /**
   * Returns the RGB thumbnail. If no RGB thumbnail was found, returns
   * an empty thumbnail image.
   *
   * @private
   * @param {!PiexWasmImageResult} result
   * @throws {!Error} Data access security error.
   * @return {!PiexPreviewImageData}
   */
  rgb_(result) {
    const thumbnail = result.thumbnail;
    if (!thumbnail || thumbnail.format !== 1) {
      return {
        thumbnail: new ArrayBuffer(0),
        colorSpace: 'sRgb',
        orientation: 1,
        ifd: null,
      };
    }

    // Expect a width and height.
    if (!thumbnail.width || !thumbnail.height) {
      throw new Error('invalid image width or height');
    }

    const offset = thumbnail.offset;
    const length = thumbnail.length;
    if (offset > this.length || (this.length - offset) < length) {
      throw new Error('Thumbnail image access failed');
    }

    const view = new Uint8Array(this.source.buffer, offset, length);

    // Compute pixel row stride.
    const rowPad = thumbnail.width & 3;
    const rowStride = 3 * thumbnail.width + rowPad;

    // Create bitmap image.
    const pixelDataOffset = 14 + 40;
    const fileSize = pixelDataOffset + rowStride * thumbnail.height;
    const bitmap = new DataView(new ArrayBuffer(fileSize));

    // BITMAPFILEHEADER 14 bytes.
    bitmap.setUint8(0, 'B'.charCodeAt(0));
    bitmap.setUint8(1, 'M'.charCodeAt(0));
    bitmap.setUint32(2, fileSize /* bytes */, true);
    bitmap.setUint32(6, /* Reserved */ 0, true);
    bitmap.setUint32(10, pixelDataOffset, true);

    // DIB BITMAPINFOHEADER 40 bytes.
    bitmap.setUint32(14, /* HeaderSize */ 40, true);
    bitmap.setInt32(18, thumbnail.width, true);
    bitmap.setInt32(22, -thumbnail.height /* top-down DIB */, true);
    bitmap.setInt16(26, /* ColorPlanes */ 1, true);
    bitmap.setInt16(28, /* BitsPerPixel BI_RGB */ 24, true);
    bitmap.setUint32(30, /* Compression: BI_RGB none */ 0, true);
    bitmap.setUint32(34, /* ImageSize: 0 not compressed */ 0, true);
    bitmap.setInt32(38, /* XPixelsPerMeter */ 0, true);
    bitmap.setInt32(42, /* YPixelPerMeter */ 0, true);
    bitmap.setUint32(46, /* TotalPalletColors */ 0, true);
    bitmap.setUint32(50, /* ImportantColors */ 0, true);

    // Write RGB row pixels in top-down DIB order.
    let output = pixelDataOffset;
    for (let i = 0, y = thumbnail.height; y > 0; --y) {
      for (let x = thumbnail.width; x > 0; --x) {
        const R = view[i++];
        const G = view[i++];
        const B = view[i++];
        bitmap.setUint8(output++, B);  // B
        bitmap.setUint8(output++, G);  // G
        bitmap.setUint8(output++, R);  // R
      }

      switch (rowPad) {
        case 3:
          bitmap.setUint8(output++, 0);
        case 2:
          bitmap.setUint8(output++, 0);
        case 1:
          bitmap.setUint8(output++, 0);
      }
    }

    return {
      thumbnail: bitmap.buffer,
      mimeType: 'image/bmp',
      ifd: this.details_(result, thumbnail.orientation),
      orientation: thumbnail.orientation,
      colorSpace: thumbnail.colorSpace,
    };
  }

  /**
   * Returns the RAW image photographic |details| in a JSON-encoded string.
   * Only number and string values are retained, and they are formatted for
   * presentation to the user.
   *
   * @private
   * @param {!PiexWasmImageResult} result
   * @param {number} orientation - image EXIF orientation
   * @return {?string}
   */
  details_(result, orientation) {
    const details = result.details;
    if (!details) {
      return null;
    }

    /** @type {!Object<string|number, number|string>} */
    const format = {};
    /** @type {!Array<!Array<string|number>>} */
    const entries = Object.entries(details);
    for (const [key, value] of entries) {
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

    const usesWidthAsHeight = orientation >= 5;
    if (usesWidthAsHeight) {
      const width = format['width'];
      format['width'] = format['height'];
      format['height'] = width;
    }

    return JSON.stringify(format);
  }

  /**
   * Release resources.
   */
  close() {
    const memory = this.memory;
    if (memory) {
      PiexModule._free(memory);
      this.memory = 0;
    }
  }
}

/**
 * PiexLoader: is a namespace.
 */
const PiexLoader = {};

/**
 * Loads a RAW image. Returns the image metadata and the image thumbnail in a
 * PiexLoaderResponse.
 *
 * piexModuleFailed() returns true if the Module is in an unrecoverable error
 * state. This is rare, but possible, and the only reliable way to recover is
 * to reload the page. Callback |onPiexModuleFailed| is used to indicate that
 * the caller should initiate failure recovery steps.
 *
 * @param {!ArrayBuffer|!File|string} source
 * @param {function()} onPiexModuleFailed
 * @return {!Promise<!PiexLoaderResponse>}
 */
PiexLoader.load = function(source, onPiexModuleFailed) {
  /** @type {?ImageBuffer} */
  let imageBuffer;

  return readSourceData(source)
      .then((buffer) => {
        if (piexModuleFailed()) {
          // Just reject here: handle in the .catch() clause below.
          return Promise.reject('piex wasm module failed');
        }
        imageBuffer = new ImageBuffer(buffer);
        return imageBuffer.process();
      })
      .then((/** !PiexWasmImageResult */ result) => {
        const buffer = /** @type {!ImageBuffer} */ (imageBuffer);
        buffer.close();
        return new PiexLoaderResponse(buffer.preview(result));
      })
      .catch((error) => {
        if (piexModuleFailed()) {
          setTimeout(onPiexModuleFailed, 0);
          return Promise.reject('piex wasm module failed');
        }
        imageBuffer && imageBuffer.close();
        console.error('[PiexLoader] ' + error);
        return Promise.reject(error);
      });
};
