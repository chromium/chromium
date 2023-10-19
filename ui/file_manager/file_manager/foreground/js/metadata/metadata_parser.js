// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MetadataParserLogger} from '../../../externs/metadata_worker_window.js';

import {ByteReader} from './byte_reader.js';

/**
 * @implements {MetadataParserLogger}
 */
export class MetadataParser {
  /**
   * @param {!MetadataParserLogger} parent Parent object.
   * @param {string} type Parser type.
   * @param {!RegExp} urlFilter RegExp to match URLs.
   */
  constructor(parent, type, urlFilter) {
    /** @private @const @type {!MetadataParserLogger} */
    this.parent_ = parent;
    /** @public @const @type {string} */
    this.type = type;
    /** @public @const @type {!RegExp} */
    this.urlFilter = urlFilter;
    /** @public @const @type {boolean} */
    // @ts-ignore: error TS2339: Property 'verbose' does not exist on type
    // 'MetadataParserLogger'.
    this.verbose = parent.verbose;
    /** @public @type {string} */
    this.mimeType = 'unknown';
  }

  /**
   * Output an error message.
   * @param {...(Object|string)} var_args Arguments.
   */
  // @ts-ignore: error TS6133: 'var_args' is declared but its value is never
  // read.
  error(var_args) {
    // @ts-ignore: error TS2345: Argument of type 'IArguments' is not assignable
    // to parameter of type '[var_args: string | Object | undefined]'.
    this.parent_.error.apply(this.parent_, arguments);
  }

  /**
   * Output a log message.
   * @param {...(Object|string)} var_args Arguments.
   */
  // @ts-ignore: error TS6133: 'var_args' is declared but its value is never
  // read.
  log(var_args) {
    // @ts-ignore: error TS2345: Argument of type 'IArguments' is not assignable
    // to parameter of type '[var_args: string | Object | undefined]'.
    this.parent_.log.apply(this.parent_, arguments);
  }

  /**
   * Output a log message if |verbose| flag is on.
   * @param {...(Object|string)} var_args Arguments.
   */
  // @ts-ignore: error TS6133: 'var_args' is declared but its value is never
  // read.
  vlog(var_args) {
    if (this.verbose) {
      // @ts-ignore: error TS2345: Argument of type 'IArguments' is not
      // assignable to parameter of type '[var_args: string | Object |
      // undefined]'.
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
   * @param {function(File, ByteReader):void} callback Callback to invoke.
   * @param {function(string):void} onError Error handler.
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
export class ImageParser extends MetadataParser {
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
