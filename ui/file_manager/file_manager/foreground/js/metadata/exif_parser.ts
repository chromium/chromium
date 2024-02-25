// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ByteOrder, ByteReader, SeekOrigin} from './byte_reader.js';
import {ExifAlign, type ExifEntry, ExifMark, ExifTag} from './exif_constants.js';
import type {ImageTransformation, ParserMetadata} from './metadata_item.js';
import {ImageParser, type MetadataParserLogger} from './metadata_parser.js';


/** @final */
export class ExifParser extends ImageParser {
  /**
   * @param parent Parent object.
   */
  constructor(parent: MetadataParserLogger) {
    super(parent, 'jpeg', /\.jpe?g$/i);
  }

  /**
   * @param file File object to parse.
   * @param metadata Metadata object for the file.
   * @param callback Callback to be called on success.
   * @param errorCallback Error callback.
   */
  parse(
      file: File,
      metadata: ParserMetadata,
      callback: (metadata: ParserMetadata) => void,
      errorCallback: (error: Event | string) => void) {
    this.requestSlice(file, callback, errorCallback, metadata, 0);
  }

  /**
   * @param file File object to parse.
   * @param callback Callback to be called on success.
   * @param errorCallback Error callback.
   * @param metadata Metadata object.
   * @param filePos Position to slice at.
   * @param length Number of bytes to slice. By default 1 KB.
   */
  requestSlice(
      file: File,
      callback: (metadata: ParserMetadata) => void,
      errorCallback: (error: Event | string) => void,
      metadata: ParserMetadata,
      filePos: number,
      length?: number) {
    // Read at least 1Kb so that we do not issue too many read requests.
    length = Math.max(1024, length || 0);

    const self = this;
    const reader = new FileReader();
    reader.onerror = errorCallback;
    reader.onload = () => {
      self.parseSlice(
          file, callback, errorCallback, metadata, filePos,
          reader.result as ArrayBuffer);
    };
    reader.readAsArrayBuffer(file.slice(filePos, filePos + length));
  }

  /**
   * @param file File object to parse.
   * @param callback Callback to be called on success.
   * @param errorCallback Error callback.
   * @param metadata Metadata object.
   * @param filePos Position to slice at.
   * @param buf Buffer to be parsed.
   */
  parseSlice(
      file: File,
      callback: (metadata: ParserMetadata) => void,
      errorCallback: (error: Event | string) => void,
      metadata: ParserMetadata,
      filePos: number,
      buf: ArrayBuffer) {
    try {
      const br = new ByteReader(buf);

      if (!br.canRead(4)) {
        // We never ask for less than 4 bytes. This can only mean we reached
        // EOF.
        throw new Error('Unexpected EOF @' + (filePos + buf.byteLength));
      }

      if (filePos === 0) {
        // First slice, check for the SOI mark.
        const firstMark = this.readMark(br);
        if (firstMark !== ExifMark.SOI) {
          throw new Error('Invalid file header: ' + firstMark.toString(16));
        }
      }

      const self = this;

      /**
       */
      const reread = (offset?: number, bytes?: number) => {
        self.requestSlice(
            file, callback, errorCallback, metadata,
            filePos + br.tell() + (offset || 0), bytes);
      };

      while (true) {
        if (!br.canRead(4)) {
          // Cannot read the mark and the length, request a minimum-size slice.
          reread();
          return;
        }

        const mark = this.readMark(br);
        if (mark === ExifMark.SOS) {
          throw new Error('SOS marker found before SOF');
        }

        const markLength = this.readMarkLength(br);

        const nextSectionStart = br.tell() + markLength;
        if (!br.canRead(markLength)) {
          // Get the entire section.
          if (filePos + br.tell() + markLength > file.size) {
            throw new Error(
                'Invalid section length @' + (filePos + br.tell() - 2));
          }
          reread(-4, markLength + 4);
          return;
        }

        if (mark === ExifMark.EXIF) {
          this.parseExifSection(metadata, buf, br);
        } else if (ExifParser.isSof_(mark)) {
          // The most reliable size information is encoded in the SOF section.
          br.seek(1, SeekOrigin.SEEK_CUR);  // Skip the precision byte.
          const height = br.readScalar(2);
          const width = br.readScalar(2);
          ExifParser.setImageSize(metadata, width, height);
          callback(metadata);  // We are done!
          return;
        }

        br.seek(nextSectionStart, SeekOrigin.SEEK_BEG);
      }
    } catch (e) {
      errorCallback(e!.toString());
    }
  }

  /**
   * @param mark Mark to be checked.
   * @return True if the mark is SOF (Start of Frame).
   */
  private static isSof_(mark: number): boolean {
    // There are 13 variants of SOF fragment format distinguished by the last
    // hex digit of the mark, but the part we want is always the same.
    if ((mark & ~0xF) !== ExifMark.SOF) {
      return false;
    }

    // If the last digit is 4, 8 or 12 it is not really a SOF.
    const type = mark & 0xF;
    return (type !== 4 && type !== 8 && type !== 12);
  }

  /**
   * @param metadata Metadata object.
   * @param buf Buffer to be parsed.
   * @param br Byte reader to be used.
   */
  parseExifSection(metadata: ParserMetadata, buf: ArrayBuffer, br: ByteReader) {
    const magic = br.readString(6);
    if (magic !== 'Exif\0\0') {
      // Some JPEG files may have sections marked with EXIF_MARK_EXIF
      // but containing something else (e.g. XML text). Ignore such sections.
      this.vlog('Invalid EXIF magic: ' + magic + br.readString(100));
      return;
    }

    // Offsets inside the EXIF block are based after the magic string.
    // Create a new ByteReader based on the current position to make offset
    // calculations simpler.
    br = new ByteReader(buf, br.tell());

    const order = br.readScalar(2);
    if (order === ExifAlign.LITTLE) {
      br.setByteOrder(ByteOrder.LITTLE_ENDIAN);
    } else if (order !== ExifAlign.BIG) {
      this.log('Invalid alignment value: ' + order.toString(16));
      return;
    }

    const tag = br.readScalar(2);
    if (tag !== ExifTag.TIFF) {
      this.log('Invalid TIFF tag: ' + tag.toString(16));
      return;
    }

    metadata.littleEndian = (order === ExifAlign.LITTLE);
    metadata.ifd = {
      image: {} as Record<ExifTag, ExifEntry>,
      thumbnail: {} as Record<ExifTag, ExifEntry>,
    };
    let directoryOffset = br.readScalar(4);

    // Image directory.
    this.vlog('Read image directory');
    br.seek(directoryOffset);
    directoryOffset = this.readDirectory(br, metadata.ifd.image!);
    metadata.imageTransform = this.parseOrientation(metadata.ifd.image!);

    // Thumbnail Directory chained from the end of the image directory.
    if (directoryOffset) {
      this.vlog('Read thumbnail directory');
      br.seek(directoryOffset);
      this.readDirectory(br, metadata.ifd.thumbnail!);
      // If no thumbnail orientation is encoded, assume same orientation as
      // the primary image.
      metadata.thumbnailTransform =
          this.parseOrientation(metadata.ifd.thumbnail!) ||
          metadata.imageTransform;
    }

    // EXIF Directory may be specified as a tag in the image directory.
    if (ExifTag.EXIFDATA in metadata.ifd.image!) {
      this.vlog('Read EXIF directory');
      directoryOffset = metadata.ifd.image[ExifTag.EXIFDATA].value;
      br.seek(directoryOffset);
      metadata.ifd.exif = {} as Record<ExifTag, ExifEntry>;
      this.readDirectory(br, metadata.ifd.exif);
    }

    // GPS Directory may also be linked from the image directory.
    if (ExifTag.GPSDATA in metadata.ifd.image!) {
      this.vlog('Read GPS directory');
      directoryOffset = metadata.ifd.image[ExifTag.GPSDATA].value;
      br.seek(directoryOffset);
      metadata.ifd.gps = {} as Record<ExifTag, ExifEntry>;
      this.readDirectory(br, metadata.ifd.gps);
    }

    // Thumbnail may be linked from the image directory.
    if (ExifTag.JPG_THUMB_OFFSET in metadata.ifd.thumbnail! &&
        ExifTag.JPG_THUMB_LENGTH in metadata.ifd.thumbnail) {
      this.vlog('Read thumbnail image');
      br.seek(metadata.ifd.thumbnail[ExifTag.JPG_THUMB_OFFSET].value);
      metadata.thumbnailURL =
          br.readImage(metadata.ifd.thumbnail[ExifTag.JPG_THUMB_LENGTH].value);
    } else {
      this.vlog('Image has EXIF data, but no JPG thumbnail');
    }
  }

  /**
   * @param metadata Metadata object.
   * @param width Width in pixels.
   * @param height Height in pixels.
   */
  static setImageSize(metadata: ParserMetadata, width: number, height: number) {
    if (metadata.imageTransform && metadata.imageTransform.rotate90) {
      metadata.width = height;
      metadata.height = width;
    } else {
      metadata.width = width;
      metadata.height = height;
    }
  }

  /**
   * @param br Byte reader to be used for reading.
   * @return Mark value.
   */
  readMark(br: ByteReader): number {
    return br.readScalar(2);
  }

  /**
   * @param br Bye reader to be used for reading.
   * @return Size of the mark at the current position.
   */
  readMarkLength(br: ByteReader): number {
    // Length includes the 2 bytes used to store the length.
    return br.readScalar(2) - 2;
  }

  /**
   * @param br Byte reader to be used for reading.
   * @param tags Map of tags to be written to.
   * @return Directory offset.
   */
  readDirectory(br: ByteReader, tags: Record<ExifTag, ExifEntry>): number {
    const entryCount = br.readScalar(2);
    for (let i = 0; i < entryCount; i++) {
      const tagId = br.readScalar(2) as ExifTag;
      const tag: ExifEntry = tags[tagId] =
          {id: tagId, format: 0, componentCount: 0, value: undefined};
      tag.format = br.readScalar(2);
      tag.componentCount = br.readScalar(4);
      this.readTagValue(br, tag);
    }

    return br.readScalar(4);
  }

  /**
   * @param br Byte reader to be used for reading.
   * @param tag Tag object.
   */
  readTagValue(br: ByteReader, tag: ExifEntry) {
    const self = this;

    function safeRead(
        size: (1|2|4|8),
        readFunction?: () => [number, number],
        signed?: boolean) {
      try {
        unsafeRead(size, readFunction, signed);
      } catch (ex) {
        self.log(
            'Error reading tag 0x' + tag.id.toString(16) + '/' + tag.format +
            ', size ' + tag.componentCount + '*' + size + ' ' +
            ((ex as {stack: string}).stack || '<no stack>') + ': ' + ex);
        tag.value = null;
      }
    }

    function unsafeRead(
        size: (1|2|4|8),
        readFunction?: () => [number, number],
        signed?: boolean) {
      const reader = readFunction || ((size: (1|2|4)) => {
                             // Every time this function is called with `size` =
                             // 8, `readFunction` is also passed, so
                             // readScalar is only ever called with `size` = 1,2
                             // or 4.
                             return br.readScalar(size, signed);
                           });

      const totalSize = tag.componentCount * size;
      if (totalSize < 1) {
        // This is probably invalid exif data, skip it.
        tag.componentCount = 1;
        tag.value = br.readScalar(4);
        return;
      }

      if (totalSize > 4) {
        // If the total size is > 4, the next 4 bytes will be a pointer to the
        // actual data.
        br.pushSeek(br.readScalar(4));
      }

      if (tag.componentCount === 1) {
        tag.value = reader(size as (1|2|4));
      } else {
        // Read multiple components into an array.
        tag.value = [];
        for (let i = 0; i < tag.componentCount; i++) {
          tag.value[i] = reader(size as (1|2|4));
        }
      }

      if (totalSize > 4) {
        // Go back to the previous position if we had to jump to the data.
        br.popSeek();
      } else if (totalSize < 4) {
        // Otherwise, if the value wasn't exactly 4 bytes, skip over the
        // unread data.
        br.seek(4 - totalSize, SeekOrigin.SEEK_CUR);
      }
    }

    switch (tag.format) {
      case 1:  // Byte
      case 7:  // Undefined
        safeRead(1);
        break;

      case 2:  // String
        safeRead(1);
        if (tag.componentCount === 0) {
          tag.value = '';
        } else if (tag.componentCount === 1) {
          tag.value = String.fromCharCode(tag.value);
        } else {
          tag.value = String.fromCharCode.apply(null, tag.value);
        }
        this.validateAndFixStringTag_(tag);
        break;

      case 3:  // Short
        safeRead(2);
        break;

      case 4:  // Long
        safeRead(4);
        break;

      case 9:  // Signed Long
        safeRead(4, undefined, true);
        break;

      case 5:  // Rational
        safeRead(8, () => {
          return [br.readScalar(4), br.readScalar(4)];
        });
        break;

      case 10:  // Signed Rational
        safeRead(8, () => {
          return [br.readScalar(4, true), br.readScalar(4, true)];
        });
        break;

      default:  // ???
        this.vlog(
            'Unknown tag format 0x' + Number(tag.id).toString(16) + ': ' +
            tag.format);
        safeRead(4);
        break;
    }

    this.vlog(
        'Read tag: 0x' + tag.id.toString(16) + '/' + tag.format + ': ' +
        tag.value);
  }

  /**
   * Validates string tag value, and fix it if necessary.
   * @param tag A tag to be validated and fixed.
   */
  private validateAndFixStringTag_(tag: ExifEntry) {
    if (tag.format === 2) {  // string
      // String should end with null character.
      if (tag.value.charAt(tag.value.length - 1) !== '\0') {
        tag.value += '\0';
        tag.componentCount = tag.value.length;
        this.vlog(
            'Appended missing null character at the end of tag 0x' +
            tag.id.toString(16) + '/' + tag.format);
      }
    }
  }

  /**
   * Transform exif-encoded orientation into a set of parameters compatible with
   * CSS and canvas transforms (scaleX, scaleY, rotation).
   *
   * @param ifd Exif property dictionary (image or thumbnail).
   * @return Orientation object.
   */
  parseOrientation(ifd: Record<ExifTag, ExifEntry>)
      : ImageTransformation|undefined {
    if (ifd[ExifTag.ORIENTATION]) {
      const index = (ifd[ExifTag.ORIENTATION].value || 1) - 1;
      return {
        scaleX: SCALEX[index]!,
        scaleY: SCALEY[index]!,
        rotate90: ROTATE90[index]!,
      };
    }
    return undefined;
  }
}

/**
 * Map from the exif orientation value to the horizontal scale value.
 */
const SCALEX = [1, -1, -1, 1, 1, 1, -1, -1];

/**
 * Map from the exif orientation value to the vertical scale value.
 */
const SCALEY = [1, 1, -1, -1, -1, 1, 1, -1];

/**
 * Map from the exif orientation value to the rotation value.
 */
const ROTATE90 = [0, 0, 0, 0, 1, 1, 1, 1];
