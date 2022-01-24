/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A menu item class that supports checkbox semantics.
 */

goog.provide('goog.ui.CheckBoxMenuItem');

goog.require('goog.ui.MenuItem');
goog.require('goog.ui.registry');
goog.requireType('goog.dom.DomHelper');
goog.requireType('goog.ui.ControlContent');



/**
 * Class representing a checkbox menu item.  This is just a convenience class
 * that extends {@link goog.ui.MenuItem} by making it checkable.
 *
 * @param {goog.ui.ControlContent} content Text caption or DOM structure to
 *     display as the content of the item (use to add icons or styling to
 *     menus).
 * @param {*=} opt_model Data/model associated with the menu item.
 * @param {goog.dom.DomHelper=} opt_domHelper Optional DOM helper used for
 *     document interactions.
 * @constructor
 * @extends {goog.ui.MenuItem}
 */
goog.ui.CheckBoxMenuItem = function(content, opt_model, opt_domHelper) {
  'use strict';
  goog.ui.MenuItem.call(this, content, opt_model, opt_domHelper);
  this.setCheckable(true);
};
goog.inherits(goog.ui.CheckBoxMenuItem, goog.ui.MenuItem);


// Register a decorator factory function for goog.ui.CheckBoxMenuItems.
goog.ui.registry.setDecoratorByClassName(
    goog.getCssName('goog-checkbox-menuitem'), function() {
      'use strict';
      // CheckBoxMenuItem defaults to using MenuItemRenderer.
      return new goog.ui.CheckBoxMenuItem(null);
    });
