// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {ExifEntry} from '../../../externs/exif_entry.js';
import {MetadataParserLogger} from '../../../externs/metadata_worker_window.js';

import {ByteReader} from './byte_reader.js';
import {Exif} from './exif_constants.js';
import {ExifParser} from './exif_parser.js';

class ByteWriter {
  /**
   * @param {!ArrayBuffer} arrayBuffer Underlying buffer to use.
   * @param {number} offset Offset at which to start writing.
   * @param {number=} opt_length Maximum length to use.
   */
  constructor(arrayBuffer, offset, opt_length) {
    const length = opt_length || (arrayBuffer.byteLength - offset);
    this.view_ = new DataView(arrayBuffer, offset, length);
    this.littleEndian_ = false;
    this.pos_ = 0;
    this.forwards_ = {};
  }

  /**
   * If key is a number, format it in hex style.
   * @param {!(string|Exif.Tag)} key A key.
   * @return {string} Formatted representation.
   */
  static prettyKeyFormat(key) {
    if (typeof key === 'number') {
      return '0x' + key.toString(16);
    } else {
      return key;
    }
  }

  /**
   * Set the byte ordering for future writes.
   * @param {ByteWriter.ByteOrder} order ByteOrder to use
   *     {ByteWriter.LITTLE_ENDIAN} or {ByteWriter.BIG_ENDIAN}.
   */
  setByteOrder(order) {
    this.littleEndian_ = (order === ByteWriter.ByteOrder.LITTLE_ENDIAN);
  }

  /**
   * @return {number} the current write position.
   */
  tell() {
    return this.pos_;
  }

  /**
   * Skips desired amount of bytes in output stream.
   * @param {number} count Byte count to skip.
   */
  skip(count) {
    this.validateWrite(count);
    this.pos_ += count;
  }

  /**
   * Check if the buffer has enough room to read 'width' bytes. Throws an error
   * if it has not.
   * @param {number} width Amount of bytes to check.
   */
  validateWrite(width) {
    if (this.pos_ + width > this.view_.byteLength) {
      throw new Error('Writing past the end of the buffer');
    }
  }

  /**
   * Writes scalar value to output stream.
   * @param {number} value Value to write.
   * @param {number} width Desired width of written value.
   * @param {boolean=} opt_signed True if value represents signed number.
   */
  writeScalar(value, width, opt_signed) {
    let method;
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
  }

  /**
   * Writes string.
   * @param {string} str String to write.
   */
  writeString(str) {
    this.validateWrite(str.length);
    for (let i = 0; i != str.length; i++) {
      this.view_.setUint8(this.pos_++, str.charCodeAt(i));
    }
  }

  /**
   * Allocate the space for 'width' bytes for the value that will be set later.
   * To be followed by a 'resolve' call with the same key.
   * @param {(string|Exif.Tag)} key A key to identify the value.
   * @param {number} width Width of the value in bytes.
   */
  forward(key, width) {
    if (key in this.forwards_) {
      throw new Error('Duplicate forward key ' + key);
    }
    this.validateWrite(width);
    this.forwards_[key] = {
      pos: this.pos_,
      width: width,
    };
    this.pos_ += width;
  }

  /**
   * Set the value previously allocated with a 'forward' call.
   * @param {(string|Exif.Tag)} key A key to identify the value.
   * @param {number} value value to write in pre-allocated space.
   */
  resolve(key, value) {
    if (!(key in this.forwards_)) {
      throw new Error('Undeclared forward key ' + key.toString(16));
    }
    const forward = this.forwards_[key];
    const curPos = this.pos_;
    this.pos_ = forward.pos;
    this.writeScalar(value, forward.width);
    this.pos_ = curPos;
    delete this.forwards_[key];
  }

  /**
   * A shortcut to resolve the value to the current write position.
   * @param {(string|Exif.Tag)} key A key to identify pre-allocated position.
   */
  resolveOffset(key) {
    this.resolve(key, this.tell());
  }

  /**
   * Check if every forward has been resolved, throw and error if not.
   */
  checkResolved() {
    for (const key in this.forwards_) {
      throw new Error(
          'Unresolved forward pointer ' + ByteWriter.prettyKeyFormat(key));
    }
  }
}

/**
 * Byte order.
 * @enum {number}
 */
ByteWriter.ByteOrder = {
  // Little endian byte order.
  LITTLE_ENDIAN: 0,
  // Big endian byte order.
  BIG_ENDIAN: 1,
};

/**
 * Creates a directory with specified tag. This method only supports string
 * format tag, which is longer than 4 characters.
 * @param {!TypedArray} bytes Bytes to be written.
 * @param {!ExifEntry} tag An exif entry which will be written.
 */
function writeDirectory_(bytes, tag) {
  assertEquals(2, tag.format);
  assertTrue(tag.componentCount > 4);

  const byteWriter = new ByteWriter(bytes.buffer, 0);
  byteWriter.writeScalar(1, 2);  // Number of fields.

  byteWriter.writeScalar(tag.id, 2);
  byteWriter.writeScalar(tag.format, 2);
  byteWriter.writeScalar(tag.componentCount, 4);
  byteWriter.forward(tag.id, 4);

  byteWriter.writeScalar(0, 4);  // Offset to next IFD.

  byteWriter.resolveOffset(tag.id);
  const string = /** @type {string} */ (tag.value);
  byteWriter.writeString(string);

  byteWriter.checkResolved();
}

/**
 * @implements {MetadataParserLogger}
 * @final
 */
class ConsoleLogger {
  constructor() {
    this.verbose = true;
  }

  error(arg) {
    console.error(arg);
  }

  log(arg) {
    console.log(arg);
  }

  vlog(arg) {
    console.log(arg);
  }
}

/**
 * Parses exif data bytes (with logging) and returns the parsed tags.
 * @param {!TypedArray} bytes Bytes to be read.
 * @return {!Object<!Exif.Tag, !ExifEntry>} Tags.
 */
function parseExifData_(bytes) {
  const exifParser = new ExifParser(new ConsoleLogger());

  const tags = {};
  const byteReader = new ByteReader(bytes.buffer);
  assertEquals(0, exifParser.readDirectory(byteReader, tags));
  return tags;
}

/**
 * Tests that parsed exif strings have a null character termination.
 */
export function testWithoutNullCharacterTermination() {
  // Create exif with a value that does not end with null character.
  const data = new Uint8Array(0x10000);
  writeDirectory_(data, /** @type {!ExifEntry} */ ({
                    id: 0x10f,          // Manufacturer Id.
                    format: 2,          // String format.
                    componentCount: 8,  // Length of value 'Manufact'.
                    value: 'Manufact',
                  }));

  // Parse the exif data.
  const tags = parseExifData_(data);

  // The parsed value should end in a null character.
  const parsedTag = tags[/** @type {!Exif.Tag<number>} */ (0x10f)];
  assertEquals(9, parsedTag.componentCount);
  assertEquals('Manufact\0', parsedTag.value);
}
