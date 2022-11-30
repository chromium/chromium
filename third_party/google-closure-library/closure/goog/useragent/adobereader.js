/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Detects the Adobe Reader PDF browser plugin.
 *
 * @see ../demos/useragent.html
 */

goog.module('goog.userAgent.adobeReader');
goog.module.declareLegacyNamespace();

var googString = goog.require('goog.string');
var userAgent = goog.require('goog.userAgent');


var version = '';
if (userAgent.IE) {
  var detectOnIe = function(classId) {
    try {
      new ActiveXObject(classId);
      return true;
    } catch (ex) {
      return false;
    }
  };
  if (detectOnIe('AcroPDF.PDF.1')) {
    version = '7';
  } else if (detectOnIe('PDF.PdfCtrl.6')) {
    version = '6';
  }
  // TODO(chrisn): Add detection for previous versions if anyone needs them.
} else {
  if (navigator.mimeTypes && navigator.mimeTypes.length > 0) {
    var mimeType = navigator.mimeTypes['application/pdf'];
    if (mimeType && mimeType.enabledPlugin) {
      var description = mimeType.enabledPlugin.description;
      if (description && description.indexOf('Adobe') != -1) {
        // Newer plugins do not include the version in the description, so we
        // default to 7.
        version = description.indexOf('Version') != -1 ?
            description.split('Version')[1] :
            '7';
      }
    }
  }
}

/**
 * Whether we detect the user has the Adobe Reader browser plugin installed.
 * @type {boolean}
 */
exports.HAS_READER = !!version;


/**
 * The version of the installed Adobe Reader plugin. Versions after 7
 * will all be reported as '7'.
 * @type {string}
 */
exports.VERSION = version;


/**
 * On certain combinations of platform/browser/plugin, a print dialog
 * can be shown for PDF files without a download dialog or making the
 * PDF visible to the user, by loading the PDF into a hidden iframe.
 *
 * Currently this variable is true if Adobe Reader version 6 or later
 * is detected on Windows.
 *
 * @type {boolean}
 */
exports.SILENT_PRINT =
    userAgent.WINDOWS && googString.compareVersions(version, '6') >= 0;
