/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Definition of MenuBarRenderer decorator, a static call into
 * the goog.ui.registry.
 *
 * @see ../demos/menubar.html
 */

goog.provide('goog.ui.menuBarDecorator');

goog.require('goog.ui.MenuBarRenderer');
goog.require('goog.ui.menuBar');
goog.require('goog.ui.registry');


/**
 * Register a decorator factory function. 'goog-menubar' defaults to
 * goog.ui.MenuBarRenderer.
 */
goog.ui.registry.setDecoratorByClassName(
    goog.ui.MenuBarRenderer.CSS_CLASS, goog.ui.menuBar.create);
