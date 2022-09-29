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
import { BrowsingContext, CDP, Script } from '../protocol/bidiProtocolTypes';
import Protocol from 'devtools-protocol';
import { IBidiServer } from '../../utils/bidiServer';
import { IEventManager } from '../events/EventManager';
import {
  InvalidArgumentErrorResponse,
  NoSuchFrameException,
} from '../protocol/error';
import { BrowsingContextImpl, ScriptTarget } from './browsingContextImpl';
import { Realm } from '../script/realm';

const logContext = log('context');

export class BrowsingContextProcessor {
  static #contexts: Map<string, BrowsingContextImpl> = new Map();

  static getTopLevelContexts(): BrowsingContextImpl[] {
    return Array.from(BrowsingContextProcessor.#contexts.values()).filter(
      (c) => c.parentId === null
    );
  }

  async #removeChildContexts(context: BrowsingContextImpl): Promise<void> {
    await Promise.all(
      context.children.map((child) => this.#removeContext(child.contextId))
    );
  }

  async #removeContext(contextId: string): Promise<void> {
    if (!BrowsingContextProcessor.#hasKnownContext(contextId)) {
      return;
    }
    const context = BrowsingContextProcessor.#getKnownContext(contextId);

    // Remove context's children.
    await this.#removeChildContexts(context);

    // Remove context from the parent.
    if (context.parentId !== null) {
      BrowsingContextProcessor.#findContext(context.parentId)?.removeChild(
        context.contextId
      );
    }

    await this.#eventManager.sendEvent(
      new BrowsingContext.ContextDestroyedEvent(
        context.serializeToBidiValue(0, true)
      ),
      context.contextId
    );
    BrowsingContextProcessor.#contexts.delete(contextId);
  }

  static #registerContext(context: BrowsingContextImpl) {
    BrowsingContextProcessor.#contexts.set(context.contextId, context);
    if (context.parentId !== null) {
      BrowsingContextProcessor.#getKnownContext(context.parentId).addChild(
        context
      );
    }
  }

  static #hasKnownContext(contextId: string): boolean {
    return BrowsingContextProcessor.#contexts.has(contextId);
  }

  static #findContext(contextId: string): BrowsingContextImpl | undefined {
    return BrowsingContextProcessor.#contexts.get(contextId)!;
  }

  static #getKnownContext(contextId: string): BrowsingContextImpl {
    const result = BrowsingContextProcessor.#findContext(contextId);
    if (result === undefined) {
      throw new NoSuchFrameException(`Context ${contextId} not found`);
    }
    return result;
  }

  readonly sessions: Set<string> = new Set();
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

    this.#setBrowserClientEventListeners(this.#cdpConnection.browserClient());
  }

  #setBrowserClientEventListeners(browserClient: CdpClient) {
    this.#setTargetEventListeners(browserClient);
  }

  #setTargetEventListeners(cdpClient: CdpClient) {
    cdpClient.Target.on('attachedToTarget', async (params) => {
      await this.#handleAttachedToTargetEvent(params, cdpClient);
    });
    cdpClient.Target.on('detachedFromTarget', async (params) => {
      await this.#handleDetachedFromTargetEvent(params);
    });
  }

  #setSessionEventListeners(sessionId: string) {
    if (this.sessions.has(sessionId)) {
      return;
    }
    this.sessions.add(sessionId);

    const sessionCdpClient = this.#cdpConnection.getCdpClient(sessionId);

    this.#setTargetEventListeners(sessionCdpClient);

    sessionCdpClient.on('event', async (method, params) => {
      await this.#eventManager.sendEvent(
        {
          method: 'PROTO.cdp.eventReceived',
          params: {
            cdpMethod: method,
            cdpParams: params,
            cdpSession: sessionId,
          },
        },
        null
      );
    });

    sessionCdpClient.Page.on(
      'frameNavigated',
      async (params: Protocol.Page.FrameNavigatedEvent) => {
        const contextId = params.frame.id;
        if (!BrowsingContextProcessor.#hasKnownContext(contextId)) {
          return;
        }
        const context = BrowsingContextProcessor.#getKnownContext(contextId);
        // At the point the page is initiated, all the nested iframes from the
        // previous page are detached and realms are destroyed.
        // Remove context's children.
        await this.#removeChildContexts(context);
      }
    );

    sessionCdpClient.Page.on(
      'frameAttached',
      async (params: Protocol.Page.FrameAttachedEvent) => {
        const context = BrowsingContextImpl.createFrameContext(
          params.frameId,
          params.parentFrameId,
          sessionCdpClient,
          this.#bidiServer,
          sessionId,
          this.#eventManager
        );

        BrowsingContextProcessor.#registerContext(context);
        await this.#eventManager.sendEvent(
          new BrowsingContext.ContextCreatedEvent(
            context.serializeToBidiValue(0, true)
          ),
          context.contextId
        );
      }
    );
  }

  async #handleAttachedToTargetEvent(
    params: Protocol.Target.AttachedToTargetEvent,
    parentSessionCdpClient: CdpClient
  ) {
    logContext('AttachedToTarget event received: ' + JSON.stringify(params));

    const { sessionId, targetInfo } = params;

    let targetSessionCdpClient = this.#cdpConnection.getCdpClient(sessionId);

    if (!this.#isValidTarget(targetInfo)) {
      // DevTools or some other not supported by BiDi target.
      await targetSessionCdpClient.Runtime.runIfWaitingForDebugger();
      await parentSessionCdpClient.Target.detachFromTarget(params);
      return;
    }

    this.#setSessionEventListeners(sessionId);

    if (BrowsingContextProcessor.#hasKnownContext(targetInfo.targetId)) {
      // OOPiF.
      BrowsingContextImpl.convertFrameToTargetContext(
        BrowsingContextProcessor.#getKnownContext(targetInfo.targetId),
        targetSessionCdpClient,
        sessionId
      );
    } else {
      const context = BrowsingContextImpl.createTargetContext(
        targetInfo.targetId,
        null,
        targetSessionCdpClient,
        this.#bidiServer,
        sessionId,
        this.#eventManager
      );

      BrowsingContextProcessor.#registerContext(context);
      await this.#eventManager.sendEvent(
        new BrowsingContext.ContextCreatedEvent(
          context.serializeToBidiValue(0, true)
        ),
        context.contextId
      );
    }
  }

  // { "method": "Target.detachedFromTarget",
  //   "params": {
  //     "sessionId": "7EFBFB2A4942A8989B3EADC561BC46E9",
  //     "targetId": "19416886405CBA4E03DBB59FA67FF4E8" } }
  async #handleDetachedFromTargetEvent(
    params: Protocol.Target.DetachedFromTargetEvent
  ) {
    // TODO: params.targetId is deprecated. Update this class to track using
    // params.sessionId instead.
    // https://github.com/GoogleChromeLabs/chromium-bidi/issues/60
    const contextId = params.targetId!;
    if (!BrowsingContextProcessor.#hasKnownContext(contextId)) {
      return;
    }
    await this.#removeContext(contextId);
  }

  async process_browsingContext_getTree(
    params: BrowsingContext.GetTreeParameters
  ): Promise<BrowsingContext.GetTreeResult> {
    const resultContexts =
      params.root === undefined
        ? BrowsingContextProcessor.getTopLevelContexts()
        : [BrowsingContextProcessor.#getKnownContext(params.root)];

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

    // Wait for the new tab to be loaded to avoid race conditions in the
    // `browsingContext` events, when the `browsingContext.domContentLoaded` and
    // `browsingContext.load` events from the initial `about:blank` navigation
    // are emitted after the next navigation is started.
    // Details: https://github.com/web-platform-tests/wpt/issues/35846
    const contextId = result.targetId;
    const context = BrowsingContextProcessor.#getKnownContext(contextId);
    await context.awaitLoaded();

    return {
      result: {
        context: contextId,
        parent: null,
        url: 'about:blank',
        children: [],
      },
    };
  }

  async process_browsingContext_navigate(
    params: BrowsingContext.NavigateParameters
  ): Promise<BrowsingContext.NavigateResult> {
    const context = BrowsingContextProcessor.#getKnownContext(params.context);

    return await context.navigate(
      params.url,
      params.wait !== undefined ? params.wait : 'none'
    );
  }

  async process_script_evaluate(
    params: Script.EvaluateParameters
  ): Promise<Script.EvaluateResult> {
    const scriptTarget = BrowsingContextProcessor.#getScriptTarget(
      params.target
    );

    return await scriptTarget.context.scriptEvaluate(
      params.expression,
      scriptTarget.target,
      params.awaitPromise,
      params.resultOwnership ?? 'none'
    );
  }

  static #getScriptTarget(target: Script.Target): {
    context: BrowsingContextImpl;
    target: ScriptTarget;
  } {
    if ('realm' in target) {
      const { executionContextId, browsingContextId } = Realm.getRealm(
        target.realm
      );
      return {
        context: BrowsingContextProcessor.#getKnownContext(browsingContextId),
        target: { executionContext: executionContextId },
      };
    } else {
      return {
        context: BrowsingContextProcessor.#getKnownContext(target.context),
        target: { sandbox: target.sandbox ?? null },
      };
    }
  }

  process_script_getRealms(
    params: Script.GetRealmsParameters
  ): Script.GetRealmsResult {
    if (params.context !== undefined) {
      // Make sure the context is known.
      BrowsingContextProcessor.#getKnownContext(params.context);
    }
    const realms = Realm.findRealms({
      browsingContextId: params.context,
      type: params.type,
    }).map((realm) => realm.toBiDi());
    return { result: { realms } };
  }

  async process_script_callFunction(
    params: Script.CallFunctionParameters
  ): Promise<Script.CallFunctionResult> {
    const scriptTarget = BrowsingContextProcessor.#getScriptTarget(
      params.target
    );

    return await scriptTarget.context.callFunction(
      params.functionDeclaration,
      params.this || {
        type: 'undefined',
      }, // `this` is `undefined` by default.
      params.arguments || [], // `arguments` is `[]` by default.
      scriptTarget.target,
      params.awaitPromise,
      params.resultOwnership ?? 'none'
    );
  }

  async process_PROTO_browsingContext_findElement(
    params: BrowsingContext.PROTO.FindElementParameters
  ): Promise<BrowsingContext.PROTO.FindElementResult> {
    const context = BrowsingContextProcessor.#getKnownContext(params.context);
    return await context.findElement(params.selector);
  }

  async process_browsingContext_close(
    commandParams: BrowsingContext.CloseParameters
  ): Promise<BrowsingContext.CloseResult> {
    const browserCdpClient = this.#cdpConnection.browserClient();

    const context = BrowsingContextProcessor.#getKnownContext(
      commandParams.context
    );
    if (context.parentId !== null) {
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
    return {
      result: sendCdpCommandResult,
      cdpSession: params.cdpSession,
    };
  }

  async process_PROTO_cdp_getSession(params: CDP.PROTO.GetSessionParams) {
    const context = params.context;
    const sessionId =
      BrowsingContextProcessor.#getKnownContext(context).cdpSessionId;
    if (sessionId === undefined) {
      return { result: { cdpSession: null } };
    }
    return { result: { cdpSession: sessionId } };
  }
}
