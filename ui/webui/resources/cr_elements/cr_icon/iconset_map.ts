// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';

// Common interface implemented by CrIconsetElement and IronIconset.
export interface Iconset {
  name: string;
  applyIcon(element: HTMLElement, iconName: string): SVGElement|null;
  createIcon(iconName: string): SVGElement|null;
  removeIcon(element: HTMLElement): void;
}

let iconsetMap: IconsetMap|null = null;

export class IconsetMap extends EventTarget {
  private iconsets_: Map<string, Iconset> = new Map();
  private ironIconsets_: Map<string, Iconset> = new Map();
  private tracker_: EventTracker = new EventTracker();

  constructor() {
    super();

    // Firstly add any 'iron-iconset-svg' instances that have possibly already
    // been added by the time this instance is created.
    const iconsets =
        document.head.querySelectorAll<HTMLElement&Iconset>('iron-iconset-svg');
    for (const iconset of iconsets) {
      this.setIronIconset(iconset.name!, iconset);
    }

    // Secondly, detect any new 'iron-iconset-svg' instances and add them to the
    // map. This is so that every iconset in the codebase does not need to
    // migrate to CrIconset at once. Remove once iron-iconset is no longer used.
    this.tracker_.add(
        window, 'iron-iconset-added', (e: CustomEvent<Iconset>) => {
          this.setIronIconset(e.detail.name, e.detail);
        });
  }

  static getInstance() {
    return iconsetMap || (iconsetMap = new IconsetMap());
  }

  static resetInstanceForTesting(instance: IconsetMap) {
    if (iconsetMap !== null) {
      iconsetMap.tracker_.removeAll();
    }
    iconsetMap = instance;
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
