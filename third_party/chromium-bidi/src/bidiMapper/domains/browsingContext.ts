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
import { log } from '../utils/log';
import { CdpServer } from '../utils/cdpServer';
const logContext = log('context');

export default class Context {
  private static _contexts: Map<string, Context> = new Map();
  private static _sessionToTargets: Map<string, Context> = new Map();

  // Set from outside.
  static cdpServer: CdpServer;
  static getCurrentContextId: () => string;

  static onContextCreated: (t: Context) => Promise<void>;
  static onContextDestroyed: (t: Context) => Promise<void>;

  private static _getOrCreateContext(contextId: string): Context {
    if (!Context._isKnownContext(contextId))
      Context._contexts.set(contextId, new Context(contextId));
    return Context._getContext(contextId);
  }

  private static _getContext(contextId: string): Context {
    if (!Context._isKnownContext(contextId))
      throw new Error('context not found');
    return Context._contexts.get(contextId);
  }

  private static _isKnownContext(contextId: string): boolean {
    return Context._contexts.has(contextId);
  }

  static handleAttachedToTargetEvent(eventData: any) {
    logContext('AttachedToTarget event recevied', eventData);

    const targetInfo: TargetInfo = eventData.params.targetInfo;
    if (!Context._isValidTarget(targetInfo)) return;

    const context = Context._getOrCreateContext(targetInfo.targetId);
    context._updateTargetInfo(targetInfo);
    context._setSessionId(eventData.params.sessionId);

    Context.onContextCreated(context);
  }

  static handleInfoChangedEvent(eventData: any) {
    logContext('infoChangedEvent event recevied', eventData);

    const targetInfo: TargetInfo = eventData.params.targetInfo;
    if (!Context._isValidTarget(targetInfo)) return;

    const context = Context._getOrCreateContext(targetInfo.targetId);
    context._onInfoChangedEvent(targetInfo);
  }

  // { "method": "Target.detachedFromTarget", "params": { "sessionId": "7EFBFB2A4942A8989B3EADC561BC46E9", "targetId": "19416886405CBA4E03DBB59FA67FF4E8" } }
  static async handleDetachedFromTargetEvent(eventData: any) {
    logContext('detachedFromTarget event recevied', eventData);

    const targetId = eventData.params.targetId;
    if (!Context._isKnownContext(targetId)) return;

    const context = Context._getOrCreateContext(targetId);
    Context.onContextDestroyed(context);

    if (context._sessionId)
      Context._sessionToTargets.delete(context._sessionId);

    delete Context._contexts[context._contextId];
  }

  static async process_createContext(params: any): Promise<any> {
    const { targetId } = await Context.cdpServer.sendMessage({
      method: 'Target.createTarget',
      params: { url: params.url },
    });
    return Context._getOrCreateContext(targetId).toBidi();
  }

  static _isValidTarget = (target: TargetInfo) => {
    if (target.targetId === Context.getCurrentContextId()) return false;
    if (!target.type || target.type !== 'page') return false;
    return true;
  };

  private _targetInfo: TargetInfo;
  private _contextId: string;
  private _sessionId: string;

  constructor(contextId: string) {
    Context._contexts[contextId] = this;
  }

  private _setSessionId(sessionId: string): void {
    if (this._sessionId) Context._sessionToTargets.delete(sessionId);

    this._sessionId = sessionId;
    Context._sessionToTargets.set(sessionId, this);
  }

  private _updateTargetInfo(targetInfo: TargetInfo) {
    this._targetInfo = targetInfo;
  }

  private _onInfoChangedEvent(targetInfo: TargetInfo) {
    this._updateTargetInfo(targetInfo);
  }

  getId(): string {
    return this._contextId;
  }

  toBidi(): any {
    return {
      context: this._targetInfo.targetId,
      parent: this._targetInfo.openerId ? this._targetInfo.openerId : null,
      url: this._targetInfo.url,
    };
  }
}

class TargetInfo {
  targetId: string;
  openerId: string | null;
  url: string | null;
  type: string;
}
