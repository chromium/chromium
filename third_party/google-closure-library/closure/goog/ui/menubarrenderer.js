/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Renderer for {@link goog.ui.menuBar}.
 */

goog.provide('goog.ui.MenuBarRenderer');

goog.require('goog.a11y.aria.Role');
goog.require('goog.ui.Container');
goog.require('goog.ui.ContainerRenderer');



/**
 * Default renderer for {@link goog.ui.menuBar}s, based on {@link
 * goog.ui.ContainerRenderer}.
 * @constructor
 * @extends {goog.ui.ContainerRenderer}
 * @final
 */
goog.ui.MenuBarRenderer = function() {
  'use strict';
  goog.ui.MenuBarRenderer.base(
      this, 'constructor', goog.a11y.aria.Role.MENUBAR);
};
goog.inherits(goog.ui.MenuBarRenderer, goog.ui.ContainerRenderer);
goog.addSingletonGetter(goog.ui.MenuBarRenderer);


/**
 * Default CSS class to be applied to the root element of elements rendered
 * by this renderer.
 * @type {string}
 */
goog.ui.MenuBarRenderer.CSS_CLASS = goog.getCssName('goog-menubar');


/**
 * @override
 */
goog.ui.MenuBarRenderer.prototype.getCssClass = function() {
  'use strict';
  return goog.ui.MenuBarRenderer.CSS_CLASS;
};


/**
 * Returns the default orientation of containers rendered or decorated by this
 * renderer.  This implementation returns `HORIZONTAL`.
 * @return {!goog.ui.Container.Orientation} Default orientation for containers
 *     created or decorated by this renderer.
 * @override
 */
goog.ui.MenuBarRenderer.prototype.getDefaultOrientation = function() {
  'use strict';
  return goog.ui.Container.Orientation.HORIZONTAL;
};
