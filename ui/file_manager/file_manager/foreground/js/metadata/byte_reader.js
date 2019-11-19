// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @final */
class ByteReader {
  /**
   * @param {ArrayBuffer} arrayBuffer An array of buffers to be read from.
   * @param {number=} opt_offset Offset to read bytes at.
   * @param {number=} opt_length Number of bytes to read.
   */
  constructor(arrayBuffer, opt_offset, opt_length) {
    opt_offset = opt_offset || 0;
    opt_length = opt_length || (arrayBuffer.byteLength - opt_offset);
    /** @private @const {!DataView} */
    this.view_ = new DataView(arrayBuffer, opt_offset, opt_length);
    /** @private {number} */
    this.pos_ = 0;
    /** @private @const {!Array<number>} */
    this.seekStack_ = [];
    /** @private {boolean} */
    this.littleEndian_ = false;
  }

  /**
   * Throw an error if (0 > pos >= end) or if (pos + size > end).
   *
   * Static utility function.
   *
   * @param {number} pos Position in the file.
   * @param {number} size Number of bytes to read.
   * @param {number} end Maximum position to read from.
   */
  static validateRead(pos, size, end) {
    if (pos < 0 || pos >= end) {
      throw new Error('Invalid read position');
    }

    if (pos + size > end) {
      throw new Error('Read past end of buffer');
    }
  }

  /**
   * Read as a sequence of characters, returning them as a single string.
   *
   * This is a static utility function.  There is a member function with the
   * same name which side-effects the current read position.
   *
   * @param {DataView} dataView Data view instance.
   * @param {number} pos Position in bytes to read from.
   * @param {number} size Number of bytes to read.
   * @param {number=} opt_end Maximum position to read from.
   * @return {string} Read string.
   */
  static readString(dataView, pos, size, opt_end) {
    ByteReader.validateRead(pos, size, opt_end || dataView.byteLength);

    const codes = [];

    for (let i = 0; i < size; ++i) {
      codes.push(dataView.getUint8(pos + i));
    }

    return String.fromCharCode.apply(null, codes);
  }

  /**
   * Read as a sequence of characters, returning them as a single string.
   *
   * This is a static utility function.  There is a member function with the
   * same name which side-effects the current read position.
   *
   * @param {DataView} dataView Data view instance.
   * @param {number} pos Position in bytes to read from.
   * @param {number} size Number of bytes to read.
   * @param {number=} opt_end Maximum position to read from.
   * @return {string} Read string.
   */
  static readNullTerminatedString(dataView, pos, size, opt_end) {
    ByteReader.validateRead(pos, size, opt_end || dataView.byteLength);

    const codes = [];

    for (let i = 0; i < size; ++i) {
      const code = dataView.getUint8(pos + i);
      if (code == 0) {
        break;
      }
      codes.push(code);
    }

    return String.fromCharCode.apply(null, codes);
  }

  /**
   * Read as a sequence of UTF16 characters, returning them as a single string.
   *
   * This is a static utility function.  There is a member function with the
   * same name which side-effects the current read position.
   *
   * @param {DataView} dataView Data view instance.
   * @param {number} pos Position in bytes to read from.
   * @param {boolean} bom True if BOM should be parsed.
   * @param {number} size Number of bytes to read.
   * @param {number=} opt_end Maximum position to read from.
   * @return {string} Read string.
   */
  static readNullTerminatedStringUTF16(dataView, pos, bom, size, opt_end) {
    ByteReader.validateRead(pos, size, opt_end || dataView.byteLength);

    let littleEndian = false;
    let start = 0;

    if (bom) {
      littleEndian = (dataView.getUint8(pos) == 0xFF);
      start = 2;
    }

    const codes = [];

    for (let i = start; i < size; i += 2) {
      const code = dataView.getUint16(pos + i, littleEndian);
      if (code == 0) {
        break;
      }
      codes.push(code);
    }

    return String.fromCharCode.apply(null, codes);
  }

  /**
   * Read as a sequence of bytes, returning them as a single base64 encoded
   * string.
   *
   * This is a static utility function.  There is a member function with the
   * same name which side-effects the current read position.
   *
   * @param {DataView} dataView Data view instance.
   * @param {number} pos Position in bytes to read from.
   * @param {number} size Number of bytes to read.
   * @param {number=} opt_end Maximum position to read from.
   * @return {string} Base 64 encoded value.
   */
  static readBase64(dataView, pos, size, opt_end) {
    ByteReader.validateRead(pos, size, opt_end || dataView.byteLength);

    const rv = [];
    const chars = [];
    let padding = 0;

    for (let i = 0; i < size; /* incremented inside */) {
      let bits = dataView.getUint8(pos + (i++)) << 16;

      if (i < size) {
        bits |= dataView.getUint8(pos + (i++)) << 8;

        if (i < size) {
          bits |= dataView.getUint8(pos + (i++));
        } else {
          padding = 1;
        }
      } else {
        padding = 2;
      }

      chars[3] = ByteReader.base64Alphabet_[bits & 63];
      chars[2] = ByteReader.base64Alphabet_[(bits >> 6) & 63];
      chars[1] = ByteReader.base64Alphabet_[(bits >> 12) & 63];
      chars[0] = ByteReader.base64Alphabet_[(bits >> 18) & 63];

      rv.push.apply(rv, chars);
    }

    if (padding > 0) {
      rv[rv.length - 1] = '=';
    }
    if (padding > 1) {
      rv[rv.length - 2] = '=';
    }

    return rv.join('');
  }

  /**
   * Read as an image encoded in a data url.
   *
   * This is a static utility function.  There is a member function with the
   * same name which side-effects the current read position.
   *
   * @param {DataView} dataView Data view instance.
   * @param {number} pos Position in bytes to read from.
   * @param {number} size Number of bytes to read.
   * @param {number=} opt_end Maximum position to read from.
   * @return {string} Image as a data url.
   */
  static readImage(dataView, pos, size, opt_end) {
    opt_end = opt_end || dataView.byteLength;
    ByteReader.validateRead(pos, size, opt_end);

    // Two bytes is enough to identify the mime type.
    const prefixToMime = {
      '\x89P': 'png',
      '\xFF\xD8': 'jpeg',
      'BM': 'bmp',
      'GI': 'gif',
    };

    const prefix = ByteReader.readString(dataView, pos, 2, opt_end);
    const mime = prefixToMime[prefix] ||
        dataView.getUint16(pos, false).toString(16);  // For debugging.

    const b64 = ByteReader.readBase64(dataView, pos, size, opt_end);
    return 'data:image/' + mime + ';base64,' + b64;
  }

  /**
   * Return true if the requested number of bytes can be read from the buffer.
   *
   * @param {number} size Number of bytes to read.
   * @return {boolean} True if allowed, false otherwise.
   */
  canRead(size) {
    return this.pos_ + size <= this.view_.byteLength;
  }

  /**
   * Return true if the current position is past the end of the buffer.
   * @return {boolean} True if EOF, otherwise false.
   */
  eof() {
    return this.pos_ >= this.view_.byteLength;
  }

  /**
   * Return true if the current position is before the beginning of the buffer.
   * @return {boolean} True if BOF, otherwise false.
   */
  bof() {
    return this.pos_ < 0;
  }

  /**
   * Return true if the current position is outside the buffer.
   * @return {boolean} True if outside, false if inside.
   */
  beof() {
    return this.pos_ >= this.view_.byteLength || this.pos_ < 0;
  }

  /**
   * Set the expected byte ordering for future reads.
   * @param {number} order Byte order. Either LITTLE_ENDIAN or BIG_ENDIAN.
   */
  setByteOrder(order) {
    this.littleEndian_ = order == ByteReader.LITTLE_ENDIAN;
  }

  /**
   * Throw an error if the reader is at an invalid position, or if a read a read
   * of |size| would put it in one.
   *
   * You may optionally pass opt_end to override what is considered to be the
   * end of the buffer.
   *
   * @param {number} size Number of bytes to read.
   * @param {number=} opt_end Maximum position to read from.
   */
  validateRead(size, opt_end) {
    if (typeof opt_end == 'undefined') {
      opt_end = this.view_.byteLength;
    }

    ByteReader.validateRead(this.pos_, size, opt_end);
  }

  /**
   * @param {number} width Number of bytes to read.
   * @param {boolean=} opt_signed True if signed, false otherwise.
   * @param {number=} opt_end Maximum position to read from.
   * @return {number} Scalar value.
   */
  readScalar(width, opt_signed, opt_end) {
    let method = opt_signed ? 'getInt' : 'getUint';

    switch (width) {
      case 1:
        method += '8';
        break;

      case 2:
        method += '16';
        break;

      case 4:
        method += '32';
        break;

      case 8:
        method += '64';
        break;

      default:
        throw new Error('Invalid width: ' + width);
        break;
    }

    this.validateRead(width, opt_end);
    const rv = this.view_[method](this.pos_, this.littleEndian_);
    this.pos_ += width;
    return rv;
  }

  /**
   * Read as a sequence of characters, returning them as a single string.
   *
   * Adjusts the current position on success.  Throws an exception if the
   * read would go past the end of the buffer.
   *
   * @param {number} size Number of bytes to read.
   * @param {number=} opt_end Maximum position to read from.
   * @return {string} String value.
   */
  readString(size, opt_end) {
    const rv = ByteReader.readString(this.view_, this.pos_, size, opt_end);
    this.pos_ += size;
    return rv;
  }

  /**
   * Read as a sequence of characters, returning them as a single string.
   *
   * Adjusts the current position on success.  Throws an exception if the
   * read would go past the end of the buffer.
   *
   * @param {number} size Number of bytes to read.
   * @param {number=} opt_end Maximum position to read from.
   * @return {string} Null-terminated string value.
   */
  readNullTerminatedString(size, opt_end) {
    const rv = ByteReader.readNullTerminatedString(
        this.view_, this.pos_, size, opt_end);
    this.pos_ += rv.length;

    if (rv.length < size) {
      // If we've stopped reading because we found '0' but didn't hit size limit
      // then we should skip additional '0' character
      this.pos_++;
    }

    return rv;
  }

  /**
   * Read as a sequence of UTF16 characters, returning them as a single string.
   *
   * Adjusts the current position on success.  Throws an exception if the
   * read would go past the end of the buffer.
   *
   * @param {boolean} bom True if BOM should be parsed.
   * @param {number} size Number of bytes to read.
   * @param {number=} opt_end Maximum position to read from.
   * @return {string} Read string.
   */
  readNullTerminatedStringUTF16(bom, size, opt_end) {
    const rv = ByteReader.readNullTerminatedStringUTF16(
        this.view_, this.pos_, bom, size, opt_end);

    if (bom) {
      // If the BOM word was present advance the position.
      this.pos_ += 2;
    }

    this.pos_ += rv.length;

    if (rv.length < size) {
      // If we've stopped reading because we found '0' but didn't hit size limit
      // then we should skip additional '0' character
      this.pos_ += 2;
    }

    return rv;
  }

  /**
   * Read as an array of numbers.
   *
   * Adjusts the current position on success.  Throws an exception if the
   * read would go past the end of the buffer.
   *
   * @param {number} size Number of bytes to read.
   * @param {number=} opt_end Maximum position to read from.
   * @param {function(new:Array<*>)=} opt_arrayConstructor Array constructor.
   * @return {Array<*>} Array of bytes.
   */
  readSlice(size, opt_end, opt_arrayConstructor) {
    this.validateRead(size, opt_end);

    const arrayConstructor = opt_arrayConstructor || Uint8Array;
    const slice = new arrayConstructor(
        this.view_.buffer, this.view_.byteOffset + this.pos_, size);
    this.pos_ += size;

    return slice;
  }

  /**
   * Read as a sequence of bytes, returning them as a single base64 encoded
   * string.
   *
   * Adjusts the current position on success.  Throws an exception if the
   * read would go past the end of the buffer.
   *
   * @param {number} size Number of bytes to read.
   * @param {number=} opt_end Maximum position to read from.
   * @return {string} Base 64 encoded value.
   */
  readBase64(size, opt_end) {
    const rv = ByteReader.readBase64(this.view_, this.pos_, size, opt_end);
    this.pos_ += size;
    return rv;
  }

  /**
   * Read an image returning it as a data url.
   *
   * Adjusts the current position on success.  Throws an exception if the
   * read would go past the end of the buffer.
   *
   * @param {number} size Number of bytes to read.
   * @param {number=} opt_end Maximum position to read from.
   * @return {string} Image as a data url.
   */
  readImage(size, opt_end) {
    const rv = ByteReader.readImage(this.view_, this.pos_, size, opt_end);
    this.pos_ += size;
    return rv;
  }

  /**
   * Seek to a give position relative to opt_seekStart.
   *
   * @param {number} pos Position in bytes to seek to.
   * @param {number=} opt_seekStart Relative position in bytes.
   * @param {number=} opt_end Maximum position to seek to.
   */
  seek(pos, opt_seekStart, opt_end) {
    opt_end = opt_end || this.view_.byteLength;

    let newPos;
    if (opt_seekStart == ByteReader.SEEK_CUR) {
      newPos = this.pos_ + pos;
    } else if (opt_seekStart == ByteReader.SEEK_END) {
      newPos = opt_end + pos;
    } else {
      newPos = pos;
    }

    if (newPos < 0 || newPos > this.view_.byteLength) {
      throw new Error('Seek outside of buffer: ' + (newPos - opt_end));
    }

    this.pos_ = newPos;
  }

  /**
   * Seek to a given position relative to opt_seekStart, saving the current
   * position.
   *
   * Recover the current position with a call to seekPop.
   *
   * @param {number} pos Position in bytes to seek to.
   * @param {number=} opt_seekStart Relative position in bytes.
   */
  pushSeek(pos, opt_seekStart) {
    const oldPos = this.pos_;
    this.seek(pos, opt_seekStart);
    // Alter the seekStack_ after the call to seek(), in case it throws.
    this.seekStack_.push(oldPos);
  }

  /**
   * Undo a previous seekPush.
   */
  popSeek() {
    this.seek(this.seekStack_.pop());
  }

  /**
   * Return the current read position.
   * @return {number} Current position in bytes.
   */
  tell() {
    return this.pos_;
  }
}

/**
 * Intel, 0x1234 is [0x34, 0x12]
 * @const {number}
 */
ByteReader.LITTLE_ENDIAN = 0;

/**
 * Motorola, 0x1234 is [0x12, 0x34]
 * @const {number}
 */
ByteReader.BIG_ENDIAN = 1;

/**
 * Seek relative to the beginning of the buffer.
 * @const {number}
 */
ByteReader.SEEK_BEG = 0;

/**
 * Seek relative to the current position.
 * @const {number}
 */
ByteReader.SEEK_CUR = 1;

/**
 * Seek relative to the end of the buffer.
 * @const {number}
 */
ByteReader.SEEK_END = 2;

/**
 * @private @const {Array<string>}
 */
ByteReader.base64Alphabet_ =
    ('ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/')
        .split('');
