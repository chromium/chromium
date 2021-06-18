/**
 * Copyright 2021 Google Inc. All rights reserved.
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
import { log } from '../../utils/log';
import { IServer } from '../../utils/iServer';
import { Context } from './context';
const logContext = log('context');

export class BrowsingContextProcessor {
  private _contexts: Map<string, Context> = new Map();
  private _sessionToTargets: Map<string, Context> = new Map();

  // Set from outside.
  private _cdpServer: IServer;
  private _selfTargetId: string;

  private _onContextCreated: (t: Context) => Promise<void>;
  private _onContextDestroyed: (t: Context) => Promise<void>;

  constructor(
    cdpServer: IServer,
    selfTargetId: string,
    onContextCreated: (t: Context) => Promise<void>,
    onContextDestroyed: (t: Context) => Promise<void>
  ) {
    this._cdpServer = cdpServer;
    this._selfTargetId = selfTargetId;
    this._onContextCreated = onContextCreated;
    this._onContextDestroyed = onContextDestroyed;
  }

  private _getOrCreateContext(contextId: string): Context {
    if (!this._isKnownContext(contextId)) {
      this._contexts.set(contextId, new Context(contextId));
    }
    return this._getContext(contextId);
  }

  private _getContext(contextId: string): Context {
    if (!this._isKnownContext(contextId)) throw new Error('context not found');
    return this._contexts.get(contextId);
  }

  private _isKnownContext(contextId: string): boolean {
    return this._contexts.has(contextId);
  }

  handleAttachedToTargetEvent(eventData: any) {
    logContext('AttachedToTarget event recevied', eventData);

    const targetInfo: TargetInfo = eventData.params.targetInfo;
    if (!this._isValidTarget(targetInfo)) return;

    const context = this._getOrCreateContext(targetInfo.targetId);
    context._updateTargetInfo(targetInfo);

    const sessionId = eventData.params.sessionId;
    if (sessionId) this._sessionToTargets.delete(sessionId);

    this._sessionToTargets.set(sessionId, context);

    // context._setSessionId(eventData.params.sessionId);

    this._onContextCreated(context);
  }

  handleInfoChangedEvent(eventData: any) {
    logContext('infoChangedEvent event recevied', eventData);

    const targetInfo: TargetInfo = eventData.params.targetInfo;
    if (!this._isValidTarget(targetInfo)) return;

    const context = this._getOrCreateContext(targetInfo.targetId);
    context._onInfoChangedEvent(targetInfo);
  }

  // { "method": "Target.detachedFromTarget", "params": { "sessionId": "7EFBFB2A4942A8989B3EADC561BC46E9", "targetId": "19416886405CBA4E03DBB59FA67FF4E8" } }
  async handleDetachedFromTargetEvent(eventData: any) {
    logContext('detachedFromTarget event recevied', eventData);

    const targetId = eventData.params.targetId;
    if (!this._isKnownContext(targetId)) return;

    const context = this._getOrCreateContext(targetId);
    this._onContextDestroyed(context);

    if (context._sessionId) this._sessionToTargets.delete(context._sessionId);

    delete this._contexts[context._contextId];
  }

  async process_createContext(params: any): Promise<any> {
    const { targetId } = await this._cdpServer.sendMessage({
      method: 'Target.createTarget',
      params: { url: params.url },
    });
    return this._getOrCreateContext(targetId).toBidi();
  }

  _isValidTarget = (target: TargetInfo) => {
    if (target.targetId === this._selfTargetId) return false;
    if (!target.type || target.type !== 'page') return false;
    return true;
  };
}
