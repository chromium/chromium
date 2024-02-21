// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ByteReader} from './byte_reader.js';
import {ByteOrder} from './byte_reader.js';
import type {ParserMetadata} from './metadata_item.js';
import {ImageParser, MetadataParser, type MetadataParserLogger} from './metadata_parser.js';

/**
 * Base class for image metadata parsers that only need to look at a short
 * fragment at the start of the file.
 */
export abstract class SimpleImageParser extends ImageParser {
  /**
   * @param parent Parent object.
   * @param type Image type.
   * @param urlFilter RegExp to match URLs.
   * @param headerSize Size of header.
   */
  constructor(
      parent: MetadataParserLogger, type: string, urlFilter: RegExp,
      public readonly headerSize: number) {
    super(parent, type, urlFilter);
  }

  /**
   * @param file File to be parsed.
   * @param metadata Metadata object of the file.
   * @param callback Success callback.
   * @param errorCallback Error callback.
   */
  parse(
      file: File, metadata: ParserMetadata,
      callback: (metadata: ParserMetadata) => void,
      errorCallback: (error: string) => void) {
    MetadataParser.readFileBytes(file, 0, this.headerSize)
        .then(byteReader => {
          this.parseHeader(metadata, byteReader);
          callback(metadata);
        })
        .catch((e: unknown) => {
          errorCallback(e!.toString());
        });
  }

  /**
   * Parse header of an image. Inherited class must implement this.
   * @param metadata Dictionary to store the parsed metadata.
   * @param byteReader Reader for header binary data.
   */
  abstract parseHeader(metadata: ParserMetadata, byteReader: ByteReader): void;
}

/**
 * Parser for the header of png files.
 */
export class PngParser extends SimpleImageParser {
  constructor(parent: MetadataParserLogger) {
    super(parent, 'png', /\.png$/i, 24);
  }

  override parseHeader(metadata: ParserMetadata, br: ByteReader) {
    br.setByteOrder(ByteOrder.BIG_ENDIAN);

    const signature = br.readString(8);
    if (signature !== '\x89PNG\x0D\x0A\x1A\x0A') {
      throw new Error('Invalid PNG signature: ' + signature);
    }

    br.seek(12);
    const ihdr = br.readString(4);
    if (ihdr !== 'IHDR') {
      throw new Error('Missing IHDR chunk');
    }

    metadata.width = br.readScalar(4);
    metadata.height = br.readScalar(4);
  }
}

/**
 * Parser for the header of bmp files.
 */
export class BmpParser extends SimpleImageParser {
  constructor(parent: MetadataParserLogger) {
    super(parent, 'bmp', /\.bmp$/i, 28);
  }

  override parseHeader(metadata: ParserMetadata, br: ByteReader) {
    br.setByteOrder(ByteOrder.LITTLE_ENDIAN);

    const signature = br.readString(2);
    if (signature !== 'BM') {
      throw new Error('Invalid BMP signature: ' + signature);
    }

    br.seek(18);
    metadata.width = br.readScalar(4);
    metadata.height = br.readScalar(4);
  }
}

/**
 * Parser for the header of gif files.
 */
export class GifParser extends SimpleImageParser {
  constructor(parent: MetadataParserLogger) {
    super(parent, 'gif', /\.Gif$/i, 10);
  }

  override parseHeader(metadata: ParserMetadata, br: ByteReader) {
    br.setByteOrder(ByteOrder.LITTLE_ENDIAN);

    const signature = br.readString(6);
    if (!signature.match(/GIF8(7|9)a/)) {
      throw new Error('Invalid GIF signature: ' + signature);
    }

    metadata.width = br.readScalar(2);
    metadata.height = br.readScalar(2);
  }
}

/**
 * Parser for the header of webp files.
 */
export class WebpParser extends SimpleImageParser {
  constructor(parent: MetadataParserLogger) {
    super(parent, 'webp', /\.webp$/i, 30);
  }

  override parseHeader(metadata: ParserMetadata, br: ByteReader) {
    br.setByteOrder(ByteOrder.LITTLE_ENDIAN);

    const riffSignature = br.readString(4);
    if (riffSignature !== 'RIFF') {
      throw new Error('Invalid RIFF signature: ' + riffSignature);
    }

    br.seek(8);
    const webpSignature = br.readString(4);
    if (webpSignature !== 'WEBP') {
      throw new Error('Invalid WEBP signature: ' + webpSignature);
    }

    const chunkFormat = br.readString(4);
    switch (chunkFormat) {
      // VP8 lossy bitstream format.
      case 'VP8 ':
        br.seek(23);
        const lossySignature = br.readScalar(2) | (br.readScalar(1) << 16);
        if (lossySignature !== 0x2a019d) {
          throw new Error(
              'Invalid VP8 lossy bitstream signature: ' + lossySignature);
        }
        {
          const dimensionBits = br.readScalar(4);
          metadata.width = dimensionBits & 0x3fff;
          metadata.height = (dimensionBits >> 16) & 0x3fff;
        }
        break;

      // VP8 lossless bitstream format.
      case 'VP8L':
        br.seek(20);
        const losslessSignature = br.readScalar(1);
        if (losslessSignature !== 0x2f) {
          throw new Error(
              'Invalid VP8 lossless bitstream signature: ' + losslessSignature);
        }
        {
          const dimensionBits = br.readScalar(4);
          metadata.width = (dimensionBits & 0x3fff) + 1;
          metadata.height = ((dimensionBits >> 14) & 0x3fff) + 1;
        }
        break;

      // VP8 extended file format.
      case 'VP8X':
        br.seek(24);
        // Read 24-bit value. ECMAScript assures left-to-right evaluation order.
        metadata.width = (br.readScalar(2) | (br.readScalar(1) << 16)) + 1;
        metadata.height = (br.readScalar(2) | (br.readScalar(1) << 16)) + 1;
        break;

      default:
        throw new Error('Invalid chunk format: ' + chunkFormat);
    }
  }
}

/**
 * Parser for the header of .ico icon files.
 */
export class IcoParser extends SimpleImageParser {
  constructor(parent: MetadataParserLogger) {
    super(parent, 'ico', /\.ico$/i, 8);
  }

  override parseHeader(metadata: ParserMetadata, byteReader: ByteReader) {
    byteReader.setByteOrder(ByteOrder.LITTLE_ENDIAN);

    const signature = byteReader.readString(4);
    if (signature !== '\x00\x00\x00\x01') {
      throw new Error('Invalid ICO signature: ' + signature);
    }

    byteReader.seek(2);
    metadata.width = byteReader.readScalar(1);
    metadata.height = byteReader.readScalar(1);
  }
}
