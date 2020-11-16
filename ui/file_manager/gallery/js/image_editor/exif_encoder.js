// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* eslint-disable no-var */

// clang-format off
// #import {ImageEncoder} from './image_encoder.m.js';
// #import * as wrappedExif from '../../../file_manager/foreground/js/metadata/exif_constants.m.js'; const {Exif} = wrappedExif;
// #import {MetadataItem} from '../../../file_manager/foreground/js/metadata/metadata_item.m.js';
// #import {assert} from 'chrome://resources/js/assert.m.js'
// #import {ExifEntry} from '../../../externs/exif_entry.m.js';
// clang-format on

/**
 * The Exif metadata encoder.
 * Uses the metadata format as defined by ExifParser.
 * @param {!MetadataItem} originalMetadata Metadata to encode.
 * @constructor
 * @extends {ImageEncoder.MetadataEncoder}
 * @struct
 */
/* #export */ function ExifEncoder(originalMetadata) {
  ImageEncoder.MetadataEncoder.apply(this, arguments);
  /**
   * Image File Directory obtained from EXIF header.
   * @private {!Object}
   * @const
   */
  this.ifd_ = /** @type {!Object} */(
      JSON.parse(JSON.stringify(originalMetadata.ifd || {})));

  /**
   * Note use little endian if the original metadata does not have the
   * information.
   * @private {boolean}
   * @const
   */
  this.exifLittleEndian_ = !!originalMetadata.exifLittleEndian;

  /**
   * Modification time to be stored in EXIF header.
   * @private {!Date}
   * @const
   */
  this.modificationTime_ = assert(originalMetadata.modificationTime);
}

ExifEncoder.prototype = {__proto__: ImageEncoder.MetadataEncoder.prototype};

ImageEncoder.registerMetadataEncoder(ExifEncoder, 'image/jpeg');

/**
 * Software name of the Gallery app.
 * @type {string}
 * @const
 */
ExifEncoder.SOFTWARE = 'Chrome OS Gallery App\0';

/**
 * Maximum size of exif data.
 * @const {number}
 */
ExifEncoder.MAXIMUM_EXIF_DATA_SIZE = 0x10000;

/**
 * Size of metadata for thumbnail.
 * @const {number}
 */
ExifEncoder.THUMBNAIL_METADATA_SIZE = 76;

/**
 * @param {!HTMLCanvasElement} canvas
 * @override
 */
ExifEncoder.prototype.setImageData = function(canvas) {
  ImageEncoder.MetadataEncoder.prototype.setImageData.call(this, canvas);

  var image = this.ifd_.image;
  if (!image) {
    image = this.ifd_.image = {};
  }

  // Only update width/height in this directory if they are present.
  if (image[Exif.Tag.IMAGE_WIDTH] && image[Exif.Tag.IMAGE_HEIGHT]) {
    image[Exif.Tag.IMAGE_WIDTH].value = canvas.width;
    image[Exif.Tag.IMAGE_HEIGHT].value = canvas.height;
  }

  var exif = this.ifd_.exif;
  if (!exif) {
    exif = this.ifd_.exif = {};
  }
  ExifEncoder.findOrCreateTag(image, Exif.Tag.EXIFDATA);
  ExifEncoder.findOrCreateTag(exif, Exif.Tag.X_DIMENSION).value = canvas.width;
  ExifEncoder.findOrCreateTag(exif, Exif.Tag.Y_DIMENSION).value = canvas.height;

  // Always save in default orientation.
  ExifEncoder.findOrCreateTag(image, Exif.Tag.ORIENTATION).value = 1;

  // Update software name.
  var softwareTag = ExifEncoder.findOrCreateTag(image, Exif.Tag.SOFTWARE, 2);
  softwareTag.value = ExifEncoder.SOFTWARE;
  softwareTag.componentCount = ExifEncoder.SOFTWARE.length;

  // Update modification date time.
  var padNumWithZero = function(num, length) {
    var str = num.toString();
    while (str.length < length) {
      str = '0' + str;
    }
    return str;
  };

  var dateTimeTag = ExifEncoder.findOrCreateTag(image, Exif.Tag.DATETIME, 2);
  dateTimeTag.value =
      padNumWithZero(this.modificationTime_.getFullYear(), 4) + ':' +
      padNumWithZero(this.modificationTime_.getMonth() + 1, 2) + ':' +
      padNumWithZero(this.modificationTime_.getDate(), 2) + ' ' +
      padNumWithZero(this.modificationTime_.getHours(), 2) + ':' +
      padNumWithZero(this.modificationTime_.getMinutes(), 2) + ':' +
      padNumWithZero(this.modificationTime_.getSeconds(), 2) + '\0';
  dateTimeTag.componentCount = 20;
};

/**
 * @override
 */
ExifEncoder.prototype.setThumbnailData = function(canvas, quality) {
  if (canvas) {
    // Empirical formula with reasonable behavior:
    // 10K for 1Mpix, 30K for 5Mpix, 50K for 9Mpix and up.
    var pixelCount = this.imageWidth * this.imageHeight;
    var maxEncodedSize = 5000 * Math.min(10, 1 + pixelCount / 1000000);
    var DATA_URL_PREFIX = 'data:image/jpeg;base64,';
    var BASE64_BLOAT = 4 / 3;
    var maxDataURLLength =
        DATA_URL_PREFIX.length + Math.ceil(maxEncodedSize * BASE64_BLOAT);
    for (; quality > 0.2; quality *= 0.8) {
      ImageEncoder.MetadataEncoder.prototype.setThumbnailData.call(
          this, canvas, quality);
      // If the obtained thumbnail URL is too long, reset the URL and try again
      // with less quality value.
      if (this.thumbnailDataUrl.length > maxDataURLLength) {
        this.thumbnailDataUrl = '';
        continue;
      }
      break;
    }
  }
  if (this.thumbnailDataUrl) {
    var thumbnail = this.ifd_.thumbnail;
    if (!thumbnail) {
      thumbnail = this.ifd_.thumbnail = {};
    }

    ExifEncoder.findOrCreateTag(thumbnail, Exif.Tag.IMAGE_WIDTH).value =
        canvas.width;

    ExifEncoder.findOrCreateTag(thumbnail, Exif.Tag.IMAGE_HEIGHT).value =
        canvas.height;

    // The values for these tags will be set in ExifWriter.encode.
    ExifEncoder.findOrCreateTag(thumbnail, Exif.Tag.JPG_THUMB_OFFSET);
    ExifEncoder.findOrCreateTag(thumbnail, Exif.Tag.JPG_THUMB_LENGTH);

    // Always save in default orientation.
    ExifEncoder.findOrCreateTag(thumbnail, Exif.Tag.ORIENTATION).value = 1;

    // When thumbnail is compressed with JPEG, compression must be set as 6.
    ExifEncoder.findOrCreateTag(this.ifd_.image, Exif.Tag.COMPRESSION).value =
        6;
  } else {
    if (this.ifd_.thumbnail) {
      delete this.ifd_.thumbnail;
    }
  }
};

/**
 * @override
 */
ExifEncoder.prototype.findInsertionRange = function(encodedImage) {
  function getWord(pos) {
    if (pos + 2 > encodedImage.length) {
      throw 'Reading past the buffer end @' + pos;
    }
    return encodedImage.charCodeAt(pos) << 8 | encodedImage.charCodeAt(pos + 1);
  }

  if (getWord(0) != Exif.Mark.SOI) {
    throw new Error('Jpeg data starts from 0x' + getWord(0).toString(16));
  }

  var sectionStart = 2;

  // Default: an empty range right after SOI.
  // Will be returned in absence of APP0 or Exif sections.
  var range = {from: sectionStart, to: sectionStart};

  for (;;) {
    var tag = getWord(sectionStart);

    if (tag == Exif.Mark.SOS) {
      break;
    }

    var nextSectionStart = sectionStart + 2 + getWord(sectionStart + 2);
    if (nextSectionStart <= sectionStart ||
        nextSectionStart > encodedImage.length) {
      throw new Error('Invalid section size in jpeg data');
    }

    if (tag == Exif.Mark.APP0) {
      // Assert that we have not seen the Exif section yet.
      if (range.from != range.to) {
        throw new Error('APP0 section found after EXIF section');
      }
      // An empty range right after the APP0 segment.
      range.from = range.to = nextSectionStart;
    } else if (tag == Exif.Mark.EXIF) {
      // A range containing the existing EXIF section.
      range.from = sectionStart;
      range.to = nextSectionStart;
    }
    sectionStart = nextSectionStart;
  }

  return range;
};

/**
 * @override
 */
ExifEncoder.prototype.encode = function() {
  var HEADER_SIZE = 10;

  // Allocate the largest theoretically possible size.
  var bytes = new Uint8Array(ExifEncoder.MAXIMUM_EXIF_DATA_SIZE);

  // Serialize header
  var hw = new ByteWriter(bytes.buffer, 0, HEADER_SIZE);
  hw.writeScalar(Exif.Mark.EXIF, 2);
  hw.forward('size', 2);
  hw.writeString('Exif\0\0');  // Magic string.

  // First serialize the content of the exif section.
  // Use a ByteWriter starting at HEADER_SIZE offset so that tell() positions
  // can be directly mapped to offsets as encoded in the dictionaries.
  var bw = new ByteWriter(bytes.buffer, HEADER_SIZE);

  if (this.exifLittleEndian_) {
    bw.setByteOrder(ByteWriter.ByteOrder.LITTLE_ENDIAN);
    bw.writeScalar(Exif.Align.LITTLE, 2);
  } else {
    bw.setByteOrder(ByteWriter.ByteOrder.BIG_ENDIAN);
    bw.writeScalar(Exif.Align.BIG, 2);
  }

  bw.writeScalar(Exif.Tag.TIFF, 2);

  bw.forward('image-dir', 4);  // The pointer should point right after itself.
  bw.resolveOffset('image-dir');

  ExifEncoder.encodeDirectory(bw, this.ifd_.image,
      [Exif.Tag.EXIFDATA, Exif.Tag.GPSDATA], 'thumb-dir');

  if (this.ifd_.exif) {
    bw.resolveOffset(Exif.Tag.EXIFDATA);
    ExifEncoder.encodeDirectory(bw, this.ifd_.exif);
  } else {
    if (Exif.Tag.EXIFDATA in this.ifd_.image) {
      throw new Error('Corrupt exif dictionary reference');
    }
  }

  if (this.ifd_.gps) {
    bw.resolveOffset(Exif.Tag.GPSDATA);
    ExifEncoder.encodeDirectory(bw, this.ifd_.gps);
  } else {
    if (Exif.Tag.GPSDATA in this.ifd_.image) {
      throw new Error('Missing gps dictionary reference');
    }
  }

  // Since thumbnail data can be large, check enough space has left for
  // thumbnail data.
  var thumbnailDecoded = this.ifd_.thumbnail ?
      ImageEncoder.decodeDataURL(this.thumbnailDataUrl) : '';
  if (this.ifd_.thumbnail &&
      (ExifEncoder.MAXIMUM_EXIF_DATA_SIZE - bw.tell() >=
       thumbnailDecoded.length + ExifEncoder.THUMBNAIL_METADATA_SIZE)) {
    bw.resolveOffset('thumb-dir');
    ExifEncoder.encodeDirectory(
        bw,
        this.ifd_.thumbnail,
        [Exif.Tag.JPG_THUMB_OFFSET, Exif.Tag.JPG_THUMB_LENGTH]);

    bw.resolveOffset(Exif.Tag.JPG_THUMB_OFFSET);
    bw.resolve(Exif.Tag.JPG_THUMB_LENGTH, thumbnailDecoded.length);
    bw.writeString(thumbnailDecoded);
  } else {
    bw.resolve('thumb-dir', 0);
  }

  bw.checkResolved();

  var totalSize = HEADER_SIZE + bw.tell();
  hw.resolve('size', totalSize - 2);  // The marker is excluded.
  hw.checkResolved();

  var subarray = new Uint8Array(totalSize);
  for (var i = 0; i != totalSize; i++) {
    subarray[i] = bytes[i];
  }
  return subarray.buffer;
};

/*
 * Static methods.
 */

/**
 * Write the contents of an IFD directory.
 * @param {!ByteWriter} bw ByteWriter to use.
 * @param {!Object<!Exif.Tag, ExifEntry>} directory A directory map as created
 *     by ExifParser.
 * @param {Array=} opt_resolveLater An array of tag ids for which the values
 *     will be resolved later.
 * @param {string=} opt_nextDirPointer A forward key for the pointer to the next
 *     directory. If omitted the pointer is set to 0.
 */
ExifEncoder.encodeDirectory = function(
    bw, directory, opt_resolveLater, opt_nextDirPointer) {

  var longValues = [];

  bw.forward('dir-count', 2);
  var count = 0;

  for (var key in directory) {
    var tag = directory[/** @type {!Exif.Tag} */ (parseInt(key, 10))];
    bw.writeScalar(/** @type {number}*/ (tag.id), 2);
    bw.writeScalar(tag.format, 2);
    bw.writeScalar(tag.componentCount, 4);

    var width = ExifEncoder.getComponentWidth(tag) * tag.componentCount;

    if (opt_resolveLater && (opt_resolveLater.indexOf(tag.id) >= 0)) {
      // The actual value depends on further computations.
      if (tag.componentCount != 1 || width > 4) {
        throw new Error('Cannot forward the pointer for ' + tag.id);
      }
      bw.forward(tag.id, width);
    } else if (width <= 4) {
      // The value fits into 4 bytes, write it immediately.
      ExifEncoder.writeValue(bw, tag);
    } else {
      // The value does not fit, forward the 4 byte offset to the actual value.
      width = 4;
      bw.forward(tag.id, width);
      longValues.push(tag);
    }
    bw.skip(4 - width);  // Align so that the value take up exactly 4 bytes.
    count++;
  }

  bw.resolve('dir-count', count);

  if (opt_nextDirPointer) {
    bw.forward(opt_nextDirPointer, 4);
  } else {
    bw.writeScalar(0, 4);
  }

  // Write out the long values and resolve pointers.
  for (var i = 0; i != longValues.length; i++) {
    var longValue = longValues[i];
    bw.resolveOffset(longValue.id);
    ExifEncoder.writeValue(bw, longValue);
  }
};

/**
 * @param {ExifEntry} tag EXIF tag object.
 * @return {number} Width in bytes of the data unit associated with this tag.
 * TODO(kaznacheev): Share with ExifParser?
 */
ExifEncoder.getComponentWidth = function(tag) {
  switch (tag.format) {
    case 1:  // Byte
    case 2:  // String
    case 7:  // Undefined
      return 1;

    case 3:  // Short
      return 2;

    case 4:  // Long
    case 9:  // Signed Long
      return 4;

    case 5:  // Rational
    case 10:  // Signed Rational
      return 8;

    default:  // ???
      console.warn('Unknown tag format 0x' +
          Number(tag.id).toString(16) + ': ' + tag.format);
      return 4;
  }
};

/**
 * Writes out the tag value.
 * @param {!ByteWriter} bw Writer to use.
 * @param {ExifEntry} tag Tag, which value to write.
 */
ExifEncoder.writeValue = function(bw, tag) {
  if (tag.format === 2) {  // String
    if (tag.componentCount !== tag.value.length) {
      throw new Error(
          'String size mismatch for 0x' + Number(tag.id).toString(16));
    }

    if (tag.value.charAt(tag.value.length - 1) !== '\0') {
      throw new Error('String must end with null character.');
    }

    bw.writeString(/** @type {string} */ (tag.value));
  } else {  // Scalar or rational
    var width = ExifEncoder.getComponentWidth(tag);

    var writeComponent = function(value, signed) {
      if (width == 8) {
        bw.writeScalar(value[0], 4, signed);
        bw.writeScalar(value[1], 4, signed);
      } else {
        bw.writeScalar(value, width, signed);
      }
    };

    var signed = (tag.format == 9 || tag.format == 10);
    if (tag.componentCount == 1) {
      writeComponent(tag.value, signed);
    } else {
      for (var i = 0; i != tag.componentCount; i++) {
        writeComponent(tag.value[i], signed);
      }
    }
  }
};

/**
 * Finds a tag. If not exist, creates a tag.
 *
 * @param {!Object<!Exif.Tag, ExifEntry>} directory EXIF directory.
 * @param {!Exif.Tag} id Tag id.
 * @param {number=} opt_format Tag format
 *     (used in {@link ExifEncoder#getComponentWidth}).
 * @param {number=} opt_componentCount Number of components in this tag.
 * @return {ExifEntry}
 *     Tag found or created.
 */
ExifEncoder.findOrCreateTag = function(directory, id, opt_format,
    opt_componentCount) {
  if (!(id in directory)) {
    directory[id] = {
      id: id,
      format: opt_format || 3,  // Short
      componentCount: opt_componentCount || 1,
      value: 0
    };
  }
  return directory[id];
};

/**
 * ByteWriter class.
 * @param {!ArrayBuffer} arrayBuffer Underlying buffer to use.
 * @param {number} offset Offset at which to start writing.
 * @param {number=} opt_length Maximum length to use.
 * @constructor
 * @struct
 */
/* #export */ function ByteWriter(arrayBuffer, offset, opt_length) {
  var length = opt_length || (arrayBuffer.byteLength - offset);
  this.view_ = new DataView(arrayBuffer, offset, length);
  this.littleEndian_ = false;
  this.pos_ = 0;
  this.forwards_ = {};
}

/**
 * Byte order.
 * @enum {number}
 */
ByteWriter.ByteOrder = {
  // Little endian byte order.
  LITTLE_ENDIAN: 0,
  // Big endian byte order.
  BIG_ENDIAN: 1
};

/**
 * Set the byte ordering for future writes.
 * @param {ByteWriter.ByteOrder} order ByteOrder to use
 *     {ByteWriter.LITTLE_ENDIAN} or {ByteWriter.BIG_ENDIAN}.
 */
ByteWriter.prototype.setByteOrder = function(order) {
  this.littleEndian_ = (order === ByteWriter.ByteOrder.LITTLE_ENDIAN);
};

/**
 * @return {number} the current write position.
 */
ByteWriter.prototype.tell = function() {
  return this.pos_;
};

/**
 * Skips desired amount of bytes in output stream.
 * @param {number} count Byte count to skip.
 */
ByteWriter.prototype.skip = function(count) {
  this.validateWrite(count);
  this.pos_ += count;
};

/**
 * Check if the buffer has enough room to read 'width' bytes. Throws an error
 * if it has not.
 * @param {number} width Amount of bytes to check.
 */
ByteWriter.prototype.validateWrite = function(width) {
  if (this.pos_ + width > this.view_.byteLength) {
    throw new Error('Writing past the end of the buffer');
  }
};

/**
 * Writes scalar value to output stream.
 * @param {number} value Value to write.
 * @param {number} width Desired width of written value.
 * @param {boolean=} opt_signed True if value represents signed number.
 */
ByteWriter.prototype.writeScalar = function(value, width, opt_signed) {
  var method;
  // The below switch is so verbose for two reasons:
  // 1. V8 is faster on method names which are 'symbols'.
  // 2. Method names are discoverable by full text search.
  switch (width) {
    case 1:
      method = opt_signed ? 'setInt8' : 'setUint8';
      break;

    case 2:
      method = opt_signed ? 'setInt16' : 'setUint16';
      break;

    case 4:
      method = opt_signed ? 'setInt32' : 'setUint32';
      break;

    case 8:
      method = opt_signed ? 'setInt64' : 'setUint64';
      break;

    default:
      throw new Error('Invalid width: ' + width);
      break;
  }

  this.validateWrite(width);
  this.view_[method](this.pos_, value, this.littleEndian_);
  this.pos_ += width;
};

/**
 * Writes string.
 * @param {string} str String to write.
 */
ByteWriter.prototype.writeString = function(str) {
  this.validateWrite(str.length);
  for (var i = 0; i != str.length; i++) {
    this.view_.setUint8(this.pos_++, str.charCodeAt(i));
  }
};

/**
 * Allocate the space for 'width' bytes for the value that will be set later.
 * To be followed by a 'resolve' call with the same key.
 * @param {(string|Exif.Tag)} key A key to identify the value.
 * @param {number} width Width of the value in bytes.
 */
ByteWriter.prototype.forward = function(key, width) {
  if (key in this.forwards_) {
    throw new Error('Duplicate forward key ' + key);
  }
  this.validateWrite(width);
  this.forwards_[key] = {
    pos: this.pos_,
    width: width
  };
  this.pos_ += width;
};

/**
 * Set the value previously allocated with a 'forward' call.
 * @param {(string|Exif.Tag)} key A key to identify the value.
 * @param {number} value value to write in pre-allocated space.
 */
ByteWriter.prototype.resolve = function(key, value) {
  if (!(key in this.forwards_)) {
    throw new Error('Undeclared forward key ' + key.toString(16));
  }
  var forward = this.forwards_[key];
  var curPos = this.pos_;
  this.pos_ = forward.pos;
  this.writeScalar(value, forward.width);
  this.pos_ = curPos;
  delete this.forwards_[key];
};

/**
 * A shortcut to resolve the value to the current write position.
 * @param {(string|Exif.Tag)} key A key to identify pre-allocated position.
 */
ByteWriter.prototype.resolveOffset = function(key) {
  this.resolve(key, this.tell());
};

/**
 * Check if every forward has been resolved, throw and error if not.
 */
ByteWriter.prototype.checkResolved = function() {
  for (var key in this.forwards_) {
    throw new Error('Unresolved forward pointer ' +
        ByteWriter.prettyKeyFormat(key));
  }
};

/**
 * If key is a number, format it in hex style.
 * @param {!(string|Exif.Tag)} key A key.
 * @return {string} Formatted representation.
 */
ByteWriter.prettyKeyFormat = function(key) {
  if (typeof key === 'number') {
    return '0x' + key.toString(16);
  } else {
    return key;
  }
};
