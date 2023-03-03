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
import {
  BrowsingContext,
  CDP,
  Message,
  Script,
} from '../../../protocol/protocol.js';
import {CdpClient, CdpConnection} from '../../CdpConnection.js';
import {LogType, LoggerFn} from '../../../utils/log.js';
import {BrowsingContextImpl} from './browsingContextImpl.js';
import {BrowsingContextStorage} from './browsingContextStorage.js';
import {IEventManager} from '../events/EventManager.js';
import Protocol from 'devtools-protocol';
import {Realm} from '../script/realm.js';
import {RealmStorage} from '../script/realmStorage.js';

export class BrowsingContextProcessor {
  readonly #browsingContextStorage: BrowsingContextStorage;
  readonly #cdpConnection: CdpConnection;
  readonly #eventManager: IEventManager;
  readonly #logger?: LoggerFn;
  readonly #realmStorage: RealmStorage;
  readonly #selfTargetId: string;
  readonly #sessions: Set<string>;

  constructor(
    realmStorage: RealmStorage,
    cdpConnection: CdpConnection,
    selfTargetId: string,
    eventManager: IEventManager,
    browsingContextStorage: BrowsingContextStorage,
    logger?: LoggerFn
  ) {
    this.#browsingContextStorage = browsingContextStorage;
    this.#cdpConnection = cdpConnection;
    this.#eventManager = eventManager;
    this.#logger = logger;
    this.#realmStorage = realmStorage;
    this.#selfTargetId = selfTargetId;
    this.#sessions = new Set();

    this.#setBrowserClientEventListeners(this.#cdpConnection.browserClient());
  }

  #setBrowserClientEventListeners(browserClient: CdpClient) {
    this.#setTargetEventListeners(browserClient);
  }

  #setTargetEventListeners(cdpClient: CdpClient) {
    cdpClient.on('Target.attachedToTarget', async (params) => {
      await this.#handleAttachedToTargetEvent(params, cdpClient);
    });
    cdpClient.on('Target.detachedFromTarget', async (params) => {
      await this.#handleDetachedFromTargetEvent(params);
    });
  }

  #setSessionEventListeners(sessionId: string) {
    if (this.#sessions.has(sessionId)) {
      return;
    }
    this.#sessions.add(sessionId);

    const sessionCdpClient = this.#cdpConnection.getCdpClient(sessionId);

    this.#setTargetEventListeners(sessionCdpClient);

    sessionCdpClient.on('*', async (method, params) => {
      await this.#eventManager.registerEvent(
        {
          method: CDP.EventNames.EventReceivedEvent,
          params: {
            cdpMethod: method,
            cdpParams: params || {},
            cdpSession: sessionId,
          },
        },
        null
      );
    });

    sessionCdpClient.on(
      'Page.frameAttached',
      async (params: Protocol.Page.FrameAttachedEvent) => {
        await BrowsingContextImpl.createFrameContext(
          this.#realmStorage,
          params.frameId,
          params.parentFrameId,
          sessionCdpClient,
          sessionId,
          this.#eventManager,
          this.#browsingContextStorage,
          this.#logger
        );
      }
    );
  }

  async #handleAttachedToTargetEvent(
    params: Protocol.Target.AttachedToTargetEvent,
    parentSessionCdpClient: CdpClient
  ) {
    const {sessionId, targetInfo} = params;

    const targetSessionCdpClient = this.#cdpConnection.getCdpClient(sessionId);

    if (!this.#isValidTarget(targetInfo)) {
      // DevTools or some other not supported by BiDi target.
      await targetSessionCdpClient.sendCommand(
        'Runtime.runIfWaitingForDebugger'
      );
      await parentSessionCdpClient.sendCommand(
        'Target.detachFromTarget',
        params
      );
      return;
    }

    this.#logger?.(
      LogType.browsingContexts,
      'AttachedToTarget event received:',
      JSON.stringify(params, null, 2)
    );

    this.#setSessionEventListeners(sessionId);

    if (this.#browsingContextStorage.hasKnownContext(targetInfo.targetId)) {
      // OOPiF.
      this.#browsingContextStorage
        .getKnownContext(targetInfo.targetId)
        .convertFrameToTargetContext(targetSessionCdpClient, sessionId);
    } else {
      await BrowsingContextImpl.createTargetContext(
        this.#realmStorage,
        targetInfo.targetId,
        null,
        targetSessionCdpClient,
        sessionId,
        this.#eventManager,
        this.#browsingContextStorage,
        this.#logger
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
    await this.#browsingContextStorage.findContext(contextId)?.delete();
  }

  process_browsingContext_getTree(
    params: BrowsingContext.GetTreeParameters
  ): BrowsingContext.GetTreeResult {
    const resultContexts =
      params.root === undefined
        ? this.#browsingContextStorage.getTopLevelContexts()
        : [this.#browsingContextStorage.getKnownContext(params.root)];

    return {
      result: {
        contexts: resultContexts.map((c) =>
          c.serializeToBidiValue(params.maxDepth ?? Number.MAX_VALUE)
        ),
      },
    };
  }

  async process_browsingContext_create(
    params: BrowsingContext.CreateParameters
  ): Promise<BrowsingContext.CreateResult> {
    const browserCdpClient = this.#cdpConnection.browserClient();
    let referenceContext = undefined;
    if (params.referenceContext !== undefined) {
      referenceContext = this.#browsingContextStorage.getKnownContext(
        params.referenceContext
      );
      if (referenceContext.parentId !== null) {
        throw new Message.InvalidArgumentException(
          `referenceContext should be a top-level context`
        );
      }
    }

    const result = await browserCdpClient.sendCommand('Target.createTarget', {
      url: 'about:blank',
      newWindow: params.type === 'window',
    });

    // Wait for the new tab to be loaded to avoid race conditions in the
    // `browsingContext` events, when the `browsingContext.domContentLoaded` and
    // `browsingContext.load` events from the initial `about:blank` navigation
    // are emitted after the next navigation is started.
    // Details: https://github.com/web-platform-tests/wpt/issues/35846
    const contextId = result.targetId;
    const context = this.#browsingContextStorage.getKnownContext(contextId);
    await context.awaitLoaded();

    return {
      result: context.serializeToBidiValue(1),
    };
  }

  async process_browsingContext_navigate(
    params: BrowsingContext.NavigateParameters
  ): Promise<BrowsingContext.NavigateResult> {
    const context = this.#browsingContextStorage.getKnownContext(
      params.context
    );

    return context.navigate(
      params.url,
      params.wait === undefined ? 'none' : params.wait
    );
  }

  async #getRealm(target: Script.Target): Promise<Realm> {
    if ('realm' in target) {
      return this.#realmStorage.getRealm({
        realmId: target.realm,
      });
    }
    const context = this.#browsingContextStorage.getKnownContext(
      target.context
    );
    return context.getOrCreateSandbox(target.sandbox);
  }

  async process_script_evaluate(
    params: Script.EvaluateParameters
  ): Promise<Script.EvaluateResult> {
    const realm = await this.#getRealm(params.target);
    return realm.scriptEvaluate(
      params.expression,
      params.awaitPromise,
      params.resultOwnership ?? 'none'
    );
  }

  process_script_getRealms(
    params: Script.GetRealmsParameters
  ): Script.GetRealmsResult {
    if (params.context !== undefined) {
      // Make sure the context is known.
      this.#browsingContextStorage.getKnownContext(params.context);
    }
    const realms = this.#realmStorage
      .findRealms({
        browsingContextId: params.context,
        type: params.type,
      })
      .map((realm: Realm) => realm.toBiDi());
    return {result: {realms}};
  }

  async process_script_callFunction(
    params: Script.CallFunctionParameters
  ): Promise<Script.CallFunctionResult> {
    const realm = await this.#getRealm(params.target);
    return realm.callFunction(
      params.functionDeclaration,
      params.this || {
        type: 'undefined',
      }, // `this` is `undefined` by default.
      params.arguments || [], // `arguments` is `[]` by default.
      params.awaitPromise,
      params.resultOwnership ?? 'none'
    );
  }

  async process_script_disown(
    params: Script.DisownParameters
  ): Promise<Script.DisownResult> {
    const realm = await this.#getRealm(params.target);
    await Promise.all(params.handles.map(async (h) => realm.disown(h)));
    return {result: {}};
  }

  async process_browsingContext_close(
    commandParams: BrowsingContext.CloseParameters
  ): Promise<BrowsingContext.CloseResult> {
    const browserCdpClient = this.#cdpConnection.browserClient();

    const context = this.#browsingContextStorage.getKnownContext(
      commandParams.context
    );
    if (context.parentId !== null) {
      throw new Message.InvalidArgumentException(
        'Not a top-level browsing context cannot be closed.'
      );
    }

    const detachedFromTargetPromise = new Promise<void>((resolve) => {
      const onContextDestroyed = (
        eventParams: Protocol.Target.DetachedFromTargetEvent
      ) => {
        if (eventParams.targetId === commandParams.context) {
          browserCdpClient.off('Target.detachedFromTarget', onContextDestroyed);
          resolve();
        }
      };
      browserCdpClient.on('Target.detachedFromTarget', onContextDestroyed);
    });

    await this.#cdpConnection
      .browserClient()
      .sendCommand('Target.closeTarget', {
        targetId: commandParams.context,
      });

    // Sometimes CDP command finishes before `detachedFromTarget` event,
    // sometimes after. Wait for the CDP command to be finished, and then wait
    // for `detachedFromTarget` if it hasn't emitted.
    await detachedFromTargetPromise;

    return {result: {}};
  }

  #isValidTarget(target: Protocol.Target.TargetInfo) {
    if (target.targetId === this.#selfTargetId) {
      return false;
    }
    return ['page', 'iframe'].includes(target.type);
  }

  async process_cdp_sendCommand(params: CDP.SendCommandParams) {
    const client = params.cdpSession
      ? this.#cdpConnection.getCdpClient(params.cdpSession)
      : this.#cdpConnection.browserClient();
    const sendCdpCommandResult = await client.sendCommand(
      params.cdpMethod as any,
      params.cdpParams
    );
    return {
      result: sendCdpCommandResult,
      cdpSession: params.cdpSession,
    };
  }

  process_cdp_getSession(params: CDP.GetSessionParams) {
    const context = params.context;
    const sessionId =
      this.#browsingContextStorage.getKnownContext(context).cdpSessionId;
    if (sessionId === undefined) {
      return {result: {cdpSession: null}};
    }
    return {result: {cdpSession: sessionId}};
  }
}
