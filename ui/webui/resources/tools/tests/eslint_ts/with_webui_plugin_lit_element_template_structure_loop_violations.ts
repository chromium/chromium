// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

export interface LoopItem {
  id: string;
  name: string;
  children: Array<{label: string}>;
}

export function asyncMap<T>(
    items: T[], callback: (item: T, index: number) => unknown,
    _initialCount: number): unknown {
  return items.map(callback);
}

export class LoopVariablesDummyElement extends CrLitElement {
  static get is() {
    return 'loop-variables-dummy';
  }

  static get properties() {
    return {
      items: {type: Array},
      pairs: {type: Array},
      groups: {type: Array},
    };
  }

  accessor items: LoopItem[] = [];
  accessor pairs: Array<[string, number]> = [];
  accessor groups: LoopItem[] = [];
}
