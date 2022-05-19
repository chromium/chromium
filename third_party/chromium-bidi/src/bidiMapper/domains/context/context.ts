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

  private constructor(
    _contextId: string,
    _cdpClient: CdpClient,
    _bidiServer: IBidiServer,
    _eventManager: IEventManager,
    _serializer: ScriptEvaluator
  ) {
    this.#contextId = _contextId;
    this.#cdpClient = _cdpClient;
    this.#bidiServer = _bidiServer;
    this.#eventManager = _eventManager;
    this.#scriptEvaluator = _serializer;
  }

  public static async create(
    contextId: string,
    cdpClient: CdpClient,
    bidiServer: IBidiServer,
    eventManager: IEventManager
  ) {
    const context = new Context(
      contextId,
      cdpClient,
      bidiServer,
      eventManager,
      ScriptEvaluator.create(cdpClient)
    );

    await context.#initialize();

    await LogManager.create(
      contextId,
      cdpClient,
      bidiServer,
      context.#scriptEvaluator
    );
    return context;
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

  public serializeToBidiValue(): BrowsingContext.BrowsingContextInfo {
    return {
      context: this.#targetInfo!.targetId,
      parent: this.#targetInfo!.openerId
        ? this.#targetInfo!.openerId
        : undefined,
      url: this.#targetInfo!.url,
      // TODO sadym: implement.
      children: [],
    };
  }

  public async navigate(
    url: string,
    wait: BrowsingContext.ReadinessState
  ): Promise<BrowsingContext.NavigateResult> {
    // TODO: handle loading errors.
    // noinspection TypeScriptValidateJSTypes
    const cdpNavigateResult = await this.#cdpClient.Page.navigate({ url });

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
        await this.#waitPageLifeCycleEvent('load', cdpNavigateResult.loaderId!);
        break;

      default:
        throw new Error(`Not implemented wait '${wait}'`);
    }

    return {
      result: {
        navigation: cdpNavigateResult.loaderId,
        url: url,
      },
    };
  }

  public async callFunction(
    functionDeclaration: string,
    _this: Script.ArgumentValue,
    args: Script.ArgumentValue[],
    awaitPromise: boolean
  ): Promise<Script.CallFunctionResult> {
    return this.#scriptEvaluator.callFunction(
      functionDeclaration,
      _this,
      args,
      awaitPromise
    );
  }

  public async findElement(
    selector: string
  ): Promise<BrowsingContext.PROTO.FindElementResult> {
    const functionDeclaration = String((resultsSelector: string) =>
      document.querySelector(resultsSelector)
    );
    const args: Script.ArgumentValue[] = [{ type: 'string', value: selector }];

    return (await this.#scriptEvaluator.callFunction(
      functionDeclaration,
      {
        type: 'undefined',
      },
      args,
      true
    )) as BrowsingContext.PROTO.FindElementResult;
  }

  async #initialize() {
    // Enabling Runtime doamin needed to have an exception stacktrace in
    // `evaluateScript`.
    await this.#cdpClient.Runtime.enable();
    await this.#cdpClient.Page.enable();
    await this.#cdpClient.Page.setLifecycleEventsEnabled({ enabled: true });
    this.#initializeEventListeners();
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
    this.#cdpClient.Page.setLifecycleEventsEnabled({ enabled: true });

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
    return this.#scriptEvaluator.scriptEvaluate(expression, awaitPromise);
  }
}
