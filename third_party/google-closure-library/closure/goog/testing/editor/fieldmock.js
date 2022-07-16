/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Mock of goog.editor.field.
 */

goog.setTestOnly('goog.testing.editor.FieldMock');
goog.provide('goog.testing.editor.FieldMock');

goog.require('goog.dom');
goog.require('goog.dom.Range');
goog.require('goog.editor.Field');
goog.require('goog.testing.LooseMock');
goog.require('goog.testing.mockmatchers');
goog.requireType('goog.dom.AbstractRange');



/**
 * Mock of goog.editor.Field.
 * @param {Window=} opt_window Window the field would edit.  Defaults to
 *     `window`.
 * @param {Window=} opt_appWindow "AppWindow" of the field, which can be
 *     different from `opt_window` when mocking a field that uses an
 *     iframe. Defaults to `opt_window`.
 * @param {goog.dom.AbstractRange=} opt_range An object (mock or real) to be
 *     returned by getRange(). If omitted, a new goog.dom.Range is created
 *     from the window every time getRange() is called.
 * @constructor
 * @extends {goog.testing.LooseMock}
 * @suppress {missingProperties} Mocks do not fit in the type system well.
 * @final
 */
goog.testing.editor.FieldMock = function(opt_window, opt_appWindow, opt_range) {
  'use strict';
  goog.testing.LooseMock.call(this, goog.editor.Field);
  opt_window = opt_window || window;
  opt_appWindow = opt_appWindow || opt_window;

  // We want to pretend this is a Field even though it can't actaully be a
  // subclass.
  const thisField = /** @type {!goog.editor.Field} */ (/** @type {*} */ (this));

  thisField.getAppWindow();
  this.$anyTimes();
  this.$returns(opt_appWindow);

  thisField.getRange();
  this.$anyTimes();
  this.$does(function() {
    'use strict';
    return opt_range || goog.dom.Range.createFromWindow(opt_window);
  });

  thisField.getEditableDomHelper();
  this.$anyTimes();
  this.$returns(goog.dom.getDomHelper(opt_window.document));

  thisField.usesIframe();
  this.$anyTimes();

  thisField.getBaseZindex();
  this.$anyTimes();
  this.$returns(0);

  thisField.restoreSavedRange(
      /** @type {?} */ (goog.testing.mockmatchers.ignoreArgument));
  this.$anyTimes();
  this.$does(function(range) {
    'use strict';
    if (range) {
      range.restore();
    }
    thisField.focus();
  });

  // These methods cannot be set on the prototype, because the prototype
  // gets stepped on by the mock framework.
  let inModalMode = false;

  /**
   * @return {boolean} Whether we're in modal interaction mode.
   */
  this.inModalMode = function() {
    'use strict';
    return inModalMode;
  };

  /**
   * @param {boolean} mode Sets whether we're in modal interaction mode.
   */
  this.setModalMode = function(mode) {
    'use strict';
    inModalMode = mode;
  };

  let uneditable = false;

  /**
   * @return {boolean} Whether the field is uneditable.
   */
  this.isUneditable = function() {
    'use strict';
    return uneditable;
  };

  /**
   * @param {boolean} isUneditable Whether the field is uneditable.
   */
  this.setUneditable = function(isUneditable) {
    'use strict';
    uneditable = isUneditable;
  };
};
goog.inherits(goog.testing.editor.FieldMock, goog.testing.LooseMock);
