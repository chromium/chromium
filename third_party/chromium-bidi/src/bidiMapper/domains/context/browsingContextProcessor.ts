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
import Protocol from 'devtools-protocol';

import {
  BrowsingContext,
  CDP,
  Input,
  Message,
  Script,
} from '../../../protocol/protocol.js';
import {LogType, LoggerFn} from '../../../utils/log.js';
import {CdpClient, CdpConnection} from '../../CdpConnection.js';
import {IEventManager} from '../events/EventManager.js';
import {Realm} from '../script/realm.js';
import {RealmStorage} from '../script/realmStorage.js';

import {
  BidiPreloadScript,
  CdpPreloadScript,
  PreloadScriptStorage,
} from './PreloadScriptStorage.js';
import {BrowsingContextImpl} from './browsingContextImpl.js';
import {BrowsingContextStorage} from './browsingContextStorage.js';
import {CdpTarget} from './cdpTarget.js';

export class BrowsingContextProcessor {
  readonly #browsingContextStorage: BrowsingContextStorage;
  readonly #cdpConnection: CdpConnection;
  readonly #eventManager: IEventManager;
  readonly #logger?: LoggerFn;
  readonly #realmStorage: RealmStorage;
  readonly #selfTargetId: string;
  readonly #preloadScriptStorage: PreloadScriptStorage;

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
    this.#preloadScriptStorage = new PreloadScriptStorage();

    this.#setEventListeners(this.#cdpConnection.browserClient());
  }

  /**
   * This method is called for each CDP session, since this class is responsible
   * for creating and destroying all targets and browsing contexts.
   */
  #setEventListeners(cdpClient: CdpClient) {
    cdpClient.on('Target.attachedToTarget', (params) => {
      this.#handleAttachedToTargetEvent(params, cdpClient);
    });
    cdpClient.on('Target.detachedFromTarget', (params) => {
      this.#handleDetachedFromTargetEvent(params);
    });

    cdpClient.on(
      'Page.frameAttached',
      (params: Protocol.Page.FrameAttachedEvent) => {
        this.#handleFrameAttachedEvent(params);
      }
    );
    cdpClient.on(
      'Page.frameDetached',
      (params: Protocol.Page.FrameDetachedEvent) => {
        this.#handleFrameDetachedEvent(params);
      }
    );
  }

  #handleFrameAttachedEvent(params: Protocol.Page.FrameAttachedEvent) {
    const parentBrowsingContext = this.#browsingContextStorage.findContext(
      params.parentFrameId
    );
    if (parentBrowsingContext !== undefined) {
      BrowsingContextImpl.create(
        parentBrowsingContext.cdpTarget,
        this.#realmStorage,
        params.frameId,
        params.parentFrameId,
        this.#eventManager,
        this.#browsingContextStorage,
        this.#logger
      );
    }
  }

  #handleFrameDetachedEvent(params: Protocol.Page.FrameDetachedEvent) {
    // In case of OOPiF no need in deleting BrowsingContext.
    if (params.reason === 'swap') {
      return;
    }
    this.#browsingContextStorage.findContext(params.frameId)?.delete();
  }

  #handleAttachedToTargetEvent(
    params: Protocol.Target.AttachedToTargetEvent,
    parentSessionCdpClient: CdpClient
  ) {
    const {sessionId, targetInfo} = params;

    const targetCdpClient = this.#cdpConnection.getCdpClient(sessionId);

    if (!this.#isValidTarget(targetInfo)) {
      // DevTools or some other not supported by BiDi target. Just release
      // debugger  and ignore them.
      targetCdpClient
        .sendCommand('Runtime.runIfWaitingForDebugger')
        .then(() =>
          parentSessionCdpClient.sendCommand('Target.detachFromTarget', params)
        )
        .catch((error) => this.#logger?.(LogType.system, error));
      return;
    }

    this.#logger?.(
      LogType.browsingContexts,
      'AttachedToTarget event received:',
      JSON.stringify(params, null, 2)
    );

    this.#setEventListeners(targetCdpClient);

    const maybeContext = this.#browsingContextStorage.findContext(
      targetInfo.targetId
    );

    const cdpTarget = CdpTarget.create(
      targetInfo.targetId,
      maybeContext?.parentId ?? null,
      targetCdpClient,
      sessionId,
      this.#realmStorage,
      this.#eventManager,
      this.#preloadScriptStorage,
      this.#browsingContextStorage,
      this.#logger
    );

    if (maybeContext) {
      // OOPiF.
      maybeContext.updateCdpTarget(cdpTarget);
    } else {
      // New context.
      BrowsingContextImpl.create(
        cdpTarget,
        this.#realmStorage,
        targetInfo.targetId,
        null,
        this.#eventManager,
        this.#browsingContextStorage,
        this.#logger
      );
    }
  }

  #handleDetachedFromTargetEvent(
    params: Protocol.Target.DetachedFromTargetEvent
  ) {
    // XXX: params.targetId is deprecated. Update this class to track using
    // params.sessionId instead.
    // https://github.com/GoogleChromeLabs/chromium-bidi/issues/60
    const contextId = params.targetId!;
    this.#browsingContextStorage.findContext(contextId)?.delete();

    this.#preloadScriptStorage.removeCdpPreloadScripts({targetId: contextId});
  }

  async #getRealm(target: Script.Target): Promise<Realm> {
    if ('realm' in target) {
      return this.#realmStorage.getRealm({
        realmId: target.realm,
      });
    }
    const context = this.#browsingContextStorage.getContext(target.context);
    return context.getOrCreateSandbox(target.sandbox);
  }

  process_browsingContext_getTree(
    params: BrowsingContext.GetTreeParameters
  ): BrowsingContext.GetTreeResult {
    const resultContexts =
      params.root === undefined
        ? this.#browsingContextStorage.getTopLevelContexts()
        : [this.#browsingContextStorage.getContext(params.root)];

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

    let referenceContext: BrowsingContextImpl | undefined;
    if (params.referenceContext !== undefined) {
      referenceContext = this.#browsingContextStorage.getContext(
        params.referenceContext
      );
      if (!referenceContext.isTopLevelContext()) {
        throw new Message.InvalidArgumentException(
          `referenceContext should be a top-level context`
        );
      }
    }

    let result: Protocol.Target.CreateTargetResponse;

    switch (params.type) {
      case 'tab':
        result = await browserCdpClient.sendCommand('Target.createTarget', {
          url: 'about:blank',
          newWindow: false,
        });
        break;
      case 'window':
        result = await browserCdpClient.sendCommand('Target.createTarget', {
          url: 'about:blank',
          newWindow: true,
        });
        break;
    }

    // Wait for the new tab to be loaded to avoid race conditions in the
    // `browsingContext` events, when the `browsingContext.domContentLoaded` and
    // `browsingContext.load` events from the initial `about:blank` navigation
    // are emitted after the next navigation is started.
    // Details: https://github.com/web-platform-tests/wpt/issues/35846
    const contextId = result.targetId;
    const context = this.#browsingContextStorage.getContext(contextId);
    await context.awaitLoaded();

    return {
      result: {
        context: context.id,
      },
    };
  }

  process_browsingContext_navigate(
    params: BrowsingContext.NavigateParameters
  ): Promise<BrowsingContext.NavigateResult> {
    const context = this.#browsingContextStorage.getContext(params.context);

    return context.navigate(params.url, params.wait ?? 'none');
  }

  process_browsingContext_reload(
    params: BrowsingContext.ReloadParameters
  ): Promise<Message.EmptyResult> {
    const context = this.#browsingContextStorage.getContext(params.context);

    return context.reload(params.ignoreCache ?? false, params.wait ?? 'none');
  }

  async process_browsingContext_captureScreenshot(
    params: BrowsingContext.CaptureScreenshotParameters
  ): Promise<BrowsingContext.CaptureScreenshotResult> {
    const context = this.#browsingContextStorage.getContext(params.context);
    return context.captureScreenshot();
  }

  async process_browsingContext_print(
    params: BrowsingContext.PrintParameters
  ): Promise<BrowsingContext.PrintResult> {
    const context = this.#browsingContextStorage.getContext(params.context);
    return context.print(params);
  }

  async process_script_addPreloadScript(
    params: Script.AddPreloadScriptParameters
  ): Promise<Script.AddPreloadScriptResult> {
    if (params.arguments !== undefined && params.arguments.length > 0) {
      // TODO: Handle arguments.
      throw new Error('add preload script arguments are not supported');
    }

    const cdpTargets = new Set<CdpTarget>(
      // TODO: The unique target can be in a non-top-level browsing context.
      // We need all the targets.
      // To get them, we can walk through all the contexts and collect their targets into the set.
      params.context === undefined || params.context === null
        ? this.#browsingContextStorage
            .getTopLevelContexts()
            .map((context) => context.cdpTarget)
        : [this.#browsingContextStorage.getContext(params.context).cdpTarget]
    );

    const cdpPreloadScripts: CdpPreloadScript[] = [];

    for (const cdpTarget of cdpTargets) {
      const cdpPreloadScriptId = await cdpTarget.addPreloadScript(
        // The spec provides a function, and CDP expects an evaluation.
        `(${params.functionDeclaration})();`,
        params.sandbox
      );
      cdpPreloadScripts.push({
        target: cdpTarget,
        preloadScriptId: cdpPreloadScriptId,
      });
    }

    const preloadScript: BidiPreloadScript =
      this.#preloadScriptStorage.addPreloadScripts(
        params.context ?? null,
        cdpPreloadScripts,
        params.functionDeclaration,
        params.sandbox
      );

    return {
      result: {
        script: preloadScript.id,
      },
    };
  }

  async process_script_removePreloadScript(
    params: Script.RemovePreloadScriptParameters
  ): Promise<Message.EmptyResult> {
    const bidiId = params.script;

    const scripts = this.#preloadScriptStorage.findPreloadScripts({
      id: bidiId,
    });

    if (scripts.length === 0) {
      throw new Message.NoSuchScriptException(
        `No preload script with BiDi ID '${bidiId}'`
      );
    }

    for (const script of scripts) {
      for (const cdpPreloadScript of script.cdpPreloadScripts) {
        const cdpTarget = cdpPreloadScript.target;
        const cdpPreloadScriptId = cdpPreloadScript.preloadScriptId;
        await cdpTarget.removePreloadScript(cdpPreloadScriptId);
      }
    }

    this.#preloadScriptStorage.removeBiDiPreloadScripts({
      id: bidiId,
    });

    return {result: {}};
  }

  async process_script_evaluate(
    params: Script.EvaluateParameters
  ): Promise<Script.EvaluateResult> {
    const realm = await this.#getRealm(params.target);
    return realm.scriptEvaluate(
      params.expression,
      params.awaitPromise,
      params.resultOwnership ?? 'none',
      params.serializationOptions ?? {}
    );
  }

  process_script_getRealms(
    params: Script.GetRealmsParameters
  ): Script.GetRealmsResult {
    if (params.context !== undefined) {
      // Make sure the context is known.
      this.#browsingContextStorage.getContext(params.context);
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
      params.resultOwnership ?? 'none',
      params.serializationOptions ?? {}
    );
  }

  async process_script_disown(
    params: Script.DisownParameters
  ): Promise<Script.DisownResult> {
    const realm = await this.#getRealm(params.target);
    await Promise.all(params.handles.map(async (h) => realm.disown(h)));
    return {result: {}};
  }

  process_input_performActions(
    _params: Input.PerformActionsParameters
  ): Promise<Message.EmptyResult> {
    throw new Message.UnsupportedOperationException('Not implemented yet.');
  }
  process_input_releaseActions(
    _params: Input.ReleaseActionsParameters
  ): Promise<Message.EmptyResult> {
    throw new Message.UnsupportedOperationException('Not implemented yet.');
  }

  async process_browsingContext_close(
    commandParams: BrowsingContext.CloseParameters
  ): Promise<Message.EmptyResult> {
    const browserCdpClient = this.#cdpConnection.browserClient();

    const context = this.#browsingContextStorage.getContext(
      commandParams.context
    );
    if (!context.isTopLevelContext()) {
      throw new Message.InvalidArgumentException(
        'A top-level browsing context cannot be closed.'
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

    await browserCdpClient.sendCommand('Target.closeTarget', {
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
      params.cdpMethod,
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
      this.#browsingContextStorage.getContext(context).cdpTarget.cdpSessionId;
    if (sessionId === undefined) {
      return {result: {cdpSession: null}};
    }
    return {result: {cdpSession: sessionId}};
  }
}
