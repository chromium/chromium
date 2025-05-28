/**
 * Copyright 2024 Google LLC.
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

import type {Protocol} from 'devtools-protocol';

import type {CdpClient} from '../../../cdp/CdpClient.js';
import {
  NoSuchNodeException,
  UnknownErrorException,
  type BrowsingContext,
  type Script,
} from '../../../protocol/protocol.js';
import {CdpErrorConstants} from '../../../utils/cdpErrorConstants.js';
import type {LoggerFn} from '../../../utils/log.js';
import type {BrowsingContextImpl} from '../context/BrowsingContextImpl.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';
import type {EventManager} from '../session/EventManager.js';

import {Realm} from './Realm.js';
import type {RealmStorage} from './RealmStorage.js';
import {getSharedId, parseSharedId} from './SharedId.js';

export class WindowRealm extends Realm {
  readonly #browsingContextId: BrowsingContext.BrowsingContext;
  readonly #browsingContextStorage: BrowsingContextStorage;
  readonly sandbox: string | undefined;

  constructor(
    browsingContextId: BrowsingContext.BrowsingContext,
    browsingContextStorage: BrowsingContextStorage,
    cdpClient: CdpClient,
    eventManager: EventManager,
    executionContextId: Protocol.Runtime.ExecutionContextId,
    logger: LoggerFn | undefined,
    origin: string,
    realmId: Script.Realm,
    realmStorage: RealmStorage,
    sandbox: string | undefined,
  ) {
    super(
      cdpClient,
      eventManager,
      executionContextId,
      logger,
      origin,
      realmId,
      realmStorage,
    );

    this.#browsingContextId = browsingContextId;
    this.#browsingContextStorage = browsingContextStorage;
    this.sandbox = sandbox;

    this.initialize();
  }

  #getBrowsingContextId(navigableId: string): string {
    const maybeBrowsingContext = this.#browsingContextStorage
      .getAllContexts()
      .find((context) => context.navigableId === navigableId);
    return maybeBrowsingContext?.id ?? 'UNKNOWN';
  }

  get browsingContext(): BrowsingContextImpl {
    return this.#browsingContextStorage.getContext(this.#browsingContextId);
  }

  /**
   * Do not expose to user hidden realms.
   */
  override isHidden(): boolean {
    return this.realmStorage.hiddenSandboxes.has(this.sandbox);
  }

  override get associatedBrowsingContexts(): [BrowsingContextImpl] {
    return [this.browsingContext];
  }

  override get realmType(): 'window' {
    return 'window';
  }

  override get realmInfo(): Script.WindowRealmInfo {
    return {
      ...this.baseInfo,
      type: this.realmType,
      context: this.#browsingContextId,
      sandbox: this.sandbox,
    };
  }

  override get source(): Script.Source {
    return {
      realm: this.realmId,
      context: this.browsingContext.id,
    };
  }

  override serializeForBiDi(
    deepSerializedValue: Protocol.Runtime.DeepSerializedValue,
    internalIdMap: Map<number, string>,
  ) {
    const bidiValue = deepSerializedValue.value;
    if (deepSerializedValue.type === 'node' && bidiValue !== undefined) {
      if (Object.hasOwn(bidiValue, 'backendNodeId')) {
        let navigableId = this.browsingContext.navigableId ?? 'UNKNOWN';
        if (Object.hasOwn(bidiValue, 'loaderId')) {
          // `loaderId` should be always there after ~2024-03-05, when
          // https://crrev.com/c/5116240 reaches stable.
          // TODO: remove the check after the date.
          navigableId = bidiValue.loaderId;
          delete bidiValue['loaderId'];
        }
        (deepSerializedValue as unknown as Script.SharedReference).sharedId =
          getSharedId(
            this.#getBrowsingContextId(navigableId),
            navigableId,
            bidiValue.backendNodeId,
          );
        delete bidiValue['backendNodeId'];
      }
      if (Object.hasOwn(bidiValue, 'children')) {
        for (const i in bidiValue.children) {
          bidiValue.children[i] = this.serializeForBiDi(
            bidiValue.children[i],
            internalIdMap,
          );
        }
      }
      if (
        Object.hasOwn(bidiValue, 'shadowRoot') &&
        bidiValue.shadowRoot !== null
      ) {
        bidiValue.shadowRoot = this.serializeForBiDi(
          bidiValue.shadowRoot,
          internalIdMap,
        );
      }
      // `namespaceURI` can be is either `null` or non-empty string.
      if (bidiValue.namespaceURI === '') {
        bidiValue.namespaceURI = null;
      }
    }
    return super.serializeForBiDi(deepSerializedValue, internalIdMap);
  }

  override async deserializeForCdp(
    localValue: Script.LocalValue,
  ): Promise<Protocol.Runtime.CallArgument> {
    if ('sharedId' in localValue && localValue.sharedId) {
      const parsedSharedId = parseSharedId(localValue.sharedId);
      if (parsedSharedId === null) {
        throw new NoSuchNodeException(
          `SharedId "${localValue.sharedId}" was not found.`,
        );
      }
      const {documentId, backendNodeId} = parsedSharedId;
      // TODO: add proper validation if the element is accessible from the current realm.
      if (this.browsingContext.navigableId !== documentId) {
        throw new NoSuchNodeException(
          `SharedId "${localValue.sharedId}" belongs to different document. Current document is ${this.browsingContext.navigableId}.`,
        );
      }

      try {
        const {object} = await this.cdpClient.sendCommand('DOM.resolveNode', {
          backendNodeId,
          executionContextId: this.executionContextId,
        });
        // TODO(#375): Release `obj.object.objectId` after using.
        return {objectId: object.objectId};
      } catch (error: any) {
        // Heuristic to detect "no such node" exception. Based on the  specific
        // CDP implementation.
        if (
          error.code === CdpErrorConstants.GENERIC_ERROR &&
          error.message === 'No node with given id found'
        ) {
          throw new NoSuchNodeException(
            `SharedId "${localValue.sharedId}" was not found.`,
          );
        }
        throw new UnknownErrorException(error.message, error.stack);
      }
    }
    return await super.deserializeForCdp(localValue);
  }

  override async evaluate(
    expression: string,
    awaitPromise: boolean,
    resultOwnership: Script.ResultOwnership,
    serializationOptions: Script.SerializationOptions,
    userActivation?: boolean,
    includeCommandLineApi?: boolean,
  ): Promise<Script.EvaluateResult> {
    await this.#browsingContextStorage
      .getContext(this.#browsingContextId)
      .targetUnblockedOrThrow();

    return await super.evaluate(
      expression,
      awaitPromise,
      resultOwnership,
      serializationOptions,
      userActivation,
      includeCommandLineApi,
    );
  }

  override async callFunction(
    functionDeclaration: string,
    awaitPromise: boolean,
    thisLocalValue: Script.LocalValue,
    argumentsLocalValues: Script.LocalValue[],
    resultOwnership: Script.ResultOwnership,
    serializationOptions: Script.SerializationOptions,
    userActivation?: boolean,
  ): Promise<Script.EvaluateResult> {
    await this.#browsingContextStorage
      .getContext(this.#browsingContextId)
      .targetUnblockedOrThrow();

    return await super.callFunction(
      functionDeclaration,
      awaitPromise,
      thisLocalValue,
      argumentsLocalValues,
      resultOwnership,
      serializationOptions,
      userActivation,
    );
  }
}
