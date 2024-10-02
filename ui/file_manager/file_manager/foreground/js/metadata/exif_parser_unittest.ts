// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {ByteOrder, ByteReader} from './byte_reader.js';
import {type ExifEntry, ExifTag} from './exif_constants.js';
import {ExifParser} from './exif_parser.js';
import type {MetadataParserLogger} from './metadata_parser.js';

class ByteWriter {
  private view_: DataView;
  private littleEndian_ = false;
  private pos_ = 0;
  private forwards_ = {} as Record<ExifTag, {pos: number, width: number}>;

  /**
   * @param arrayBuffer Underlying buffer to use.
   * @param offset Offset at which to start writing.
   * @param length Maximum length to use.
   */
  constructor(arrayBuffer: ArrayBuffer, offset: number, length?: number) {
    const calculatedLength = length || (arrayBuffer.byteLength - offset);
    this.view_ = new DataView(arrayBuffer, offset, calculatedLength);
  }

  /**
   * If key is a number, format it in hex style.
   * @param key A key.
   * @return Formatted representation.
   */
  static prettyKeyFormat(key: (string|ExifTag)): string {
    if (typeof key === 'number') {
      return '0x' + key.toString(16);
    } else {
      return key;
    }
  }

  /**
   * Set the byte ordering for future writes.
   * @param order ByteOrder to use
   *     {ByteOrder.LITTLE_ENDIAN} or {ByteOrder.BIG_ENDIAN}.
   */
  setByteOrder(order: ByteOrder) {
    this.littleEndian_ = (order === ByteOrder.LITTLE_ENDIAN);
  }

  /**
   * @return the current write position.
   */
  tell(): number {
    return this.pos_;
  }

  /**
   * Skips desired amount of bytes in output stream.
   * @param count Byte count to skip.
   */
  skip(count: number) {
    this.validateWrite(count);
    this.pos_ += count;
  }

  /**
   * Check if the buffer has enough room to read 'width' bytes. Throws an error
   * if it has not.
   * @param width Amount of bytes to check.
   */
  validateWrite(width: number) {
    if (this.pos_ + width > this.view_.byteLength) {
      throw new Error('Writing past the end of the buffer');
    }
  }

  /**
   * Writes scalar value to output stream.
   * @param value Value to write.
   * @param width Desired width of written value.
   * @param signed True if value represents signed number.
   */
  writeScalar(value: number, width: number, signed?: boolean) {
    this.validateWrite(width);

    switch (width) {
      case 1:
        if (signed) {
          this.view_.setInt8(this.pos_, value);
        } else {
          this.view_.setUint8(this.pos_, value);
        }
        break;

      case 2:
        if (signed) {
          this.view_.setInt16(this.pos_, value, this.littleEndian_);
        } else {
          this.view_.setUint16(this.pos_, value, this.littleEndian_);
        }
        break;

      case 4:
        if (signed) {
          this.view_.setInt32(this.pos_, value, this.littleEndian_);
        } else {
          this.view_.setUint32(this.pos_, value, this.littleEndian_);
        }
        break;

      default:
        throw new Error('Invalid width: ' + width);
    }

    this.pos_ += width;
  }

  /**
   * Writes string.
   * @param str String to write.
   */
  writeString(str: string) {
    this.validateWrite(str.length);
    for (let i = 0; i !== str.length; i++) {
      this.view_.setUint8(this.pos_++, str.charCodeAt(i));
    }
  }

  /**
   * Allocate the space for 'width' bytes for the value that will be set later.
   * To be followed by a 'resolve' call with the same key.
   * @param key A key to identify the value.
   * @param width Width of the value in bytes.
   */
  forward(key: ExifTag, width: number) {
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
   * @param key A key to identify the value.
   * @param value value to write in pre-allocated space.
   */
  resolve(key: ExifTag, value: number) {
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
   * @param key A key to identify pre-allocated position.
   */
  resolveOffset(key: ExifTag) {
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
 * Creates a directory with specified tag. This method only supports string
 * format tag, which is longer than 4 characters.
 * @param bytes Bytes to be written.
 * @param tag An exif entry which will be written.
 */
function writeDirectory(bytes: ArrayBufferView, tag: ExifEntry) {
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
  const string = tag.value;
  byteWriter.writeString(string);

  byteWriter.checkResolved();
}

class ConsoleLogger implements MetadataParserLogger {
  verbose = true;

  error(...args: Array<Object|string>) {
    console.error(...args);
  }

  log(...args: Array<Object|string>) {
    console.info(...args);
  }

  vlog(...args: Array<Object|string>) {
    console.info(...args);
  }
}

/**
 * Parses exif data bytes (with logging) and returns the parsed tags.
 * @param bytes Bytes to be read.
 * @return Tags.
 */
function parseExifData(bytes: ArrayBufferView): Record<ExifTag, ExifEntry> {
  const exifParser = new ExifParser(new ConsoleLogger());

  const tags = {} as Record<ExifTag, ExifEntry>;
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
  writeDirectory(data, {
    id: ExifTag.MAKE,   // Manufacturer Id.
    format: 2,          // String format.
    componentCount: 8,  // Length of value 'Manufact'.
    value: 'Manufact',
  });

  // Parse the exif data.
  const tags = parseExifData(data);

  // The parsed value should end in a null character.
  const parsedTag = tags[ExifTag.MAKE];
  assertEquals(9, parsedTag.componentCount);
  assertEquals('Manufact\0', parsedTag.value);
}
