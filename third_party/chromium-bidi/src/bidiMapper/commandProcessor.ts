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

import { CdpClient, CdpConnection } from '../cdp';
import { BrowsingContextProcessor } from './domains/context/browsingContextProcessor';
import { Protocol } from 'devtools-protocol';
import { BidiCommandMessage, IBidiServer } from './utils/bidiServer';
import { BrowsingContext, Script, Session } from './bidiProtocolTypes';

export class CommandProcessor {
  private _browserCdpClient: CdpClient;
  private _contextProcessor: BrowsingContextProcessor;

  static run(
    cdpConnection: CdpConnection,
    bidiServer: IBidiServer,
    selfTargetId: string,
    EVALUATOR_SCRIPT: string
  ) {
    const commandProcessor = new CommandProcessor(
      cdpConnection,
      bidiServer,
      selfTargetId,
      EVALUATOR_SCRIPT
    );

    commandProcessor.run();
  }

  private constructor(
    private _cdpConnection: CdpConnection,
    private _bidiServer: IBidiServer,
    private _selfTargetId: string,
    EVALUATOR_SCRIPT: string
  ) {
    this._browserCdpClient = this._cdpConnection.browserClient();

    this._contextProcessor = new BrowsingContextProcessor(
      this._cdpConnection,
      this._selfTargetId,
      this._bidiServer,
      EVALUATOR_SCRIPT
    );
  }

  private run() {
    this._bidiServer.on('message', (messageObj) => {
      return this._onBidiMessage(messageObj);
    });
  }

  private _isValidTarget = (target: Protocol.Target.TargetInfo) => {
    if (target.targetId === this._selfTargetId) return false;
    if (!target.type || target.type !== 'page') return false;
    return true;
  };

  private _getErrorResponse(
    commandData: any,
    errorCode: string,
    errorMessage: string
  ) {
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

  private _respondWithError(
    commandData: any,
    errorCode: string,
    errorMessage: string
  ) {
    const errorResponse = this._getErrorResponse(
      commandData,
      errorCode,
      errorMessage
    );
    this._bidiServer.sendMessage(errorResponse);
  }

  private _targetToContext(
    target: Protocol.Target.TargetInfo
  ): BrowsingContext.BrowsingContextInfo {
    return {
      context: target.targetId,
      parent: target.openerId ? target.openerId : undefined,
      url: target.url,
      // TODO sadym: implement.
      children: [],
    };
  }

  private async _process_browsingContext_getTree(
    commandData: BrowsingContext.BrowsingContextGetTreeCommand
  ): Promise<BrowsingContext.BrowsingContextGetTreeResult> {
    const { targetInfos } = await this._browserCdpClient.Target.getTargets();
    // TODO sadym: implement.
    if (commandData.params.maxDepth || commandData.params.parent)
      throw new Error('not implemented yet');
    const contexts = targetInfos
      // Don't expose any information about the tab with Mapper running.
      .filter(this._isValidTarget)
      .map(this._targetToContext);
    return { contexts };
  }

  private async _process_DEBUG_Page_close(params: { context: string }) {
    await this._browserCdpClient.Target.closeTarget({
      targetId: params.context,
    });
    return {};
  }

  private _process_session_status = async function (
    commandData: Session.SessionStatusCommand
  ): Promise<Session.SessionStatusResult> {
    return { ready: true, message: 'ready' };
  };

  private async _processCommand(commandData: BidiCommandMessage) {
    switch (commandData.method) {
      case 'session.status':
        return await this._process_session_status(
          commandData as Session.SessionStatusCommand
        );
      case 'browsingContext.getTree':
        return await this._process_browsingContext_getTree(
          commandData as BrowsingContext.BrowsingContextGetTreeCommand
        );
      case 'script.evaluate':
        return await this._contextProcessor.process_script_evaluate(
          commandData as Script.ScriptEvaluateCommand
        );
      case 'browsingContext.navigate':
        return await this._contextProcessor.process_browsingContext_navigate(
          commandData as BrowsingContext.BrowsingContextNavigateCommand
        );

      case 'PROTO.browsingContext.create':
        return await this._contextProcessor.process_PROTO_browsingContext_create(
          commandData as BrowsingContext.PROTO.BrowsingContextCreateCommand
        );
      case 'PROTO.script.invoke':
        return await this._contextProcessor.process_PROTO_script_invoke(
          commandData as Script.PROTO.ScriptInvokeCommand
        );

      case 'DEBUG.Page.close':
        return await this._process_DEBUG_Page_close(commandData.params as any);

      default:
        throw new Error('unknown command');
    }
  }

  private _onBidiMessage = async (message: BidiCommandMessage) => {
    try {
      const result = await this._processCommand(message);

      const response = {
        id: message.id,
        result,
      };

      this._bidiServer.sendMessage(response);
    } catch (e) {
      const error = e as Error;
      console.error(error);
      this._respondWithError(message, 'unknown error', error.message);
    }
  };
}
