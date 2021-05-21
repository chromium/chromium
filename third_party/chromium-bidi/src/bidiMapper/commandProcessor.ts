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
import { IServer } from './utils/iServer';
import { Context, BrowsingContextProcessor } from './domains/browsingContext';

export class CommandProcessor {
  private _cdpServer: IServer;
  private _bidiServer: IServer;
  private _selfTargetId: string;
  private _contextProcessor: BrowsingContextProcessor;

  static run(cdpServer: IServer, bidiServer: IServer, selfTargetId: string) {
    const commandProcessor = new CommandProcessor(
      cdpServer,
      bidiServer,
      selfTargetId
    );

    commandProcessor.run();
  }

  private constructor(
    cdpServer: IServer,
    bidiServer: IServer,
    selfTargetId: string
  ) {
    this._bidiServer = bidiServer;
    this._cdpServer = cdpServer;
    this._selfTargetId = selfTargetId;
  }

  private run() {
    this._contextProcessor = new BrowsingContextProcessor(
      this._cdpServer,
      this._selfTargetId,
      (t: Context) => {
        return this._onContextCreated(t);
      },
      (t: Context) => {
        return this._onContextDestroyed(t);
      }
    );

    this._cdpServer.setOnMessage((messageObj) => {
      return this._onCdpMessage(messageObj);
    });
    this._bidiServer.setOnMessage((messageObj) => {
      return this._onBidiMessage(messageObj);
    });
  }

  private _isValidTarget = (target) => {
    if (target.targetId === this._selfTargetId) return false;
    if (!target.type || target.type !== 'page') return false;
    return true;
  };

  private _getErrorResponse(commandData, errorCode, errorMessage) {
    // TODO: this is bizarre per spec. We reparse the payload and
    // extract the ID, regardless of what kind of value it was.
    let commandId = undefined;
    try {
      commandId = commandData.id;
    } catch {}

    return {
      id: commandId,
      error: errorCode,
      message: errorMessage,
      // TODO: optional stacktrace field.
    };
  }

  private _respondWithError(commandData, errorCode, errorMessage) {
    const errorResponse = this._getErrorResponse(
      commandData,
      errorCode,
      errorMessage
    );
    this._bidiServer.sendMessage(errorResponse);
  }

  private _targetToContext = (t) => ({
    context: t.targetId,
    parent: t.openerId ? t.openerId : null,
    url: t.url,
  });

  private _process_browsingContext_getTree = async function (params) {
    const cdpTargets = await this._cdpServer.sendMessage({
      method: 'Target.getTargets',
    });
    const contexts = cdpTargets.targetInfos
      // Don't expose any information about the tab with Mapper running.
      .filter(this._isValidTarget)
      .map(this._targetToContext);
    return { contexts };
  };

  private async _process_DEBUG_Page_close(params) {
    await this._cdpServer.sendMessage({
      method: 'Target.closeTarget',
      params: { targetId: params.context },
    });
    return {};
  }

  private _process_session_status = async function (params) {
    return { ready: true, message: 'ready' };
  };

  private async _onContextCreated(context) {
    await this._bidiServer.sendMessage({
      method: 'browsingContext.contextCreated',
      params: context.toBidi(),
    });
  }
  private async _onContextDestroyed(context) {
    await this._bidiServer.sendMessage({
      method: 'browsingContext.contextDestroyed',
      params: context.toBidi(),
    });
  }

  private _onCdpMessage = function (messageObj: any) {
    switch (messageObj.method) {
      case 'Target.attachedToTarget':
        this._contextProcessor.handleAttachedToTargetEvent(messageObj);
        return Promise.resolve();
      case 'Target.targetInfoChanged':
        this._contextProcessor.handleInfoChangedEvent(messageObj);
        return Promise.resolve();
      case 'Target.detachedFromTarget':
        this._contextProcessor.handleDetachedFromTargetEvent(messageObj);
        return Promise.resolve();
    }
  };

  private async _processCommand(commandData) {
    const response: any = {};
    response.id = commandData.id;

    switch (commandData.method) {
      case 'session.status':
        return await this._process_session_status(commandData.params);
      case 'browsingContext.getTree':
        return await this._process_browsingContext_getTree(commandData.params);

      case 'PROTO.browsingContext.createContext':
        return await this._contextProcessor.process_createContext(
          commandData.params
        );

      case 'DEBUG.Page.close':
        return await this._process_DEBUG_Page_close(commandData.params);

      default:
        throw new Error('unknown command');
    }
  }

  private _onBidiMessage = async (message) => {
    await this._processCommand(message)
      .then((result) => {
        const response = {
          id: message.id,
          result,
        };

        this._bidiServer.sendMessage(response);
      })
      .catch((e) => {
        console.error(e);
        this._respondWithError(message, 'unknown error', e.message);
      });
  };
}
