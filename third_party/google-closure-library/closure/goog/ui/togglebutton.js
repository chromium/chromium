/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A toggle button control.  Extends {@link goog.ui.Button} by
 * providing checkbox-like semantics.
 */

goog.provide('goog.ui.ToggleButton');

goog.require('goog.ui.Button');
goog.require('goog.ui.Component');
goog.require('goog.ui.CustomButtonRenderer');
goog.require('goog.ui.registry');
goog.requireType('goog.dom.DomHelper');
goog.requireType('goog.ui.ButtonRenderer');
goog.requireType('goog.ui.ControlContent');



/**
 * A toggle button, with checkbox-like semantics.  Rendered using
 * {@link goog.ui.CustomButtonRenderer} by default, though any
 * {@link goog.ui.ButtonRenderer} would work.
 *
 * @param {goog.ui.ControlContent} content Text caption or existing DOM
 *     structure to display as the button's caption.
 * @param {goog.ui.ButtonRenderer=} opt_renderer Renderer used to render or
 *     decorate the button; defaults to {@link goog.ui.CustomButtonRenderer}.
 * @param {goog.dom.DomHelper=} opt_domHelper Optional DOM helper, used for
 *     document interaction.
 * @constructor
 * @extends {goog.ui.Button}
 */
goog.ui.ToggleButton = function(content, opt_renderer, opt_domHelper) {
  'use strict';
  goog.ui.Button.call(
      this, content, opt_renderer || goog.ui.CustomButtonRenderer.getInstance(),
      opt_domHelper);
  this.setSupportedState(goog.ui.Component.State.CHECKED, true);
};
goog.inherits(goog.ui.ToggleButton, goog.ui.Button);


// Register a decorator factory function for goog.ui.ToggleButtons.
goog.ui.registry.setDecoratorByClassName(
    goog.getCssName('goog-toggle-button'), function() {
      'use strict';
      // ToggleButton defaults to using CustomButtonRenderer.
      return new goog.ui.ToggleButton(null);
    });
