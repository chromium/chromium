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

import {
  type CommonDataTypes,
  Input,
  Message,
} from '../../../protocol/protocol.js';
import {assert} from '../../../utils/assert.js';
import type {BrowsingContextImpl} from '../context/browsingContextImpl.js';

import type {ActionOption} from './ActionOption.js';
import type {KeySource, PointerSource, WheelSource} from './InputSource.js';
import type {InputState} from './InputState.js';
import {KeyToKeyCode} from './USKeyboardLayout.js';
import {getNormalizedKey, getKeyCode, getKeyLocation} from './keyUtils.js';

/** https://w3c.github.io/webdriver/#dfn-center-point */
const CALCULATE_IN_VIEW_CENTER_PT_DECL = ((i: Element) => {
  const t = i.getClientRects()[0] as DOMRect,
    e = Math.max(0, Math.min(t.x, t.x + t.width)),
    n = Math.min(window.innerWidth, Math.max(t.x, t.x + t.width)),
    h = Math.max(0, Math.min(t.y, t.y + t.height)),
    m = Math.min(window.innerHeight, Math.max(t.y, t.y + t.height));
  return [e + ((n - e) >> 1), h + ((m - h) >> 1)];
}).toString();

async function getElementCenter(
  context: BrowsingContextImpl,
  element: CommonDataTypes.SharedReference
) {
  const {result} = await (
    await context.getOrCreateSandbox(undefined)
  ).callFunction(
    CALCULATE_IN_VIEW_CENTER_PT_DECL,
    {type: 'undefined'},
    [element],
    false,
    'none',
    {}
  );
  if (result.type === 'exception') {
    throw new Message.NoSuchElementException(
      `Origin element ${element.sharedId} was not found`
    );
  }
  assert(result.result.type === 'array');
  assert(result.result.value?.[0]?.type === 'number');
  assert(result.result.value?.[1]?.type === 'number');
  const {
    result: {
      value: [{value: x}, {value: y}],
    },
  } = result;
  return {x: x as number, y: y as number};
}

export class ActionDispatcher {
  #tickStart = 0;
  #tickDuration = 0;
  #inputState: InputState;
  #context: BrowsingContextImpl;
  constructor(inputState: InputState, context: BrowsingContextImpl) {
    this.#inputState = inputState;
    this.#context = context;
  }

  async dispatchActions(
    optionsByTick: readonly (readonly Readonly<ActionOption>[])[]
  ) {
    await this.#inputState.queue.run(async () => {
      for (const options of optionsByTick) {
        await this.dispatchTickActions(options);
      }
    });
  }

  async dispatchTickActions(
    options: readonly Readonly<ActionOption>[]
  ): Promise<void> {
    this.#tickStart = performance.now();
    this.#tickDuration = 0;
    for (const {action} of options) {
      if ('duration' in action && action.duration !== undefined) {
        this.#tickDuration = Math.max(this.#tickDuration, action.duration);
      }
    }
    const promises: Promise<void>[] = [
      new Promise((resolve) => setTimeout(resolve, this.#tickDuration)),
    ];
    for (const option of options) {
      promises.push(this.#dispatchAction(option));
    }
    await Promise.all(promises);
  }

  async #dispatchAction({id, action}: Readonly<ActionOption>) {
    const source = this.#inputState.get(id);
    const keyState = this.#inputState.getGlobalKeyState();
    switch (action.type) {
      case Input.ActionType.KeyDown: {
        // SAFETY: The source is validated before.
        await this.#dispatchKeyDownAction(source as KeySource, action);
        this.#inputState.cancelList.push({
          id,
          action: {
            ...action,
            type: Input.ActionType.KeyUp,
          },
        });
        break;
      }
      case Input.ActionType.KeyUp: {
        // SAFETY: The source is validated before.
        await this.#dispatchKeyUpAction(source as KeySource, action);
        break;
      }
      case Input.ActionType.Pause: {
        // TODO: Implement waiting on the input source.
        break;
      }
      case Input.ActionType.PointerDown: {
        // SAFETY: The source is validated before.
        await this.#dispatchPointerDownAction(
          source as PointerSource,
          keyState,
          action
        );
        this.#inputState.cancelList.push({
          id,
          action: {
            ...action,
            type: Input.ActionType.PointerUp,
          },
        });
        break;
      }
      case Input.ActionType.PointerMove: {
        // SAFETY: The source is validated before.
        await this.#dispatchPointerMoveAction(
          source as PointerSource,
          keyState,
          action
        );
        break;
      }
      case Input.ActionType.PointerUp: {
        // SAFETY: The source is validated before.
        await this.#dispatchPointerUpAction(
          source as PointerSource,
          keyState,
          action
        );
        break;
      }
      case Input.ActionType.Scroll: {
        // SAFETY: The source is validated before.
        await this.#dispatchScrollAction(
          source as WheelSource,
          keyState,
          action
        );
        break;
      }
    }
  }

  #dispatchPointerDownAction(
    source: PointerSource,
    keyState: KeySource,
    action: Readonly<Input.PointerDownAction>
  ) {
    const {button} = action;
    if (source.pressed.has(button)) {
      return;
    }
    source.pressed.add(button);
    const {x, y, subtype: pointerType} = source;
    const {width, height, pressure, twist, tangentialPressure} = action;
    const {tiltX, tiltY} = 'tiltX' in action ? action : ({} as never);
    // TODO: Implement azimuth/altitude angle.

    // --- Platform-specific code begins here ---
    const {modifiers} = keyState;
    switch (pointerType) {
      case Input.PointerType.Mouse:
      case Input.PointerType.Pen:
        source.setClickCount({x, y, timeStamp: performance.now()});
        // TODO: Implement width and height when available.
        return this.#context.cdpTarget.cdpClient.sendCommand(
          'Input.dispatchMouseEvent',
          {
            type: 'mousePressed',
            x,
            y,
            modifiers,
            button: (() => {
              switch (button) {
                case 0:
                  return 'left';
                case 1:
                  return 'middle';
                case 2:
                  return 'right';
                case 3:
                  return 'back';
                case 4:
                  return 'forward';
                default:
                  return 'none';
              }
            })(),
            buttons: source.buttons,
            clickCount: source.clickCount,
            pointerType,
            tangentialPressure,
            tiltX,
            tiltY,
            twist,
            force: pressure,
          }
        );
      case Input.PointerType.Touch:
        return this.#context.cdpTarget.cdpClient.sendCommand(
          'Input.dispatchTouchEvent',
          {
            type: 'touchStart',
            touchPoints: [
              {
                x,
                y,
                radiusX: width,
                radiusY: height,
                tangentialPressure,
                tiltX,
                tiltY,
                twist,
                force: pressure,
                id: source.pointerId,
              },
            ],
            modifiers,
          }
        );
    }
    // --- Platform-specific code ends here ---
  }

  #dispatchPointerUpAction(
    source: PointerSource,
    keyState: KeySource,
    action: Readonly<Input.PointerUpAction>
  ) {
    const {button} = action;
    if (!source.pressed.has(button)) {
      return;
    }
    source.pressed.delete(button);
    const {x, y, subtype: pointerType} = source;

    // --- Platform-specific code begins here ---
    const {modifiers} = keyState;
    switch (pointerType) {
      case Input.PointerType.Mouse:
      case Input.PointerType.Pen:
        // TODO: Implement width and height when available.
        return this.#context.cdpTarget.cdpClient.sendCommand(
          'Input.dispatchMouseEvent',
          {
            type: 'mouseReleased',
            x,
            y,
            modifiers,
            button: (() => {
              switch (button) {
                case 0:
                  return 'left';
                case 1:
                  return 'middle';
                case 2:
                  return 'right';
                case 3:
                  return 'back';
                case 4:
                  return 'forward';
                default:
                  return 'none';
              }
            })(),
            buttons: source.buttons,
            clickCount: source.clickCount,
            pointerType,
          }
        );
      case Input.PointerType.Touch:
        return this.#context.cdpTarget.cdpClient.sendCommand(
          'Input.dispatchTouchEvent',
          {
            type: 'touchEnd',
            touchPoints: [
              {
                x,
                y,
                id: source.pointerId,
              },
            ],
            modifiers,
          }
        );
    }
    // --- Platform-specific code ends here ---
  }

  async #dispatchPointerMoveAction(
    source: PointerSource,
    keyState: KeySource,
    action: Readonly<Input.PointerMoveAction>
  ): Promise<void> {
    const {x: startX, y: startY, subtype: pointerType} = source;
    const {
      width,
      height,
      pressure,
      twist,
      tangentialPressure,
      x: offsetX,
      y: offsetY,
      origin = 'viewport',
      duration = this.#tickDuration,
    } = action;
    const {tiltX, tiltY} = 'tiltX' in action ? action : ({} as never);
    // TODO: Implement azimuth/altitude angle.

    const {targetX, targetY} = await this.#getCoordinateFromOrigin(
      origin,
      offsetX,
      offsetY,
      startX,
      startY
    );

    if (targetX < 0 || targetY < 0) {
      throw new Message.MoveTargetOutOfBoundsException(
        `Cannot move beyond viewport (x: ${targetX}, y: ${targetY})`
      );
    }

    let last: boolean;
    do {
      const ratio =
        duration > 0 ? (performance.now() - this.#tickStart) / duration : 1;
      last = ratio >= 1;

      let x: number;
      let y: number;
      if (last) {
        x = targetX;
        y = targetY;
      } else {
        x = Math.round(ratio * (targetX - startX) + startX);
        y = Math.round(ratio * (targetY - startY) + startY);
      }

      if (source.x !== x || source.y !== y) {
        // --- Platform-specific code begins here ---
        const {modifiers} = keyState;
        switch (pointerType) {
          case Input.PointerType.Mouse:
          case Input.PointerType.Pen:
            // TODO: Implement width and height when available.
            await this.#context.cdpTarget.cdpClient.sendCommand(
              'Input.dispatchMouseEvent',
              {
                type: 'mouseMoved',
                x,
                y,
                modifiers,
                clickCount: 0,
                buttons: source.buttons,
                pointerType,
                tangentialPressure,
                tiltX,
                tiltY,
                twist,
                force: pressure,
              }
            );
            break;
          case Input.PointerType.Touch:
            await this.#context.cdpTarget.cdpClient.sendCommand(
              'Input.dispatchTouchEvent',
              {
                type: 'touchMove',
                touchPoints: [
                  {
                    x,
                    y,
                    radiusX: width,
                    radiusY: height,
                    tangentialPressure,
                    tiltX,
                    tiltY,
                    twist,
                    force: pressure,
                    id: source.pointerId,
                  },
                ],
                modifiers,
              }
            );
            break;
        }
        // --- Platform-specific code ends here ---

        source.x = x;
        source.y = y;
      }
    } while (!last);
  }

  async #getCoordinateFromOrigin(
    origin: Input.Origin,
    offsetX: number,
    offsetY: number,
    startX: number,
    startY: number
  ) {
    let targetX: number;
    let targetY: number;
    switch (origin) {
      case 'viewport':
        targetX = offsetX;
        targetY = offsetY;
        break;
      case 'pointer':
        targetX = startX + offsetX;
        targetY = startY + offsetY;
        break;
      default: {
        const {x: posX, y: posY} = await getElementCenter(
          this.#context,
          origin.element
        );
        // SAFETY: These can never be special numbers.
        targetX = posX + offsetX;
        targetY = posY + offsetY;
        break;
      }
    }
    return {targetX, targetY};
  }

  async #dispatchScrollAction(
    _source: WheelSource,
    keyState: KeySource,
    action: Readonly<Input.WheelScrollAction>
  ): Promise<void> {
    const {
      deltaX: targetDeltaX,
      deltaY: targetDeltaY,
      x: offsetX,
      y: offsetY,
      origin = 'viewport',
      duration = this.#tickDuration,
    } = action;

    if (origin === 'pointer') {
      throw new Message.InvalidArgumentException(
        '"pointer" origin is invalid for scrolling.'
      );
    }

    const {targetX, targetY} = await this.#getCoordinateFromOrigin(
      origin,
      offsetX,
      offsetY,
      0,
      0
    );

    if (targetX < 0 || targetY < 0) {
      throw new Message.MoveTargetOutOfBoundsException(
        `Cannot move beyond viewport (x: ${targetX}, y: ${targetY})`
      );
    }

    let currentDeltaX = 0;
    let currentDeltaY = 0;
    let last: boolean;
    do {
      const ratio =
        duration > 0 ? (performance.now() - this.#tickStart) / duration : 1;
      last = ratio >= 1;

      let deltaX: number;
      let deltaY: number;
      if (last) {
        deltaX = targetDeltaX - currentDeltaX;
        deltaY = targetDeltaY - currentDeltaY;
      } else {
        deltaX = Math.round(ratio * targetDeltaX - currentDeltaX);
        deltaY = Math.round(ratio * targetDeltaY - currentDeltaY);
      }

      if (deltaX !== 0 || deltaY !== 0) {
        // --- Platform-specific code begins here ---
        const {modifiers} = keyState;
        await this.#context.cdpTarget.cdpClient.sendCommand(
          'Input.dispatchMouseEvent',
          {
            type: 'mouseWheel',
            deltaX,
            deltaY,
            x: targetX,
            y: targetY,
            modifiers,
          }
        );
        // --- Platform-specific code ends here ---

        currentDeltaX += deltaX;
        currentDeltaY += deltaY;
      }
    } while (!last);
  }

  #dispatchKeyDownAction(
    source: KeySource,
    action: Readonly<Input.KeyDownAction>
  ) {
    const rawKey = action.value;
    const key = getNormalizedKey(rawKey);
    const repeat = source.pressed.has(key);
    const code = getKeyCode(rawKey);
    const location = getKeyLocation(rawKey);
    switch (key) {
      case 'Alt':
        source.alt = true;
        break;
      case 'Shift':
        source.shift = true;
        break;
      case 'Control':
        source.ctrl = true;
        break;
      case 'Meta':
        source.meta = true;
        break;
    }
    source.pressed.add(key);
    const {modifiers} = source;

    // --- Platform-specific code begins here ---
    // The spread is a little hack so JS gives us an array of unicode characters
    // to measure.
    const text = [...key].length === 1 ? key : undefined;
    return this.#context.cdpTarget.cdpClient.sendCommand(
      'Input.dispatchKeyEvent',
      {
        type: text ? 'keyDown' : 'rawKeyDown',
        windowsVirtualKeyCode: KeyToKeyCode[key],
        key,
        code,
        text,
        unmodifiedText: text,
        autoRepeat: repeat,
        isSystemKey: source.alt || undefined,
        location: location < 3 ? location : undefined,
        isKeypad: location === 3,
        modifiers,
      }
    );
    // --- Platform-specific code ends here ---
  }

  #dispatchKeyUpAction(source: KeySource, action: Readonly<Input.KeyUpAction>) {
    const rawKey = action.value;
    const key = getNormalizedKey(rawKey);
    if (!source.pressed.has(key)) {
      return;
    }
    const code = getKeyCode(rawKey);
    const location = getKeyLocation(rawKey);
    switch (key) {
      case 'Alt':
        source.alt = false;
        break;
      case 'Shift':
        source.shift = false;
        break;
      case 'Control':
        source.ctrl = false;
        break;
      case 'Meta':
        source.meta = false;
        break;
    }
    source.pressed.delete(key);
    const {modifiers} = source;

    // --- Platform-specific code begins here ---
    // The spread is a little hack so JS gives us an array of unicode characters
    // to measure.
    const text = [...key].length === 1 ? key : undefined;
    return this.#context.cdpTarget.cdpClient.sendCommand(
      'Input.dispatchKeyEvent',
      {
        type: 'keyUp',
        windowsVirtualKeyCode: KeyToKeyCode[key],
        key,
        code,
        text,
        unmodifiedText: text,
        location: location < 3 ? location : undefined,
        isSystemKey: source.alt || undefined,
        isKeypad: location === 3,
        modifiers,
      }
    );
    // --- Platform-specific code ends here ---
  }
}
