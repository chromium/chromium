// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrLitElement} from '/resources/lit/v3_0/lit.rollup.js';

type Constructor<T> = new (...args: any[]) => T;

export interface MyTestMixinInterface {
  mixinString: string;
}

export const MyTestMixin =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<MyTestMixinInterface> => {
      class MyTestMixin extends superClass {
        static get properties() {
          return {
            mixinString: {type: String},
          };
        }

        mixinString: string = 'hello mixin';
      }

      return MyTestMixin;
    };
