/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Definition of the MenuBase class.
 */

goog.provide('goog.ui.MenuBase');

goog.require('goog.events.EventHandler');
goog.require('goog.events.EventType');
goog.require('goog.events.KeyHandler');
goog.require('goog.ui.Popup');
goog.requireType('goog.events.Event');
goog.requireType('goog.events.KeyEvent');



/**
 * The MenuBase class provides an abstract base class for different
 * implementations of menu controls.
 *
 * @param {Element=} opt_element A DOM element for the popup.
 * @deprecated Use goog.ui.Menu.
 * @constructor
 * @extends {goog.ui.Popup}
 */
goog.ui.MenuBase = function(opt_element) {
  'use strict';
  goog.ui.Popup.call(this, opt_element);

  /**
   * Event handler for simplifiying adding/removing listeners.
   * @type {goog.events.EventHandler<!goog.ui.MenuBase>}
   * @private
   */
  this.eventHandler_ = new goog.events.EventHandler(this);

  /**
   * KeyHandler to cope with the vagaries of cross-browser key events.
   * @type {goog.events.KeyHandler}
   * @private
   */
  this.keyHandler_ = new goog.events.KeyHandler(this.getElement());
};
goog.inherits(goog.ui.MenuBase, goog.ui.Popup);


/**
 * Events fired by the Menu
 * @const
 */
goog.ui.MenuBase.Events = {};


/**
 * Event fired by the Menu when an item is "clicked".
 */
goog.ui.MenuBase.Events.ITEM_ACTION = 'itemaction';


/** @override */
goog.ui.MenuBase.prototype.disposeInternal = function() {
  'use strict';
  goog.ui.MenuBase.superClass_.disposeInternal.call(this);
  this.eventHandler_.dispose();
  this.keyHandler_.dispose();
};


/**
 * Called after the menu is shown. Derived classes can override to hook this
 * event but should make sure to call the parent class method.
 *
 * @protected
 * @override
 */
goog.ui.MenuBase.prototype.onShow = function() {
  'use strict';
  goog.ui.MenuBase.superClass_.onShow.call(this);

  // register common event handlers for derived classes
  var el = this.getElement();
  this.eventHandler_.listen(
      el, goog.events.EventType.MOUSEOVER, this.onMouseOver);
  this.eventHandler_.listen(
      el, goog.events.EventType.MOUSEOUT, this.onMouseOut);
  this.eventHandler_.listen(
      el, goog.events.EventType.MOUSEDOWN, this.onMouseDown);
  this.eventHandler_.listen(el, goog.events.EventType.MOUSEUP, this.onMouseUp);

  this.eventHandler_.listen(
      this.keyHandler_, goog.events.KeyHandler.EventType.KEY, this.onKeyDown);
};


/**
 * Called after the menu is hidden. Derived classes can override to hook this
 * event but should make sure to call the parent class method.
 * @param {?Node=} opt_target Target of the event causing the hide.
 * @protected
 * @override
 */
goog.ui.MenuBase.prototype.onHide = function(opt_target) {
  'use strict';
  goog.ui.MenuBase.superClass_.onHide.call(this, opt_target);

  // remove listeners when hidden
  this.eventHandler_.removeAll();
};


/**
 * Returns the selected item
 *
 * @return {Object} The item selected or null if no item is selected.
 */
goog.ui.MenuBase.prototype.getSelectedItem = function() {
  'use strict';
  return null;
};


/**
 * Sets the selected item
 *
 * @param {Object} item The item to select. The type of this item is specific
 *     to the menu class.
 */
goog.ui.MenuBase.prototype.setSelectedItem = function(item) {};


/**
 * Mouse over handler for the menu. Derived classes should override.
 *
 * @param {goog.events.Event} e The event object.
 * @protected
 */
goog.ui.MenuBase.prototype.onMouseOver = function(e) {};


/**
 * Mouse out handler for the menu. Derived classes should override.
 *
 * @param {goog.events.Event} e The event object.
 * @protected
 */
goog.ui.MenuBase.prototype.onMouseOut = function(e) {};


/**
 * Mouse down handler for the menu. Derived classes should override.
 *
 * @param {!goog.events.Event} e The event object.
 * @protected
 */
goog.ui.MenuBase.prototype.onMouseDown = function(e) {};


/**
 * Mouse up handler for the menu. Derived classes should override.
 *
 * @param {goog.events.Event} e The event object.
 * @protected
 */
goog.ui.MenuBase.prototype.onMouseUp = function(e) {};


/**
 * Key down handler for the menu. Derived classes should override.
 *
 * @param {goog.events.KeyEvent} e The event object.
 * @protected
 */
goog.ui.MenuBase.prototype.onKeyDown = function(e) {};
