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
import { Context } from './context';
import { BrowsingContext, Script } from '../protocol/bidiProtocolTypes';
import { UnknownErrorResponse } from '../protocol/error';
import { CdpClient } from '../../../cdp';
import { IEventManager } from '../events/EventManager';

export class FrameContext extends Context {
  private constructor(
    params: Protocol.Page.FrameAttachedEvent,
    sessionId: string,
    cdpClient: CdpClient
  ) {
    super(params.frameId, params.parentFrameId, sessionId, cdpClient);
  }

  public static async create(
    params: Protocol.Page.FrameAttachedEvent,
    sessionId: string,
    cdpClient: CdpClient,
    eventManager: IEventManager
  ): Promise<void> {
    const context = new FrameContext(params, sessionId, cdpClient);
    Context.addContext(context);
    Context.getKnownContext(params.parentFrameId).addChild(context);

    await eventManager.sendEvent(
      new BrowsingContext.ContextCreatedEvent(
        context.serializeToBidiValue(0, true)
      ),
      context.getContextId()
    );
  }

  callFunction(
    functionDeclaration: string,
    _this: Script.ArgumentValue,
    _arguments: Script.ArgumentValue[],
    awaitPromise: boolean,
    resultOwnership: Script.OwnershipModel
  ): Promise<Script.CallFunctionResult> {
    throw new UnknownErrorResponse('Not implemented');
  }

  scriptEvaluate(
    expression: string,
    awaitPromise: boolean,
    resultOwnership: Script.OwnershipModel
  ): Promise<Script.EvaluateResult> {
    throw new UnknownErrorResponse('Not implemented');
  }

  async waitInitialized(): Promise<void> {
    if (this.getParentId() !== null) {
      await Context.getKnownContext(this.getParentId()!).waitInitialized();
    }
  }
}
