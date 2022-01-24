/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A wrapper for the HTML5 File ProgressEvent objects.
 */
goog.provide('goog.fs.ProgressEvent');

goog.require('goog.events.Event');



/**
 * A wrapper for the progress events emitted by the File APIs.
 *
 * @param {!ProgressEvent} event The underlying event object.
 * @param {!Object} target The file access object emitting the event.
 * @extends {goog.events.Event}
 * @constructor
 * @final
 */
goog.fs.ProgressEvent = function(event, target) {
  'use strict';
  goog.fs.ProgressEvent.base(this, 'constructor', event.type, target);

  /**
   * The underlying event object.
   * @type {!ProgressEvent}
   * @private
   */
  this.event_ = event;
};
goog.inherits(goog.fs.ProgressEvent, goog.events.Event);


/**
 * @return {boolean} Whether or not the total size of the of the file being
 *     saved is known.
 */
goog.fs.ProgressEvent.prototype.isLengthComputable = function() {
  'use strict';
  return this.event_.lengthComputable;
};


/**
 * @return {number} The number of bytes saved so far.
 */
goog.fs.ProgressEvent.prototype.getLoaded = function() {
  'use strict';
  return this.event_.loaded;
};


/**
 * @return {number} The total number of bytes in the file being saved.
 */
goog.fs.ProgressEvent.prototype.getTotal = function() {
  'use strict';
  return this.event_.total;
};
