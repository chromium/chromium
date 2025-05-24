/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Renderer for {@link goog.ui.TriStateMenuItem}s.
 */

goog.provide('goog.ui.TriStateMenuItemRenderer');

goog.forwardDeclare('goog.ui.TriStateMenuItem.State');  // TODO(user): remove this
goog.require('goog.asserts');
goog.require('goog.dom.classlist');
goog.require('goog.ui.MenuItemRenderer');
goog.requireType('goog.ui.Control');


/**
 * Default renderer for {@link goog.ui.TriStateMenuItemRenderer}s. Each item has
 * the following structure:
 *
 *    <div class="goog-tristatemenuitem">
 *        <div class="goog-tristatemenuitem-checkbox"></div>
 *        <div>...(content)...</div>
 *    </div>
 *
 * @constructor
 * @extends {goog.ui.MenuItemRenderer}
 * @final
 */
goog.ui.TriStateMenuItemRenderer = function() {
  'use strict';
  goog.ui.MenuItemRenderer.call(this);
};
goog.inherits(goog.ui.TriStateMenuItemRenderer, goog.ui.MenuItemRenderer);
goog.addSingletonGetter(goog.ui.TriStateMenuItemRenderer);


/**
 * CSS class name the renderer applies to menu item elements.
 * @type {string}
 */
goog.ui.TriStateMenuItemRenderer.CSS_CLASS =
    goog.getCssName('goog-tristatemenuitem');


/**
 * Overrides {@link goog.ui.ControlRenderer#decorate} by initializing the
 * menu item to checkable based on whether the element to be decorated has
 * extra styling indicating that it should be.
 * @param {goog.ui.Control} item goog.ui.TriStateMenuItem to decorate
 *     the element.
 * @param {Element} element Element to decorate.
 * @return {!Element} Decorated element.
 * @override
 * @suppress {strictMissingProperties} Added to tighten compiler checks
 * @suppress {missingRequire} TODO(user): remove this
 */
goog.ui.TriStateMenuItemRenderer.prototype.decorate = function(item, element) {
  'use strict';
  element = goog.ui.TriStateMenuItemRenderer.superClass_.decorate.call(
      this, item, element);
  this.setCheckable(item, element, true);

  goog.asserts.assert(element);

  if (goog.dom.classlist.contains(
          element, goog.getCssName(this.getCssClass(), 'fully-checked'))) {
    item.setCheckedState(/** @suppress {missingRequire} */
        goog.ui.TriStateMenuItem.State.FULLY_CHECKED);
  } else if (
      goog.dom.classlist.contains(
          element, goog.getCssName(this.getCssClass(), 'partially-checked'))) {
    /** @suppress {missingRequire} */
    item.setCheckedState(goog.ui.TriStateMenuItem.State.PARTIALLY_CHECKED);
  } else {
    /** @suppress {missingRequire} */
    item.setCheckedState(goog.ui.TriStateMenuItem.State.NOT_CHECKED);
  }

  return element;
};


/** @override */
goog.ui.TriStateMenuItemRenderer.prototype.getCssClass = function() {
  'use strict';
  return goog.ui.TriStateMenuItemRenderer.CSS_CLASS;
};
