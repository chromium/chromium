// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export type CustomEventMap = Record<string, CustomEvent>;

/**
 * `FilesEventTarget` is a strongly typed version of EventTarget, it accepts a
 * generic `EventMap` to restrict what kind of custom events it can add/remove.
 * For example:
 *
 * ```
 * type MyCustomEvent = CustomEvent<{ data: xxx }>
 *
 * interface MyEventMap extend CustomEventMap {
 *   'my-event': MyCustomEvent;
 * }
 *
 * class MyClass extends FilesEventTarget<MyEventMap> {}
 * ```
 *
 * We rely on TS's declaration merge to support both typing and implementation.
 * The above interface will provide the typing and the below class (same name)
 * will provide the actual implementation (no implementation override, we just
 * use whatever is provided by the native EventTarget).
 *
 */
export interface FilesEventTarget<EventMap extends CustomEventMap> {
  addEventListener<K extends keyof EventMap>(
      type: K, listener: (event: EventMap[K]) => void,
      options?: boolean|AddEventListenerOptions|undefined): void;
  addEventListener(
      type: string, callback: EventListenerOrEventListenerObject|null,
      options?: AddEventListenerOptions|boolean): void;
  removeEventListener<K extends keyof EventMap>(
      type: K, listener: (event: EventMap[K]) => void,
      options?: boolean|EventListenerOptions): void;
  removeEventListener(
      type: string, listener: EventListenerOrEventListenerObject|null,
      options?: boolean|EventListenerOptions): void;
}

// TS is complaining `EventMap` is not used and we can't use `_EventMap` here
// because we need to keep the class and interface exactly the same to do
// declaration merge, hence adding the eslint-disable below.
// eslint-disable-next-line @typescript-eslint/no-unused-vars
export class FilesEventTarget<EventMap extends CustomEventMap> extends
    EventTarget {}
