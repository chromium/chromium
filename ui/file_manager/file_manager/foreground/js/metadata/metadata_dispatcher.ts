// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ExifParser} from './exif_parser.js';
import {Id3Parser} from './id3_parser.js';
import {BmpParser, GifParser, IcoParser, PngParser, WebpParser} from './image_parsers.js';
import type {ParserMetadata} from './metadata_item.js';
import type {MetadataParser} from './metadata_parser.js';
import {type MetadataParserLogger} from './metadata_parser.js';
import {MpegParser} from './mpeg_parser.js';

// Helper function to type entries as FileEntry. We redefine it here because
// importing entry_utils.js has some transitive side effects that access objects
// not accessible in a shared worker.
function isFileEntry(entry: Entry): entry is FileEntry {
  return entry.isFile;
}

/**
 * Dispatches metadata requests to the correct parser.
 */
class MetadataDispatcher implements MetadataParserLogger {
  /**
   * Verbose logging for the dispatcher.
   *
   * Individual parsers also take this as their default verbosity setting.
   */
  readonly verbose = false;
  private parserInstances_: MetadataParser[];
  private parserRegexp_: RegExp;

  // Explicitly type this as a record so we can index into this object with a
  // string.
  private messageHandlers_: Record<string, Function> = {
    init: this.init_.bind(this),
    request: this.request_.bind(this),
  };

  /***
   * @param port Worker port.
   */
  constructor(private port_: WorkerGlobalScope|MessagePort) {
    this.port_.onmessage = this.onMessage.bind(this);

    const patterns = [];

    this.parserInstances_ = [];

    const parserClasses = [
      BmpParser,
      ExifParser,
      GifParser,
      IcoParser,
      Id3Parser,
      MpegParser,
      PngParser,
      WebpParser,
    ];

    for (const parserClass of parserClasses) {
      const parser = new parserClass(this);
      this.parserInstances_.push(parser);
      patterns.push(parser.urlFilter.source);
    }

    this.parserRegexp_ = new RegExp('(' + patterns.join('|') + ')', 'i');
  }

  /**
   * |init| message handler.
   */
  private init_() {
    // Inform our owner that we're done initializing.
    // If we need to pass more data back, we can add it to the param array.
    // TODO(cleanup): parserRegexp_ looks unused in content_metadata_provider
    // and in this file, too.
    this.postMessage('initialized', [this.parserRegexp_]);
    this.vlog('initialized with URL filter ' + this.parserRegexp_);
  }

  /**
   * |request| message handler.
   * @param fileURL File URL.
   */
  private request_(fileURL: string) {
    try {
      this.processOneFile(fileURL, (metadata: ParserMetadata) => {
        this.postMessage('result', [fileURL, metadata]);
      });
    } catch (ex) {
      this.error(fileURL, ex!);
    }
  }

  /**
   * Indicate to the caller that an operation has failed.
   *
   * No other messages relating to the failed operation should be sent.
   */
  error(...args: Array<object|string>) {
    // TODO(cleanup): Strictly type these arguments to the [url, step, cause]
    // format that ContentMetadataProvider expects.

    this.postMessage('error', args);
  }

  /**
   * Send a log message to the caller.
   *
   * Callers must not parse log messages for control flow.
   */
  log(...args: Array<object|string>) {
    this.postMessage('log', args);
  }

  /**
   * Send a log message to the caller only if this.verbose is true.
   */
  vlog(...args: Array<object|string>) {
    if (this.verbose) {
      this.log(...args);
    }
  }

  /**
   * Post a properly formatted message to the caller.
   * @param verb Message type descriptor.
   * @param args Arguments array.
   */
  postMessage(verb: string, args: any[]) {
    this.port_.postMessage({verb: verb, arguments: args});
  }

  /**
   * Message handler.
   * @param event Event object.
   */
  onMessage(event: MessageEvent) {
    const data = event.data;

    const handler = this.messageHandlers_[data.verb];
    if (handler instanceof Function) {
      handler.apply(this, data.arguments);
    } else {
      this.log('Unknown message from client: ' + data.verb, data);
    }
  }

  private detectFormat_(fileURL: string): MetadataParser|null {
    for (const parser of this.parserInstances_) {
      if (fileURL.match(parser.urlFilter)) {
        return parser;
      }
    }
    return null;
  }

  /**
   * @param fileURL File URL.
   * @param callback Completion callback.
   */
  async processOneFile(
      fileURL: string, callback: (metadata: ParserMetadata) => void) {
    // Step one, find the parser matching the url.
    const parser = this.detectFormat_(fileURL);
    if (!parser) {
      this.error(fileURL, 'detectFormat', 'unsupported format');
      return;
    }
    // Create the metadata object as early as possible so that we can
    // pass it with the error message.
    const metadata: ParserMetadata = parser.createDefaultMetadata();

    // Step two, turn the url into an entry.
    const entry = await new Promise<Entry>(
        (resolve, reject) => globalThis.webkitResolveLocalFileSystemURL(
            fileURL, resolve, reject));
    if (!isFileEntry(entry)) {
      this.error(fileURL, 'getEntry', 'url does not refer a file', metadata);
      return;
    }

    // Step three, turn the entry into a file.
    const file = await new Promise(entry.file.bind(entry));

    // Step four, parse the file.
    metadata.fileSize = file.size;
    try {
      parser.parse(
          file, metadata, callback,
          (error) => this.error(fileURL, 'parseContent', error));
    } catch (e) {
      this.error(fileURL, 'parseContent', (e as Error).stack!);
    }
  }
}

// This interface and the following self type assertion is needed as we
// currently use the same tsconfig to build this as with the rest of Files App.
// TODO(b/289003444): Use a separate tsconfig to build this file with webworker
// definitions, and then remove this interface and the following type assertion.
interface WorkerGlobalScope {
  onmessage: (ev: MessageEvent) => any;
  addEventListener(type: 'connect', listener: (ev: MessageEvent) => any): void;
  postMessage(message: any): void;
}

// Webworker spec says that the worker global object is called self.  That's
// a terrible name since we use it all over the chrome codebase to capture
// the 'this' keyword in lambdas.
const global = self as WorkerGlobalScope;

if (global.constructor.name === 'SharedWorkerGlobalScope') {
  global.addEventListener('connect', e => {
    const port = e.ports[0]!;
    new MetadataDispatcher(port);
    port.start();
  });
} else {
  // Non-shared worker.
  new MetadataDispatcher(global);
}
