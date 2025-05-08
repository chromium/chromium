/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Displays and edits the value of a cookie.
 * Intended only for debugging.
 */
goog.provide('goog.ui.CookieEditor');

goog.require('goog.asserts');
goog.require('goog.dom');
goog.require('goog.dom.TagName');
goog.require('goog.events.EventType');
goog.require('goog.net.Cookies');
goog.require('goog.string');
goog.require('goog.style');
goog.require('goog.ui.Component');
goog.requireType('goog.events.Event');



/**
 * Displays and edits the value of a cookie.
 * @final
 * @unrestricted
 */
goog.ui.CookieEditor = class extends goog.ui.Component {
  /**
   * @param {goog.dom.DomHelper=} opt_domHelper Optional DOM helper.
   */
  constructor(opt_domHelper) {
    'use strict';
    super(opt_domHelper);
  }

  /**
   * Sets the cookie which this component will edit.
   * @param {string} cookieKey Cookie key.
   */
  selectCookie(cookieKey) {
    'use strict';
    goog.asserts.assert(goog.net.Cookies.getInstance().isValidName(cookieKey));
    this.cookieKey_ = cookieKey;
    if (this.textAreaElem_) {
      this.textAreaElem_.value =
          goog.net.Cookies.getInstance().get(cookieKey) || '';
    }
  }

  /** @override */
  canDecorate() {
    'use strict';
    return false;
  }

  /** @override */
  createDom() {
    'use strict';
    // Debug-only, so we don't need i18n.
    this.clearButtonElem_ = goog.dom.createDom(
        goog.dom.TagName.BUTTON, /* attributes */ null, 'Clear');
    this.updateButtonElem_ = goog.dom.createDom(
        goog.dom.TagName.BUTTON, /* attributes */ null, 'Update');
    var value =
        this.cookieKey_ && goog.net.Cookies.getInstance().get(this.cookieKey_);
    this.textAreaElem_ = goog.dom.createDom(
        goog.dom.TagName.TEXTAREA, /* attibutes */ null, value || '');
    this.valueWarningElem_ = goog.dom.createDom(
        goog.dom.TagName.SPAN,
        /* attibutes */ {'style': 'display:none;color:red'},
        'Invalid cookie value.');
    this.setElementInternal(goog.dom.createDom(
        goog.dom.TagName.DIV,
        /* attibutes */ null, this.valueWarningElem_,
        goog.dom.createDom(goog.dom.TagName.BR), this.textAreaElem_,
        goog.dom.createDom(goog.dom.TagName.BR), this.clearButtonElem_,
        this.updateButtonElem_));
  }

  /** @override */
  enterDocument() {
    'use strict';
    super.enterDocument();
    this.getHandler().listen(
        this.clearButtonElem_, goog.events.EventType.CLICK, this.handleClear_);
    this.getHandler().listen(
        this.updateButtonElem_, goog.events.EventType.CLICK,
        this.handleUpdate_);
  }

  /**
   * Handles user clicking clear button.
   * @param {!goog.events.Event} e The click event.
   * @private
   */
  handleClear_(e) {
    'use strict';
    if (this.cookieKey_) {
      goog.net.Cookies.getInstance().remove(this.cookieKey_);
    }
    this.textAreaElem_.value = '';
  }

  /**
   * Handles user clicking update button.
   * @param {!goog.events.Event} e The click event.
   * @private
   */
  handleUpdate_(e) {
    'use strict';
    if (this.cookieKey_) {
      var value = this.textAreaElem_.value;
      if (value) {
        // Strip line breaks.
        value = goog.string.stripNewlines(value);
      }
      if (goog.net.Cookies.getInstance().isValidValue(value)) {
        goog.net.Cookies.getInstance().set(this.cookieKey_, value);
        goog.style.setElementShown(this.valueWarningElem_, false);
      } else {
        goog.style.setElementShown(this.valueWarningElem_, true);
      }
    }
  }

  /** @override */
  disposeInternal() {
    'use strict';
    this.clearButtonElem_ = null;
    this.cookieKey_ = null;
    this.textAreaElem_ = null;
    this.updateButtonElem_ = null;
    this.valueWarningElem_ = null;
  }
};



/**
 * Cookie key.
 * @type {?string}
 * @private
 */
goog.ui.CookieEditor.prototype.cookieKey_;


/**
 * Text area.
 * @type {HTMLTextAreaElement}
 * @private
 */
goog.ui.CookieEditor.prototype.textAreaElem_;


/**
 * Clear button.
 * @type {HTMLButtonElement}
 * @private
 */
goog.ui.CookieEditor.prototype.clearButtonElem_;


/**
 * Invalid value warning text.
 * @type {HTMLSpanElement}
 * @private
 */
goog.ui.CookieEditor.prototype.valueWarningElem_;


/**
 * Update button.
 * @type {HTMLButtonElement}
 * @private
 */
goog.ui.CookieEditor.prototype.updateButtonElem_;


// TODO(user): add combobox for user to select different cookies
