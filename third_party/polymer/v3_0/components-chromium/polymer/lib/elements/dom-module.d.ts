// tslint:disable:variable-name Describing an API that's defined elsewhere.
// tslint:disable:no-any describes the API as best we are able today

import {resolveUrl, pathFromUrl} from '../utils/resolve-url.js';

export {DomModule};

/**
 * The `dom-module` element registers the dom it contains to the name given
 * by the module's id attribute. It provides a unified database of dom
 * accessible via its static `import` API.
 *
 * A key use case of `dom-module` is for providing custom element `<template>`s
 * via HTML imports that are parsed by the native HTML parser, that can be
 * relocated during a bundling pass and still looked up by `id`.
 *
 * Example:
 *
 *     <dom-module id="foo">
 *       <img src="stuff.png">
 *     </dom-module>
 *
 * Then in code in some other location that cannot access the dom-module above
 *
 *     let img = customElements.get('dom-module').import('foo', 'img');
 */
declare class DomModule extends HTMLElement {

  /**
   * The absolute URL of the original location of this `dom-module`.
   *
   * This value will differ from this element's `ownerDocument` in the
   * following ways:
   * - Takes into account any `assetpath` attribute added during bundling
   *   to indicate the original location relative to the bundled location
   * - Uses the HTMLImports polyfill's `importForElement` API to ensure
   *   the path is relative to the import document's location since
   *   `ownerDocument` is not currently polyfilled
   *    
   */
  readonly assetpath: any;

  /**
   * Retrieves the element specified by the css `selector` in the module
   * registered by `id`. For example, this.import('foo', 'img');
   *
   * @param id The id of the dom-module in which to search.
   * @param selector The css selector by which to find the element.
   * @returns Returns the element which matches `selector` in the
   * module registered at the specified `id`.
   */
  static import(id: string, selector?: string): Element|null;

  /**
   * @param name Name of attribute.
   * @param old Old value of attribute.
   * @param value Current value of attribute.
   * @param namespace Attribute namespace.
   */
  attributeChangedCallback(name: string, old: string|null, value: string|null, namespace: string|null): void;

  /**
   * Registers the dom-module at a given id. This method should only be called
   * when a dom-module is imperatively created. For
   * example, `document.createElement('dom-module').register('foo')`.
   *
   * @param id The id at which to register the dom-module.
   */
  register(id?: string): void;
}

declare global {

  interface HTMLElementTagNameMap {
    "dom-module": DomModule;
  }
}
