/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Menu item observing the filter text in a
 * {@link goog.ui.FilteredMenu}. The observer method is called when the filter
 * text changes and allows the menu item to update its content and state based
 * on the filter.
 */

goog.provide('goog.ui.FilterObservingMenuItem');

goog.require('goog.ui.FilterObservingMenuItemRenderer');
goog.require('goog.ui.MenuItem');
goog.require('goog.ui.registry');
goog.requireType('goog.dom.DomHelper');
goog.requireType('goog.ui.ControlContent');
goog.requireType('goog.ui.MenuItemRenderer');



/**
 * Class representing a filter observing menu item.
 *
 * @param {goog.ui.ControlContent} content Text caption or DOM structure to
 *     display as the content of the item (use to add icons or styling to
 *     menus).
 * @param {*=} opt_model Data/model associated with the menu item.
 * @param {goog.dom.DomHelper=} opt_domHelper Optional DOM helper used for
 *     document interactions.
 * @param {goog.ui.MenuItemRenderer=} opt_renderer Optional renderer.
 * @constructor
 * @extends {goog.ui.MenuItem}
 */
goog.ui.FilterObservingMenuItem = function(
    content, opt_model, opt_domHelper, opt_renderer) {
  'use strict';
  goog.ui.MenuItem.call(
      this, content, opt_model, opt_domHelper,
      opt_renderer || new goog.ui.FilterObservingMenuItemRenderer());
};
goog.inherits(goog.ui.FilterObservingMenuItem, goog.ui.MenuItem);


/**
 * Function called when the filter text changes.
 * @type {?Function} function(goog.ui.FilterObservingMenuItem, string)
 * @private
 */
goog.ui.FilterObservingMenuItem.prototype.observer_ = null;


/** @override */
goog.ui.FilterObservingMenuItem.prototype.enterDocument = function() {
  'use strict';
  goog.ui.FilterObservingMenuItem.superClass_.enterDocument.call(this);
  this.callObserver();
};


/**
 * Sets the observer functions.
 * @param {Function} f function(goog.ui.FilterObservingMenuItem, string).
 */
goog.ui.FilterObservingMenuItem.prototype.setObserver = function(f) {
  'use strict';
  this.observer_ = f;
  this.callObserver();
};


/**
 * Calls the observer function if one has been specified.
 * @param {?string=} opt_str Filter string.
 */
goog.ui.FilterObservingMenuItem.prototype.callObserver = function(opt_str) {
  'use strict';
  if (this.observer_) {
    this.observer_(this, opt_str || '');
  }
};


// Register a decorator factory function for
// goog.ui.FilterObservingMenuItemRenderer.
goog.ui.registry.setDecoratorByClassName(
    goog.ui.FilterObservingMenuItemRenderer.CSS_CLASS, function() {
      'use strict';
      // FilterObservingMenuItem defaults to using
      // FilterObservingMenuItemRenderer.
      return new goog.ui.FilterObservingMenuItem(null);
    });
