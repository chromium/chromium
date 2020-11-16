// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* eslint-disable no-var */

// clang-format off
// #import * as wrappedImageUtil from './image_util.m.js'; const {ImageUtil} = wrappedImageUtil;
// #import {MetadataItem} from '../../../file_manager/foreground/js/metadata/metadata_item.m.js';
// #import {assert, assertInstanceof} from 'chrome://resources/js/assert.m.js'
// clang-format on

/**
 * A namespace class for image encoding functions. All methods are static.
 * @constructor
 */
/* #export */ function ImageEncoder() {}

/**
 * The value 360 px is enough in the Files app grid view for HiDPI devices.
 * @const {number}
 */
ImageEncoder.MAX_THUMBNAIL_DIMENSION = 360;

/**
 * Tries to create thumbnail if the image width or height longer than the size.
 * @const {number}
 */
ImageEncoder.MIN_IMAGE_DIMENSION_FOR_THUMBNAIL =
    ImageEncoder.MAX_THUMBNAIL_DIMENSION * 4;

/**
 * Metadata encoders.
 * @type {!Object<function(new:ImageEncoder.MetadataEncoder,!MetadataItem)>}
 * @const
 */
ImageEncoder.metadataEncoders = {};

/**
 * Registers metadata encoder.
 * @param {function(new:ImageEncoder.MetadataEncoder,!MetadataItem)} constructor
 *     Constructor of a metadata encoder.
 * @param {string} mimeType Mime type of the metadata encoder.
 */
ImageEncoder.registerMetadataEncoder = function(constructor, mimeType) {
  ImageEncoder.metadataEncoders[mimeType] = constructor;
};

/**
 * Create a metadata encoder.
 *
 * The encoder will own and modify a copy of the original metadata.
 *
 * @param {!MetadataItem} metadata Original metadata.
 * @return {!ImageEncoder.MetadataEncoder} Created metadata encoder.
 */
ImageEncoder.createMetadataEncoder = function(metadata) {
  var constructor =
      ImageEncoder.metadataEncoders[metadata.mediaMimeType || ""] ||
      ImageEncoder.MetadataEncoder;
  return new constructor(metadata);
};

/**
 * Create a metadata encoder object holding a copy of metadata
 * modified according to the properties of the supplied image.
 *
 * @param {!MetadataItem} metadata Original metadata.
 * @param {!HTMLCanvasElement} canvas Canvas to use for metadata.
 * @param {number} thumbnailQuality Encoding quality of a thumbnail.
 * @return {!ImageEncoder.MetadataEncoder} Encoder with encoded metadata.
 *
 * TODO(yawano): rename to a better name, e.g. prepareMetadataEncoder.
 */
ImageEncoder.encodeMetadata = function(metadata, canvas, thumbnailQuality) {
  var encoder = ImageEncoder.createMetadataEncoder(metadata);
  encoder.setImageData(canvas);
  encoder.setThumbnailData(ImageEncoder.createThumbnail(canvas),
      thumbnailQuality);
  return encoder;
};

/**
 * Return a blob with the encoded image with metadata inserted.
 * @param {!HTMLCanvasElement} canvas The canvas with the image to be encoded.
 * @param {!ImageEncoder.MetadataEncoder} metadataEncoder Encoder to use.
 * @param {number} imageQuality (0..1], Encoding quality of an image.
 * @return {!Blob} encoded data.
 */
ImageEncoder.getBlob = function(canvas, metadataEncoder, imageQuality) {
  ImageUtil.trace.resetTimer('dataurl');
  // WebKit does not support canvas.toBlob yet so canvas.toDataURL is
  // the only way to use the Chrome built-in image encoder.
  var dataURL = canvas.toDataURL(metadataEncoder.mimeType, imageQuality);
  ImageUtil.trace.reportTimer('dataurl');

  var encodedImage = ImageEncoder.decodeDataURL(dataURL);

  var encodedMetadata = metadataEncoder.encode();

  var slices = [];

  // TODO(kaznacheev): refactor |stringToArrayBuffer| and |encode| to return
  // arrays instead of array buffers.
  function appendSlice(arrayBuffer) {
    slices.push(new DataView(arrayBuffer));
  }

  ImageUtil.trace.resetTimer('blob');
  if (encodedMetadata.byteLength != 0) {
    var metadataRange = metadataEncoder.findInsertionRange(encodedImage);
    appendSlice(ImageEncoder.stringToArrayBuffer(
        encodedImage, 0, metadataRange.from));

    appendSlice(metadataEncoder.encode());

    appendSlice(ImageEncoder.stringToArrayBuffer(
        encodedImage, metadataRange.to, encodedImage.length));
  } else {
    appendSlice(ImageEncoder.stringToArrayBuffer(
        encodedImage, 0, encodedImage.length));
  }
  var blob = new Blob(slices, {type: metadataEncoder.mimeType});
  ImageUtil.trace.reportTimer('blob');
  return blob;
};

/**
 * Decode a dataURL into a binary string containing the encoded image.
 *
 * Why return a string? Calling atob and having the rest of the code deal
 * with a string is several times faster than decoding base64 in Javascript.
 *
 * @param {string} dataURL Data URL to decode.
 * @return {string} A binary string (char codes are the actual byte values).
 */
ImageEncoder.decodeDataURL = function(dataURL) {
  // Skip the prefix ('data:image/<type>;base64,')
  var base64string = dataURL.substring(dataURL.indexOf(',') + 1);
  return window.atob(base64string);
};

/**
 * Return a thumbnail for an image.
 * @param {!HTMLCanvasElement} canvas Original image.
 * @return {HTMLCanvasElement} Thumbnail canvas.
 */
ImageEncoder.createThumbnail = function(canvas) {
  if (canvas.width < ImageEncoder.MIN_IMAGE_DIMENSION_FOR_THUMBNAIL &&
      canvas.height < ImageEncoder.MIN_IMAGE_DIMENSION_FOR_THUMBNAIL) {
    return null;
  }

  var ratio = Math.min(ImageEncoder.MAX_THUMBNAIL_DIMENSION / canvas.width,
                       ImageEncoder.MAX_THUMBNAIL_DIMENSION / canvas.height);
  var thumbnailCanvas = assertInstanceof(
      canvas.ownerDocument.createElement('canvas'), HTMLCanvasElement);
  thumbnailCanvas.width = Math.round(canvas.width * ratio);
  thumbnailCanvas.height = Math.round(canvas.height * ratio);

  var context = thumbnailCanvas.getContext('2d');
  context.drawImage(canvas,
      0, 0, canvas.width, canvas.height,
      0, 0, thumbnailCanvas.width, thumbnailCanvas.height);

  return thumbnailCanvas;
};

/**
 * Converts string to an array buffer.
 * @param {string} string A string.
 * @param {number} from Start index.
 * @param {number} to End index.
 * @return {!ArrayBuffer}  A created array buffer is returned.
 */
ImageEncoder.stringToArrayBuffer = function(string, from, to) {
  var size = to - from;
  var array = new Uint8Array(size);
  for (var i = 0; i != size; i++) {
    array[i] = string.charCodeAt(from + i);
  }
  return array.buffer;
};

/**
 * A base class for a metadata encoder.
 *
 * Serves as a default metadata encoder for images that none of the metadata
 * parsers recognized.
 *
 * @param {!MetadataItem} originalMetadata Starting metadata.
 * @constructor
 * @struct
 */
ImageEncoder.MetadataEncoder = function(originalMetadata) {
  var mimeType = ImageEncoder.MetadataEncoder.getMimeType_(originalMetadata);

  /**
   * Chrome can only encode JPEG and PNG. Force PNG mime type so that we
   * can save to file and generate a thumbnail.
   * @public {string}
   */
  this.mimeType = mimeType === 'image/jpeg' ? 'image/jpeg' : 'image/png';

  /**
   * @protected {string}
   */
  this.thumbnailDataUrl = '';

  /**
   * @protected {number}
   */
  this.imageWidth = 0;

  /**
   * @protected {number}
   */
  this.imageHeight = 0;
};

/**
 * Gets mime type from metadata. It reads media.mimeType at first, and if it
 * fails, it falls back to external.contentMimeType. If both fields are
 * undefined, it means that metadata is broken. Then it throws an exception.
 *
 * @param {!MetadataItem} metadata Metadata.
 * @return {string} Mime type.
 * @private
 */
ImageEncoder.MetadataEncoder.getMimeType_ = function(metadata) {
  return assert(metadata.mediaMimeType || metadata.contentMimeType);
};

/**
 * Sets an image data.
 * @param {!HTMLCanvasElement} canvas Canvas or anything with width and height
 *     properties.
 */
ImageEncoder.MetadataEncoder.prototype.setImageData = function(canvas) {
  this.imageWidth = canvas.width;
  this.imageHeight = canvas.height;
};

/**
 * @param {HTMLCanvasElement} canvas Canvas to use as thumbnail. Note that it
 *     can be null.
 * @param {number} quality Thumbnail quality.
 */
ImageEncoder.MetadataEncoder.prototype.setThumbnailData =
    function(canvas, quality) {
  this.thumbnailDataUrl =
      canvas ? canvas.toDataURL(this.mimeType, quality) : '';
};

/**
 * Returns a range where the metadata is (or should be) located.
 * @param {string} encodedImage An encoded image.
 * @return {{from:number, to:number}} An object with from and to properties.
 */
ImageEncoder.MetadataEncoder.prototype.findInsertionRange = function(
    encodedImage) {
  return {from: 0, to: 0};
};

/**
 * Returns serialized metadata ready to write to an image file.
 * The return type is optimized for passing to Blob.append.
 * @return {!ArrayBuffer} Serialized metadata.
 */
ImageEncoder.MetadataEncoder.prototype.encode = function() {
  return new Uint8Array(0).buffer;
};
