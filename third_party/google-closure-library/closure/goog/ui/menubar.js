/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A base menu bar factory. Can be bound to an existing
 * HTML structure or can generate its own DOM.
 *
 * To decorate, the menu bar should be bound to an element containing children
 * with the classname 'goog-menu-button'.  See menubar.html for example.
 *
 * @see ../demos/menubar.html
 */

goog.provide('goog.ui.menuBar');

goog.require('goog.ui.Container');
goog.require('goog.ui.MenuBarRenderer');
goog.requireType('goog.dom.DomHelper');
goog.requireType('goog.ui.ContainerRenderer');


/**
 * The menuBar factory creates a new menu bar.
 * @param {goog.ui.ContainerRenderer=} opt_renderer Renderer used to render or
 *     decorate the menu bar; defaults to {@link goog.ui.MenuBarRenderer}.
 * @param {goog.dom.DomHelper=} opt_domHelper DOM helper, used for document
 *     interaction.
 * @return {!goog.ui.Container} The created menu bar.
 */
goog.ui.menuBar.create = function(opt_renderer, opt_domHelper) {
  'use strict';
  return new goog.ui.Container(
      null, opt_renderer ? opt_renderer : goog.ui.MenuBarRenderer.getInstance(),
      opt_domHelper);
};
