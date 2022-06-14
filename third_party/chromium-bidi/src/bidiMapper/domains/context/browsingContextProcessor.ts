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
import { Context } from './context';
import { BrowsingContext, CDP, Script } from '../protocol/bidiProtocolTypes';
import Protocol from 'devtools-protocol';
import { IBidiServer } from '../../utils/bidiServer';
import { IEventManager } from '../events/EventManager';
import { NoSuchFrameException } from '../protocol/error';

const logContext = log('context');

export class BrowsingContextProcessor {
  private _contexts: Map<string, Context> = new Map();
  private _sessionToTargets: Map<string, Context> = new Map();

  // Set from outside.
  private _cdpConnection: CdpConnection;

  constructor(
    cdpConnection: CdpConnection,
    private _selfTargetId: string,
    private _bidiServer: IBidiServer,
    private _eventManager: IEventManager
  ) {
    this._cdpConnection = cdpConnection;

    this._setCdpEventListeners(this._cdpConnection.browserClient());
  }

  private _setCdpEventListeners(browserCdpClient: CdpClient) {
    browserCdpClient.Target.on('attachedToTarget', async (params) => {
      await this._handleAttachedToTargetEvent(params);
    });
    browserCdpClient.Target.on('targetInfoChanged', (params) => {
      this._handleInfoChangedEvent(params);
    });
    browserCdpClient.Target.on('detachedFromTarget', (params) => {
      this._handleDetachedFromTargetEvent(params);
    });
  }

  private _getOrCreateContext(
    contextId: string,
    cdpSessionId: string
  ): Context {
    if (this._contexts.has(contextId)) {
      return this._contexts.get(contextId)!;
    }

    const context = Context.create(
      contextId,
      this._cdpConnection.getCdpClient(cdpSessionId),
      this._bidiServer,
      this._eventManager
    );
    this._contexts.set(contextId, context);
    return context;
  }

  private _hasKnownContext(contextId: string): boolean {
    return this._contexts.has(contextId);
  }

  private _tryGetContext(contextId: string): Context | undefined {
    return this._contexts.get(contextId);
  }

  private _getKnownContext(contextId: string): Context {
    if (!this._hasKnownContext(contextId)) {
      throw new NoSuchFrameException(`Context ${contextId} not found`);
    }
    return this._contexts.get(contextId)!;
  }

  private async _handleAttachedToTargetEvent(
    params: Protocol.Target.AttachedToTargetEvent
  ) {
    logContext('AttachedToTarget event received: ' + JSON.stringify(params));

    const { sessionId, targetInfo } = params;
    if (!this._isValidTarget(targetInfo)) {
      return;
    }

    const context = this._getOrCreateContext(targetInfo.targetId, sessionId);
    context.updateTargetInfo(targetInfo);
    this._sessionToTargets.delete(sessionId);
    this._sessionToTargets.set(sessionId, context);
    context.setSessionId(sessionId);

    await this._eventManager.sendEvent(
      new BrowsingContext.ContextCreatedEvent({
        context: context.id,
        parent: targetInfo.openerId ? targetInfo.openerId : null,
        // New-opened context `url` is always `about:blank`:
        // https://github.com/w3c/webdriver-bidi/issues/220
        url: 'about:blank',
        // New-opened `children` are always empty:
        // https://github.com/w3c/webdriver-bidi/issues/220
        children: [],
      }),
      context.id
    );
  }

  private _handleInfoChangedEvent(
    params: Protocol.Target.TargetInfoChangedEvent
  ) {
    logContext('infoChangedEvent event received: ' + JSON.stringify(params));

    const targetInfo = params.targetInfo;
    if (!this._isValidTarget(targetInfo)) {
      return;
    }

    const context = this._tryGetContext(targetInfo.targetId);
    if (context) {
      context.onInfoChangedEvent(targetInfo);
    }
  }

  // { "method": "Target.detachedFromTarget",
  //   "params": {
  //     "sessionId": "7EFBFB2A4942A8989B3EADC561BC46E9",
  //     "targetId": "19416886405CBA4E03DBB59FA67FF4E8" } }
  private async _handleDetachedFromTargetEvent(
    params: Protocol.Target.DetachedFromTargetEvent
  ) {
    logContext('detachedFromTarget event received: ' + JSON.stringify(params));

    // TODO: params.targetId is deprecated. Update this class to track using
    // params.sessionId instead.
    // https://github.com/GoogleChromeLabs/chromium-bidi/issues/60
    const targetId = params.targetId!;
    const context = await this._tryGetContext(targetId);
    if (context) {
      if (context._sessionId) {
        this._sessionToTargets.delete(context._sessionId);
      }
      this._contexts.delete(context.id);
      await this._eventManager.sendEvent(
        new BrowsingContext.ContextDestroyedEvent(
          context.serializeToBidiValue()
        ),
        context.id
      );
    }
  }

  private static _targetToBiDiContext(
    targetInfo: Protocol.Target.TargetInfo
  ): BrowsingContext.Info {
    return {
      context: targetInfo.targetId,
      parent: targetInfo.openerId ? targetInfo.openerId : null,
      url: targetInfo.url,
      // TODO sadym: implement.
      children: null,
    };
  }

  async process_browsingContext_getTree(
    params: BrowsingContext.GetTreeParameters
  ): Promise<BrowsingContext.GetTreeResult> {
    // TODO sadym: consider replacing with known targets.
    const { targetInfos } = await this._cdpConnection
      .browserClient()
      .Target.getTargets();
    // TODO sadym: implement.
    if (params.maxDepth) {
      throw new Error('not implemented yet');
    }
    const contexts = targetInfos
      // Don't expose any information about the tab with Mapper running.
      .filter((target) => this._isValidTarget(target))
      // Filter by `root`, if specified.
      .filter(
        (target) => params.root === undefined || params.root === target.targetId
      )
      .map(BrowsingContextProcessor._targetToBiDiContext);
    return { result: { contexts } };
  }

  async process_browsingContext_create(
    params: BrowsingContext.CreateParameters
  ): Promise<BrowsingContext.CreateResult> {
    const browserCdpClient = this._cdpConnection.browserClient();

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
    const context = await this._getKnownContext(params.context);

    return await context.navigate(
      params.url,
      params.wait !== undefined ? params.wait : 'none'
    );
  }

  async process_script_evaluate(
    params: Script.EvaluateParameters
  ): Promise<Script.EvaluateResult> {
    const context = await this._getKnownContext(
      (params.target as Script.ContextTarget).context
    );
    return await context.scriptEvaluate(
      params.expression,
      params.awaitPromise !== false // `awaitPromise` by default is `true`.
    );
  }

  async process_script_callFunction(
    params: Script.CallFunctionParameters
  ): Promise<Script.CallFunctionResult> {
    const context = await this._getKnownContext(
      (params.target as Script.ContextTarget).context
    );
    return await context.callFunction(
      params.functionDeclaration,
      params.this || {
        type: 'undefined',
      }, // `this` is `undefined` by default.
      params.arguments || [], // `arguments` is `[]` by default.
      params.awaitPromise !== false // `awaitPromise` is `true` by default.
    );
  }

  async process_PROTO_browsingContext_findElement(
    params: BrowsingContext.PROTO.FindElementParameters
  ): Promise<BrowsingContext.PROTO.FindElementResult> {
    const context = await this._getKnownContext(params.context);
    return await context.findElement(params.selector);
  }

  async process_browsingContext_close(
    commandParams: BrowsingContext.CloseParameters
  ): Promise<BrowsingContext.CloseResult> {
    const browserCdpClient = this._cdpConnection.browserClient();

    if (!this._hasKnownContext(commandParams.context)) {
      throw new NoSuchFrameException(
        `Context ${commandParams.context} not found`
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

    await this._cdpConnection.browserClient().Target.closeTarget({
      targetId: commandParams.context,
    });

    // Sometimes CDP command finishes before `detachedFromTarget` event,
    // sometimes after. Wait for the CDP command to be finished, and then wait
    // for `detachedFromTarget` if it hasn't emitted.
    await detachedFromTargetPromise;

    return { result: {} };
  }

  private _isValidTarget(target: Protocol.Target.TargetInfo) {
    if (target.targetId === this._selfTargetId) {
      return false;
    }
    if (!target.type || target.type !== 'page') {
      return false;
    }
    return true;
  }

  async process_PROTO_cdp_sendCommand(params: CDP.PROTO.SendCommandParams) {
    const sendCdpCommandResult = await this._cdpConnection.sendCommand(
      params.cdpMethod,
      params.cdpParams,
      params.cdpSession ?? null
    );
    return { result: sendCdpCommandResult };
  }

  async process_PROTO_cdp_getSession(params: CDP.PROTO.GetSessionParams) {
    const context = params.context;
    const sessionId = (await this._getKnownContext(context))._sessionId;
    if (sessionId === undefined) {
      return { result: { session: null } };
    }
    return { result: { session: sessionId } };
  }
}
