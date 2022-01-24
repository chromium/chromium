/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Helper class to load a list of URIs in bulk. All URIs
 * must be a successfully loaded in order for the entire load to be considered
 * a success.
 */

goog.provide('goog.net.BulkLoaderHelper');

goog.require('goog.Disposable');
goog.requireType('goog.Uri');



/**
 * Helper class used to load multiple URIs.
 * @param {Array<string|goog.Uri>} uris The URIs to load.
 * @constructor
 * @extends {goog.Disposable}
 * @final
 */
goog.net.BulkLoaderHelper = function(uris) {
  'use strict';
  goog.Disposable.call(this);

  /**
   * The URIs to load.
   * @type {Array<string|goog.Uri>}
   * @private
   */
  this.uris_ = uris;

  /**
   * The response from the XHR's.
   * @type {Array<string>}
   * @private
   */
  this.responseTexts_ = [];
};
goog.inherits(goog.net.BulkLoaderHelper, goog.Disposable);



/**
 * Gets the URI by id.
 * @param {number} id The id.
 * @return {string|goog.Uri} The URI specified by the id.
 */
goog.net.BulkLoaderHelper.prototype.getUri = function(id) {
  'use strict';
  return this.uris_[id];
};


/**
 * Gets the URIs.
 * @return {Array<string|goog.Uri>} The URIs.
 */
goog.net.BulkLoaderHelper.prototype.getUris = function() {
  'use strict';
  return this.uris_;
};


/**
 * Gets the response texts.
 * @return {Array<string>} The response texts.
 */
goog.net.BulkLoaderHelper.prototype.getResponseTexts = function() {
  'use strict';
  return this.responseTexts_;
};


/**
 * Sets the response text by id.
 * @param {number} id The id.
 * @param {string} responseText The response texts.
 */
goog.net.BulkLoaderHelper.prototype.setResponseText = function(
    id, responseText) {
  'use strict';
  this.responseTexts_[id] = responseText;
};


/**
 * Determines if the load of the URIs is complete.
 * @return {boolean} TRUE iff the load is complete.
 */
goog.net.BulkLoaderHelper.prototype.isLoadComplete = function() {
  'use strict';
  const responseTexts = this.responseTexts_;
  if (responseTexts.length == this.uris_.length) {
    for (let i = 0; i < responseTexts.length; i++) {
      if (responseTexts[i] == null) {
        return false;
      }
    }
    return true;
  }
  return false;
};


/** @override */
goog.net.BulkLoaderHelper.prototype.disposeInternal = function() {
  'use strict';
  goog.net.BulkLoaderHelper.superClass_.disposeInternal.call(this);

  this.uris_ = null;
  this.responseTexts_ = null;
};
