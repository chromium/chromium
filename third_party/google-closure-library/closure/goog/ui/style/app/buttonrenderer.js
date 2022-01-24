/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Renderer for {@link goog.ui.Button}s in App style.
 *
 * Based on ImagelessButtonRender. Uses even more CSS voodoo than the default
 * implementation to render custom buttons with fake rounded corners and
 * dimensionality (via a subtle flat shadow on the bottom half of the button)
 * without the use of images.
 *
 * Based on the Custom Buttons 3.1 visual specification, see
 * http://go/custombuttons
 */

goog.provide('goog.ui.style.app.ButtonRenderer');

goog.require('goog.dom.TagName');
goog.require('goog.dom.classlist');
goog.require('goog.ui.Button');
goog.require('goog.ui.CustomButtonRenderer');
goog.require('goog.ui.INLINE_BLOCK_CLASSNAME');
goog.require('goog.ui.registry');
goog.requireType('goog.dom.DomHelper');
goog.requireType('goog.ui.ControlContent');



/**
 * Custom renderer for {@link goog.ui.Button}s. Imageless buttons can contain
 * almost arbitrary HTML content, will flow like inline elements, but can be
 * styled like block-level elements.
 *
 * @constructor
 * @extends {goog.ui.CustomButtonRenderer}
 */
goog.ui.style.app.ButtonRenderer = function() {
  'use strict';
  goog.ui.CustomButtonRenderer.call(this);
};
goog.inherits(goog.ui.style.app.ButtonRenderer, goog.ui.CustomButtonRenderer);
goog.addSingletonGetter(goog.ui.style.app.ButtonRenderer);


/**
 * Default CSS class to be applied to the root element of components rendered
 * by this renderer.
 * @type {string}
 */
goog.ui.style.app.ButtonRenderer.CSS_CLASS = goog.getCssName('goog-button');


/**
 * Array of arrays of CSS classes that we want composite classes added and
 * removed for in IE6 and lower as a workaround for lack of multi-class CSS
 * selector support.
 * @type {!Array<Array<string>>}
 */
goog.ui.style.app.ButtonRenderer.IE6_CLASS_COMBINATIONS = [];


/**
 * Returns the button's contents wrapped in the following DOM structure:
 *
 *    <div class="goog-inline-block goog-button-base goog-button">
 *      <div class="goog-inline-block goog-button-base-outer-box">
 *        <div class="goog-button-base-inner-box">
 *          <div class="goog-button-base-pos">
 *            <div class="goog-button-base-top-shadow">&nbsp;</div>
 *            <div class="goog-button-base-content">Contents...</div>
 *          </div>
 *        </div>
 *      </div>
 *    </div>
 * @override
 */
goog.ui.style.app.ButtonRenderer.prototype.createDom;


/** @override */
goog.ui.style.app.ButtonRenderer.prototype.getContentElement = function(
    element) {
  'use strict';
  return element && /** @type {Element} */
      (element.firstChild.firstChild.firstChild.lastChild);
};


/**
 * Takes a text caption or existing DOM structure, and returns the content
 * wrapped in a pseudo-rounded-corner box.  Creates the following DOM structure:
 *
 *    <div class="goog-inline-block goog-button-base-outer-box">
 *      <div class="goog-inline-block goog-button-base-inner-box">
 *        <div class="goog-button-base-pos">
 *          <div class="goog-button-base-top-shadow">&nbsp;</div>
 *          <div class="goog-button-base-content">Contents...</div>
 *        </div>
 *      </div>
 *    </div>
 *
 * Used by both {@link #createDom} and {@link #decorate}.  To be overridden
 * by subclasses.
 * @param {goog.ui.ControlContent} content Text caption or DOM structure to wrap
 *     in a box.
 * @param {goog.dom.DomHelper} dom DOM helper, used for document interaction.
 * @return {!Element} Pseudo-rounded-corner box containing the content.
 * @override
 */
goog.ui.style.app.ButtonRenderer.prototype.createButton = function(
    content, dom) {
  'use strict';
  const baseClass = this.getStructuralCssClass();
  const inlineBlock = goog.ui.INLINE_BLOCK_CLASSNAME + ' ';
  return dom.createDom(
      goog.dom.TagName.DIV,
      inlineBlock + goog.getCssName(baseClass, 'outer-box'),
      dom.createDom(
          goog.dom.TagName.DIV,
          inlineBlock + goog.getCssName(baseClass, 'inner-box'),
          dom.createDom(
              goog.dom.TagName.DIV, goog.getCssName(baseClass, 'pos'),
              dom.createDom(
                  goog.dom.TagName.DIV,
                  goog.getCssName(baseClass, 'top-shadow'), '\u00A0'),
              dom.createDom(
                  goog.dom.TagName.DIV, goog.getCssName(baseClass, 'content'),
                  content))));
};


/**
 * Check if the button's element has a box structure.
 * @param {goog.ui.Button} button Button instance whose structure is being
 *     checked.
 * @param {Element} element Element of the button.
 * @return {boolean} Whether the element has a box structure.
 * @protected
 * @override
 */
goog.ui.style.app.ButtonRenderer.prototype.hasBoxStructure = function(
    button, element) {
  'use strict';
  const baseClass = this.getStructuralCssClass();
  const outer = button.getDomHelper().getFirstElementChild(element);
  const outerClassName = goog.getCssName(baseClass, 'outer-box');
  if (outer && goog.dom.classlist.contains(outer, outerClassName)) {
    const inner = button.getDomHelper().getFirstElementChild(outer);
    const innerClassName = goog.getCssName(baseClass, 'inner-box');
    if (inner && goog.dom.classlist.contains(inner, innerClassName)) {
      const pos = button.getDomHelper().getFirstElementChild(inner);
      const posClassName = goog.getCssName(baseClass, 'pos');
      if (pos && goog.dom.classlist.contains(pos, posClassName)) {
        const shadow = button.getDomHelper().getFirstElementChild(pos);
        const shadowClassName = goog.getCssName(baseClass, 'top-shadow');
        if (shadow && goog.dom.classlist.contains(shadow, shadowClassName)) {
          const content = button.getDomHelper().getNextElementSibling(shadow);
          const contentClassName = goog.getCssName(baseClass, 'content');
          if (content &&
              goog.dom.classlist.contains(content, contentClassName)) {
            // We have a proper box structure.
            return true;
          }
        }
      }
    }
  }
  return false;
};


/** @override */
goog.ui.style.app.ButtonRenderer.prototype.getCssClass = function() {
  'use strict';
  return goog.ui.style.app.ButtonRenderer.CSS_CLASS;
};


/** @override */
goog.ui.style.app.ButtonRenderer.prototype.getStructuralCssClass = function() {
  'use strict';
  // TODO(user): extract to a constant.
  return goog.getCssName('goog-button-base');
};


/** @override */
goog.ui.style.app.ButtonRenderer.prototype.getIe6ClassCombinations =
    function() {
  'use strict';
  return goog.ui.style.app.ButtonRenderer.IE6_CLASS_COMBINATIONS;
};



// Register a decorator factory function for goog.ui.style.app.ButtonRenderer.
goog.ui.registry.setDecoratorByClassName(
    goog.ui.style.app.ButtonRenderer.CSS_CLASS, function() {
      'use strict';
      return new goog.ui.Button(
          null, goog.ui.style.app.ButtonRenderer.getInstance());
    });
