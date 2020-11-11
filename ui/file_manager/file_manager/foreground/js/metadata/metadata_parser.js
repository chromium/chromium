// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @implements {MetadataParserLogger}
 */
class MetadataParser {
  /**
   * @param {!MetadataParserLogger} parent Parent object.
   * @param {string} type Parser type.
   * @param {!RegExp} urlFilter RegExp to match URLs.
   */
  constructor(parent, type, urlFilter) {
    /** @private @const {!MetadataParserLogger} */
    this.parent_ = parent;
    /** @public @const {string} */
    this.type = type;
    /** @public @const {!RegExp} */
    this.urlFilter = urlFilter;
    /** @public @const {boolean} */
    this.verbose = parent.verbose;
    /** @public {string} */
    this.mimeType = 'unknown';
  }

  /**
   * Output an error message.
   * @param {...(Object|string)} var_args Arguments.
   */
  error(var_args) {
    this.parent_.error.apply(this.parent_, arguments);
  }

  /**
   * Output a log message.
   * @param {...(Object|string)} var_args Arguments.
   */
  log(var_args) {
    this.parent_.log.apply(this.parent_, arguments);
  }

  /**
   * Output a log message if |verbose| flag is on.
   * @param {...(Object|string)} var_args Arguments.
   */
  vlog(var_args) {
    if (this.verbose) {
      this.parent_.log.apply(this.parent_, arguments);
    }
  }

  /**
   * @return {Object} Metadata object with the minimal set of properties.
   */
  createDefaultMetadata() {
    return {type: this.type, mimeType: this.mimeType};
  }

  /**
   * Utility function to read specified range of bytes from file
   * @param {File} file The file to read.
   * @param {number} begin Starting byte(included).
   * @param {number} end Last byte(excluded).
   * @param {function(File, ByteReader)} callback Callback to invoke.
   * @param {function(string)} onError Error handler.
   */
  static readFileBytes(file, begin, end, callback, onError) {
    const fileReader = new FileReader();
    fileReader.onerror = event => {
      onError(event.type);
    };
    fileReader.onloadend = () => {
      callback(
          file,
          new ByteReader(
              /** @type {ArrayBuffer} */ (fileReader.result)));
    };
    fileReader.readAsArrayBuffer(file.slice(begin, end));
  }
}

/**
 * Base class for image metadata parsers.
 */
class ImageParser extends MetadataParser {
  /**
   * @param {!MetadataParserLogger} parent Parent object.
   * @param {string} type Image type.
   * @param {!RegExp} urlFilter RegExp to match URLs.
   */
  constructor(parent, type, urlFilter) {
    super(parent, type, urlFilter);
    this.mimeType = 'image/' + this.type;
  }
}
