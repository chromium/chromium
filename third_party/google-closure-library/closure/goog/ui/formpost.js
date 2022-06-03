/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Utility for making the browser submit a hidden form, which can
 * be used to effect a POST from JavaScript.
 */

goog.provide('goog.ui.FormPost');

goog.require('goog.array');
goog.require('goog.dom.InputType');
goog.require('goog.dom.TagName');
goog.require('goog.dom.safe');
goog.require('goog.html.SafeHtml');
goog.require('goog.ui.Component');
goog.requireType('goog.dom.DomHelper');



/**
 * Creates a formpost object.
 * @constructor
 * @extends {goog.ui.Component}
 * @param {goog.dom.DomHelper=} opt_dom The DOM helper.
 * @final
 */
goog.ui.FormPost = function(opt_dom) {
  'use strict';
  goog.ui.Component.call(this, opt_dom);
};
goog.inherits(goog.ui.FormPost, goog.ui.Component);


/** @override */
goog.ui.FormPost.prototype.createDom = function() {
  'use strict';
  this.setElementInternal(this.getDomHelper().createDom(
      goog.dom.TagName.FORM, {'method': 'POST', 'style': 'display:none'}));
};


/**
 * Constructs a POST request and directs the browser as if a form were
 * submitted.
 * @param {Object} parameters Object with parameter values. Values can be
 *     strings, numbers, or arrays of strings or numbers.
 * @param {string=} opt_url The destination URL. If not specified, uses the
 *     current URL for window for the DOM specified in the constructor.
 * @param {string=} opt_target An optional name of a window in which to open the
 *     URL. If not specified, uses the window for the DOM specified in the
 *     constructor.
 * @suppress {strictMissingProperties} Part of the go/strict_warnings_migration
 */
goog.ui.FormPost.prototype.post = function(parameters, opt_url, opt_target) {
  'use strict';
  var form = this.getElement();
  if (!form) {
    this.render();
    form = this.getElement();
  }
  form.action = opt_url || '';
  form.target = opt_target || '';
  this.setParameters_(form, parameters);
  form.submit();
};


/**
 * Creates hidden inputs in a form to match parameters.
 * @param {!Element} form The form element.
 * @param {Object} parameters Object with parameter values. Values can be
 *     strings, numbers, or arrays of strings or numbers.
 * @private
 */
goog.ui.FormPost.prototype.setParameters_ = function(form, parameters) {
  'use strict';
  var name, value, html = [];
  for (name in parameters) {
    value = parameters[name];
    if (goog.isArrayLike(value)) {
      goog.array.forEach(value, goog.bind(function(innerValue) {
        'use strict';
        html.push(this.createInput_(name, String(innerValue)));
      }, this));
    } else {
      html.push(this.createInput_(name, String(value)));
    }
  }
  goog.dom.safe.setInnerHtml(form, goog.html.SafeHtml.concat(html));
};


/**
 * Creates a hidden <input> tag.
 * @param {string} name The name of the input.
 * @param {string} value The value of the input.
 * @return {!goog.html.SafeHtml}
 * @private
 */
goog.ui.FormPost.prototype.createInput_ = function(name, value) {
  'use strict';
  return goog.html.SafeHtml.create(
      'input',
      {'type': goog.dom.InputType.HIDDEN, 'name': name, 'value': value});
};
