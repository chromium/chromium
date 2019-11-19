// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Creates a directory with specified tag. This method only supports string
 * format tag, which is longer than 4 characters.
 * @param {!TypedArray} bytes Bytes to be written.
 * @param {!ExifEntry} tag An exif entry which will be written.
 */
function writeDirectory_(bytes, tag) {
  assertEquals(2, tag.format);
  assertTrue(tag.componentCount > 4);

  let byteWriter = new ByteWriter(bytes.buffer, 0);
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

  let tags = {};
  let byteReader = new ByteReader(bytes.buffer);
  assertEquals(0, exifParser.readDirectory(byteReader, tags));
  return tags;
}

/**
 * Tests that parsed exif strings have a null character termination.
 */
function testWithoutNullCharacterTermination() {
  // Create exif with a value that does not end with null character.
  let data = new Uint8Array(0x10000);
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
