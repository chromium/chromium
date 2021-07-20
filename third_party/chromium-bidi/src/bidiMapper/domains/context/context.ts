import { Protocol } from 'devtools-protocol';

export class Context {
  _targetInfo: Protocol.Target.TargetInfo;
  _contextId: string;
  _sessionId: string;

  constructor(contextId: string) {
    this._contextId = contextId;
  }

  _setSessionId(sessionId: string): void {
    this._sessionId = sessionId;
  }

  _updateTargetInfo(targetInfo: Protocol.Target.TargetInfo) {
    this._targetInfo = targetInfo;
  }

  _onInfoChangedEvent(targetInfo: Protocol.Target.TargetInfo) {
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
