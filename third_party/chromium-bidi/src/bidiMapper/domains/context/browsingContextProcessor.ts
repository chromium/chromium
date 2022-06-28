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

import { log } from '../../../utils/log';
import { CdpClient, CdpConnection } from '../../../cdp';
import { TargetContext } from './targetContext';
import { BrowsingContext, CDP, Script } from '../protocol/bidiProtocolTypes';
import Protocol from 'devtools-protocol';
import { IBidiServer } from '../../utils/bidiServer';
import { IEventManager } from '../events/EventManager';
import { InvalidArgumentErrorResponse } from '../protocol/error';
import { Context } from './context';

const logContext = log('context');

export class BrowsingContextProcessor {
  readonly #cdpConnection: CdpConnection;
  readonly #selfTargetId: string;
  readonly #bidiServer: IBidiServer;
  readonly #eventManager: IEventManager;

  constructor(
    cdpConnection: CdpConnection,
    selfTargetId: string,
    bidiServer: IBidiServer,
    eventManager: IEventManager
  ) {
    this.#cdpConnection = cdpConnection;
    this.#selfTargetId = selfTargetId;
    this.#bidiServer = bidiServer;
    this.#eventManager = eventManager;

    this.#setCdpEventListeners(this.#cdpConnection.browserClient());
  }

  #setCdpEventListeners(cdpClient: CdpClient) {
    cdpClient.Target.on('attachedToTarget', async (params) => {
      await this.#handleAttachedToTargetEvent(params);
    });
    cdpClient.Target.on('detachedFromTarget', async (params) => {
      await this.#handleDetachedFromTargetEvent(params);
    });
  }

  async #handleAttachedToTargetEvent(
    params: Protocol.Target.AttachedToTargetEvent
  ) {
    logContext('AttachedToTarget event received: ' + JSON.stringify(params));

    const { sessionId, targetInfo } = params;

    // TODO(sadym): Set listeners only once per session.
    this.#setCdpEventListeners(this.#cdpConnection.getCdpClient(sessionId));

    if (!this.#isValidTarget(targetInfo)) {
      // DevTools or some other not supported by BiDi target.
      await this.#cdpConnection
        .getCdpClient(sessionId)
        .Runtime.runIfWaitingForDebugger();
      await this.#cdpConnection.browserClient().Target.detachFromTarget(params);
      return;
    }

    const context = await TargetContext.create(
      targetInfo,
      sessionId,
      this.#cdpConnection,
      this.#bidiServer,
      this.#eventManager
    );

    Context.addContext(context);

    await this.#eventManager.sendEvent(
      new BrowsingContext.ContextCreatedEvent(
        context.serializeToBidiValue(0, true)
      ),
      context.getContextId()
    );
  }

  // { "method": "Target.detachedFromTarget",
  //   "params": {
  //     "sessionId": "7EFBFB2A4942A8989B3EADC561BC46E9",
  //     "targetId": "19416886405CBA4E03DBB59FA67FF4E8" } }
  async #handleDetachedFromTargetEvent(
    params: Protocol.Target.DetachedFromTargetEvent
  ) {
    logContext('detachedFromTarget event received: ' + JSON.stringify(params));

    // TODO: params.targetId is deprecated. Update this class to track using
    // params.sessionId instead.
    // https://github.com/GoogleChromeLabs/chromium-bidi/issues/60
    const contextId = params.targetId!;
    if (!Context.hasKnownContext(contextId)) {
      return;
    }
    const context = Context.getKnownContext(contextId);
    Context.removeContext(contextId);
    await this.#eventManager.sendEvent(
      new BrowsingContext.ContextDestroyedEvent(
        context.serializeToBidiValue(0, true)
      ),
      contextId
    );
  }

  async process_browsingContext_getTree(
    params: BrowsingContext.GetTreeParameters
  ): Promise<BrowsingContext.GetTreeResult> {
    const resultContexts =
      params.root === undefined
        ? Context.getTopLevelContexts()
        : [Context.getKnownContext(params.root)];

    return {
      result: {
        contexts: resultContexts.map((c) =>
          c.serializeToBidiValue(params.maxDepth ?? Number.MAX_VALUE, true)
        ),
      },
    };
  }

  async process_browsingContext_create(
    params: BrowsingContext.CreateParameters
  ): Promise<BrowsingContext.CreateResult> {
    const browserCdpClient = this.#cdpConnection.browserClient();

    const result = await browserCdpClient.Target.createTarget({
      url: 'about:blank',
      newWindow: params.type === 'window',
    });

    return {
      result: {
        context: result.targetId,
        parent: null,
        url: 'about:blank',
        children: [],
      },
    };
  }

  async process_browsingContext_navigate(
    params: BrowsingContext.NavigateParameters
  ): Promise<BrowsingContext.NavigateResult> {
    const context = Context.getKnownContext(params.context);

    return await context.navigate(
      params.url,
      params.wait !== undefined ? params.wait : 'none'
    );
  }

  async process_script_evaluate(
    params: Script.EvaluateParameters
  ): Promise<Script.EvaluateResult> {
    const context = Context.getKnownContext(
      (params.target as Script.ContextTarget).context
    );
    return await context.scriptEvaluate(params.expression, params.awaitPromise);
  }

  async process_script_callFunction(
    params: Script.CallFunctionParameters
  ): Promise<Script.CallFunctionResult> {
    const context = Context.getKnownContext(
      (params.target as Script.ContextTarget).context
    );
    return await context.callFunction(
      params.functionDeclaration,
      params.this || {
        type: 'undefined',
      }, // `this` is `undefined` by default.
      params.arguments || [], // `arguments` is `[]` by default.
      params.awaitPromise
    );
  }

  async process_PROTO_browsingContext_findElement(
    params: BrowsingContext.PROTO.FindElementParameters
  ): Promise<BrowsingContext.PROTO.FindElementResult> {
    const context = Context.getKnownContext(params.context);
    return await context.findElement(params.selector);
  }

  async process_browsingContext_close(
    commandParams: BrowsingContext.CloseParameters
  ): Promise<BrowsingContext.CloseResult> {
    const browserCdpClient = this.#cdpConnection.browserClient();

    const context = Context.getKnownContext(commandParams.context);
    if (context.getParentId() !== null) {
      throw new InvalidArgumentErrorResponse(
        'Not a top-level browsing context cannot be closed.'
      );
    }

    const detachedFromTargetPromise = new Promise<void>(async (resolve) => {
      const onContextDestroyed = (
        eventParams: Protocol.Target.DetachedFromTargetEvent
      ) => {
        if (eventParams.targetId === commandParams.context) {
          browserCdpClient.Target.removeListener(
            'detachedFromTarget',
            onContextDestroyed
          );
          resolve();
        }
      };
      browserCdpClient.Target.on('detachedFromTarget', onContextDestroyed);
    });

    await this.#cdpConnection.browserClient().Target.closeTarget({
      targetId: commandParams.context,
    });

    // Sometimes CDP command finishes before `detachedFromTarget` event,
    // sometimes after. Wait for the CDP command to be finished, and then wait
    // for `detachedFromTarget` if it hasn't emitted.
    await detachedFromTargetPromise;

    return { result: {} };
  }

  #isValidTarget(target: Protocol.Target.TargetInfo) {
    if (target.targetId === this.#selfTargetId) {
      return false;
    }
    return ['page', 'iframe'].includes(target.type);
  }

  async process_PROTO_cdp_sendCommand(params: CDP.PROTO.SendCommandParams) {
    const sendCdpCommandResult = await this.#cdpConnection.sendCommand(
      params.cdpMethod,
      params.cdpParams,
      params.cdpSession ?? null
    );
    return { result: sendCdpCommandResult };
  }

  async process_PROTO_cdp_getSession(params: CDP.PROTO.GetSessionParams) {
    const context = params.context;
    const sessionId = Context.getKnownContext(context).getSessionId();
    if (sessionId === undefined) {
      return { result: { session: null } };
    }
    return { result: { session: sessionId } };
  }
}
