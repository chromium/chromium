/*
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
import {Input, InvalidArgumentException} from '../../../protocol/protocol.js';
import type {EmptyResult} from '../../../protocol/webdriver-bidi';
import {InputStateManager} from '../input/InputStateManager.js';
import {ActionDispatcher} from '../input/ActionDispatcher.js';
import type {ActionOption} from '../input/ActionOption.js';
import {SourceType} from '../input/InputSource.js';
import type {InputState} from '../input/InputState.js';
import type {BrowsingContextStorage} from '../context/browsingContextStorage.js';

export class InputProcessor {
  readonly #browsingContextStorage: BrowsingContextStorage;

  readonly #inputStateManager = new InputStateManager();

  constructor(browsingContextStorage: BrowsingContextStorage) {
    this.#browsingContextStorage = browsingContextStorage;
  }

  async performActions(
    params: Input.PerformActionsParameters
  ): Promise<EmptyResult> {
    const context = this.#browsingContextStorage.getContext(params.context);
    const inputState = this.#inputStateManager.get(context.top);
    const actionsByTick = this.#getActionsByTick(params, inputState);
    const dispatcher = new ActionDispatcher(
      inputState,
      context,
      await ActionDispatcher.isMacOS(context).catch(() => false)
    );
    await dispatcher.dispatchActions(actionsByTick);
    return {};
  }

  async releaseActions(
    params: Input.ReleaseActionsParameters
  ): Promise<EmptyResult> {
    const context = this.#browsingContextStorage.getContext(params.context);
    const topContext = context.top;
    const inputState = this.#inputStateManager.get(topContext);
    const dispatcher = new ActionDispatcher(
      inputState,
      context,
      await ActionDispatcher.isMacOS(context).catch(() => false)
    );
    await dispatcher.dispatchTickActions(inputState.cancelList.reverse());
    this.#inputStateManager.delete(topContext);
    return {};
  }

  #getActionsByTick(
    params: Input.PerformActionsParameters,
    inputState: InputState
  ): ActionOption[][] {
    const actionsByTick: ActionOption[][] = [];
    for (const action of params.actions) {
      switch (action.type) {
        case SourceType.Pointer: {
          action.parameters ??= {pointerType: Input.PointerType.Mouse};
          action.parameters.pointerType ??= Input.PointerType.Mouse;

          const source = inputState.getOrCreate(
            action.id,
            SourceType.Pointer,
            action.parameters.pointerType
          );
          if (source.subtype !== action.parameters.pointerType) {
            throw new InvalidArgumentException(
              `Expected input source ${action.id} to be ${source.subtype}; got ${action.parameters.pointerType}.`
            );
          }
          break;
        }
        default:
          inputState.getOrCreate(action.id, action.type as SourceType);
      }
      const actions = action.actions.map((item) => ({
        id: action.id,
        action: item,
      }));
      for (let i = 0; i < actions.length; i++) {
        if (actionsByTick.length === i) {
          actionsByTick.push([]);
        }
        actionsByTick[i]!.push(actions[i]!);
      }
    }
    return actionsByTick;
  }
}
