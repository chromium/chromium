// tslint:disable:variable-name Describing an API that's defined elsewhere.

import {PolymerElement} from '../../polymer-element.js';

import {Debouncer} from '../utils/debounce.js';

import {enqueueDebouncer, flush} from '../utils/flush.js';

import {microTask} from '../utils/async.js';

import {root} from '../utils/path.js';

import {hideElementsGlobally} from '../utils/hide-template-controls.js';

import {showHideChildren, templatize} from '../utils/templatize.js';

declare class DomIfBase extends PolymerElement {
  _templateInfo: TemplateInfo|undefined;

  /**
   * A boolean indicating whether this template should stamp.
   */
  if: boolean|null|undefined;

  /**
   * When true, elements will be removed from DOM and discarded when `if`
   * becomes false and re-created and added back to the DOM when `if`
   * becomes true.  By default, stamped elements will be hidden but left
   * in the DOM when `if` becomes false, which is generally results
   * in better performance.
   */
  restamp: boolean|null|undefined;

  /**
   * When the global `suppressTemplateNotifications` setting is used, setting
   * `notifyDomChange: true` will enable firing `dom-change` events on this
   * element.
   */
  notifyDomChange: boolean|null|undefined;
  connectedCallback(): void;
  disconnectedCallback(): void;

  /**
   * Forces the element to render its content. Normally rendering is
   * asynchronous to a provoking change. This is done for efficiency so
   * that multiple changes trigger only a single render. The render method
   * should be called if, for example, template rendering is required to
   * validate application state.
   */
  render(): void;

  /**
   * Abstract API to be implemented by subclass: Returns true if a template
   * instance has been created and inserted.
   *
   * @returns True when an instance has been created.
   */
  __hasInstance(): boolean;

  /**
   * Abstract API to be implemented by subclass: Returns the child nodes stamped
   * from a template instance.
   *
   * @returns Array of child nodes stamped from the template
   * instance.
   */
  __getInstanceNodes(): Array<Node|null>|null;

  /**
   * Abstract API to be implemented by subclass: Creates an instance of the
   * template and inserts it into the given parent node.
   *
   * @param parentNode The parent node to insert the instance into
   */
  __createAndInsertInstance(parentNode: Node|null): void;

  /**
   * Abstract API to be implemented by subclass: Removes nodes created by an
   * instance of a template and any associated cleanup.
   */
  __teardownInstance(): void;

  /**
   * Abstract API to be implemented by subclass: Shows or hides any template
   * instance childNodes based on the `if` state of the element and its
   * `__hideTemplateChildren__` property.
   */
  _showHideChildren(): void;
}

declare global {

  interface HTMLElementTagNameMap {
    "dom-if": DomIfBase;
  }
}

/**
 * The version of DomIf used when `fastDomIf` setting is in use, which is
 * optimized for first-render (but adds a tax to all subsequent property updates
 * on the host, whether they were used in a given `dom-if` or not).
 *
 * This implementation avoids use of `Templatizer`, which introduces a new scope
 * (a non-element PropertyEffects instance), which is not strictly necessary
 * since `dom-if` never introduces new properties to its scope (unlike
 * `dom-repeat`). Taking advantage of this fact, the `dom-if` reaches up to its
 * `__dataHost` and stamps the template directly from the host using the host's
 * runtime `_stampTemplate` API, which binds the property effects of the
 * template directly to the host. This both avoids the intermediary
 * `Templatizer` instance, but also avoids the need to bind host properties to
 * the `<template>` element and forward those into the template instance.
 *
 * In this version of `dom-if`, the `this.__instance` method is the
 * `DocumentFragment` returned from `_stampTemplate`, which also serves as the
 * handle for later removing it using the `_removeBoundDom` method.
 */
declare class DomIfFast extends DomIfBase {
  constructor();

  /**
   * Implementation of abstract API needed by DomIfBase.
   *
   * Shows or hides the template instance top level child nodes. For
   * text nodes, `textContent` is removed while "hidden" and replaced when
   * "shown."
   */
  _showHideChildren(): void;
}

/**
 * The "legacy" implementation of `dom-if`, implemented using `Templatizer`.
 *
 * In this version, `this.__instance` is the `TemplateInstance` returned
 * from the templatized constructor.
 */
declare class DomIfLegacy extends DomIfBase {
  constructor();

  /**
   * Implementation of abstract API needed by DomIfBase.
   *
   * Shows or hides the template instance top level child elements. For
   * text nodes, `textContent` is removed while "hidden" and replaced when
   * "shown."
   */
  _showHideChildren(): void;
}

export {DomIf};

/**
 * The `<dom-if>` element will stamp a light-dom `<template>` child when
 * the `if` property becomes truthy, and the template can use Polymer
 * data-binding and declarative event features when used in the context of
 * a Polymer element's template.
 *
 * When `if` becomes falsy, the stamped content is hidden but not
 * removed from dom. When `if` subsequently becomes truthy again, the content
 * is simply re-shown. This approach is used due to its favorable performance
 * characteristics: the expense of creating template content is paid only
 * once and lazily.
 *
 * Set the `restamp` property to true to force the stamped content to be
 * created / destroyed when the `if` condition changes.
 */
declare class DomIf extends DomIfBase {
}

import {TemplateInfo} from '../../interfaces';
