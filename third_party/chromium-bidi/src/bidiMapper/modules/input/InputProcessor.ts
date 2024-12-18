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
  NoSuchElementException,
  Script,
  UnableToSetFileInputException,
  type EmptyResult,
  NoSuchNodeException,
} from '../../../protocol/protocol.js';
import {assert} from '../../../utils/assert.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';
import {ActionDispatcher} from '../input/ActionDispatcher.js';
import type {ActionOption} from '../input/ActionOption.js';
import {SourceType} from '../input/InputSource.js';
import type {InputState} from '../input/InputState.js';
import {InputStateManager} from '../input/InputStateManager.js';

export class InputProcessor {
  readonly #browsingContextStorage: BrowsingContextStorage;

  readonly #inputStateManager = new InputStateManager();

  constructor(browsingContextStorage: BrowsingContextStorage) {
    this.#browsingContextStorage = browsingContextStorage;
  }

  async performActions(
    params: Input.PerformActionsParameters,
  ): Promise<EmptyResult> {
    const context = this.#browsingContextStorage.getContext(params.context);
    const inputState = this.#inputStateManager.get(context.top);
    const actionsByTick = this.#getActionsByTick(params, inputState);
    const dispatcher = new ActionDispatcher(
      inputState,
      this.#browsingContextStorage,
      params.context,
      await ActionDispatcher.isMacOS(context).catch(() => false),
    );
    await dispatcher.dispatchActions(actionsByTick);
    return {};
  }

  async releaseActions(
    params: Input.ReleaseActionsParameters,
  ): Promise<EmptyResult> {
    const context = this.#browsingContextStorage.getContext(params.context);
    const topContext = context.top;
    const inputState = this.#inputStateManager.get(topContext);
    const dispatcher = new ActionDispatcher(
      inputState,
      this.#browsingContextStorage,
      params.context,
      await ActionDispatcher.isMacOS(context).catch(() => false),
    );
    await dispatcher.dispatchTickActions(inputState.cancelList.reverse());
    this.#inputStateManager.delete(topContext);
    return {};
  }

  async setFiles(params: Input.SetFilesParameters): Promise<EmptyResult> {
    const context = this.#browsingContextStorage.getContext(params.context);
    const realm = await context.getOrCreateSandbox(undefined);

    const enum ErrorCode {
      Node,
      Element,
      Type,
      Disabled,
      Multiple,
    }

    let result;
    try {
      result = await realm.callFunction(
        String(function getFiles(this: unknown, fileListLength: number) {
          if (!(this instanceof HTMLInputElement)) {
            if (this instanceof Element) {
              return ErrorCode.Element;
            }
            return ErrorCode.Node;
          }
          if (this.type !== 'file') {
            return ErrorCode.Type;
          }
          if (this.disabled) {
            return ErrorCode.Disabled;
          }
          if (fileListLength > 1 && !this.multiple) {
            return ErrorCode.Multiple;
          }
          return;
        }),
        false,
        params.element,
        [{type: 'number', value: params.files.length}],
      );
    } catch {
      throw new NoSuchNodeException(
        `Could not find element ${params.element.sharedId}`,
      );
    }

    assert(result.type === 'success');
    if (result.result.type === 'number') {
      switch (result.result.value as ErrorCode) {
        case ErrorCode.Node: {
          throw new NoSuchElementException(
            `Could not find element ${params.element.sharedId}`,
          );
        }
        case ErrorCode.Element: {
          throw new UnableToSetFileInputException(
            `Element ${params.element.sharedId} is not a input`,
          );
        }
        case ErrorCode.Type: {
          throw new UnableToSetFileInputException(
            `Input element ${params.element.sharedId} is not a file type`,
          );
        }
        case ErrorCode.Disabled: {
          throw new UnableToSetFileInputException(
            `Input element ${params.element.sharedId} is disabled`,
          );
        }
        case ErrorCode.Multiple: {
          throw new UnableToSetFileInputException(
            `Cannot set multiple files on a non-multiple input element`,
          );
        }
      }
    }

    /**
     * The zero-length array is a special case, it seems that
     * DOM.setFileInputFiles does not actually update the files in that case, so
     * the solution is to eval the element value to a new FileList directly.
     */
    if (params.files.length === 0) {
      // XXX: These events should converted to trusted events. Perhaps do this
      // in `DOM.setFileInputFiles`?
      await realm.callFunction(
        String(function dispatchEvent(this: HTMLInputElement) {
          if (this.files?.length === 0) {
            this.dispatchEvent(
              new Event('cancel', {
                bubbles: true,
              }),
            );
            return;
          }

          this.files = new DataTransfer().files;

          // Dispatch events for this case because it should behave akin to a user action.
          this.dispatchEvent(
            new Event('input', {bubbles: true, composed: true}),
          );
          this.dispatchEvent(new Event('change', {bubbles: true}));
        }),
        false,
        params.element,
      );
      return {};
    }

    // Our goal here is to iterate over the input element files and get their
    // file paths.
    const paths: string[] = [];
    for (let i = 0; i < params.files.length; ++i) {
      const result: Script.EvaluateResult = await realm.callFunction(
        String(function getFiles(this: HTMLInputElement, index: number) {
          return this.files?.item(index);
        }),
        false,
        params.element,
        [{type: 'number', value: 0}],
        Script.ResultOwnership.Root,
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
      const {objectId} = await realm.deserializeForCdp(params.element);
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
            }),
          );
        }),
        false,
        params.element,
      );
    }
    return {};
  }

  #getActionsByTick(
    params: Input.PerformActionsParameters,
    inputState: InputState,
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
            action.parameters.pointerType,
          );
          if (source.subtype !== action.parameters.pointerType) {
            throw new InvalidArgumentException(
              `Expected input source ${action.id} to be ${source.subtype}; got ${action.parameters.pointerType}.`,
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
