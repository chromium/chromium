/**
 * Copyright 2021 Google LLC.
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
import { CdpClient, CdpConnection } from '../../../cdp';
import { BrowsingContext, Script } from '../protocol/bidiProtocolTypes';
import { IBidiServer } from '../../utils/bidiServer';
import { IEventManager } from '../events/EventManager';
import { LogManager } from '../log/logManager';
import { ScriptEvaluator } from '../script/scriptEvaluator';
import { Context } from './context';
import LoadEvent = BrowsingContext.LoadEvent;
import { FrameContext } from './frameContext';

export class TargetContext extends Context {
  readonly #sessionId: string;
  readonly #cdpClient: CdpClient;
  readonly #browserClient: CdpClient;
  readonly #bidiServer: IBidiServer;
  readonly #eventManager: IEventManager;
  readonly #scriptEvaluator: ScriptEvaluator;

  // Delegate to resolve `#initialized`.
  #markContextInitialized: () => void = () => {
    throw Error('Context is not created yet.');
  };

  // `#initialized` is resolved when `#markContextInitialized` is called.
  #initialized: Promise<void> = new Promise((resolve) => {
    this.#markContextInitialized = () => {
      resolve();
    };
  });

  async #waitInitialized(): Promise<void> {
    await this.#initialized;
  }

  private constructor(
    targetInfo: Protocol.Target.TargetInfo,
    sessionId: string,
    cdpConnection: CdpConnection,
    bidiServer: IBidiServer,
    eventManager: IEventManager
  ) {
    let parentId = null;
    if (Context.hasKnownContext(targetInfo.targetId)) {
      parentId = Context.getKnownContext(targetInfo.targetId).getParentId();
    }
    super(targetInfo.targetId, parentId, sessionId);
    this.#sessionId = sessionId;
    this.#cdpClient = cdpConnection.getCdpClient(sessionId);
    this.#browserClient = cdpConnection.browserClient();
    this.#bidiServer = bidiServer;
    this.#eventManager = eventManager;
    this.#scriptEvaluator = ScriptEvaluator.create(this.#cdpClient);

    // Just initiate initialization, don't wait for it to complete.
    // Field `initialized` has a promise, which is resolved after initialization
    // is completed.
    // noinspection JSIgnoredPromiseFromCall
    this.#initialize();
  }

  public static async create(
    _targetInfo: Protocol.Target.TargetInfo,
    _sessionId: string,
    _cdpConnection: CdpConnection,
    _bidiServer: IBidiServer,
    _eventManager: IEventManager
  ): Promise<TargetContext> {
    return new TargetContext(
      _targetInfo,
      _sessionId,
      _cdpConnection,
      _bidiServer,
      _eventManager
    );
  }

  public async navigate(
    url: string,
    wait: BrowsingContext.ReadinessState
  ): Promise<BrowsingContext.NavigateResult> {
    await this.#waitInitialized();

    // TODO: handle loading errors.
    const cdpNavigateResult = await this.#cdpClient.Page.navigate({ url });

    // No `loaderId` means same-document navigation.
    if (cdpNavigateResult.loaderId !== undefined) {
      // Wait for `wait` condition.
      switch (wait) {
        case 'none':
          break;

        case 'interactive':
          await this.#waitPageLifeCycleEvent(
            'DOMContentLoaded',
            cdpNavigateResult.loaderId!
          );
          break;

        case 'complete':
          await this.#waitPageLifeCycleEvent(
            'load',
            cdpNavigateResult.loaderId!
          );
          break;

        default:
          throw new Error(`Not implemented wait '${wait}'`);
      }
    }

    return {
      result: {
        navigation: cdpNavigateResult.loaderId || null,
        url: url,
      },
    };
  }

  public async scriptEvaluate(
    expression: string,
    awaitPromise: boolean
  ): Promise<Script.EvaluateResult> {
    await this.#waitInitialized();
    return this.#scriptEvaluator.scriptEvaluate(expression, awaitPromise);
  }

  public async callFunction(
    functionDeclaration: string,
    _this: Script.ArgumentValue,
    _arguments: Script.ArgumentValue[],
    awaitPromise: boolean
  ): Promise<Script.CallFunctionResult> {
    await this.#waitInitialized();
    return {
      result: await this.#scriptEvaluator.callFunction(
        functionDeclaration,
        _this,
        _arguments,
        awaitPromise
      ),
    };
  }

  async #initialize() {
    await LogManager.create(
      this.getContextId(),
      this.#cdpClient,
      this.#bidiServer,
      this.#scriptEvaluator
    );
    this.#initializeEventListeners();
    await this.#cdpClient.Runtime.enable();
    await this.#cdpClient.Page.enable();
    await this.#cdpClient.Page.setLifecycleEventsEnabled({ enabled: true });
    await this.#cdpClient.Target.setAutoAttach({
      autoAttach: true,
      waitForDebuggerOnStart: true,
      flatten: true,
    });

    await this.#cdpClient.Runtime.runIfWaitingForDebugger();
    this.#markContextInitialized();
  }

  #initializeEventListeners() {
    this.#initializePageLifecycleEventListener();
    this.#initializeCdpEventListeners();
    // TODO(sadym): implement.
    // this.#handleBindingCalledEvent();

    // TODO(sadym): consider using only 1 listener.
    this.#browserClient.Target.on('targetInfoChanged', (params) => {
      if (params.targetInfo.targetId === this.getContextId()) {
        this.#updateTargetInfo(params.targetInfo);
      }
    });
  }

  #initializeCdpEventListeners() {
    this.#cdpClient.on('event', async (method, params) => {
      await this.#eventManager.sendEvent(
        {
          method: 'PROTO.cdp.eventReceived',
          params: {
            cdpMethod: method,
            cdpParams: params,
            session: this.#sessionId,
          },
        },
        null
      );
    });
  }

  #initializePageLifecycleEventListener() {
    this.#cdpClient.Page.on(
      'frameAttached',
      async (params: Protocol.Page.FrameAttachedEvent) => {
        const frameContext = await FrameContext.create(
          params,
          this.#sessionId,
          this.#cdpClient
        );
        Context.addContext(frameContext);
        this.addChild(frameContext);
      }
    );

    this.#cdpClient.Page.on('lifecycleEvent', async (params) => {
      switch (params.name) {
        case 'DOMContentLoaded':
          await this.#eventManager.sendEvent(
            new BrowsingContext.DomContentLoadedEvent({
              context: this.getContextId(),
              navigation: params.loaderId,
            }),
            this.getContextId()
          );
          break;

        case 'load':
          await this.#eventManager.sendEvent(
            new LoadEvent({
              context: this.getContextId(),
              navigation: params.loaderId,
            }),
            this.getContextId()
          );
          break;
      }
    });
  }

  #updateTargetInfo(targetInfo: Protocol.Target.TargetInfo) {
    this.setUrl(targetInfo.url);
  }

  async #waitPageLifeCycleEvent(eventName: string, loaderId: string) {
    return new Promise<Protocol.Page.LifecycleEventEvent>((resolve) => {
      const handleLifecycleEvent = async (
        params: Protocol.Page.LifecycleEventEvent
      ) => {
        if (params.name !== eventName || params.loaderId !== loaderId) {
          return;
        }
        this.#cdpClient.Page.removeListener(
          'lifecycleEvent',
          handleLifecycleEvent
        );
        resolve(params);
      };

      this.#cdpClient.Page.on('lifecycleEvent', handleLifecycleEvent);
    });
  }
}
