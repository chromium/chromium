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
 * JFIF APP2 ICC_PROFILE segment containing an AdobeRGB1998 Color Profile.
 * @const {!Uint8Array}
 */
const adobeProfile = new Uint8Array([
  // APP2 ICC_PROFILE\0 segment header.
  0xff, 0xe2, 0x02, 0x40, 0x49, 0x43, 0x43, 0x5f, 0x50, 0x52, 0x4f, 0x46,
  0x49, 0x4c, 0x45, 0x00, 0x01, 0x01,
  // AdobeRGB98 ICC Color Profile data.
  0x00, 0x00, 0x02, 0x30, 0x41, 0x44, 0x42, 0x45, 0x02, 0x10, 0x00, 0x00,
  0x6d, 0x6e, 0x74, 0x72, 0x52, 0x47, 0x42, 0x20, 0x58, 0x59, 0x5a, 0x20,
  0x07, 0xd0, 0x00, 0x08, 0x00, 0x0b, 0x00, 0x13, 0x00, 0x33, 0x00, 0x3b,
  0x61, 0x63, 0x73, 0x70, 0x41, 0x50, 0x50, 0x4c, 0x00, 0x00, 0x00, 0x00,
  0x6e, 0x6f, 0x6e, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf6, 0xd6,
  0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xd3, 0x2d, 0x41, 0x44, 0x42, 0x45,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a,
  0x63, 0x70, 0x72, 0x74, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x32,
  0x64, 0x65, 0x73, 0x63, 0x00, 0x00, 0x01, 0x30, 0x00, 0x00, 0x00, 0x6b,
  0x77, 0x74, 0x70, 0x74, 0x00, 0x00, 0x01, 0x9c, 0x00, 0x00, 0x00, 0x14,
  0x62, 0x6b, 0x70, 0x74, 0x00, 0x00, 0x01, 0xb0, 0x00, 0x00, 0x00, 0x14,
  0x72, 0x54, 0x52, 0x43, 0x00, 0x00, 0x01, 0xc4, 0x00, 0x00, 0x00, 0x0e,
  0x67, 0x54, 0x52, 0x43, 0x00, 0x00, 0x01, 0xd4, 0x00, 0x00, 0x00, 0x0e,
  0x62, 0x54, 0x52, 0x43, 0x00, 0x00, 0x01, 0xe4, 0x00, 0x00, 0x00, 0x0e,
  0x72, 0x58, 0x59, 0x5a, 0x00, 0x00, 0x01, 0xf4, 0x00, 0x00, 0x00, 0x14,
  0x67, 0x58, 0x59, 0x5a, 0x00, 0x00, 0x02, 0x08, 0x00, 0x00, 0x00, 0x14,
  0x62, 0x58, 0x59, 0x5a, 0x00, 0x00, 0x02, 0x1c, 0x00, 0x00, 0x00, 0x14,
  0x74, 0x65, 0x78, 0x74, 0x00, 0x00, 0x00, 0x00, 0x43, 0x6f, 0x70, 0x79,
  0x72, 0x69, 0x67, 0x68, 0x74, 0x20, 0x32, 0x30, 0x30, 0x30, 0x20, 0x41,
  0x64, 0x6f, 0x62, 0x65, 0x20, 0x53, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x73,
  0x20, 0x49, 0x6e, 0x63, 0x6f, 0x72, 0x70, 0x6f, 0x72, 0x61, 0x74, 0x65,
  0x64, 0x00, 0x00, 0x00, 0x64, 0x65, 0x73, 0x63, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x11, 0x41, 0x64, 0x6f, 0x62, 0x65, 0x20, 0x52, 0x47,
  0x42, 0x20, 0x28, 0x31, 0x39, 0x39, 0x38, 0x29, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x58, 0x59, 0x5a, 0x20, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xf3, 0x51, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x16, 0xcc,
  0x58, 0x59, 0x5a, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x63, 0x75, 0x72, 0x76,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x33, 0x00, 0x00,
  0x63, 0x75, 0x72, 0x76, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
  0x02, 0x33, 0x00, 0x00, 0x63, 0x75, 0x72, 0x76, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x02, 0x33, 0x00, 0x00, 0x58, 0x59, 0x5a, 0x20,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9c, 0x18, 0x00, 0x00, 0x4f, 0xa5,
  0x00, 0x00, 0x04, 0xfc, 0x58, 0x59, 0x5a, 0x20, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x34, 0x8d, 0x00, 0x00, 0xa0, 0x2c, 0x00, 0x00, 0x0f, 0x95,
  0x58, 0x59, 0x5a, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x26, 0x31,
  0x00, 0x00, 0x10, 0x2f, 0x00, 0x00, 0xbe, 0x9c,
]);

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
      thumbnail: this.createImageDataArray_(view, preview).buffer,
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
      thumbnail: this.createImageDataArray_(view, thumbnail).buffer,
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
    const pixelDataOffset = 14 + 108;
    const fileSize = pixelDataOffset + rowStride * thumbnail.height;
    const bitmap = new DataView(new ArrayBuffer(fileSize));

    // BITMAPFILEHEADER 14 bytes.
    bitmap.setUint8(0, 'B'.charCodeAt(0));
    bitmap.setUint8(1, 'M'.charCodeAt(0));
    bitmap.setUint32(2, fileSize /* bytes */, true);
    bitmap.setUint32(6, /* Reserved */ 0, true);
    bitmap.setUint32(10, pixelDataOffset, true);

    // DIB BITMAPV4HEADER 108 bytes.
    bitmap.setUint32(14, /* HeaderSize */ 108, true);
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

    bitmap.setUint32(54, /* RedMask */ 0, true);
    bitmap.setUint32(58, /* GreenMask */ 0, true);
    bitmap.setUint32(62, /* BlueMask */ 0, true);
    bitmap.setUint32(66, /* AlphaMask */ 0, true);

    let rx = 0, ry = 0;
    let gx = 0, gy = 0;
    let bx = 0, by = 0;
    let zz = 0, gg = 0;

    if (thumbnail.colorSpace !== 'adobeRgb') {
      bitmap.setUint8(70, 's'.charCodeAt(0));
      bitmap.setUint8(71, 'R'.charCodeAt(0));
      bitmap.setUint8(72, 'G'.charCodeAt(0));
      bitmap.setUint8(73, 'B'.charCodeAt(0));
    } else {
      bitmap.setUint32(70, /* adobeRgb LCS_CALIBRATED_RGB */ 0);
      rx = Math.round(0.6400 * (1 << 30));
      ry = Math.round(0.3300 * (1 << 30));
      gx = Math.round(0.2100 * (1 << 30));
      gy = Math.round(0.7100 * (1 << 30));
      bx = Math.round(0.1500 * (1 << 30));
      by = Math.round(0.0600 * (1 << 30));
      zz = Math.round(1.0000 * (1 << 30));
      gg = Math.round(2.1992187 * (1 << 16));
    }

    // RGB CIEXYZ.
    bitmap.setUint32( 74, /* R CIEXYZ x */ rx, true);
    bitmap.setUint32( 78, /* R CIEXYZ y */ ry, true);
    bitmap.setUint32( 82, /* R CIEXYZ z */ zz, true);
    bitmap.setUint32( 86, /* G CIEXYZ x */ gx, true);
    bitmap.setUint32( 90, /* G CIEXYZ y */ gy, true);
    bitmap.setUint32( 94, /* G CIEXYZ z */ zz, true);
    bitmap.setUint32( 98, /* B CIEXYZ x */ bx, true);
    bitmap.setUint32(102, /* B CIEXYZ y */ by, true);
    bitmap.setUint32(106, /* B CIEXYZ z */ zz, true);

    // RGB gamma.
    bitmap.setUint32(110, /* R Gamma */ gg, true);
    bitmap.setUint32(114, /* G Gamma */ gg, true);
    bitmap.setUint32(118, /* B Gamma */ gg, true);

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
   * Converts a |view| of the "preview image" to Uint8Array data. Embeds an
   * AdobeRGB1998 ICC Color Profile in that data if the preview is JPEG and
   * it has 'adodeRgb' color space.
   *
   * @private
   * @param {!PiexWasmPreviewImageMetadata} preview
   * @param {!Uint8Array} view
   * @return {!Uint8Array}
   */
  createImageDataArray_(view, preview) {
    const jpeg = view.byteLength > 2 && view[0] === 0xff && view[1] === 0xd8;

    if (jpeg && preview.colorSpace === 'adobeRgb') {
      const data = new Uint8Array(view.byteLength + adobeProfile.byteLength);
      data.set(view.subarray(2), 2 + adobeProfile.byteLength);
      data.set(adobeProfile, 2);
      data.set([0xff, 0xd8], 0);
      return data;
    }

    return new Uint8Array(view);
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
