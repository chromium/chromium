// tslint:disable:variable-name Describing an API that's defined elsewhere.

import {PropertyEffects} from '../mixins/property-effects.js';

import {OptionalMutableData} from '../mixins/mutable-data.js';

import {GestureEventListeners} from '../mixins/gesture-event-listeners.js';

import {hideElementsGlobally} from '../utils/hide-template-controls.js';

export {DomBind};

/**
 * Custom element to allow using Polymer's template features (data binding,
 * declarative event listeners, etc.) in the main document without defining
 * a new custom element.
 *
 * `<template>` tags utilizing bindings may be wrapped with the `<dom-bind>`
 * element, which will immediately stamp the wrapped template into the main
 * document and bind elements to the `dom-bind` element itself as the
 * binding scope.
 */
declare class DomBind extends
  PropertyEffects(
  OptionalMutableData(
  GestureEventListeners(
  HTMLElement))) {

  /**
   * @param name Name of attribute that changed
   * @param old Old attribute value
   * @param value New attribute value
   * @param namespace Attribute namespace.
   */
  attributeChangedCallback(name: string, old: string|null, value: string|null, namespace: string|null): void;
  connectedCallback(): void;
  disconnectedCallback(): void;

  /**
   * Forces the element to render its content. This is typically only
   * necessary to call if HTMLImports with the async attribute are used.
   */
  render(): void;
}

declare global {

  interface HTMLElementTagNameMap {
    "dom-bind": DomBind;
  }
}
