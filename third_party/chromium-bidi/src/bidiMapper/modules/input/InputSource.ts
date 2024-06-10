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

export class PointerSource {
  type = SourceType.Pointer as const;
  subtype: Input.PointerType;
  pointerId: number;
  pressed = new Set<number>();
  x = 0;
  y = 0;
  radiusX?: number;
  radiusY?: number;
  force?: number;

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

  // --- Platform-specific code starts here ---
  // Input.dispatchMouseEvent doesn't know the concept of double click, so we
  // need to create the logic, similar to how it's done for OSes:
  // https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:ui/events/event.cc;l=479
  static ClickContext = class ClickContext {
    static #DOUBLE_CLICK_TIME_MS = 500;
    static #MAX_DOUBLE_CLICK_RADIUS = 2;

    count = 0;

    #x;
    #y;
    #time;
    constructor(x: number, y: number, time: number) {
      this.#x = x;
      this.#y = y;
      this.#time = time;
    }

    compare(context: ClickContext) {
      return (
        // The click needs to be within a certain amount of ms.
        context.#time - this.#time > ClickContext.#DOUBLE_CLICK_TIME_MS ||
        // The click needs to be within a certain square radius.
        Math.abs(context.#x - this.#x) >
          ClickContext.#MAX_DOUBLE_CLICK_RADIUS ||
        Math.abs(context.#y - this.#y) > ClickContext.#MAX_DOUBLE_CLICK_RADIUS
      );
    }
  };

  #clickContexts = new Map<
    number,
    InstanceType<typeof PointerSource.ClickContext>
  >();

  setClickCount(
    button: number,
    context: InstanceType<typeof PointerSource.ClickContext>
  ) {
    let storedContext = this.#clickContexts.get(button);
    if (!storedContext || storedContext.compare(context)) {
      storedContext = context;
    }
    ++storedContext.count;
    this.#clickContexts.set(button, storedContext);
    return storedContext.count;
  }

  getClickCount(button: number) {
    return this.#clickContexts.get(button)?.count ?? 0;
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
