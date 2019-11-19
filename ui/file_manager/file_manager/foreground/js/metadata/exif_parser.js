// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts(
    'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/foreground/js/metadata/exif_constants.js');

/** @final */
class ExifParser extends ImageParser {
  /**
   * @param {!MetadataParserLogger} parent Parent object.
   */
  constructor(parent) {
    super(parent, 'jpeg', /\.jpe?g$/i);
  }

  /**
   * @param {File} file File object to parse.
   * @param {!Object} metadata Metadata object for the file.
   * @param {function(!Object)} callback Callback to be called on success.
   * @param {function((Event|string))} errorCallback Error callback.
   */
  parse(file, metadata, callback, errorCallback) {
    this.requestSlice(file, callback, errorCallback, metadata, 0);
  }

  /**
   * @param {File} file File object to parse.
   * @param {function(!Object)} callback Callback to be called on success.
   * @param {function((Event|string))} errorCallback Error callback.
   * @param {!Object} metadata Metadata object.
   * @param {number} filePos Position to slice at.
   * @param {number=} opt_length Number of bytes to slice. By default 1 KB.
   */
  requestSlice(file, callback, errorCallback, metadata, filePos, opt_length) {
    // Read at least 1Kb so that we do not issue too many read requests.
    opt_length = Math.max(1024, opt_length || 0);

    const self = this;
    const reader = new FileReader();
    reader.onerror = errorCallback;
    reader.onload = () => {
      self.parseSlice(
          file, callback, errorCallback, metadata, filePos,
          /** @type{ArrayBuffer} */ (reader.result));
    };
    reader.readAsArrayBuffer(file.slice(filePos, filePos + opt_length));
  }

  /**
   * @param {File} file File object to parse.
   * @param {function(!Object)} callback Callback to be called on success.
   * @param {function((Event|string))} errorCallback Error callback.
   * @param {!Object} metadata Metadata object.
   * @param {number} filePos Position to slice at.
   * @param {ArrayBuffer} buf Buffer to be parsed.
   */
  parseSlice(file, callback, errorCallback, metadata, filePos, buf) {
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
        if (firstMark !== Exif.Mark.SOI) {
          throw new Error('Invalid file header: ' + firstMark.toString(16));
        }
      }

      const self = this;

      /**
       * @param {number=} opt_offset
       * @param {number=} opt_bytes
       */
      const reread = (opt_offset, opt_bytes) => {
        self.requestSlice(
            file, callback, errorCallback, metadata,
            filePos + br.tell() + (opt_offset || 0), opt_bytes);
      };

      while (true) {
        if (!br.canRead(4)) {
          // Cannot read the mark and the length, request a minimum-size slice.
          reread();
          return;
        }

        const mark = this.readMark(br);
        if (mark === Exif.Mark.SOS) {
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

        if (mark === Exif.Mark.EXIF) {
          this.parseExifSection(metadata, buf, br);
        } else if (ExifParser.isSOF_(mark)) {
          // The most reliable size information is encoded in the SOF section.
          br.seek(1, ByteReader.SEEK_CUR);  // Skip the precision byte.
          const height = br.readScalar(2);
          const width = br.readScalar(2);
          ExifParser.setImageSize(metadata, width, height);
          callback(metadata);  // We are done!
          return;
        }

        br.seek(nextSectionStart, ByteReader.SEEK_BEG);
      }
    } catch (e) {
      errorCallback(e.toString());
    }
  }

  /**
   * @private
   * @param {number} mark Mark to be checked.
   * @return {boolean} True if the mark is SOF.
   */
  static isSOF_(mark) {
    // There are 13 variants of SOF fragment format distinguished by the last
    // hex digit of the mark, but the part we want is always the same.
    if ((mark & ~0xF) !== Exif.Mark.SOF) {
      return false;
    }

    // If the last digit is 4, 8 or 12 it is not really a SOF.
    const type = mark & 0xF;
    return (type !== 4 && type !== 8 && type !== 12);
  }

  /**
   * @param {Object} metadata Metadata object.
   * @param {ArrayBuffer} buf Buffer to be parsed.
   * @param {ByteReader} br Byte reader to be used.
   */
  parseExifSection(metadata, buf, br) {
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
    if (order === Exif.Align.LITTLE) {
      br.setByteOrder(ByteReader.LITTLE_ENDIAN);
    } else if (order !== Exif.Align.BIG) {
      this.log('Invalid alignment value: ' + order.toString(16));
      return;
    }

    const tag = br.readScalar(2);
    if (tag !== Exif.Tag.TIFF) {
      this.log('Invalid TIFF tag: ' + tag.toString(16));
      return;
    }

    metadata.littleEndian = (order === Exif.Align.LITTLE);
    metadata.ifd = {
      image: {},
      thumbnail: {},
    };
    let directoryOffset = br.readScalar(4);

    // Image directory.
    this.vlog('Read image directory');
    br.seek(directoryOffset);
    directoryOffset = this.readDirectory(br, metadata.ifd.image);
    metadata.imageTransform = this.parseOrientation(metadata.ifd.image);

    // Thumbnail Directory chained from the end of the image directory.
    if (directoryOffset) {
      this.vlog('Read thumbnail directory');
      br.seek(directoryOffset);
      this.readDirectory(br, metadata.ifd.thumbnail);
      // If no thumbnail orientation is encoded, assume same orientation as
      // the primary image.
      metadata.thumbnailTransform =
          this.parseOrientation(metadata.ifd.thumbnail) ||
          metadata.imageTransform;
    }

    // EXIF Directory may be specified as a tag in the image directory.
    if (Exif.Tag.EXIFDATA in metadata.ifd.image) {
      this.vlog('Read EXIF directory');
      directoryOffset = metadata.ifd.image[Exif.Tag.EXIFDATA].value;
      br.seek(directoryOffset);
      metadata.ifd.exif = {};
      this.readDirectory(br, metadata.ifd.exif);
    }

    // GPS Directory may also be linked from the image directory.
    if (Exif.Tag.GPSDATA in metadata.ifd.image) {
      this.vlog('Read GPS directory');
      directoryOffset = metadata.ifd.image[Exif.Tag.GPSDATA].value;
      br.seek(directoryOffset);
      metadata.ifd.gps = {};
      this.readDirectory(br, metadata.ifd.gps);
    }

    // Thumbnail may be linked from the image directory.
    if (Exif.Tag.JPG_THUMB_OFFSET in metadata.ifd.thumbnail &&
        Exif.Tag.JPG_THUMB_LENGTH in metadata.ifd.thumbnail) {
      this.vlog('Read thumbnail image');
      br.seek(metadata.ifd.thumbnail[Exif.Tag.JPG_THUMB_OFFSET].value);
      metadata.thumbnailURL =
          br.readImage(metadata.ifd.thumbnail[Exif.Tag.JPG_THUMB_LENGTH].value);
    } else {
      this.vlog('Image has EXIF data, but no JPG thumbnail');
    }
  }

  /**
   * @param {Object} metadata Metadata object.
   * @param {number} width Width in pixels.
   * @param {number} height Height in pixels.
   */
  static setImageSize(metadata, width, height) {
    if (metadata.imageTransform && metadata.imageTransform.rotate90) {
      metadata.width = height;
      metadata.height = width;
    } else {
      metadata.width = width;
      metadata.height = height;
    }
  }

  /**
   * @param {ByteReader} br Byte reader to be used for reading.
   * @return {number} Mark value.
   */
  readMark(br) {
    return br.readScalar(2);
  }

  /**
   * @param {ByteReader} br Bye reader to be used for reading.
   * @return {number} Size of the mark at the current position.
   */
  readMarkLength(br) {
    // Length includes the 2 bytes used to store the length.
    return br.readScalar(2) - 2;
  }

  /**
   * @param {ByteReader} br Byte reader to be used for reading.
   * @param {Object<number, Object>} tags Map of tags to be written to.
   * @return {number} Directory offset.
   */
  readDirectory(br, tags) {
    const entryCount = br.readScalar(2);
    for (let i = 0; i < entryCount; i++) {
      const tagId = /** @type Exif.Tag<number> */ (br.readScalar(2));
      const tag = tags[tagId] = {id: tagId};
      tag.format = br.readScalar(2);
      tag.componentCount = br.readScalar(4);
      this.readTagValue(br, tag);
    }

    return br.readScalar(4);
  }

  /**
   * @param {ByteReader} br Byte reader to be used for reading.
   * @param {ExifEntry} tag Tag object.
   */
  readTagValue(br, tag) {
    const self = this;

    /**
     * @param {number} size
     * @param {function(number)=} opt_readFunction
     * @param {boolean=} opt_signed
     */
    function safeRead(size, opt_readFunction, opt_signed) {
      try {
        unsafeRead(size, opt_readFunction, opt_signed);
      } catch (ex) {
        self.log(
            'Error reading tag 0x' + tag.id.toString(16) + '/' + tag.format +
            ', size ' + tag.componentCount + '*' + size + ' ' +
            (ex.stack || '<no stack>') + ': ' + ex);
        tag.value = null;
      }
    }

    /**
     * @param {number} size
     * @param {function(number)=} opt_readFunction
     * @param {boolean=} opt_signed
     */
    function unsafeRead(size, opt_readFunction, opt_signed) {
      const readFunction = opt_readFunction || (size => {
                             return br.readScalar(size, opt_signed);
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
        tag.value = readFunction(size);
      } else {
        // Read multiple components into an array.
        tag.value = [];
        for (let i = 0; i < tag.componentCount; i++) {
          tag.value[i] = readFunction(size);
        }
      }

      if (totalSize > 4) {
        // Go back to the previous position if we had to jump to the data.
        br.popSeek();
      } else if (totalSize < 4) {
        // Otherwise, if the value wasn't exactly 4 bytes, skip over the
        // unread data.
        br.seek(4 - totalSize, ByteReader.SEEK_CUR);
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
          tag.value = String.fromCharCode(/** @type {number} */ (tag.value));
        } else {
          tag.value = String.fromCharCode.apply(
              null, /** @type{Array<number>} */ (tag.value));
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
   * @param {!ExifEntry} tag A tag to be validated and fixed.
   * @private
   */
  validateAndFixStringTag_(tag) {
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
   * @param {Object} ifd Exif property dictionary (image or thumbnail).
   * @return {Object} Orientation object.
   */
  parseOrientation(ifd) {
    if (ifd[Exif.Tag.ORIENTATION]) {
      const index = (ifd[Exif.Tag.ORIENTATION].value || 1) - 1;
      return {
        scaleX: ExifParser.SCALEX[index],
        scaleY: ExifParser.SCALEY[index],
        rotate90: ExifParser.ROTATE90[index]
      };
    }
    return null;
  }
}

/**
 * Map from the exif orientation value to the horizontal scale value.
 * @const {Array<number>}
 */
ExifParser.SCALEX = [1, -1, -1, 1, 1, 1, -1, -1];

/**
 * Map from the exif orientation value to the vertical scale value.
 * @const {Array<number>}
 */
ExifParser.SCALEY = [1, 1, -1, -1, -1, 1, 1, -1];

/**
 * Map from the exit orientation value to the rotation value.
 * @const {Array<number>}
 */
ExifParser.ROTATE90 = [0, 0, 0, 0, 1, 1, 1, 1];
