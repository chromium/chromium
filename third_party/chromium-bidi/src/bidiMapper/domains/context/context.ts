export class Context {
  _targetInfo: TargetInfo;
  _contextId: string;
  _sessionId: string;

  constructor(contextId: string) {
    this._contextId = contextId;
  }

  _setSessionId(sessionId: string): void {
    this._sessionId = sessionId;
  }

  _updateTargetInfo(targetInfo: TargetInfo) {
    this._targetInfo = targetInfo;
  }

  _onInfoChangedEvent(targetInfo: TargetInfo) {
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
