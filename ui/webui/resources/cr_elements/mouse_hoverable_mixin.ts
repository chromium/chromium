// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file provides a mixin to manage a `hovered` style on mouse
 * events. Relies on listening for pointer events as touch devices may fire
 * mouse events too.
 */

import type {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const HOVERED_STYLE: string = 'hovered';

type Constructor<T> = new (...args: any[]) => T;

export const MouseHoverableMixin =
    dedupingMixin(<T extends Constructor<PolymerElement>>(superClass: T): T => {
      class MouseHoverableMixin extends superClass {
        override ready() {
          super.ready();

          this.addEventListener('pointerenter', (e) => {
            const hostElement = e.currentTarget as HTMLElement;
            hostElement.classList.toggle(
                HOVERED_STYLE, e.pointerType === 'mouse');
          });

          this.addEventListener('pointerleave', (e) => {
            if (e.pointerType !== 'mouse') {
              return;
            }

            const hostElement = e.currentTarget as HTMLElement;
            hostElement.classList.remove(HOVERED_STYLE);
          });
        }
      }

      return MouseHoverableMixin;
    });
