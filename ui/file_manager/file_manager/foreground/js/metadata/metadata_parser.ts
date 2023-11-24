// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ByteReader} from './byte_reader.js';

export interface MetadataParserLogger {
  /**
   * Verbose logging for the dispatcher.
   * Individual parsers also take this as their default verbosity setting.
   */
  verbose: boolean;

  /**
   * Indicate to the caller that an operation has failed.
   *
   * No other messages relating to the failed operation should be sent.
   */
  error(...args: Array<Object|string>): void;

  /**
   * Send a log message to the caller.
   *
   * Callers must not parse log messages for control flow.
   */
  log(...args: Array<Object|string>): void;

  /**
   * Send a log message to the caller only if this.verbose is true.
   */
  vlog(...args: Array<Object|string>): void;
}

export class MetadataParser implements MetadataParserLogger {
  readonly verbose: boolean;
  mimeType = 'unknown';

  /**
   * @param parent_ Parent object.
   * @param type_ Parser type.
   * @param urlFilter_ RegExp to match URLs.
   */
  constructor(
      private readonly parent_: MetadataParserLogger,
      public readonly type: string, public readonly urlFilter: RegExp) {
    this.verbose = parent_.verbose;
  }

  /**
   * Output an error message.
   */
  error(...args: Array<Object|string>) {
    this.parent_.error.apply(this.parent_, args);
  }

  /**
   * Output a log message.
   */
  log(...args: Array<Object|string>) {
    this.parent_.log.apply(this.parent_, args);
  }

  /**
   * Output a log message if |verbose| flag is on.
   */
  vlog(...args: Array<Object|string>) {
    if (this.verbose) {
      this.parent_.log.apply(this.parent_, args);
    }
  }

  /**
   * @return Metadata object with the minimal set of properties.
   */
  createDefaultMetadata(): {type: string, mimeType: string} {
    return {type: this.type, mimeType: this.mimeType};
  }

  /**
   * Utility function to read specified range of bytes from file
   * @param file The file to read.
   * @param begin Starting byte(included).
   * @param end Last byte(excluded).
   * @param callback Callback to invoke.
   * @param onError Error handler.
   */
  static readFileBytes(
      file: File, begin: number, end: number,
      callback: (file: File, byteReader: ByteReader) => void,
      onError: (s: string) => void) {
    const fileReader = new FileReader();
    fileReader.onerror = event => {
      onError(event.type);
    };
    fileReader.onloadend = () => {
      callback(file, new ByteReader(fileReader.result as ArrayBuffer));
    };
    fileReader.readAsArrayBuffer(file.slice(begin, end));
  }

  // TODO(cleanup): Add parse() abstract method which fills in a ParserMetadata.
}

/**
 * Base class for image metadata parsers.
 */
export class ImageParser extends MetadataParser {
  /**
   * @param parent Parent object.
   * @param type Image type.
   * @param urlFilter RegExp to match URLs.
   */
  constructor(parent: MetadataParserLogger, type: string, urlFilter: RegExp) {
    super(parent, type, urlFilter);
    this.mimeType = 'image/' + this.type;
  }
}
