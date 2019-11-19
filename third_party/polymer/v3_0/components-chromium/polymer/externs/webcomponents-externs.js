/**
 * @fileoverview Externs for webcomponents polyfills
 * @externs
 *
 * @license
 * Copyright (c) 2017 The Polymer Project Authors. All rights reserved.
 * This code may only be used under the BSD style license found at http://polymer.github.io/LICENSE.txt
 * The complete set of authors may be found at http://polymer.github.io/AUTHORS.txt
 * The complete set of contributors may be found at http://polymer.github.io/CONTRIBUTORS.txt
 * Code distributed by Google as part of the polymer project is also
 * subject to an additional IP rights grant found at http://polymer.github.io/PATENTS.txt
 */
/* eslint-disable */

var HTMLImports = {
  /**
   * @param {function()} callback
   */
  whenReady(callback) {},
  /**
   * @param {!Node} element
   * @return {?HTMLLinkElement|?Document|undefined}
   */
  importForElement(element) {}
};

window.HTMLImports = HTMLImports;

var ShadyDOM = {
  inUse: false,
  flush() {},
  /**
   * @param {!Node} target
   * @param {function(Array<MutationRecord>, MutationObserver)} callback
   * @return {MutationObserver}
   */
  observeChildren(target, callback) {},
  /**
   * @param {MutationObserver} observer
   */
  unobserveChildren(observer) {},
  /**
   * @param {Node} node
   */
  patch(node) {},
  /**
   * @param {!ShadowRoot} shadowroot
   */
  flushInitial(shadowroot) {}
};

window.ShadyDOM = ShadyDOM;

var WebComponents = {};
window.WebComponents = WebComponents;

/** @type {Element} */
HTMLElement.prototype._activeElement;

/**
 * @param {HTMLTemplateElement} template
 */
HTMLTemplateElement.decorate = function(template){};

/**
 * @param {function(function())} cb callback
 */
CustomElementRegistry.prototype.polyfillWrapFlushCallback = function(cb){};
