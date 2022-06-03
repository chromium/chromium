/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Creates a pool of XhrIo objects to use. This allows multiple
 * XhrIo objects to be grouped together and requests will use next available
 * XhrIo object.
 */

goog.provide('goog.net.XhrIoPool');

goog.require('goog.net.XhrIo');
goog.require('goog.structs.PriorityPool');
goog.requireType('goog.structs.Map');



/**
 * A pool of XhrIo objects.
 * @param {goog.structs.Map=} opt_headers Map of default headers to add to every
 *     request.
 * @param {number=} opt_minCount Minimum number of objects (Default: 0).
 * @param {number=} opt_maxCount Maximum number of objects (Default: 10).
 * @param {boolean=} opt_withCredentials Add credentials to every request
 *     (Default: false).
 * @constructor
 * @extends {goog.structs.PriorityPool}
 */
goog.net.XhrIoPool = function(
    opt_headers, opt_minCount, opt_maxCount, opt_withCredentials) {
  'use strict';
  /**
   * Map of default headers to add to every request.
   * @type {goog.structs.Map|undefined}
   * @private
   */
  this.headers_ = opt_headers;

  /**
   * Whether a "credentialed" requests are to be sent (ones that is aware of
   * cookies and authentication). This is applicable only for cross-domain
   * requests and more recent browsers that support this part of the HTTP Access
   * Control standard.
   *
   * @see http://www.w3.org/TR/XMLHttpRequest/#the-withcredentials-attribute
   *
   * @private {boolean}
   */
  this.withCredentials_ = !!opt_withCredentials;

  // Must break convention of putting the super-class's constructor first. This
  // is because the super-class constructor calls adjustForMinMax, which calls
  // this class' createObject. In this class's implementation, it assumes that
  // there is a headers_, and will lack those if not yet present.
  goog.structs.PriorityPool.call(this, opt_minCount, opt_maxCount);
};
goog.inherits(goog.net.XhrIoPool, goog.structs.PriorityPool);


/**
 * Creates an instance of an XhrIo object to use in the pool.
 * @return {!goog.net.XhrIo} The created object.
 * @override
 */
goog.net.XhrIoPool.prototype.createObject = function() {
  'use strict';
  const xhrIo = new goog.net.XhrIo();
  const headers = this.headers_;
  if (headers) {
    headers.forEach(function(value, key) {
      'use strict';
      xhrIo.headers.set(key, value);
    });
  }
  if (this.withCredentials_) {
    xhrIo.setWithCredentials(true);
  }
  return xhrIo;
};


/**
 * Determine if an object has become unusable and should not be used.
 * @param {Object} obj The object to test.
 * @return {boolean} Whether the object can be reused, which is true if the
 *     object is not disposed and not active.
 * @override
 */
goog.net.XhrIoPool.prototype.objectCanBeReused = function(obj) {
  'use strict';
  // An active XhrIo object should never be used.
  const xhr = /** @type {goog.net.XhrIo} */ (obj);
  return !xhr.isDisposed() && !xhr.isActive();
};
