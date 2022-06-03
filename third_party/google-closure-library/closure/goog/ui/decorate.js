/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides a function that decorates an element based on its CSS
 * class name.
 */

goog.provide('goog.ui.decorate');

goog.require('goog.ui.registry');
goog.requireType('goog.ui.Component');


/**
 * Decorates the element with a suitable {@link goog.ui.Component} instance, if
 * a matching decorator is found.
 * @param {Element} element Element to decorate.
 * @return {goog.ui.Component?} New component instance, decorating the element.
 */
goog.ui.decorate = function(element) {
  'use strict';
  var decorator = goog.ui.registry.getDecorator(element);
  if (decorator) {
    decorator.decorate(element);
  }
  return decorator;
};
