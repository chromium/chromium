/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Newer versions of iPhoto include a Safari plugin which allows
 * the browser to detect if iPhoto is installed. Adapted from detection code
 * built into the Mac.com Gallery RSS feeds.
 * @see ../demos/useragent.html
 */


goog.provide('goog.userAgent.iphoto');

goog.require('goog.string');
goog.require('goog.userAgent');


(function() {
'use strict';
var hasIphoto = false;
var version = '';

/**
 * The plugin description string contains the version number as in the form
 * 'iPhoto 700'. This returns just the version number as a dotted string,
 * e.g., '7.0.0', compatible with `goog.string.compareVersions`.
 * @param {string} desc The version string.
 * @return {string} The dotted version.
 */
function getIphotoVersion(desc) {
  var matches = desc.match(/\d/g);
  return matches.join('.');
}

if (goog.userAgent.WEBKIT && navigator.mimeTypes &&
    navigator.mimeTypes.length > 0) {
  var iphoto = navigator.mimeTypes['application/photo'];

  if (iphoto) {
    hasIphoto = true;
    var description = iphoto['description'];

    if (description) {
      version = getIphotoVersion(description);
    }
  }
}

/**
 * Whether we can detect that the user has iPhoto installed.
 * @type {boolean}
 */
goog.userAgent.iphoto.HAS_IPHOTO = hasIphoto;


/**
 * The version of iPhoto installed if found.
 * @type {string}
 */
goog.userAgent.iphoto.VERSION = version;
})();


/**
 * Whether the installed version of iPhoto is as new or newer than a given
 * version.
 * @param {string} version The version to check.
 * @return {boolean} Whether the installed version of iPhoto is as new or newer
 *     than a given version.
 */
goog.userAgent.iphoto.isVersion = function(version) {
  'use strict';
  return goog.string.compareVersions(goog.userAgent.iphoto.VERSION, version) >=
      0;
};
