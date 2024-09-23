// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LitElement, PropertyValues} from 'lit/index.js';

type ElementCache = Record<string, HTMLElement|SVGElement>;

// Converts a 'nameLikeThis' to 'name-like-this'.
function toDashCase(name: string): string {
  return name.replace(/([a-z])([A-Z])/g, '$1-$2').toLowerCase();
}

export class CrLitElement extends LitElement {
  $: ElementCache;
  private willUpdatePending_: boolean = false;

  // Properties for which a '<property-name>-changed' event should be fired
  // whenever they change.
  private static notifyProps_: Set<PropertyKey>|null = null;

  constructor() {
    super();

    // Lazily populate a helper `$` object for easy access to any child elements
    // in the `shadowRoot` that have an ID. This works similarly to the
    // equivalent Polymer functionality, except that it can also work for
    // elements that don't exist in the DOM at the time the element is
    // connected. Should only be called after firstUpdated() lifecycle method
    // has been called or within firstUpdated() itself. Children accessed this
    // way are expected to exist in the DOM for the full lifetime of this
    // element (never removed).
    const self = this;
    this.$ = new Proxy({}, {
      get(cache: ElementCache, id: string): HTMLElement|SVGElement {
        if (!self.hasUpdated && !self.isConnected) {
          throw new Error(`CrLitElement ${
              self.tagName} $ dictionary accessed before element is connected at least once.`);
        }

        if (!self.hasUpdated) {
          // Ensure not within a willUpdate() call, otherwise the
          // performUpdate() call below will cause an endless recursion. Local
          // DOM nodes should not be accessed within willUpdate() anyway.
          if (self.willUpdatePending_) {
            throw new Error(`CrLitElement ${
                self.tagName} tried to access this.$ within willUpdate().`);
          }

          // See Case3 in `ensureInitialRender` docs.
          self.performUpdate();
        }

        // First look whether the element has already been retrieved previously.
        if (id in cache) {
          return cache[id]!;
        }

        // Otherwise query the shadow DOM and cache the reference for later use.
        const element = self.shadowRoot!.querySelector<HTMLElement>(`#${id}`);
        if (element === null) {
          throw new Error(`CrLitElement ${
              self.tagName}: Failed to find child with id ${id}`);
        }
        cache[id] = element;

        return element;
      },
    });
  }

  // In a few cases it is necessary to force-render the initial state
  // synchronously instead of waiting for Lit's asynchronous initial render, to
  // make the initial render behavior similar to Polymer, and consequently make
  // migrating from Polymer to Lit easier. Documented known such cases below.
  //
  // Case1: Calling synchronous APIs that access the ShadowDOM.
  // Addressed by the call in connectedCallback().
  //
  // For example CrActionMenuElement provides synchronous APIs showAt(),
  // showAtPosition(), close(), getDialog(), and client code should be able to
  // call these immediately after attaching this element to the DOM, without
  // having to wait for `updateComplete`.
  //
  // Case2: Calling focus() right after a parent dom-if template is stamped.
  // Addressed by CrLitElement's focus() override.
  //
  // This can happen when the following hierarchy is encountered:
  // <dom-if> grandparent > Polymer parent element > Lit child element
  // When the dom-if is stamped, and the parent's connectedCallback() is called,
  // the Lit child's connectedCallback() has not fired yet (unlike Polymer
  // children, which use `_enqueueClient` from [1]), which is problematic
  // when the parent element calls a synchronous API method on the Lit child
  // that assumes that the ShadowDOM is rendered, for example cr-icon-button's
  // focus().
  //
  // [1] https://github.com/Polymer/polymer/blob/1e8b246d01ea99adba305ea04c45d26da31f68f1/lib/mixins/property-effects.js#L1762
  //
  // Case3: Referring to child nodes right after a parent dom-if is stamped.
  // Addressed by the effectively identical logic in the this.$ Proxy above.
  //
  // This happens when the same pattern as Case 2 above is encountered.
  ensureInitialRender() {
    if (!this.hasUpdated) {
      this.performUpdate();
    }
  }

  override connectedCallback() {
    super.connectedCallback();
    // See Case1 in `ensureInitialRender` docs.
    this.ensureInitialRender();
  }

  override willUpdate(_changedProperties: PropertyValues<this>) {
    this.willUpdatePending_ = true;
  }

  override updated(changedProperties: PropertyValues<this>) {
    this.willUpdatePending_ = false;

    const notifyProps = (this.constructor as typeof CrLitElement).notifyProps_;
    if (notifyProps !== null) {
      const indexableThis = this as Record<PropertyKey, any>;
      for (const key of changedProperties.keys()) {
        if (notifyProps.has(key)) {
          if (changedProperties.get(key as keyof CrLitElement) === undefined &&
              indexableThis[key] === undefined) {
            // Don't fire events if the property was changed back to 'undefined'
            // before the element was connected. Lit still reports such
            // properties in `changedProperties` as going from 'undefined' to
            // 'undefined'.
            continue;
          }
          this.fire(
              `${toDashCase(key.toString())}-changed`,
              {value: indexableThis[key]});
        }
      }
    }
  }

  override focus(
      options?: {preventScroll?: boolean, focusVisible?: boolean}) {
    // See Case2 in `ensureInitialRender` docs.
    this.ensureInitialRender();
    super.focus(options);
  }

  fire(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  // Modifies the 'properties' object by automatically specifying
  // "attribute: <attr_name>" for each reactive property where attr_name is a
  // dash-case equivalent of the property's name. For example a 'fooBar'
  // property will be mapped to a 'foo-bar' attribute, matching Polymer's
  // behavior, instead of Lit's default behavior (which would map to 'foobar').
  // This is done to make it easier to migrate Polymer elements to Lit.
  private static patchPropertiesObject() {
    if (!this.hasOwnProperty('properties')) {
      // Return early if there's no `properties` block on the element.
      // Note: This does not take into account properties defined with
      // decorators.
      return;
    }

    const properties = this.properties;
    for (const [key, value] of Object.entries(properties)) {
      // Skip properties that explicitly specify the attribute name.
      if (value.attribute != null) {
        continue;
      }

      type Mutable<T> = { -readonly[P in keyof T]: T[P]; };

      // Specify a dash-case attribute name, derived from the property name,
      // similar to what Polymer did.
      (value as Mutable<typeof value>).attribute = toDashCase(key);
    }

    // Mutating the properties object alone isn't enough, in the case where
    // the properties block is defined as a getter, need to also override the
    // getter.
    Object.defineProperty(this, 'properties', {value: properties});
  }

  private static populateNotifyProps(): void {
    if (!this.hasOwnProperty('properties')) {
      return;
    }

    for (const [key, value] of Object.entries(this.properties)) {
      if ((value as {notify?: boolean}).notify) {
        // Lazily create `notifyProps_` only if any such property exists.
        if (this.notifyProps_ === null) {
          this.notifyProps_ = new Set();
        }
        this.notifyProps_.add(key);
      }
    }
  }

  protected static override finalize() {
    this.patchPropertiesObject();
    this.populateNotifyProps();
    super.finalize();
  }
}
