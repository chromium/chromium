/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Mock implementations of the Closure HTML5 FileSystem wrapper
 * classes. These implementations are designed to be usable in any browser, so
 * they use none of the native FileSystem-related objects.
 */

goog.setTestOnly('goog.testing.fs');
goog.provide('goog.testing.fs');

goog.require('goog.Timer');
goog.require('goog.async.Deferred');
/** @suppress {extraRequire} used in mocking */
goog.require('goog.fs');
/** @suppress {extraRequire} used in mocking */
goog.require('goog.fs.url');
goog.require('goog.testing.PropertyReplacer');
goog.require('goog.testing.fs.Blob');
goog.require('goog.testing.fs.FileSystem');


/**
 * Get a filesystem object. Since these are mocks, there's no difference between
 * temporary and persistent filesystems.
 *
 * @param {number} size Ignored.
 * @return {!goog.async.Deferred} The deferred
 *     {@link goog.testing.fs.FileSystem}.
 */
goog.testing.fs.getTemporary = function(size) {
  'use strict';
  const d = new goog.async.Deferred();
  goog.Timer.callOnce(
      goog.bind(d.callback, d, new goog.testing.fs.FileSystem()));
  return d;
};


/**
 * Get a filesystem object. Since these are mocks, there's no difference between
 * temporary and persistent filesystems.
 *
 * @param {number} size Ignored.
 * @return {!goog.async.Deferred} The deferred
 *     {@link goog.testing.fs.FileSystem}.
 */
goog.testing.fs.getPersistent = function(size) {
  'use strict';
  return goog.testing.fs.getTemporary(size);
};


/**
 * Which object URLs have been granted for fake blobs.
 * @type {!Object<boolean>}
 * @private
 */
goog.testing.fs.objectUrls_ = {};


/**
 * Create a fake object URL for a given fake blob. This can be used as a real
 * URL, and it can be created and revoked normally.
 *
 * @param {!goog.testing.fs.Blob} blob The blob for which to create the URL.
 * @return {string} The URL.
 */
goog.testing.fs.createObjectUrl = function(blob) {
  'use strict';
  const url = blob.toDataUrl();
  goog.testing.fs.objectUrls_[url] = true;
  return url;
};


/**
 * Remove a URL that was created for a fake blob.
 *
 * @param {string} url The URL to revoke.
 */
goog.testing.fs.revokeObjectUrl = function(url) {
  'use strict';
  delete goog.testing.fs.objectUrls_[url];
};


/**
 * Return whether or not a URL has been granted for the given blob.
 *
 * @param {!goog.testing.fs.Blob} blob The blob to check.
 * @return {boolean} Whether a URL has been granted.
 */
goog.testing.fs.isObjectUrlGranted = function(blob) {
  'use strict';
  return (blob.toDataUrl()) in goog.testing.fs.objectUrls_;
};


/**
 * Concatenates one or more values together and converts them to a fake blob.
 *
 * @param {...(string|!goog.testing.fs.Blob)} var_args The values that will make
 *     up the resulting blob.
 * @return {!goog.testing.fs.Blob} The blob.
 */
goog.testing.fs.getBlob = function(var_args) {
  'use strict';
  return new goog.testing.fs.Blob(
      Array.prototype.map.call(arguments, String).join(''));
};


/**
 * Creates a blob with the given properties.
 * See https://developer.mozilla.org/en-US/docs/Web/API/Blob for more details.
 *
 * @param {Array<string|!goog.testing.fs.Blob>} parts
 *     The values that will make up the resulting blob.
 * @param {string=} opt_type The MIME type of the Blob.
 * @param {string=} opt_endings Specifies how strings containing newlines are to
 *     be written out.
 * @return {!goog.testing.fs.Blob} The blob.
 */
goog.testing.fs.getBlobWithProperties = function(parts, opt_type, opt_endings) {
  'use strict';
  return new goog.testing.fs.Blob(parts.map(String).join(''), opt_type);
};


/**
 * Slices the blob. The returned blob contains data from the start byte
 * (inclusive) till the end byte (exclusive). Negative indices can be used
 * to count bytes from the end of the blob (-1 == blob.size - 1). Indices
 * are always clamped to blob range. If end is omitted, all the data till
 * the end of the blob is taken.
 *
 * @param {!goog.testing.fs.Blob} testBlob The blob to slice.
 * @param {number} start Index of the starting byte.
 * @param {number=} opt_end Index of the ending byte.
 * @return {!goog.testing.fs.Blob} The new blob or null if not supported.
 */
goog.testing.fs.sliceBlob = function(testBlob, start, opt_end) {
  'use strict';
  return testBlob.slice(start, opt_end);
};


/**
 * Installs goog.testing.fs in place of the standard goog.fs. After calling
 * this, code that uses goog.fs should work without issue using goog.testing.fs.
 *
 * @param {!goog.testing.PropertyReplacer} stubs The property replacer for
 *     stubbing out the original goog.fs functions.
 */
goog.testing.fs.install = function(stubs) {
  'use strict';
  // Prevent warnings that goog.fs may get optimized away. It's true this is
  // unsafe in compiled code, but it's only meant for tests.
  const fs = goog.getObjectByName('goog.fs');
  const fsUrl = goog.getObjectByName('goog.fs.url');
  stubs.replace(fs, 'getTemporary', goog.testing.fs.getTemporary);
  stubs.replace(fs, 'getPersistent', goog.testing.fs.getPersistent);
  stubs.replace(fsUrl, 'createObjectUrl', goog.testing.fs.createObjectUrl);
  stubs.replace(fsUrl, 'revokeObjectUrl', goog.testing.fs.revokeObjectUrl);
  stubs.replace(fsUrl, 'browserSupportsObjectUrls', function() {
    'use strict';
    return true;
  });
  const fsBlob = goog.getObjectByName('goog.fs.blob');
  stubs.replace(fsBlob, 'getBlob', goog.testing.fs.getBlob);
  stubs.replace(
      fsBlob, 'getBlobWithProperties', goog.testing.fs.getBlobWithProperties);
};
