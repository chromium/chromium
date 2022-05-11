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

import { BrowsingContextProcessor } from './domains/context/browsingContextProcessor';
import {
  BrowsingContext,
  CDP,
  Message,
  Script,
  Session,
} from './bidiProtocolTypes';
import { CdpClient, CdpConnection } from '../cdp';
import { IBidiServer } from './utils/bidiServer';
import { IEventManager } from './domains/events/EventManager';

export class CommandProcessor {
  private _browserCdpClient: CdpClient;
  private _contextProcessor: BrowsingContextProcessor;

  static run(
    cdpConnection: CdpConnection,
    bidiServer: IBidiServer,
    eventManager: IEventManager,
    selfTargetId: string
  ) {
    const commandProcessor = new CommandProcessor(
      cdpConnection,
      bidiServer,
      eventManager,
      selfTargetId
    );

    commandProcessor._run();
  }

  private constructor(
    private _cdpConnection: CdpConnection,
    private _bidiServer: IBidiServer,
    private _eventManager: IEventManager,
    _selfTargetId: string
  ) {
    this._browserCdpClient = this._cdpConnection.browserClient();

    this._contextProcessor = new BrowsingContextProcessor(
      this._cdpConnection,
      _selfTargetId,
      this._bidiServer,
      this._eventManager
    );
  }

  private _run() {
    this._bidiServer.on('message', (messageObj) => {
      return this._onBidiMessage(messageObj);
    });
  }

  // noinspection JSMethodCanBeStatic
  private _getErrorResponse(
    commandData: any,
    errorCode: string,
    errorMessage: string
  ): Message.Error {
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

  private async _respondWithError(
    commandData: any,
    errorCode: string,
    errorMessage: string
  ) {
    const errorResponse = this._getErrorResponse(
      commandData,
      errorCode,
      errorMessage
    );
    await this._bidiServer.sendMessage(errorResponse);
  }

  // noinspection JSMethodCanBeStatic,JSUnusedLocalSymbols
  private async _process_session_status(
    commandData: Session.StatusCommand
  ): Promise<Session.StatusResult> {
    return { result: { ready: true, message: 'ready' } };
  }

  private async _process_session_subscribe(
    commandData: Session.SubscribeCommand
  ): Promise<Session.SubscribeResult> {
    await this._eventManager.subscribe(
      commandData.params.events,
      commandData.params.contexts ?? null
    );

    return { result: {} };
  }

  // noinspection JSMethodCanBeStatic,JSUnusedLocalSymbols
  private _process_session_unsubscribe(
    commandData: Session.UnsubscribeCommand
  ): Promise<Session.UnsubscribeResult> {
    throw new Error('Not implemented');
  }

  private async _processCommand(
    commandData: Message.Command
  ): Promise<Message.CommandResponseResult> {
    switch (commandData.method) {
      case 'session.status':
        return await this._process_session_status(
          commandData as Session.StatusCommand
        );
      case 'session.subscribe':
        return await this._process_session_subscribe(
          commandData as Session.SubscribeCommand
        );
      case 'session.unsubscribe':
        return await this._process_session_unsubscribe(
          commandData as Session.UnsubscribeCommand
        );

      case 'browsingContext.create':
        return await this._contextProcessor.process_browsingContext_create(
          commandData as BrowsingContext.CreateCommand
        );
      case 'browsingContext.getTree':
        return await this._contextProcessor.process_browsingContext_getTree(
          commandData as BrowsingContext.GetTreeCommand
        );
      case 'browsingContext.navigate':
        return await this._contextProcessor.process_browsingContext_navigate(
          commandData as BrowsingContext.NavigateCommand
        );

      case 'script.callFunction':
        return await this._contextProcessor.process_script_callFunction(
          commandData as Script.CallFunctionCommand
        );
      case 'script.evaluate':
        return await this._contextProcessor.process_script_evaluate(
          commandData as Script.EvaluateCommand
        );

      case 'PROTO.browsingContext.findElement':
        return await this._contextProcessor.process_PROTO_browsingContext_findElement(
          commandData as BrowsingContext.PROTO.FindElementCommand
        );
      case 'PROTO.browsingContext.close':
        return await this._contextProcessor.process_PROTO_browsingContext_close(
          commandData as BrowsingContext.PROTO.CloseCommand
        );

      case 'PROTO.cdp.sendCommand':
        return await this._contextProcessor.process_PROTO_cdp_sendCommand(
          commandData as CDP.PROTO.SendCommandCommand
        );
      case 'PROTO.cdp.getSession':
        return await this._contextProcessor.process_PROTO_cdp_getSession(
          commandData as CDP.PROTO.GetSessionCommand
        );

      default:
        throw new Error('unknown command');
    }
  }

  private _onBidiMessage = async (message: Message.Command) => {
    try {
      const result = await this._processCommand(message);

      const response = {
        id: message.id,
        ...result,
      };

      await this._bidiServer.sendMessage(response);
    } catch (e) {
      const error = e as Error;
      console.error(error);
      await this._respondWithError(message, 'unknown error', error.message);
    }
  };
}
