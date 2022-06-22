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

export class FrameContext extends Context {
  readonly #cdpClient: CdpClient;

  private constructor(
    params: Protocol.Page.FrameAttachedEvent,
    sessionId: string,
    cdpClient: CdpClient
  ) {
    super(params.frameId, params.parentFrameId, sessionId);
    this.#cdpClient = cdpClient;
  }

  public static async create(
    params: Protocol.Page.FrameAttachedEvent,
    sessionId: string,
    cdpClient: CdpClient
  ): Promise<FrameContext> {
    const context = new FrameContext(params, sessionId, cdpClient);

    // TODO(sadym): Add `Page.frameNavigated` handler.
    context.#initializeEventListeners();
    return context;
  }

  callFunction(
    functionDeclaration: string,
    _this: Script.ArgumentValue,
    _arguments: Script.ArgumentValue[],
    awaitPromise: boolean
  ): Promise<Script.CallFunctionResult> {
    throw new UnknownErrorResponse('Not implemented');
  }

  navigate(
    url: string,
    wait: BrowsingContext.ReadinessState
  ): Promise<BrowsingContext.NavigateResult> {
    throw new UnknownErrorResponse('Not implemented');
  }

  scriptEvaluate(
    expression: string,
    awaitPromise: boolean
  ): Promise<Script.EvaluateResult> {
    throw new UnknownErrorResponse('Not implemented');
  }

  #initializeEventListeners() {
    this.#cdpClient.Page.on(
      'frameNavigated',
      (params: Protocol.Page.FrameNavigatedEvent) => {
        const frame = params.frame;
        if (params.frame.id === this.getContextId()) {
          this.setUrl(frame.url);
        }
      }
    );
  }
}
