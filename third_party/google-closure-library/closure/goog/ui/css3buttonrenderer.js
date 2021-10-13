/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview An alternative imageless button renderer that uses CSS3 rather
 * than voodoo to render custom buttons with rounded corners and dimensionality
 * (via a subtle flat shadow on the bottom half of the button) without the use
 * of images.
 *
 * Based on the Custom Buttons 3.1 visual specification, see
 * http://go/custombuttons
 *
 * Tested and verified to work in Gecko 1.9.2+ and WebKit 528+.
 *
 * @see ../demos/css3button.html
 */

goog.provide('goog.ui.Css3ButtonRenderer');

goog.require('goog.asserts');
goog.require('goog.dom.TagName');
goog.require('goog.dom.classlist');
goog.require('goog.ui.Button');
goog.require('goog.ui.ButtonRenderer');
goog.require('goog.ui.Component');
goog.require('goog.ui.INLINE_BLOCK_CLASSNAME');
goog.require('goog.ui.registry');
goog.requireType('goog.ui.Control');



/**
 * Custom renderer for {@link goog.ui.Button}s. Css3 buttons can contain
 * almost arbitrary HTML content, will flow like inline elements, but can be
 * styled like block-level elements.
 *
 * @constructor
 * @extends {goog.ui.ButtonRenderer}
 * @final
 */
goog.ui.Css3ButtonRenderer = function() {
  'use strict';
  goog.ui.ButtonRenderer.call(this);
};
goog.inherits(goog.ui.Css3ButtonRenderer, goog.ui.ButtonRenderer);
goog.addSingletonGetter(goog.ui.Css3ButtonRenderer);


/**
 * Default CSS class to be applied to the root element of components rendered
 * by this renderer.
 * @type {string}
 */
goog.ui.Css3ButtonRenderer.CSS_CLASS = goog.getCssName('goog-css3-button');


/** @override */
goog.ui.Css3ButtonRenderer.prototype.getContentElement = function(element) {
  'use strict';
  return /** @type {Element} */ (element);
};


/**
 * Returns the button's contents wrapped in the following DOM structure:
 *
 *    <div class="goog-inline-block goog-css3-button">
 *      Contents...
 *    </div>
 *
 * Overrides {@link goog.ui.ButtonRenderer#createDom}.
 * @param {goog.ui.Control} control goog.ui.Button to render.
 * @return {!Element} Root element for the button.
 * @override
 */
goog.ui.Css3ButtonRenderer.prototype.createDom = function(control) {
  'use strict';
  var button = /** @type {goog.ui.Button} */ (control);
  var classNames = this.getClassNames(button);
  return button.getDomHelper().createDom(
      goog.dom.TagName.DIV, {
        'class': goog.ui.INLINE_BLOCK_CLASSNAME + ' ' + classNames.join(' '),
        'title': button.getTooltip() || ''
      },
      button.getContent());
};


/**
 * Returns true if this renderer can decorate the element.  Overrides
 * {@link goog.ui.ButtonRenderer#canDecorate} by returning true if the
 * element is a DIV, false otherwise.
 * @param {Element} element Element to decorate.
 * @return {boolean} Whether the renderer can decorate the element.
 * @override
 */
goog.ui.Css3ButtonRenderer.prototype.canDecorate = function(element) {
  'use strict';
  return element.tagName == goog.dom.TagName.DIV;
};


/** @override */
goog.ui.Css3ButtonRenderer.prototype.decorate = function(button, element) {
  'use strict';
  goog.asserts.assert(element);
  goog.dom.classlist.addAll(
      element, [goog.ui.INLINE_BLOCK_CLASSNAME, this.getCssClass()]);
  return goog.ui.Css3ButtonRenderer.superClass_.decorate.call(
      this, button, element);
};


/**
 * Returns the CSS class to be applied to the root element of components
 * rendered using this renderer.
 * @return {string} Renderer-specific CSS class.
 * @override
 */
goog.ui.Css3ButtonRenderer.prototype.getCssClass = function() {
  'use strict';
  return goog.ui.Css3ButtonRenderer.CSS_CLASS;
};


// Register a decorator factory function for goog.ui.Css3ButtonRenderer.
goog.ui.registry.setDecoratorByClassName(
    goog.ui.Css3ButtonRenderer.CSS_CLASS, function() {
      'use strict';
      return new goog.ui.Button(null, goog.ui.Css3ButtonRenderer.getInstance());
    });


// Register a decorator factory function for toggle buttons using the
// goog.ui.Css3ButtonRenderer.
goog.ui.registry.setDecoratorByClassName(
    goog.getCssName('goog-css3-toggle-button'), function() {
      'use strict';
      var button =
          new goog.ui.Button(null, goog.ui.Css3ButtonRenderer.getInstance());
      button.setSupportedState(goog.ui.Component.State.CHECKED, true);
      return button;
    });
