/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Similar to {@link goog.ui.FlatButtonRenderer},
 * but underlines text instead of adds borders.
 *
 * For accessibility reasons, it is best to use this with a goog.ui.Button
 * instead of an A element for links that perform actions in the page.  Links
 * that have an href and open a new page can and should remain as A elements.
 */

goog.provide('goog.ui.LinkButtonRenderer');

goog.require('goog.ui.Button');
goog.require('goog.ui.FlatButtonRenderer');
goog.require('goog.ui.registry');



/**
 * Link renderer for {@link goog.ui.Button}s.  Link buttons can contain
 * almost arbitrary HTML content, will flow like inline elements, but can be
 * styled like block-level elements.
 * @constructor
 * @extends {goog.ui.FlatButtonRenderer}
 */
goog.ui.LinkButtonRenderer = function() {
  'use strict';
  goog.ui.FlatButtonRenderer.call(this);
};
goog.inherits(goog.ui.LinkButtonRenderer, goog.ui.FlatButtonRenderer);
goog.addSingletonGetter(goog.ui.LinkButtonRenderer);


/**
 * Default CSS class to be applied to the root element of components rendered
 * by this renderer.
 * @type {string}
 */
goog.ui.LinkButtonRenderer.CSS_CLASS = goog.getCssName('goog-link-button');


/** @override */
goog.ui.LinkButtonRenderer.prototype.getCssClass = function() {
  'use strict';
  return goog.ui.LinkButtonRenderer.CSS_CLASS;
};


// Register a decorator factory function for Link Buttons.
goog.ui.registry.setDecoratorByClassName(
    goog.ui.LinkButtonRenderer.CSS_CLASS, function() {
      'use strict';
      // Uses goog.ui.Button, but with LinkButtonRenderer.
      return new goog.ui.Button(null, goog.ui.LinkButtonRenderer.getInstance());
    });
