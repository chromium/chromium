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
  Input,
  InvalidArgumentException,
  UnknownErrorException,
} from '../../../protocol/protocol.js';
import {Mutex} from '../../../utils/Mutex.js';

import type {ActionOption} from './ActionOption.js';
import {
  KeySource,
  NoneSource,
  PointerSource,
  SourceType,
  WheelSource,
  type InputSource,
  type InputSourceFor,
} from './InputSource.js';

export class InputState {
  cancelList: ActionOption[] = [];
  #sources = new Map<string, InputSource>();
  #mutex = new Mutex();

  getOrCreate(
    id: string,
    type: SourceType.Pointer,
    subtype: Input.PointerType,
  ): PointerSource;
  getOrCreate<Type extends SourceType>(
    id: string,
    type: Type,
  ): InputSourceFor<Type>;
  getOrCreate<Type extends SourceType>(
    id: string,
    type: Type,
    subtype?: Input.PointerType,
  ): InputSourceFor<Type> {
    let source = this.#sources.get(id);
    if (!source) {
      switch (type) {
        case SourceType.None:
          source = new NoneSource();
          break;
        case SourceType.Key:
          source = new KeySource();
          break;
        case SourceType.Pointer: {
          let pointerId = subtype === Input.PointerType.Mouse ? 0 : 2;
          const pointerIds = new Set<number>();
          for (const [, source] of this.#sources) {
            if (source.type === SourceType.Pointer) {
              pointerIds.add(source.pointerId);
            }
          }
          while (pointerIds.has(pointerId)) {
            ++pointerId;
          }
          source = new PointerSource(pointerId, subtype as Input.PointerType);
          break;
        }
        case SourceType.Wheel:
          source = new WheelSource();
          break;
        default:
          throw new InvalidArgumentException(
            `Expected "${SourceType.None}", "${SourceType.Key}", "${SourceType.Pointer}", or "${SourceType.Wheel}". Found unknown source type ${type}.`,
          );
      }
      this.#sources.set(id, source);
      return source as InputSourceFor<Type>;
    }
    if (source.type !== type) {
      throw new InvalidArgumentException(
        `Input source type of ${id} is ${source.type}, but received ${type}.`,
      );
    }
    return source as InputSourceFor<Type>;
  }

  get(id: string): InputSource {
    const source = this.#sources.get(id);
    if (!source) {
      throw new UnknownErrorException(`Internal error.`);
    }
    return source;
  }

  getGlobalKeyState(): KeySource {
    const state: KeySource = new KeySource();
    for (const [, source] of this.#sources) {
      if (source.type !== SourceType.Key) {
        continue;
      }
      for (const pressed of source.pressed) {
        state.pressed.add(pressed);
      }
      state.alt ||= source.alt;
      state.ctrl ||= source.ctrl;
      state.meta ||= source.meta;
      state.shift ||= source.shift;
    }
    return state;
  }

  get queue() {
    return this.#mutex;
  }
}
