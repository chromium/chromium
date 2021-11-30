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
import { CdpConnection, CdpClient } from '../../../cdp';
import { Context } from './context';
import { BrowsingContext, Script } from '../../bidiProtocolTypes';
import Protocol from 'devtools-protocol';
import { IBidiServer } from '../../utils/bidiServer';

const logContext = log('context');

export class BrowsingContextProcessor {
  private _contexts: Map<string, Promise<Context>> = new Map();
  private _sessionToTargets: Map<string, Context> = new Map();

  // Set from outside.
  private _cdpConnection: CdpConnection;
  private _selfTargetId: string;

  constructor(
    cdpConnection: CdpConnection,
    selfTargetId: string,
    private _bidiServer: IBidiServer,
    private EVALUATOR_SCRIPT: string
  ) {
    this._cdpConnection = cdpConnection;
    this._selfTargetId = selfTargetId;

    this._setCdpEventListeners(this._cdpConnection.browserClient());
  }

  private async _onContextCreated(context: Context): Promise<void> {
    await this._bidiServer.sendMessage({
      method: 'browsingContext.contextCreated',
      params: context.toBidi(),
    });
  }

  private async _onContextDestroyed(context: Context): Promise<void> {
    await this._bidiServer.sendMessage({
      method: 'browsingContext.contextDestroyed',
      params: context.toBidi(),
    });
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

  // Creation of `Context` can take quite a while. To avoid race condition, keep
  // a Promise in the map, eventually resolved with the `Context`.
  private async _getOrCreateContext(
    contextId: string,
    cdpSessionId: string
  ): Promise<Context> {
    let contextPromise = this._contexts.get(contextId);
    if (!contextPromise) {
      const sessionCdpClient = this._cdpConnection.getCdpClient(cdpSessionId);

      // Don't wait for actual creation. Just put the Promise into map.
      contextPromise = Context.create(
        contextId,
        sessionCdpClient,
        this._bidiServer,
        this.EVALUATOR_SCRIPT
      );
      this._contexts.set(contextId, contextPromise);
    }
    return contextPromise;
  }

  private _hasKnownContext(contextId: string): boolean {
    return this._contexts.has(contextId);
  }

  private async _tryGetContext(
    contextId: string
  ): Promise<Context | undefined> {
    return await this._contexts.get(contextId);
  }

  private async _getKnownContext(contextId: string): Promise<Context> {
    if (!this._hasKnownContext(contextId)) throw new Error('context not found');
    return await this._contexts.get(contextId)!;
  }

  private async _handleAttachedToTargetEvent(
    params: Protocol.Target.AttachedToTargetEvent
  ) {
    logContext('AttachedToTarget event received', params);

    const { sessionId, targetInfo } = params;
    if (!this._isValidTarget(targetInfo)) return;

    const context = await this._getOrCreateContext(
      targetInfo.targetId,
      sessionId
    );
    context._updateTargetInfo(targetInfo);
    this._sessionToTargets.delete(sessionId);
    this._sessionToTargets.set(sessionId, context);
    context._setSessionId(sessionId);

    this._onContextCreated(context);
  }

  private async _handleInfoChangedEvent(
    params: Protocol.Target.TargetInfoChangedEvent
  ) {
    logContext('infoChangedEvent event received', params);

    const targetInfo = params.targetInfo;
    if (!this._isValidTarget(targetInfo)) return;

    const context = await this._tryGetContext(targetInfo.targetId);
    if (context) {
      context._onInfoChangedEvent(targetInfo);
    }
  }

  // { "method": "Target.detachedFromTarget",
  //   "params": {
  //     "sessionId": "7EFBFB2A4942A8989B3EADC561BC46E9",
  //     "targetId": "19416886405CBA4E03DBB59FA67FF4E8" } }
  private async _handleDetachedFromTargetEvent(
    params: Protocol.Target.DetachedFromTargetEvent
  ) {
    logContext('detachedFromTarget event received', params);

    // TODO: params.targetId is deprecated. Update this class to track using
    // params.sessionId instead.
    // https://github.com/GoogleChromeLabs/chromium-bidi/issues/60
    const targetId = params.targetId!;
    const context = await this._tryGetContext(targetId);
    if (context) {
      this._onContextDestroyed(context);

      if (context._sessionId) this._sessionToTargets.delete(context._sessionId);

      this._contexts.delete(context.id);
    }
  }

  async process_PROTO_browsingContext_create(
    commandData: BrowsingContext.PROTO.BrowsingContextCreateCommand
  ): Promise<BrowsingContext.PROTO.BrowsingContextCreateResult> {
    const params = commandData.params;

    return new Promise(async (resolve) => {
      const browserCdpClient = this._cdpConnection.browserClient();

      const result = await browserCdpClient.Target.createTarget({
        url: 'about:blank',
        newWindow: params.type === 'window',
      });

      const contextId = result.targetId;

      if (this._hasKnownContext(contextId)) {
        const existingContext = await this._getKnownContext(contextId);
        resolve(existingContext.toBidi());
        return;
      }

      const onAttachedToTarget = async (
        attachToTargetEventParams: Protocol.Target.AttachedToTargetEvent
      ) => {
        if (attachToTargetEventParams.targetInfo.targetId === contextId) {
          browserCdpClient.Target.removeListener(
            'attachedToTarget',
            onAttachedToTarget
          );

          const context = await this._getOrCreateContext(
            contextId,
            attachToTargetEventParams.sessionId
          );
          resolve(context.toBidi());
        }
      };

      browserCdpClient.Target.on('attachedToTarget', onAttachedToTarget);
    });
  }

  async process_browsingContext_navigate(
    commandData: BrowsingContext.BrowsingContextNavigateCommand
  ): Promise<BrowsingContext.BrowsingContextNavigateResult> {
    const params = commandData.params;
    const context = await this._getKnownContext(params.context);

    return await context.navigate(
      params.url,
      params.wait !== undefined ? params.wait : 'none'
    );
  }

  async process_script_evaluate(
    commandData: Script.ScriptEvaluateCommand
  ): Promise<Script.ScriptEvaluateResult> {
    const params = commandData.params;
    const context = await this._getKnownContext(
      (params.target as Script.ContextTarget).context
    );
    return await context.scriptEvaluate(
      params.expression,
      params.awaitPromise !== false // `awaitPromise` by default is `true`.
    );
  }

  async process_PROTO_script_callFunction(
    commandData: Script.PROTO.ScriptCallFunctionCommand
  ): Promise<Script.PROTO.ScriptCallFunctionResult> {
    const params = commandData.params;
    const context = await this._getKnownContext(
      (params.target as Script.ContextTarget).context
    );
    return await context.callFunction(
      params.functionDeclaration,
      params.this || {
        type: 'undefined',
      }, // `_this` is `undefined` by default.
      params.args || [], // `args` is `[]` by default.
      params.awaitPromise !== false // `awaitPromise` by default is `true`.
    );
  }

  _isValidTarget(target: Protocol.Target.TargetInfo) {
    if (target.targetId === this._selfTargetId) return false;
    if (!target.type || target.type !== 'page') return false;
    return true;
  }
}
