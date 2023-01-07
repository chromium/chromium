/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Mock ProgressEvent object.
 */

goog.setTestOnly('goog.testing.fs.ProgressEvent');
goog.provide('goog.testing.fs.ProgressEvent');

goog.require('goog.events.Event');
goog.requireType('goog.fs.FileReader.EventType');
goog.requireType('goog.fs.FileSaver.EventType');



/**
 * A mock progress event.
 *
 * @param {!goog.fs.FileSaver.EventType|!goog.fs.FileReader.EventType} type
 *     Event type.
 * @param {number} loaded The number of bytes processed.
 * @param {number} total The total data that was to be processed, in bytes.
 * @constructor
 * @extends {goog.events.Event}
 * @final
 */
goog.testing.fs.ProgressEvent = function(type, loaded, total) {
  'use strict';
  goog.testing.fs.ProgressEvent.base(this, 'constructor', type);

  /**
   * The number of bytes processed.
   * @type {number}
   * @private
   */
  this.loaded_ = loaded;


  /**
   * The total data that was to be procesed, in bytes.
   * @type {number}
   * @private
   */
  this.total_ = total;
};
goog.inherits(goog.testing.fs.ProgressEvent, goog.events.Event);


/**
 * @see {goog.fs.ProgressEvent#isLengthComputable}
 * @return {boolean} True if the length is known.
 */
goog.testing.fs.ProgressEvent.prototype.isLengthComputable = function() {
  'use strict';
  return true;
};


/**
 * @see {goog.fs.ProgressEvent#getLoaded}
 * @return {number} The number of bytes loaded or written.
 */
goog.testing.fs.ProgressEvent.prototype.getLoaded = function() {
  'use strict';
  return this.loaded_;
};


/**
 * @see {goog.fs.ProgressEvent#getTotal}
 * @return {number} The total bytes to load or write.
 */
goog.testing.fs.ProgressEvent.prototype.getTotal = function() {
  'use strict';
  return this.total_;
};
