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
import { CdpConnection } from '../../../cdp';
import { Context } from './context';
import { Script } from '../../bidiProtocolTypes';
import Protocol from 'devtools-protocol';

const logContext = log('context');

export class BrowsingContextProcessor {
  private _contexts: Map<string, Context> = new Map();
  private _sessionToTargets: Map<string, Context> = new Map();

  // Set from outside.
  private _cdpConnection: CdpConnection;
  private _selfTargetId: string;

  private _onContextCreated: (t: Context) => Promise<void>;
  private _onContextDestroyed: (t: Context) => Promise<void>;

  constructor(
    cdpConnection: CdpConnection,
    selfTargetId: string,
    onContextCreated: (t: Context) => Promise<void>,
    onContextDestroyed: (t: Context) => Promise<void>
  ) {
    this._cdpConnection = cdpConnection;
    this._selfTargetId = selfTargetId;
    this._onContextCreated = onContextCreated;
    this._onContextDestroyed = onContextDestroyed;
  }

  private _getOrCreateContext(
    contextId: string,
    cdpSessionId: string
  ): Context {
    let context = this._contexts.get(contextId);
    if (!context) {
      const sessionCdpClient = this._cdpConnection.sessionClient(cdpSessionId);
      context = Context.create(contextId, sessionCdpClient);
      this._contexts.set(contextId, context);
    }
    return context;
  }

  private _tryGetContext(contextId: string): Context | undefined {
    return this._contexts.get(contextId);
  }

  private _getKnownContext(contextId: string): Context {
    const context = this._contexts.get(contextId);
    if (!context) throw new Error('context not found');
    return context;
  }

  handleAttachedToTargetEvent(params: Protocol.Target.AttachedToTargetEvent) {
    logContext('AttachedToTarget event received', params);

    const { sessionId, targetInfo } = params;
    if (!this._isValidTarget(targetInfo)) return;

    const context = this._getOrCreateContext(targetInfo.targetId, sessionId);
    context._updateTargetInfo(targetInfo);

    this._sessionToTargets.delete(sessionId);

    this._sessionToTargets.set(sessionId, context);

    // context._setSessionId(eventData.params.sessionId);

    this._onContextCreated(context);
  }

  handleInfoChangedEvent(params: Protocol.Target.TargetInfoChangedEvent) {
    logContext('infoChangedEvent event received', params);

    const targetInfo = params.targetInfo;
    if (!this._isValidTarget(targetInfo)) return;

    const context = this._tryGetContext(targetInfo.targetId);
    if (context) {
      context._onInfoChangedEvent(targetInfo);
    }
  }

  // { "method": "Target.detachedFromTarget", "params": { "sessionId": "7EFBFB2A4942A8989B3EADC561BC46E9", "targetId": "19416886405CBA4E03DBB59FA67FF4E8" } }
  handleDetachedFromTargetEvent(
    params: Protocol.Target.DetachedFromTargetEvent
  ) {
    logContext('detachedFromTarget event received', params);

    // TODO: params.targetId is deprecated. Update this class to track using params.sessionId instead.
    const targetId = params.targetId!;
    const context = this._tryGetContext(targetId);
    if (context) {
      this._onContextDestroyed(context);

      if (context._sessionId) this._sessionToTargets.delete(context._sessionId);

      this._contexts.delete(context.id);
    }
  }

  async process_createContext(params: any): Promise<any> {
    return new Promise(async (resolve) => {
      let targetId: string;

      const onAttachedToTarget = async (
        params: Protocol.Target.AttachedToTargetEvent
      ) => {
        if (params.targetInfo.targetId === targetId) {
          browserCdpClient.Target.removeListener(
            'attachedToTarget',
            onAttachedToTarget
          );

          const context = await this._getOrCreateContext(
            targetId,
            params.sessionId
          );
          resolve(context.toBidi());
        }
      };

      const browserCdpClient = this._cdpConnection.browserClient();
      browserCdpClient.Target.on('attachedToTarget', onAttachedToTarget);

      const result = await browserCdpClient.Target.createTarget({
        url: params.url,
      });
      targetId = result.targetId;
    });
  }

  async process_script_evaluate(params: Script.ScriptEvaluateParameters): Promise<Script.ScriptEvaluateResult> {
    const context = this._getKnownContext(
      (params.target as Script.ContextTarget).context
    );
    // TODO sadym: add arguments params after they are specified.
    // https://github.com/w3c/webdriver-bidi/pull/136#issuecomment-926700556
    return await context.evaluateScript(params.expression);
  }

  _isValidTarget(target: Protocol.Target.TargetInfo) {
    if (target.targetId === this._selfTargetId) return false;
    if (!target.type || target.type !== 'page') return false;
    return true;
  }
}
