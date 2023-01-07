/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Class for managing the interactions between a rich autocomplete
 * object and a text-input or textarea.
 */

goog.provide('goog.ui.ac.RichInputHandler');

goog.require('goog.ui.ac.InputHandler');



/**
 * Class for managing the interaction between an autocomplete object and a
 * text-input or textarea.
 * @param {?string=} opt_separators Seperators to split multiple entries.
 * @param {?string=} opt_literals Characters used to delimit text literals.
 * @param {?boolean=} opt_multi Whether to allow multiple entries
 *     (Default: true).
 * @param {?number=} opt_throttleTime Number of milliseconds to throttle
 *     keyevents with (Default: 150).
 * @constructor
 * @extends {goog.ui.ac.InputHandler}
 */
goog.ui.ac.RichInputHandler = function(
    opt_separators, opt_literals, opt_multi, opt_throttleTime) {
  'use strict';
  goog.ui.ac.InputHandler.call(
      this, opt_separators, opt_literals, opt_multi, opt_throttleTime);
};
goog.inherits(goog.ui.ac.RichInputHandler, goog.ui.ac.InputHandler);


/**
 * Selects the given rich row.  The row's select(target) method is called.
 * @param {Object} row The row to select.
 * @return {boolean} Whether to suppress the update event.
 * @override
 */
goog.ui.ac.RichInputHandler.prototype.selectRow = function(row) {
  'use strict';
  var suppressUpdate =
      goog.ui.ac.RichInputHandler.superClass_.selectRow.call(this, row);
  row.select(this.ac_.getTarget());
  return suppressUpdate;
};
