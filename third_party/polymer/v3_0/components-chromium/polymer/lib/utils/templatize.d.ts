// tslint:disable:variable-name Describing an API that's defined elsewhere.
// tslint:disable:no-any describes the API as best we are able today

import {PropertyEffects} from '../mixins/property-effects.js';

import {MutableData} from '../mixins/mutable-data.js';

export {showHideChildren};

declare function showHideChildren(): void;

declare class TemplateInstanceBase extends
  PropertyEffects(
  HTMLElement) {
  root: StampedTemplate;
  children: any;

  /**
   * Find the parent model of this template instance.  The parent model
   * is either another templatize instance that had option `parentModel: true`,
   * or else the host element.
   */
  readonly parentModel: PropertyEffects;
  _methodHost: PropertyEffects;

  /**
   * Override point for adding custom or simulated event handling.
   *
   * @param node Node to add event listener to
   * @param eventName Name of event
   * @param handler Listener function to add
   */
  _addEventListenerToNode(node: Node, eventName: string, handler: (p0: Event) => void): void;

  /**
   * Overrides default property-effects implementation to intercept
   * textContent bindings while children are "hidden" and cache in
   * private storage for later retrieval.
   *
   * @param node The node to set a property on
   * @param prop The property to set
   * @param value The value to set
   */
  _setUnmanagedPropertyToNode(node: Node, prop: string, value: any): void;

  /**
   * Forwards a host property to this instance.  This method should be
   * called on instances from the `options.forwardHostProp` callback
   * to propagate changes of host properties to each instance.
   *
   * Note this method enqueues the change, which are flushed as a batch.
   *
   * @param prop Property or path name
   * @param value Value of the property to forward
   */
  forwardHostProp(prop: string, value: any): void;

  /**
   * Shows or hides the template instance top level child elements. For
   * text nodes, `textContent` is removed while "hidden" and replaced when
   * "shown."
   *
   * @param hide Set to true to hide the children;
   * set to false to show them.
   */
  _showHideChildren(hide: boolean): void;

  /**
   * Stub of HTMLElement's `dispatchEvent`, so that effects that may
   * dispatch events safely no-op.
   *
   * @param event Event to dispatch
   * @returns Always true.
   */
  dispatchEvent(event: Event|null): boolean;
}

declare class templatizerBase extends TemplateInstanceBase {
}

export {templatize};


/**
 * Returns an anonymous `PropertyEffects` class bound to the
 * `<template>` provided.  Instancing the class will result in the
 * template being stamped into a document fragment stored as the instance's
 * `root` property, after which it can be appended to the DOM.
 *
 * Templates may utilize all Polymer data-binding features as well as
 * declarative event listeners.  Event listeners and inline computing
 * functions in the template will be called on the host of the template.
 *
 * The constructor returned takes a single argument dictionary of initial
 * property values to propagate into template bindings.  Additionally
 * host properties can be forwarded in, and instance properties can be
 * notified out by providing optional callbacks in the `options` dictionary.
 *
 * Valid configuration in `options` are as follows:
 *
 * - `forwardHostProp(property, value)`: Called when a property referenced
 *   in the template changed on the template's host. As this library does
 *   not retain references to templates instanced by the user, it is the
 *   templatize owner's responsibility to forward host property changes into
 *   user-stamped instances.  The `instance.forwardHostProp(property, value)`
 *    method on the generated class should be called to forward host
 *   properties into the template to prevent unnecessary property-changed
 *   notifications. Any properties referenced in the template that are not
 *   defined in `instanceProps` will be notified up to the template's host
 *   automatically.
 * - `instanceProps`: Dictionary of property names that will be added
 *   to the instance by the templatize owner.  These properties shadow any
 *   host properties, and changes within the template to these properties
 *   will result in `notifyInstanceProp` being called.
 * - `mutableData`: When `true`, the generated class will skip strict
 *   dirty-checking for objects and arrays (always consider them to be
 *   "dirty").
 * - `notifyInstanceProp(instance, property, value)`: Called when
 *   an instance property changes.  Users may choose to call `notifyPath`
 *   on e.g. the owner to notify the change.
 * - `parentModel`: When `true`, events handled by declarative event listeners
 *   (`on-event="handler"`) will be decorated with a `model` property pointing
 *   to the template instance that stamped it.  It will also be returned
 *   from `instance.parentModel` in cases where template instance nesting
 *   causes an inner model to shadow an outer model.
 *
 * All callbacks are called bound to the `owner`. Any context
 * needed for the callbacks (such as references to `instances` stamped)
 * should be stored on the `owner` such that they can be retrieved via
 * `this`.
 *
 * When `options.forwardHostProp` is declared as an option, any properties
 * referenced in the template will be automatically forwarded from the host of
 * the `<template>` to instances, with the exception of any properties listed in
 * the `options.instanceProps` object.  `instanceProps` are assumed to be
 * managed by the owner of the instances, either passed into the constructor
 * or set after the fact.  Note, any properties passed into the constructor will
 * always be set to the instance (regardless of whether they would normally
 * be forwarded from the host).
 *
 * Note that `templatize()` can be run only once for a given `<template>`.
 * Further calls will result in an error. Also, there is a special
 * behavior if the template was duplicated through a mechanism such as
 * `<dom-repeat>` or `<test-fixture>`. In this case, all calls to
 * `templatize()` return the same class for all duplicates of a template.
 * The class returned from `templatize()` is generated only once using
 * the `options` from the first call. This means that any `options`
 * provided to subsequent calls will be ignored. Therefore, it is very
 * important not to close over any variables inside the callbacks. Also,
 * arrow functions must be avoided because they bind the outer `this`.
 * Inside the callbacks, any contextual information can be accessed
 * through `this`, which points to the `owner`.
 *
 * @returns Generated class bound
 *   to the template provided
 */
declare function templatize(template: HTMLTemplateElement, owner?: PropertyEffects|null, options?: object|null): {new(p0?: object|null): TemplateInstanceBase};

declare class baseClass extends TemplateInstanceBase {
}

export {modelForElement};


/**
 * Returns the template "model" associated with a given element, which
 * serves as the binding scope for the template instance the element is
 * contained in. A template model is an instance of
 * `TemplateInstanceBase`, and should be used to manipulate data
 * associated with this template instance.
 *
 * Example:
 *
 *   let model = modelForElement(el);
 *   if (model.index < 10) {
 *     model.set('item.checked', true);
 *   }
 *
 * @returns Template instance representing the
 *   binding scope for the element
 */
declare function modelForElement(template: HTMLElement|null, node?: Node|null): TemplateInstanceBase|null;

export {TemplateInstanceBase};

import {StampedTemplate} from '../../interfaces';
