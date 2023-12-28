// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchPropertyChange} from 'chrome://resources/ash/common/cr_deprecated.js';


export type PropertyChangeEvent<T> = Event&{
  propertyName: string,
  newValue?: T,
  oldValue?: T,
};

/**
 * Setter used by the deprecated cr.ui elements.
 * It sets the value of type T in the private `${name}_`.
 *
 * It also dispatches the event `${name}Changed` when the value actually
 * changes.
 */
export function jsSetter<T>(self: any, name: string, value: T) {
  const privateName = `${name}_`;
  const oldValue = self[name];
  if (value !== oldValue) {
    self[privateName] = value;
    dispatchPropertyChange(self, name, value, oldValue);
  }
}

/** Converts camelCase to DOM style casing: myName => my-name. */
export function convertToKebabCase(jsName: string): string {
  return jsName.replace(/([A-Z])/g, '-$1').toLowerCase();
}

/**
 * Setter used by the deprecated cr.ui elements.
 * It sets or removes the DOM attribute, the attribute name is converted
 * from the camelCase to DOM style case myName => my-name.
 *
 * It also dispatches the event `${name}Changed` when the value actually
 * changes.
 */
export function boolAttrSetter(self: any, name: string, value: boolean) {
  const attributeName = convertToKebabCase(name);
  const oldValue = self[name];
  if (value !== oldValue) {
    if (value) {
      self.setAttribute(attributeName, name);
    } else {
      self.removeAttribute(attributeName);
    }
    dispatchPropertyChange(self, name, value, oldValue);
  }
}

/**
 * Setter used by the deprecated cr.ui elements.
 * It sets the value of type T in the DOM `${name}`. NOTE: Name is converted
 * from the camelCase to DOM style case myName => my-name.
 *
 * It also dispatches the event `${name}Changed` when the value actually
 * changes.
 */
export function domAttrSetter(self: any, name: string, value: unknown) {
  const attributeName = convertToKebabCase(name);
  const oldValue = self[name];
  if (value === undefined) {
    self.removeAttribute(attributeName);
  } else {
    self.setAttribute(attributeName, value);
  }
  dispatchPropertyChange(self, name, value, oldValue);
}

/**
 * Used by the deprecated cr.ui elements. It receives a regular DOM element
 * (like a <div>) and injects the cr.ui element implementation methods in that
 * instance.
 *
 * It then calls the cr.ui element's `decorate()` which is the initializer for
 * its state, since it cannot run the constructor().
 */
export function
crInjectTypeAndInit<T extends HTMLElement, U extends typeof DecoratableElement>(
    el: T, implementationClass: U): T&InstanceType<U> {
  if (implementationClass.prototype.isPrototypeOf(el)) {
    return el as T & InstanceType<U>;
  }

  // Inject the methods of the DecoratableElement in the HTMLElement.
  Object.setPrototypeOf(el, implementationClass.prototype);

  // Initialize since it doesn't run the constructor.
  (el as InstanceType<U>).initialize();

  return el as T & InstanceType<U>;
}

export abstract class DecoratableElement {
  // If the class has a static decorate it can't be used with
  // crInjectTypeAndInit(), use TheClass.decorate(...) instead.
  static decorate?: never;

  abstract initialize(): void;
}
