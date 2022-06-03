/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A toolbar toggle button control.
 */

goog.provide('goog.ui.ToolbarToggleButton');

goog.require('goog.ui.ToggleButton');
goog.require('goog.ui.ToolbarButtonRenderer');
goog.require('goog.ui.registry');
goog.requireType('goog.dom.DomHelper');
goog.requireType('goog.ui.ControlContent');



/**
 * A toggle button control for a toolbar.
 *
 * @param {goog.ui.ControlContent} content Text caption or existing DOM
 *     structure to display as the button's caption.
 * @param {goog.ui.ToolbarButtonRenderer=} opt_renderer Optional renderer used
 *     to render or decorate the button; defaults to
 *     {@link goog.ui.ToolbarButtonRenderer}.
 * @param {goog.dom.DomHelper=} opt_domHelper Optional DOM helper, used for
 *     document interaction.
 * @constructor
 * @extends {goog.ui.ToggleButton}
 */
goog.ui.ToolbarToggleButton = function(content, opt_renderer, opt_domHelper) {
  'use strict';
  goog.ui.ToggleButton.call(
      this, content,
      opt_renderer || goog.ui.ToolbarButtonRenderer.getInstance(),
      opt_domHelper);
};
goog.inherits(goog.ui.ToolbarToggleButton, goog.ui.ToggleButton);


// Registers a decorator factory function for toggle buttons in toolbars.
goog.ui.registry.setDecoratorByClassName(
    goog.getCssName('goog-toolbar-toggle-button'), function() {
      'use strict';
      return new goog.ui.ToolbarToggleButton(null);
    });
