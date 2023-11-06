// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MetadataParserLogger} from '../../../externs/metadata_worker_window.js';

import {ExifParser} from './exif_parser.js';
import {Id3Parser} from './id3_parser.js';
import {BmpParser, GifParser, IcoParser, PngParser, WebpParser} from './image_parsers.js';
import {MetadataParser} from './metadata_parser.js';
import {MpegParser} from './mpeg_parser.js';

/**
 * Dispatches metadata requests to the correct parser.
 * @implements {MetadataParserLogger}
 */
class MetadataDispatcher {
  /***
   * @param {Object} port Worker port.
   */
  constructor(port) {
    this.port_ = port;
    // @ts-ignore: error TS2339: Property 'onmessage' does not exist on type
    // 'Object'.
    this.port_.onmessage = this.onMessage.bind(this);

    /**
     * Verbose logging for the dispatcher.
     *
     * Individual parsers also take this as their default verbosity setting.
     * @public @type {boolean}
     */
    this.verbose = false;

    const patterns = [];

    this.parserInstances_ = [];

    /** @type {!Array<function(new:MetadataParser, !MetadataParserLogger)>} */
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

    this.messageHandlers_ = {
      init: this.init_.bind(this),
      request: this.request_.bind(this),
    };
  }

  /**
   * |init| message handler.
   * @private
   */
  init_() {
    // Inform our owner that we're done initializing.
    // If we need to pass more data back, we can add it to the param array.
    this.postMessage('initialized', [this.parserRegexp_]);
    this.vlog('initialized with URL filter ' + this.parserRegexp_);
  }

  /**
   * |request| message handler.
   * @param {string} fileURL File URL.
   * @private
   */
  request_(fileURL) {
    try {
      // @ts-ignore: error TS7006: Parameter 'metadata' implicitly has an 'any'
      // type.
      this.processOneFile(fileURL, function callback(metadata) {
        // @ts-ignore: error TS2683: 'this' implicitly has type 'any' because it
        // does not have a type annotation.
        this.postMessage('result', [fileURL, metadata]);
      }.bind(this));
    } catch (ex) {
      // @ts-ignore: error TS2345: Argument of type 'unknown' is not assignable
      // to parameter of type 'string | Object'.
      this.error(fileURL, ex);
    }
  }

  /**
   * Indicate to the caller that an operation has failed.
   *
   * No other messages relating to the failed operation should be sent.
   * @param {...(Object|string)} var_args Arguments.
   */
  // @ts-ignore: error TS6133: 'var_args' is declared but its value is never
  // read.
  error(var_args) {
    // @ts-ignore: error TS2345: Argument of type 'IArguments' is not assignable
    // to parameter of type 'unknown[]'.
    const ary = Array.apply(null, arguments);
    // @ts-ignore: error TS2345: Argument of type 'unknown[]' is not assignable
    // to parameter of type 'Object[]'.
    this.postMessage('error', ary);
  }

  /**
   * Send a log message to the caller.
   *
   * Callers must not parse log messages for control flow.
   * @param {...(Object|string)} var_args Arguments.
   */
  // @ts-ignore: error TS6133: 'var_args' is declared but its value is never
  // read.
  log(var_args) {
    // @ts-ignore: error TS2345: Argument of type 'IArguments' is not assignable
    // to parameter of type 'unknown[]'.
    const ary = Array.apply(null, arguments);
    // @ts-ignore: error TS2345: Argument of type 'unknown[]' is not assignable
    // to parameter of type 'Object[]'.
    this.postMessage('log', ary);
  }

  /**
   * Send a log message to the caller only if this.verbose is true.
   * @param {...(Object|string)} var_args Arguments.
   */
  // @ts-ignore: error TS6133: 'var_args' is declared but its value is never
  // read.
  vlog(var_args) {
    if (this.verbose) {
      // @ts-ignore: error TS2345: Argument of type 'IArguments' is not
      // assignable to parameter of type '(string | Object)[]'.
      this.log.apply(this, arguments);
    }
  }

  /**
   * Post a properly formatted message to the caller.
   * @param {string} verb Message type descriptor.
   * @param {Array<Object>} args Arguments array.
   */
  postMessage(verb, args) {
    // @ts-ignore: error TS2339: Property 'postMessage' does not exist on type
    // 'Object'.
    this.port_.postMessage({verb: verb, arguments: args});
  }

  /**
   * Message handler.
   * @param {Event} event Event object.
   */
  onMessage(event) {
    // @ts-ignore: error TS2339: Property 'data' does not exist on type 'Event'.
    const data = event.data;

    if (this.messageHandlers_.hasOwnProperty(data.verb)) {
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'any' can't be used to index type '{ init: () =>
      // void; request: (fileURL: string) => void; }'.
      this.messageHandlers_[data.verb].apply(this, data.arguments);
    } else {
      this.log('Unknown message from client: ' + data.verb, data);
    }
  }

  /**
   * @param {string} fileURL File URL.
   * @param {function(Object):void} callback Completion callback.
   */
  processOneFile(fileURL, callback) {
    const self = this;
    let currentStep = -1;

    /**
     * @param {...*} var_args Arguments.
     */
    // @ts-ignore: error TS6133: 'var_args' is declared but its value is never
    // read.
    function nextStep(var_args) {
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      self.vlog('nextStep: ' + steps[currentStep + 1].name);
      // @ts-ignore: error TS2684: The 'this' context of type '((entry: any,
      // parser: any) => void) | undefined' is not assignable to method's 'this'
      // of type '(this: this, entry?: any, parser?: any) => void'.
      steps[++currentStep].apply(self, arguments);
    }

    // @ts-ignore: error TS7034: Variable 'metadata' implicitly has type 'any'
    // in some locations where its type cannot be determined.
    let metadata;

    /**
     * @param {*} err An error.
     * @param {string=} opt_stepName Step name.
     */
    function onError(err, opt_stepName) {
      self.error(
          // @ts-ignore: error TS2532: Object is possibly 'undefined'.
          fileURL, opt_stepName || steps[currentStep].name, err.toString(),
          // @ts-ignore: error TS7005: Variable 'metadata' implicitly has an
          // 'any' type.
          metadata);
    }

    const steps = [
      // Step one, find the parser matching the url.
      function detectFormat() {
        for (let i = 0; i != self.parserInstances_.length; i++) {
          const parser = self.parserInstances_[i];
          // @ts-ignore: error TS18048: 'parser' is possibly 'undefined'.
          if (fileURL.match(parser.urlFilter)) {
            // Create the metadata object as early as possible so that we can
            // pass it with the error message.
            // @ts-ignore: error TS18048: 'parser' is possibly 'undefined'.
            metadata = parser.createDefaultMetadata();
            nextStep(parser);
            return;
          }
        }
        onError('unsupported format');
      },

      // Step two, turn the url into an entry.
      // @ts-ignore: error TS7006: Parameter 'parser' implicitly has an 'any'
      // type.
      function getEntry(parser) {
        // @ts-ignore: error TS7006: Parameter 'entry' implicitly has an 'any'
        // type.
        globalThis.webkitResolveLocalFileSystemURL(fileURL, entry => {
          nextStep(entry, parser);
        }, onError);
      },

      // Step three, turn the entry into a file.
      // @ts-ignore: error TS7006: Parameter 'parser' implicitly has an 'any'
      // type.
      function getFile(entry, parser) {
        // @ts-ignore: error TS7006: Parameter 'file' implicitly has an 'any'
        // type.
        entry.file(file => {
          nextStep(file, parser);
        }, onError);
      },

      // Step four, parse the file content.
      // @ts-ignore: error TS7006: Parameter 'parser' implicitly has an 'any'
      // type.
      function parseContent(file, parser) {
        // @ts-ignore: error TS7005: Variable 'metadata' implicitly has an 'any'
        // type.
        metadata.fileSize = file.size;
        try {
          // @ts-ignore: error TS7005: Variable 'metadata' implicitly has an
          // 'any' type.
          parser.parse(file, metadata, callback, onError);
        } catch (e) {
          // @ts-ignore: error TS18046: 'e' is of type 'unknown'.
          onError(e.stack);
        }
      },
    ];

    // @ts-ignore: error TS2555: Expected at least 1 arguments, but got 0.
    nextStep();
  }
}


// Webworker spec says that the worker global object is called self.  That's
// a terrible name since we use it all over the chrome codebase to capture
// the 'this' keyword in lambdas.
const global = self;

if (global.constructor.name == 'SharedWorkerGlobalScope') {
  global.addEventListener('connect', e => {
    // @ts-ignore: error TS2339: Property 'ports' does not exist on type
    // 'Event'.
    const port = e.ports[0];
    new MetadataDispatcher(port);
    port.start();
  });
} else {
  // Non-shared worker.
  new MetadataDispatcher(global);
}
