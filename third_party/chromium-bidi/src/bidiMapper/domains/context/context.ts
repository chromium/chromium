/**
 * Copyright 2022 Google LLC.
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

import { Protocol } from 'devtools-protocol';
import { BrowsingContext, Script } from '../protocol/bidiProtocolTypes';
import { NoSuchFrameException, UnknownErrorResponse } from '../protocol/error';
import { CdpClient } from '../../../cdp';
import { Deferred } from '../../utils/deferred';

export abstract class Context {
  readonly #targetDeferres = {
    Page: {
      navigatedWithinDocument: new Deferred(),
      lifecycleEvent: {
        DOMContentLoaded: new Map<
          string,
          Deferred<Protocol.Page.LifecycleEventEvent>
        >(),
        load: new Map<string, Deferred<Protocol.Page.LifecycleEventEvent>>(),
      },
    },
  };

  static #contexts: Map<string, Context> = new Map();
  protected readonly cdpClient: CdpClient;

  public static getTopLevelContexts(): Context[] {
    return Array.from(Context.#contexts.values()).filter(
      (c) => c.getParentId() === null
    );
  }

  public static removeContext(contextId: string) {
    Context.#contexts.delete(contextId);
  }

  public static addContext(context: Context) {
    Context.#contexts.set(context.getContextId(), context);
  }

  public static hasKnownContext(contextId: string): boolean {
    return Context.#contexts.has(contextId);
  }

  public static getKnownContext(contextId: string): Context {
    if (!Context.hasKnownContext(contextId)) {
      throw new NoSuchFrameException(`Context ${contextId} not found`);
    }
    return Context.#contexts.get(contextId)!;
  }

  readonly #contextId: string;
  readonly #sessionId: string;
  readonly #parentId: string | null;
  readonly #childrenIds: Set<string> = new Set();
  #url: string = 'about:blank';

  public getContextId = (): string => this.#contextId;
  public getChildren = (): Context[] =>
    Array.from(this.#childrenIds).map((contextId) =>
      Context.getKnownContext(contextId)
    );
  public getSessionId = (): string => this.#sessionId;
  public getParentId = (): string | null => this.#parentId;
  public getUrl = (): string | null => this.#url;

  public setUrl(url: string) {
    this.#url = url;
  }

  public addChild(child: Context) {
    this.#childrenIds.add(child.getContextId());
  }

  protected constructor(
    contextId: string,
    parent: string | null,
    sessionId: string,
    cdpClient: CdpClient
  ) {
    this.#contextId = contextId;
    this.#parentId = parent;
    this.#sessionId = sessionId;
    this.cdpClient = cdpClient;

    this.#setCdpListeners();
  }

  #setCdpListeners() {
    this.cdpClient.Page.on(
      'lifecycleEvent',
      (params: Protocol.Page.LifecycleEventEvent) => {
        let map: Map<
          string,
          Deferred<Protocol.Page.LifecycleEventEvent>
        > | null = null;
        if (params.name === 'DOMContentLoaded') {
          map = this.#targetDeferres.Page.lifecycleEvent.DOMContentLoaded;
        }
        if (params.name === 'load') {
          map = this.#targetDeferres.Page.lifecycleEvent.load;
        }

        map?.get(params.loaderId)?.resolve(params);
      }
    );

    this.cdpClient.Page.on(
      'navigatedWithinDocument',
      (params: Protocol.Page.NavigatedWithinDocumentEvent) => {
        if (params.frameId !== this.getContextId()) {
          return;
        }
        this.#targetDeferres.Page.navigatedWithinDocument.resolve(params);
        this.#targetDeferres.Page.navigatedWithinDocument = new Deferred();
      }
    );
  }

  abstract callFunction(
    functionDeclaration: string,
    _this: Script.ArgumentValue,
    _arguments: Script.ArgumentValue[],
    awaitPromise: boolean,
    resultOwnership: Script.OwnershipModel
  ): Promise<Script.CallFunctionResult>;

  abstract scriptEvaluate(
    expression: string,
    awaitPromise: boolean,
    resultOwnership: Script.OwnershipModel
  ): Promise<Script.EvaluateResult>;

  abstract waitInitialized(): Promise<void>;

  public serializeToBidiValue(
    maxDepth: number,
    isRoot: boolean
  ): BrowsingContext.Info {
    return {
      context: this.#contextId,
      url: this.#url,
      children:
        maxDepth > 0
          ? this.getChildren().map((c) =>
              c.serializeToBidiValue(maxDepth - 1, false)
            )
          : null,
      ...(isRoot ? { parent: this.#parentId } : {}),
    };
  }

  public async findElement(
    selector: string
  ): Promise<BrowsingContext.PROTO.FindElementResult> {
    const functionDeclaration = String((resultsSelector: string) =>
      document.querySelector(resultsSelector)
    );
    const _arguments: Script.ArgumentValue[] = [
      { type: 'string', value: selector },
    ];

    // TODO(sadym): handle not found exception.
    const result = await this.callFunction(
      functionDeclaration,
      {
        type: 'undefined',
      },
      _arguments,
      true,
      'root'
    );

    // TODO(sadym): handle type properly.
    return result as any as BrowsingContext.PROTO.FindElementResult;
  }

  public async navigate(
    url: string,
    wait: BrowsingContext.ReadinessState
  ): Promise<BrowsingContext.NavigateResult> {
    await this.waitInitialized();

    // TODO: handle loading errors.
    const cdpNavigateResult = await this.cdpClient.Page.navigate({
      url,
      frameId: this.getContextId(),
    });

    if (cdpNavigateResult.errorText) {
      throw new UnknownErrorResponse(cdpNavigateResult.errorText);
    }

    // Wait for `wait` condition.
    switch (wait) {
      case 'none':
        break;

      case 'interactive':
        // No `loaderId` means same-document navigation.
        if (cdpNavigateResult.loaderId === undefined) {
          await this.#targetDeferres.Page.navigatedWithinDocument;
        } else {
          await Context.#waitForDeferredWithKey(
            cdpNavigateResult.loaderId,
            this.#targetDeferres.Page.lifecycleEvent.DOMContentLoaded
          );
        }
        break;

      case 'complete':
        // No `loaderId` means same-document navigation.
        if (cdpNavigateResult.loaderId === undefined) {
          await this.#targetDeferres.Page.navigatedWithinDocument;
        } else {
          await Context.#waitForDeferredWithKey(
            cdpNavigateResult.loaderId,
            this.#targetDeferres.Page.lifecycleEvent.load
          );
        }
        break;

      default:
        throw new Error(`Not implemented wait '${wait}'`);
    }

    return {
      result: {
        navigation: cdpNavigateResult.loaderId || null,
        url: url,
      },
    };
  }

  static async #waitForDeferredWithKey<T>(
    key: string,
    map: Map<string, Deferred<T>>
  ): Promise<T> {
    if (!map.has(key)) {
      map.set(key, new Deferred<T>());
    }
    return map.get(key)!;
  }
}
