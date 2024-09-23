// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import type {CrIconsetElement} from './cr_iconset.js';

let iconsetMap: IconsetMap|null = null;

export class IconsetMap extends EventTarget {
  private iconsets_: Map<string, CrIconsetElement> = new Map();

  static getInstance() {
    return iconsetMap || (iconsetMap = new IconsetMap());
  }

  static resetInstanceForTesting(instance: IconsetMap) {
    iconsetMap = instance;
  }

  get(id: string): CrIconsetElement|null {
    return this.iconsets_.get(id) || null;
  }

  set(id: string, iconset: CrIconsetElement) {
    assert(!this.iconsets_.has(id),
           'Tried to add a second iconset with id ' + id);
    this.iconsets_.set(id, iconset);
    this.dispatchEvent(new CustomEvent('cr-iconset-added', {detail: id}));
  }
}
