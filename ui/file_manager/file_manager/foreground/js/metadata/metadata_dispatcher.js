// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// All of these scripts could be imported with a single call to importScripts,
// but then load and compile time errors would all be reported from the same
// line. Note: update component_extension_resources.grd when adding new parsers.
importScripts(
    'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/foreground/js/metadata/metadata_parser.js');
importScripts(
    'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/foreground/js/metadata/byte_reader.js');
importScripts(
    'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/foreground/js/metadata/exif_parser.js');
importScripts(
    'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/foreground/js/metadata/image_parsers.js');
importScripts(
    'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/foreground/js/metadata/mpeg_parser.js');
importScripts(
    'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/foreground/js/metadata/id3_parser.js');

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
    this.port_.onmessage = this.onMessage.bind(this);

    /**
     * Verbose logging for the dispatcher.
     *
     * Individual parsers also take this as their default verbosity setting.
     * @public {boolean}
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
      request: this.request_.bind(this)
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
      this.processOneFile(fileURL, function callback(metadata) {
        this.postMessage('result', [fileURL, metadata]);
      }.bind(this));
    } catch (ex) {
      this.error(fileURL, ex);
    }
  }

  /**
   * Indicate to the caller that an operation has failed.
   *
   * No other messages relating to the failed operation should be sent.
   * @param {...(Object|string)} var_args Arguments.
   */
  error(var_args) {
    const ary = Array.apply(null, arguments);
    this.postMessage('error', ary);
  }

  /**
   * Send a log message to the caller.
   *
   * Callers must not parse log messages for control flow.
   * @param {...(Object|string)} var_args Arguments.
   */
  log(var_args) {
    const ary = Array.apply(null, arguments);
    this.postMessage('log', ary);
  }

  /**
   * Send a log message to the caller only if this.verbose is true.
   * @param {...(Object|string)} var_args Arguments.
   */
  vlog(var_args) {
    if (this.verbose) {
      this.log.apply(this, arguments);
    }
  }

  /**
   * Post a properly formatted message to the caller.
   * @param {string} verb Message type descriptor.
   * @param {Array<Object>} args Arguments array.
   */
  postMessage(verb, args) {
    this.port_.postMessage({verb: verb, arguments: args});
  }

  /**
   * Message handler.
   * @param {Event} event Event object.
   */
  onMessage(event) {
    const data = event.data;

    if (this.messageHandlers_.hasOwnProperty(data.verb)) {
      this.messageHandlers_[data.verb].apply(this, data.arguments);
    } else {
      this.log('Unknown message from client: ' + data.verb, data);
    }
  }

  /**
   * @param {string} fileURL File URL.
   * @param {function(Object)} callback Completion callback.
   */
  processOneFile(fileURL, callback) {
    const self = this;
    let currentStep = -1;

    /**
     * @param {...} var_args Arguments.
     */
    function nextStep(var_args) {
      self.vlog('nextStep: ' + steps[currentStep + 1].name);
      steps[++currentStep].apply(self, arguments);
    }

    let metadata;

    /**
     * @param {*} err An error.
     * @param {string=} opt_stepName Step name.
     */
    function onError(err, opt_stepName) {
      self.error(
          fileURL, opt_stepName || steps[currentStep].name, err.toString(),
          metadata);
    }

    const steps = [
      // Step one, find the parser matching the url.
      function detectFormat() {
        for (let i = 0; i != self.parserInstances_.length; i++) {
          const parser = self.parserInstances_[i];
          if (fileURL.match(parser.urlFilter)) {
            // Create the metadata object as early as possible so that we can
            // pass it with the error message.
            metadata = parser.createDefaultMetadata();
            nextStep(parser);
            return;
          }
        }
        onError('unsupported format');
      },

      // Step two, turn the url into an entry.
      function getEntry(parser) {
        webkitResolveLocalFileSystemURL(fileURL, entry => {
          nextStep(entry, parser);
        }, onError);
      },

      // Step three, turn the entry into a file.
      function getFile(entry, parser) {
        entry.file(file => {
          nextStep(file, parser);
        }, onError);
      },

      // Step four, parse the file content.
      function parseContent(file, parser) {
        metadata.fileSize = file.size;
        try {
          parser.parse(file, metadata, callback, onError);
        } catch (e) {
          onError(e.stack);
        }
      }
    ];

    nextStep();
  }
}


// Webworker spec says that the worker global object is called self.  That's
// a terrible name since we use it all over the chrome codebase to capture
// the 'this' keyword in lambdas.
const global = self;

if (global.constructor.name == 'SharedWorkerGlobalScope') {
  global.addEventListener('connect', e => {
    const port = e.ports[0];
    new MetadataDispatcher(port);
    port.start();
  });
} else {
  // Non-shared worker.
  new MetadataDispatcher(global);
}
