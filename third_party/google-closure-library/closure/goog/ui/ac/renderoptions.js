/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Options for rendering matches.
 */

goog.provide('goog.ui.ac.RenderOptions');



/**
 * A simple class that contains options for rendering a set of autocomplete
 * matches.  Used as an optional argument in the callback from the matcher.
 * @constructor
 */
goog.ui.ac.RenderOptions = function() {};


/**
 * Whether the current highlighting is to be preserved when displaying the new
 * set of matches.
 * @type {boolean}
 * @private
 */
goog.ui.ac.RenderOptions.prototype.preserveHilited_ = false;


/**
 * Whether the first match is to be highlighted.  When undefined the autoHilite
 * flag of the autocomplete is used.
 * @type {boolean|undefined}
 * @private
 */
goog.ui.ac.RenderOptions.prototype.autoHilite_;


/**
 * @param {boolean} flag The new value for the preserveHilited_ flag.
 */
goog.ui.ac.RenderOptions.prototype.setPreserveHilited = function(flag) {
  'use strict';
  this.preserveHilited_ = flag;
};


/**
 * @return {boolean} The value of the preserveHilited_ flag.
 */
goog.ui.ac.RenderOptions.prototype.getPreserveHilited = function() {
  'use strict';
  return this.preserveHilited_;
};


/**
 * @param {boolean} flag The new value for the autoHilite_ flag.
 */
goog.ui.ac.RenderOptions.prototype.setAutoHilite = function(flag) {
  'use strict';
  this.autoHilite_ = flag;
};


/**
 * @return {boolean|undefined} The value of the autoHilite_ flag.
 */
goog.ui.ac.RenderOptions.prototype.getAutoHilite = function() {
  'use strict';
  return this.autoHilite_;
};
