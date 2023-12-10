// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LitElement} from 'lit/index.js';

type ElementCache = Record<string, HTMLElement>;

export class CrLitElement extends LitElement {
  $: ElementCache;

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
      get(cache: ElementCache, id: string): HTMLElement {
        // First look whether the element has already been retrieved previously.
        if (id in cache) {
          return cache[id]!;
        }

        // Otherwise query the shadow DOM and cache the reference for later use.
        const element = self.shadowRoot!.querySelector<HTMLElement>(`#${id}`);
        if (element === null) {
          throw new Error(`CrLitElement: Failed to find child with id ${id}`);
        }
        cache[id] = element;

        return element;
      },
    });
  }
}
