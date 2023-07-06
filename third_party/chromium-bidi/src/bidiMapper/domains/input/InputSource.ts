/**
 * Copyright 2023 Google LLC.
 * Copyright (c) Microsoft Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import type {Input} from '../../../protocol/protocol.js';

export const enum SourceType {
  Key = 'key',
  Pointer = 'pointer',
  Wheel = 'wheel',
  None = 'none',
}

export class NoneSource {
  type = SourceType.None as const;
}
export class KeySource {
  type = SourceType.Key as const;
  pressed = new Set<string>();

  // This is a bitfield that matches the modifiers parameter of
  // https://chromedevtools.github.io/devtools-protocol/tot/Input/#method-dispatchKeyEvent
  #modifiers = 0;
  get modifiers(): number {
    return this.#modifiers;
  }
  get alt(): boolean {
    return (this.#modifiers & 1) === 1;
  }
  set alt(value: boolean) {
    this.#setModifier(value, 1);
  }
  get ctrl(): boolean {
    return (this.#modifiers & 2) === 2;
  }
  set ctrl(value: boolean) {
    this.#setModifier(value, 2);
  }
  get meta(): boolean {
    return (this.#modifiers & 4) === 4;
  }
  set meta(value: boolean) {
    this.#setModifier(value, 4);
  }
  get shift(): boolean {
    return (this.#modifiers & 8) === 8;
  }
  set shift(value: boolean) {
    this.#setModifier(value, 8);
  }

  #setModifier(value: boolean, bit: number) {
    if (value) {
      this.#modifiers |= bit;
    } else {
      this.#modifiers &= ~bit;
    }
  }
}

interface ClickContext {
  x: number;
  y: number;
  timeStamp: number;
}

export class PointerSource {
  type = SourceType.Pointer as const;
  subtype: Input.PointerType;
  pointerId: number;
  pressed = new Set<number>();
  x = 0;
  y = 0;

  constructor(id: number, subtype: Input.PointerType) {
    this.pointerId = id;
    this.subtype = subtype;
  }

  // This is a bitfield that matches the buttons parameter of
  // https://chromedevtools.github.io/devtools-protocol/tot/Input/#method-dispatchMouseEvent
  get buttons(): number {
    let buttons = 0;
    for (const button of this.pressed) {
      switch (button) {
        case 0:
          buttons |= 1;
          break;
        case 1:
          buttons |= 4;
          break;
        case 2:
          buttons |= 2;
          break;
        case 3:
          buttons |= 8;
          break;
        case 4:
          buttons |= 16;
          break;
      }
    }
    return buttons;
  }

  // --- Platform-specific state starts here ---

  // Input.dispatchMouseEvent doesn't know the concept of double click, so we
  // need to create it like for OSes:
  // https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:ui/events/event.cc;l=479
  static #DOUBLE_CLICK_TIME_MS = 500;
  static #MAX_DOUBLE_CLICK_RADIUS = 2;
  #clickCount = 0;
  #lastClick?: ClickContext;
  setClickCount(context: ClickContext) {
    if (
      !this.#lastClick ||
      // The click needs to be within a certain amount of ms.
      context.timeStamp - this.#lastClick.timeStamp >
        PointerSource.#DOUBLE_CLICK_TIME_MS ||
      // The click needs to be within a square radius.
      Math.abs(this.#lastClick.x - context.x) >
        PointerSource.#MAX_DOUBLE_CLICK_RADIUS ||
      Math.abs(this.#lastClick.y - context.y) >
        PointerSource.#MAX_DOUBLE_CLICK_RADIUS
    ) {
      this.#clickCount = 0;
    }
    ++this.#clickCount;
    this.#lastClick = context;
  }

  get clickCount(): number {
    return this.#clickCount;
  }
  // --- Platform-specific state ends here ---
}

export class WheelSource {
  type = SourceType.Wheel as const;
}

export type InputSource = NoneSource | KeySource | PointerSource | WheelSource;

export type InputSourceFor<Type extends SourceType> =
  Type extends SourceType.Key
    ? KeySource
    : Type extends SourceType.Pointer
    ? PointerSource
    : Type extends SourceType.Wheel
    ? WheelSource
    : NoneSource;
