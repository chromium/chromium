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
import { CdpClient } from '../../../cdp';
import { IEventManager } from '../events/EventManager';
import LoadEvent = BrowsingContext.LoadEvent;
import { Deferred } from '../../utils/deferred';
import { UnknownErrorResponse } from '../protocol/error';
import { LogManager } from '../log/logManager';
import { IBidiServer } from '../../utils/bidiServer';
import { ScriptEvaluator } from '../script/scriptEvaluator';

export class BrowsingContextImpl {
  readonly #targetDefers = {
    documentInitialized: new Deferred<void>(),
    targetUnblocked: new Deferred<void>(),
    Page: {
      navigatedWithinDocument:
        new Deferred<Protocol.Page.NavigatedWithinDocumentEvent>(),
      lifecycleEvent: {
        DOMContentLoaded: new Deferred<Protocol.Page.LifecycleEventEvent>(),
        load: new Deferred<Protocol.Page.LifecycleEventEvent>(),
      },
    },
  };

  readonly #contextId: string;
  readonly #parentId: string | null;
  #url: string = 'about:blank';
  #loaderId: string | null = null;

  #cdpSessionId: string;
  #cdpClient: CdpClient;
  readonly #bidiServer: IBidiServer;
  #eventManager: IEventManager;
  readonly #children: BrowsingContextImpl[] = [];
  #scriptEvaluator: ScriptEvaluator;
  #executionContext: number | null = null;

  constructor(
    contextId: string,
    parentId: string | null,
    cdpClient: CdpClient,
    bidiServer: IBidiServer,
    cdpSessionId: string,
    eventManager: IEventManager
  ) {
    this.#contextId = contextId;
    this.#parentId = parentId;
    this.#cdpClient = cdpClient;
    this.#eventManager = eventManager;
    this.#cdpSessionId = cdpSessionId;
    this.#bidiServer = bidiServer;

    this.#scriptEvaluator = ScriptEvaluator.create(cdpClient);
    this.#initListeners();
  }

  public static createFrameContext(
    contextId: string,
    parentId: string | null,
    cdpClient: CdpClient,
    bidiServer: IBidiServer,
    cdpSessionId: string,
    eventManager: IEventManager
  ): BrowsingContextImpl {
    const context = new BrowsingContextImpl(
      contextId,
      parentId,
      cdpClient,
      bidiServer,
      cdpSessionId,
      eventManager
    );
    context.#targetDefers.targetUnblocked.resolve();
    return context;
  }

  public static createTargetContext(
    contextId: string,
    parentId: string | null,
    cdpClient: CdpClient,
    bidiServer: IBidiServer,
    cdpSessionId: string,
    eventManager: IEventManager
  ): BrowsingContextImpl {
    const context = new BrowsingContextImpl(
      contextId,
      parentId,
      cdpClient,
      bidiServer,
      cdpSessionId,
      eventManager
    );

    // No need in waiting for target to be unblocked.
    // noinspection JSIgnoredPromiseFromCall
    context.#unblockAttachedTarget();

    return context;
  }

  public static convertFrameToTargetContext(
    context: BrowsingContextImpl,
    cdpClient: CdpClient,
    cdpSessionId: string
  ): BrowsingContextImpl {
    context.#updateConnection(cdpClient, cdpSessionId);
    // No need in waiting for target to be unblocked.
    // noinspection JSIgnoredPromiseFromCall
    context.#unblockAttachedTarget();
    return context;
  }

  #updateConnection(cdpClient: CdpClient, cdpSessionId: string) {
    if (!this.#targetDefers.targetUnblocked.isFinished) {
      this.#targetDefers.targetUnblocked.reject('OOPiF');
    }
    this.#targetDefers.targetUnblocked = new Deferred<void>();

    this.#cdpClient = cdpClient;
    this.#cdpSessionId = cdpSessionId;

    this.#scriptEvaluator = ScriptEvaluator.create(cdpClient);
    this.#initListeners();
  }

  async #unblockAttachedTarget() {
    LogManager.create(
      this.#contextId,
      this.#cdpClient,
      this.#bidiServer,
      this.#scriptEvaluator
    );
    await this.#cdpClient.Runtime.enable();
    await this.#cdpClient.Page.enable();
    await this.#cdpClient.Page.setLifecycleEventsEnabled({ enabled: true });
    await this.#cdpClient.Target.setAutoAttach({
      autoAttach: true,
      waitForDebuggerOnStart: true,
      flatten: true,
    });

    await this.#cdpClient.Runtime.runIfWaitingForDebugger();
    this.#targetDefers.targetUnblocked.resolve();
  }

  get contextId(): string {
    return this.#contextId;
  }

  get parentId(): string | null {
    return this.#parentId;
  }

  get cdpSessionId(): string {
    return this.#cdpSessionId;
  }

  get children(): BrowsingContextImpl[] {
    return this.#children;
  }

  get url(): string {
    return this.#url;
  }

  addChild(child: BrowsingContextImpl): void {
    this.#children.push(child);
  }

  public serializeToBidiValue(
    maxDepth: number,
    isRoot: boolean
  ): BrowsingContext.Info {
    return {
      context: this.#contextId,
      url: this.url,
      children:
        maxDepth > 0
          ? this.children.map((c) =>
              c.serializeToBidiValue(maxDepth - 1, false)
            )
          : null,
      ...(isRoot ? { parent: this.#parentId } : {}),
    };
  }

  #initListeners() {
    this.#cdpClient.Target.on(
      'targetInfoChanged',
      (params: Protocol.Target.TargetInfoChangedEvent) => {
        if (this.contextId !== params.targetInfo.targetId) {
          return;
        }
        this.#url = params.targetInfo.url;
      }
    );

    this.#cdpClient.Page.on(
      'frameNavigated',
      (params: Protocol.Page.FrameNavigatedEvent) => {
        if (this.contextId !== params.frame.id) {
          return;
        }
        this.#url = params.frame.url + (params.frame.urlFragment ?? '');
      }
    );

    this.#cdpClient.Page.on(
      'navigatedWithinDocument',
      (params: Protocol.Page.NavigatedWithinDocumentEvent) => {
        if (this.contextId !== params.frameId) {
          return;
        }

        this.#url = params.url;
        this.#targetDefers.Page.navigatedWithinDocument.resolve(params);
      }
    );

    this.#cdpClient.Page.on(
      'lifecycleEvent',
      async (params: Protocol.Page.LifecycleEventEvent) => {
        if (this.contextId !== params.frameId) {
          return;
        }

        if (params.name === 'init') {
          this.#documentChanged(params.loaderId);
          this.#targetDefers.documentInitialized.resolve();
        }

        if (params.loaderId !== this.#loaderId) {
          return;
        }

        switch (params.name) {
          case 'DOMContentLoaded':
            this.#targetDefers.Page.lifecycleEvent.DOMContentLoaded.resolve(
              params
            );
            await this.#eventManager.sendEvent(
              new BrowsingContext.DomContentLoadedEvent({
                context: this.contextId,
                navigation: this.#loaderId,
              }),
              this.contextId
            );
            break;

          case 'load':
            this.#targetDefers.Page.lifecycleEvent.load.resolve(params);
            await this.#eventManager.sendEvent(
              new LoadEvent({
                context: this.contextId,
                navigation: this.#loaderId,
              }),
              this.contextId
            );
            break;
        }
      }
    );

    this.#cdpClient.Runtime.on(
      'executionContextCreated',
      (params: Protocol.Runtime.ExecutionContextCreatedEvent) => {
        if (params.context.auxData.frameId !== this.contextId) {
          return;
        }
        if (!params.context.auxData.isDefault) {
          return;
        }
        this.#executionContext = params.context.id;
      }
    );
  }

  #documentChanged(loaderId: string) {
    if (this.#loaderId === loaderId) {
      return;
    }

    if (!this.#targetDefers.documentInitialized.isFinished) {
      this.#targetDefers.documentInitialized.reject('Document changed');
    }
    this.#targetDefers.documentInitialized = new Deferred<void>();

    if (!this.#targetDefers.Page.navigatedWithinDocument.isFinished) {
      this.#targetDefers.Page.navigatedWithinDocument.reject(
        'Document changed'
      );
    }
    this.#targetDefers.Page.navigatedWithinDocument =
      new Deferred<Protocol.Page.NavigatedWithinDocumentEvent>();

    if (!this.#targetDefers.Page.lifecycleEvent.DOMContentLoaded.isFinished) {
      this.#targetDefers.Page.lifecycleEvent.DOMContentLoaded.reject(
        'Document changed'
      );
    }
    this.#targetDefers.Page.lifecycleEvent.DOMContentLoaded =
      new Deferred<Protocol.Page.LifecycleEventEvent>();

    if (!this.#targetDefers.Page.lifecycleEvent.load.isFinished) {
      this.#targetDefers.Page.lifecycleEvent.load.reject('Document changed');
    }
    this.#targetDefers.Page.lifecycleEvent.load =
      new Deferred<Protocol.Page.LifecycleEventEvent>();

    this.#loaderId = loaderId;
  }

  async navigate(
    url: string,
    wait: BrowsingContext.ReadinessState
  ): Promise<BrowsingContext.NavigateResult> {
    await this.#targetDefers.targetUnblocked;

    // TODO: handle loading errors.
    const cdpNavigateResult = await this.#cdpClient.Page.navigate({
      url,
      frameId: this.contextId,
    });

    if (cdpNavigateResult.errorText) {
      throw new UnknownErrorResponse(cdpNavigateResult.errorText);
    }

    if (
      cdpNavigateResult.loaderId !== undefined &&
      cdpNavigateResult.loaderId !== this.#loaderId
    ) {
      this.#documentChanged(cdpNavigateResult.loaderId);
    }

    // Wait for `wait` condition.
    switch (wait) {
      case 'none':
        break;

      case 'interactive':
        // No `loaderId` means same-document navigation.
        if (cdpNavigateResult.loaderId === undefined) {
          await this.#targetDefers.Page.navigatedWithinDocument;
        } else {
          await this.#targetDefers.Page.lifecycleEvent.DOMContentLoaded;
        }
        break;

      case 'complete':
        // No `loaderId` means same-document navigation.
        if (cdpNavigateResult.loaderId === undefined) {
          await this.#targetDefers.Page.navigatedWithinDocument;
        } else {
          await this.#targetDefers.Page.lifecycleEvent.load;
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

  async callFunction(
    functionDeclaration: string,
    _this: Script.ArgumentValue,
    _arguments: Script.ArgumentValue[],
    awaitPromise: boolean,
    resultOwnership: Script.OwnershipModel
  ): Promise<Script.CallFunctionResult> {
    await this.#targetDefers.targetUnblocked;

    if (this.#executionContext === null) {
      throw Error('No execution context');
    }

    return {
      result: await this.#scriptEvaluator.callFunction(
        this.#executionContext!,
        functionDeclaration,
        _this,
        _arguments,
        awaitPromise,
        resultOwnership
      ),
    };
  }

  async scriptEvaluate(
    expression: string,
    awaitPromise: boolean,
    resultOwnership: Script.OwnershipModel
  ): Promise<Script.EvaluateResult> {
    await this.#targetDefers.targetUnblocked;

    if (this.#executionContext === null) {
      throw Error('No execution context');
    }

    return this.#scriptEvaluator.scriptEvaluate(
      this.#executionContext!,
      expression,
      awaitPromise,
      resultOwnership
    );
  }

  async findElement(
    selector: string
  ): Promise<BrowsingContext.PROTO.FindElementResult> {
    await this.#targetDefers.targetUnblocked;

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
}
