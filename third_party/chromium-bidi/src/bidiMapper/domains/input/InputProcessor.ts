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
import {
  Input,
  InvalidArgumentException,
  NoSuchFrameException,
  Script,
  UnableToSetFileInputException,
  type EmptyResult,
  NoSuchElementException,
} from '../../../protocol/protocol.js';
import {assert} from '../../../utils/assert.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';
import {ActionDispatcher} from '../input/ActionDispatcher.js';
import type {ActionOption} from '../input/ActionOption.js';
import {SourceType} from '../input/InputSource.js';
import type {InputState} from '../input/InputState.js';
import {InputStateManager} from '../input/InputStateManager.js';
import type {RealmStorage} from '../script/RealmStorage.js';

export class InputProcessor {
  readonly #browsingContextStorage: BrowsingContextStorage;
  readonly #realmStorage: RealmStorage;

  readonly #inputStateManager = new InputStateManager();

  constructor(
    browsingContextStorage: BrowsingContextStorage,
    realmStorage: RealmStorage
  ) {
    this.#browsingContextStorage = browsingContextStorage;
    this.#realmStorage = realmStorage;
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

  async setFiles(params: Input.SetFilesParameters): Promise<EmptyResult> {
    const realm = this.#realmStorage.findRealm({
      browsingContextId: params.context,
    });
    if (realm === undefined) {
      throw new NoSuchFrameException(
        `Could not find browsingContext ${params.context}`
      );
    }

    let isFileInput;
    try {
      const result = await realm.callFunction(
        String(function getFiles(this: unknown) {
          return (
            this instanceof HTMLInputElement &&
            this.type === 'file' &&
            !this.disabled
          );
        }),
        params.element,
        [],
        false,
        Script.ResultOwnership.None,
        {},
        false
      );
      assert(result.type === 'success');
      assert(result.result.type === 'boolean');
      isFileInput = result.result.value;
    } catch {
      throw new NoSuchElementException(
        `Could not find element ${params.element.sharedId}`
      );
    }

    if (!isFileInput) {
      throw new UnableToSetFileInputException(
        `Element ${params.element.sharedId} is not a mutable file input.`
      );
    }

    // Our goal here is to iterate over the input element files and get their
    // file paths.
    const paths: string[] = [];
    for (let i = 0; i < params.files.length; ++i) {
      const result: Script.EvaluateResult = await realm.callFunction(
        String(function getFiles(this: HTMLInputElement, index: number) {
          if (!this.files) {
            // We use `null` because `item` also returns null.
            return null;
          }
          return this.files.item(index);
        }),
        params.element,
        [{type: 'number', value: 0}],
        false,
        Script.ResultOwnership.Root,
        {},
        false
      );
      assert(result.type === 'success');
      if (result.result.type !== 'object') {
        break;
      }

      const {handle}: {handle?: string} = result.result;
      assert(handle !== undefined);
      const {path} = await realm.cdpClient.sendCommand('DOM.getFileInfo', {
        objectId: handle,
      });
      paths.push(path);

      // Cleanup the handle.
      void realm.disown(handle).catch(undefined);
    }

    paths.sort();
    // We create a new array so we preserve the order of the original files.
    const sortedFiles = [...params.files].sort();
    if (
      paths.length !== params.files.length ||
      sortedFiles.some((path, index) => {
        return paths[index] !== path;
      })
    ) {
      const {objectId} = await realm.deserializeToCdpArg(params.element);
      // This cannot throw since this was just used in `callFunction` above.
      assert(objectId !== undefined);
      await realm.cdpClient.sendCommand('DOM.setFileInputFiles', {
        files: params.files,
        objectId,
      });
    } else {
      // XXX: We should dispatch a trusted event.
      await realm.callFunction(
        String(function dispatchEvent(this: HTMLInputElement) {
          this.dispatchEvent(
            new Event('cancel', {
              bubbles: true,
            })
          );
        }),
        params.element,
        [],
        false,
        Script.ResultOwnership.None,
        {},
        false
      );
    }
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
