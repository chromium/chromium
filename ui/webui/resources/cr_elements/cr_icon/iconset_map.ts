// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

// Common interface implemented by CrIconsetElement and IronIconset.
export interface Iconset {
  name: string;
  applyIcon(element: HTMLElement, iconName: string): SVGElement|null;
  createIcon(iconName: string): SVGElement|null;
  removeIcon(element: HTMLElement): void;
}

let iconsetMap: IconsetMap|null = null;

// Detect iron-iconsets and add them to the map. This is so that every
// iconset in the codebase does not need to migrate to CrIconset at once.
// Remove once iron-iconset is no longer used.
window.addEventListener('iron-iconset-added', e => {
  const event = e as unknown as CustomEvent<Iconset>;
  const map = IconsetMap.getInstance();
  map.setIronIconset(event.detail.name, event.detail);
});

export class IconsetMap extends EventTarget {
  private iconsets_: Map<string, Iconset> = new Map();
  private ironIconsets_: Map<string, Iconset> = new Map();

  static getInstance() {
    return iconsetMap || (iconsetMap = new IconsetMap());
  }

  get(id: string): Iconset|null {
    return this.iconsets_.get(id) || this.ironIconsets_.get(id) || null;
  }

  // Remove this method once iron-iconset is no longer used.
  setIronIconset(id: string, iconset: Iconset) {
    assert(!this.ironIconsets_.has(id),
           'Tried to add a second iron-iconset with id ' + id);
    this.ironIconsets_.set(id, iconset);
    this.dispatchEvent(new CustomEvent('cr-iconset-added', {detail: id}));
  }

  set(id: string, iconset: Iconset) {
    assert(!this.iconsets_.has(id),
           'Tried to add a second iconset with id ' + id);
    this.iconsets_.set(id, iconset);
    this.dispatchEvent(new CustomEvent('cr-iconset-added', {detail: id}));
  }
}
