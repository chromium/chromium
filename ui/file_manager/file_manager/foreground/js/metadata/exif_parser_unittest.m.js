// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertTrue} from 'chrome://test/chai_assert.js';
import {ByteWriter} from '../../../../gallery/js/image_editor/exif_encoder.m.js';
import {ByteReader} from './byte_reader.m.js';
import * as wrappedExif from './exif_constants.m.js';
import {ExifParser} from './exif_parser.m.js';
const {Exif} = wrappedExif;
import {ExifEntry} from '../../../../externs/exif_entry.m.js';
import {MetadataParserLogger} from '../../../../externs/metadata_worker_window.m.js';

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
