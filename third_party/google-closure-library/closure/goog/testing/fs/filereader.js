/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Mock FileReader object.
 */

goog.setTestOnly('goog.testing.fs.FileReader');
goog.provide('goog.testing.fs.FileReader');

goog.require('goog.Timer');
goog.require('goog.events.EventTarget');
goog.require('goog.fs.Error');
goog.require('goog.fs.FileReader');
goog.require('goog.testing.fs.Blob');
goog.require('goog.testing.fs.ProgressEvent');



/**
 * A mock FileReader object. This emits the same events as
 * {@link goog.fs.FileReader}.
 *
 * @constructor
 * @extends {goog.events.EventTarget}
 */
goog.testing.fs.FileReader = function() {
  'use strict';
  goog.testing.fs.FileReader.base(this, 'constructor');

  /**
   * The current state of the reader.
   * @type {goog.fs.FileReader.ReadyState}
   * @private
   */
  this.readyState_ = goog.fs.FileReader.ReadyState.INIT;
};
goog.inherits(goog.testing.fs.FileReader, goog.events.EventTarget);


/**
 * The most recent error experienced by this reader.
 * @type {goog.fs.Error}
 * @private
 */
goog.testing.fs.FileReader.prototype.error_;


/**
 * Whether the current operation has been aborted.
 * @type {boolean}
 * @private
 */
goog.testing.fs.FileReader.prototype.aborted_ = false;


/**
 * The blob this reader is reading from.
 * @type {goog.testing.fs.Blob}
 * @private
 */
goog.testing.fs.FileReader.prototype.blob_;


/**
 * The possible return types.
 * @enum {number}
 */
goog.testing.fs.FileReader.ReturnType = {
  /**
   * Used when reading as text.
   */
  TEXT: 1,

  /**
   * Used when reading as binary string.
   */
  BINARY_STRING: 2,

  /**
   * Used when reading as array buffer.
   */
  ARRAY_BUFFER: 3,

  /**
   * Used when reading as data URL.
   */
  DATA_URL: 4
};


/**
 * The return type we're reading.
 * @type {goog.testing.fs.FileReader.ReturnType}
 * @private
 */
goog.testing.fs.FileReader.prototype.returnType_;


/**
 * @see {goog.fs.FileReader#getReadyState}
 * @return {goog.fs.FileReader.ReadyState} The current ready state.
 */
goog.testing.fs.FileReader.prototype.getReadyState = function() {
  'use strict';
  return this.readyState_;
};


/**
 * @see {goog.fs.FileReader#getError}
 * @return {goog.fs.Error} The current error.
 */
goog.testing.fs.FileReader.prototype.getError = function() {
  'use strict';
  return this.error_;
};


/**
 * @see {goog.fs.FileReader#abort}
 */
goog.testing.fs.FileReader.prototype.abort = function() {
  'use strict';
  if (this.readyState_ != goog.fs.FileReader.ReadyState.LOADING) {
    const msg = 'aborting read';
    throw new goog.fs.Error({'name': 'InvalidStateError'}, msg);
  }

  this.aborted_ = true;
};


/**
 * @see {goog.fs.FileReader#getResult}
 * @return {*} The result of the file read.
 */
goog.testing.fs.FileReader.prototype.getResult = function() {
  'use strict';
  if (this.readyState_ != goog.fs.FileReader.ReadyState.DONE) {
    return undefined;
  }
  if (this.error_) {
    return undefined;
  }
  if (this.returnType_ == goog.testing.fs.FileReader.ReturnType.TEXT) {
    return this.blob_.toString();
  } else if (
      this.returnType_ == goog.testing.fs.FileReader.ReturnType.ARRAY_BUFFER) {
    return this.blob_.toArrayBuffer();
  } else if (
      this.returnType_ == goog.testing.fs.FileReader.ReturnType.BINARY_STRING) {
    return this.blob_.toString();
  } else if (
      this.returnType_ == goog.testing.fs.FileReader.ReturnType.DATA_URL) {
    return this.blob_.toDataUrl();
  } else {
    return undefined;
  }
};


/**
 * Fires the read events.
 * @param {!goog.testing.fs.Blob} blob The blob to read from.
 * @private
 */
goog.testing.fs.FileReader.prototype.read_ = function(blob) {
  'use strict';
  this.blob_ = blob;
  if (this.readyState_ == goog.fs.FileReader.ReadyState.LOADING) {
    const msg = 'reading file';
    throw new goog.fs.Error({'name': 'InvalidStateError'}, msg);
  }

  this.readyState_ = goog.fs.FileReader.ReadyState.LOADING;
  goog.Timer.callOnce(function() {
    'use strict';
    if (this.aborted_) {
      this.abort_(blob.size);
      return;
    }

    this.progressEvent_(goog.fs.FileReader.EventType.LOAD_START, 0, blob.size);
    this.progressEvent_(
        goog.fs.FileReader.EventType.LOAD, blob.size / 2, blob.size);
    this.progressEvent_(
        goog.fs.FileReader.EventType.LOAD, blob.size, blob.size);
    this.readyState_ = goog.fs.FileReader.ReadyState.DONE;
    this.progressEvent_(
        goog.fs.FileReader.EventType.LOAD, blob.size, blob.size);
    this.progressEvent_(
        goog.fs.FileReader.EventType.LOAD_END, blob.size, blob.size);
  }, 0, this);
};


/**
 * @see {goog.fs.FileReader#readAsBinaryString}
 * @param {!goog.testing.fs.Blob} blob The blob to read.
 */
goog.testing.fs.FileReader.prototype.readAsBinaryString = function(blob) {
  'use strict';
  this.returnType_ = goog.testing.fs.FileReader.ReturnType.BINARY_STRING;
  this.read_(blob);
};


/**
 * @see {goog.fs.FileReader#readAsArrayBuffer}
 * @param {!goog.testing.fs.Blob} blob The blob to read.
 */
goog.testing.fs.FileReader.prototype.readAsArrayBuffer = function(blob) {
  'use strict';
  this.returnType_ = goog.testing.fs.FileReader.ReturnType.ARRAY_BUFFER;
  this.read_(blob);
};


/**
 * @see {goog.fs.FileReader#readAsText}
 * @param {!goog.testing.fs.Blob} blob The blob to read.
 * @param {string=} opt_encoding The name of the encoding to use.
 */
goog.testing.fs.FileReader.prototype.readAsText = function(blob, opt_encoding) {
  'use strict';
  this.returnType_ = goog.testing.fs.FileReader.ReturnType.TEXT;
  this.read_(blob);
};


/**
 * @see {goog.fs.FileReader#readAsDataUrl}
 * @param {!goog.testing.fs.Blob} blob The blob to read.
 */
goog.testing.fs.FileReader.prototype.readAsDataUrl = function(blob) {
  'use strict';
  this.returnType_ = goog.testing.fs.FileReader.ReturnType.DATA_URL;
  this.read_(blob);
};


/**
 * Abort the current action and emit appropriate events.
 *
 * @param {number} total The total data that was to be processed, in bytes.
 * @private
 */
goog.testing.fs.FileReader.prototype.abort_ = function(total) {
  'use strict';
  this.error_ = new goog.fs.Error({'name': 'AbortError'}, 'reading file');
  this.progressEvent_(goog.fs.FileReader.EventType.ERROR, 0, total);
  this.progressEvent_(goog.fs.FileReader.EventType.ABORT, 0, total);
  this.readyState_ = goog.fs.FileReader.ReadyState.DONE;
  this.progressEvent_(goog.fs.FileReader.EventType.LOAD_END, 0, total);
  this.aborted_ = false;
};


/**
 * Dispatch a progress event.
 *
 * @param {goog.fs.FileReader.EventType} type The event type.
 * @param {number} loaded The number of bytes processed.
 * @param {number} total The total data that was to be processed, in bytes.
 * @private
 */
goog.testing.fs.FileReader.prototype.progressEvent_ = function(
    type, loaded, total) {
  'use strict';
  this.dispatchEvent(new goog.testing.fs.ProgressEvent(type, loaded, total));
};
