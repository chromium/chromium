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
import { LogManager } from '../log/logManager';
import { ScriptEvaluator } from '../script/scriptEvaluator';
import { Context } from './context';

export class TargetContext extends Context {
  readonly #sessionId: string;
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

  async waitInitialized(): Promise<void> {
    await this.#initialized;
  }

  private constructor(
    targetInfo: Protocol.Target.TargetInfo,
    sessionId: string,
    cdpClient: CdpClient,
    bidiServer: IBidiServer,
    eventManager: IEventManager
  ) {
    let parentId = null;
    if (Context.hasKnownContext(targetInfo.targetId)) {
      // A frame target became a dedicated target. Keep parent.
      parentId = Context.getKnownContext(targetInfo.targetId).getParentId();
    }
    super(targetInfo.targetId, parentId, sessionId, cdpClient);
    this.#sessionId = sessionId;
    this.#bidiServer = bidiServer;
    this.#eventManager = eventManager;
    this.#scriptEvaluator = ScriptEvaluator.create(this.cdpClient);

    // Just initiate initialization, don't wait for it to complete.
    // Field `initialized` has a promise, which is resolved after initialization
    // is completed.
    // noinspection JSIgnoredPromiseFromCall
    this.#initialize();
  }

  public static async create(
    targetInfo: Protocol.Target.TargetInfo,
    sessionId: string,
    cdpClient: CdpClient,
    bidiServer: IBidiServer,
    eventManager: IEventManager
  ): Promise<void> {
    const context = new TargetContext(
      targetInfo,
      sessionId,
      cdpClient,
      bidiServer,
      eventManager
    );

    Context.addContext(context);

    await eventManager.sendEvent(
      new BrowsingContext.ContextCreatedEvent(
        context.serializeToBidiValue(0, true)
      ),
      context.getContextId()
    );
  }

  public async scriptEvaluate(
    expression: string,
    awaitPromise: boolean,
    resultOwnership: Script.OwnershipModel
  ): Promise<Script.EvaluateResult> {
    await this.waitInitialized();
    return this.#scriptEvaluator.scriptEvaluate(
      expression,
      awaitPromise,
      resultOwnership
    );
  }

  public async callFunction(
    functionDeclaration: string,
    _this: Script.ArgumentValue,
    _arguments: Script.ArgumentValue[],
    awaitPromise: boolean,
    resultOwnership: Script.OwnershipModel
  ): Promise<Script.CallFunctionResult> {
    await this.waitInitialized();
    return {
      result: await this.#scriptEvaluator.callFunction(
        functionDeclaration,
        _this,
        _arguments,
        awaitPromise,
        resultOwnership
      ),
    };
  }

  async #initialize() {
    await LogManager.create(
      this.getContextId(),
      this.cdpClient,
      this.#bidiServer,
      this.#scriptEvaluator
    );
    await this.cdpClient.Runtime.enable();
    await this.cdpClient.Page.enable();
    await this.cdpClient.Page.setLifecycleEventsEnabled({ enabled: true });
    await this.cdpClient.Target.setAutoAttach({
      autoAttach: true,
      waitForDebuggerOnStart: true,
      flatten: true,
    });

    await this.cdpClient.Runtime.runIfWaitingForDebugger();
    this.#markContextInitialized();
  }
}
