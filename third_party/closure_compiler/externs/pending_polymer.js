// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs for Polymer not added to the polymer-1.0.js file.
 * @externs
 */

/**
 * @see
 * https://www.polymer-project.org/2.0/docs/api/namespaces/Polymer.RenderStatus
 * Queue a function call to be run before the next render.
 * @param {!Element} element The element on which the function call is made.
 * @param {!function()} fn The function called on next render.
 * @param {...*} args The function arguments.
 * TODO(rbpotter): Remove this once it is added to Closure Compiler itself.
 */
Polymer.RenderStatus.beforeNextRender = function(element, fn, args) {};

/**
 * @see
 * https://polymer-library.polymer-project.org/2.0/api/namespaces/Polymer.Templatize
 * @constructor
 * TODO(rbpotter): Remove this once it is added to Closure Compiler itself.
 */
Polymer.Templatize = function() {};

/**
 * @param {!HTMLTemplateElement} template
 * @param {Object=} owner
 * @param {Object=} options
 * @return {!Function}
 * TODO(rbpotter): Remove this once it is added to Closure Compiler itself.
 */
Polymer.Templatize.templatize = function(template, owner, options) {};

/**
 * @see
 * https://polymer-library.polymer-project.org/2.0/api/namespaces/Polymer.Templatize
 * @constructor
 * TODO(rbpotter): Remove this once it is added to Closure Compiler itself.
 */
let TemplateInstanceBase = function() {};

/**
 * @see
 * https://polymer-library.polymer-project.org/2.0/api/elements/Polymer.DomIf
 * @constructor
 */
Polymer.DomIf = function() {};

/**
 * @param {!HTMLTemplateElement} template
 * @return {!HTMLElement}
 * TODO(dpapad): Figure out if there is a better way to type-check Polymer2
 * while still using legacy Polymer1 syntax.
 */
Polymer.DomIf._contentForTemplate = function(template) {};

/**
 * From:
 * https://github.com/Polymer/polymer/blob/2.x/lib/mixins/property-effects.html
 *
 * @param {Object} props Bag of one or more key-value pairs whose key is
 *   a property and value is the new value to set for that property.
 * @param {boolean=} setReadOnly When true, any private values set in
 *   `props` will be set. By default, `setProperties` will not set
 *   `readOnly: true` root properties.
 * @return {void}
 * @public
 */
PolymerElement.prototype.setProperties = function(props, setReadOnly) {};
