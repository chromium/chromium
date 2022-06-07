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
import { CdpClient } from '../../../cdp';
import { BrowsingContext, Script } from '../protocol/bidiProtocolTypes';
import { IBidiServer } from '../../utils/bidiServer';
import { IEventManager } from '../events/EventManager';
import { LogManager } from './logManager';
import { ScriptEvaluator } from './scriptEvaluator';
import LoadEvent = BrowsingContext.LoadEvent;

export class Context {
  #targetInfo?: Protocol.Target.TargetInfo;
  _sessionId?: string;

  readonly #contextId: string;
  readonly #cdpClient: CdpClient;
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
    _contextId: string,
    _cdpClient: CdpClient,
    _bidiServer: IBidiServer,
    _eventManager: IEventManager
  ) {
    this.#contextId = _contextId;
    this.#cdpClient = _cdpClient;
    this.#bidiServer = _bidiServer;
    this.#eventManager = _eventManager;
    this.#scriptEvaluator = ScriptEvaluator.create(_cdpClient);
    // Just initiate initialization, don't wait for it to complete.
    // Field `initialized` has a promise, which is resolved after initialization
    // is completed.
    // noinspection JSIgnoredPromiseFromCall
    this.#initialize();
  }

  public static create(
    _contextId: string,
    _cdpClient: CdpClient,
    _bidiServer: IBidiServer,
    _eventManager: IEventManager
  ): Context {
    return new Context(_contextId, _cdpClient, _bidiServer, _eventManager);
  }

  public setSessionId(sessionId: string): void {
    this._sessionId = sessionId;
  }

  public updateTargetInfo(targetInfo: Protocol.Target.TargetInfo) {
    this.#targetInfo = targetInfo;
  }

  public onInfoChangedEvent(targetInfo: Protocol.Target.TargetInfo) {
    this.updateTargetInfo(targetInfo);
  }

  public get id(): string {
    return this.#contextId;
  }

  public serializeToBidiValue(): BrowsingContext.Info {
    return {
      context: this.#targetInfo!.targetId,
      parent: this.#targetInfo!.openerId ? this.#targetInfo!.openerId : null,
      url: this.#targetInfo!.url,
      // TODO sadym: implement.
      children: [],
    };
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

  public async findElement(
    selector: string
  ): Promise<BrowsingContext.PROTO.FindElementResult> {
    await this.#waitInitialized();
    const functionDeclaration = String((resultsSelector: string) =>
      document.querySelector(resultsSelector)
    );
    const _arguments: Script.ArgumentValue[] = [
      { type: 'string', value: selector },
    ];

    // TODO(sadym): handle not found exception.
    const result = await this.#scriptEvaluator.callFunction(
      functionDeclaration,
      {
        type: 'undefined',
      },
      _arguments,
      true
    );

    // TODO(sadym): handle type properly.
    return { result } as any as BrowsingContext.PROTO.FindElementResult;
  }

  async #initialize() {
    await LogManager.create(
      this.#contextId,
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
  }

  #initializeCdpEventListeners() {
    this.#cdpClient.on('event', async (method, params) => {
      await this.#eventManager.sendEvent(
        {
          method: 'PROTO.cdp.eventReceived',
          params: {
            cdpMethod: method,
            cdpParams: params,
            session: this._sessionId,
          },
        },
        null
      );
    });
  }

  #initializePageLifecycleEventListener() {
    this.#cdpClient.Page.on('lifecycleEvent', async (params) => {
      switch (params.name) {
        case 'DOMContentLoaded':
          await this.#eventManager.sendEvent(
            new BrowsingContext.DomContentLoadedEvent({
              context: this.#contextId,
              navigation: params.loaderId,
            }),
            this.#contextId
          );
          break;

        case 'load':
          await this.#eventManager.sendEvent(
            new LoadEvent({
              context: this.#contextId,
              navigation: params.loaderId,
            }),
            this.#contextId
          );
          break;
      }
    });
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

  public async scriptEvaluate(
    expression: string,
    awaitPromise: boolean
  ): Promise<Script.EvaluateResult> {
    await this.#waitInitialized();
    return this.#scriptEvaluator.scriptEvaluate(expression, awaitPromise);
  }
}
