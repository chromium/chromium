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
} from './domains/protocol/bidiProtocolTypes';
import { CdpConnection } from '../cdp';
import { IBidiServer } from './utils/bidiServer';
import { IEventManager } from './domains/events/EventManager';
import {
  ErrorResponseClass,
  UnknownCommandErrorResponse,
  UnknownErrorResponse,
} from './domains/protocol/error';
import { SessionParser } from './domains/protocol/parsers/sessionParser';

export class CommandProcessor {
  #contextProcessor: BrowsingContextProcessor;
  #bidiServer: IBidiServer;
  #eventManager: IEventManager;

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

    commandProcessor.#run();
  }

  private constructor(
    cdpConnection: CdpConnection,
    bidiServer: IBidiServer,
    eventManager: IEventManager,
    selfTargetId: string
  ) {
    this.#eventManager = eventManager;
    this.#bidiServer = bidiServer;
    this.#contextProcessor = new BrowsingContextProcessor(
      cdpConnection,
      selfTargetId,
      bidiServer,
      eventManager
    );
  }

  #run() {
    this.#bidiServer.on('message', (messageObj) => {
      return this.#onBidiMessage(messageObj);
    });
  }

  // noinspection JSMethodCanBeStatic,JSUnusedLocalSymbols
  async #process_session_status(
    commandData: Session.StatusCommand
  ): Promise<Session.StatusResult> {
    return { result: { ready: true, message: 'ready' } };
  }

  async #process_session_subscribe(
    params: Session.SubscribeParameters
  ): Promise<Session.SubscribeResult> {
    await this.#eventManager.subscribe(params.events, params.contexts ?? null);
    return { result: {} };
  }

  async #process_session_unsubscribe(
    params: Session.SubscribeParameters
  ): Promise<Session.UnsubscribeResult> {
    await this.#eventManager.unsubscribe(
      params.events,
      params.contexts ?? null
    );
    return { result: {} };
  }

  async #processCommand(
    commandData: Message.RawCommandRequest
  ): Promise<Message.CommandResponseResult> {
    switch (commandData.method) {
      case 'session.status':
        return await this.#process_session_status(
          // TODO(sadym): add params parsing.
          commandData as Session.StatusCommand
        );
      case 'session.subscribe':
        return await this.#process_session_subscribe(
          SessionParser.SubscribeParamsParser.parse(commandData.params)
        );
      case 'session.unsubscribe':
        return await this.#process_session_unsubscribe(
          SessionParser.SubscribeParamsParser.parse(commandData.params)
        );

      case 'browsingContext.create':
        return await this.#contextProcessor.process_browsingContext_create(
          // TODO(sadym): add params parsing.
          commandData as BrowsingContext.CreateCommand
        );
      case 'browsingContext.getTree':
        return await this.#contextProcessor.process_browsingContext_getTree(
          // TODO(sadym): add params parsing.
          commandData as BrowsingContext.GetTreeCommand
        );
      case 'browsingContext.navigate':
        return await this.#contextProcessor.process_browsingContext_navigate(
          // TODO(sadym): add params parsing.
          commandData as BrowsingContext.NavigateCommand
        );

      case 'script.callFunction':
        return await this.#contextProcessor.process_script_callFunction(
          // TODO(sadym): add params parsing.
          commandData as Script.CallFunctionCommand
        );
      case 'script.evaluate':
        return await this.#contextProcessor.process_script_evaluate(
          // TODO(sadym): add params parsing.
          commandData as Script.EvaluateCommand
        );

      case 'PROTO.browsingContext.findElement':
        return await this.#contextProcessor.process_PROTO_browsingContext_findElement(
          // TODO(sadym): add params parsing.
          commandData as BrowsingContext.PROTO.FindElementCommand
        );
      case 'PROTO.browsingContext.close':
        return await this.#contextProcessor.process_PROTO_browsingContext_close(
          // TODO(sadym): add params parsing.
          commandData as BrowsingContext.PROTO.CloseCommand
        );

      case 'PROTO.cdp.sendCommand':
        return await this.#contextProcessor.process_PROTO_cdp_sendCommand(
          // TODO(sadym): add params parsing.
          commandData as CDP.PROTO.SendCommandCommand
        );
      case 'PROTO.cdp.getSession':
        return await this.#contextProcessor.process_PROTO_cdp_getSession(
          // TODO(sadym): add params parsing.
          commandData as CDP.PROTO.GetSessionCommand
        );

      default:
        throw new UnknownCommandErrorResponse(
          `Unknown command '${commandData.method}'.`
        );
    }
  }

  #onBidiMessage = async (command: Message.RawCommandRequest) => {
    try {
      const result = await this.#processCommand(command);

      const response = {
        id: command.id,
        ...result,
      };

      await this.#bidiServer.sendMessage(response);
    } catch (e) {
      if (e instanceof ErrorResponseClass) {
        const errorResponse = e as ErrorResponseClass;

        await this.#bidiServer.sendMessage(
          errorResponse.toErrorResponse(command.id)
        );
      } else {
        const error = e as Error;
        console.error(error);

        await this.#bidiServer.sendMessage(
          new UnknownErrorResponse(error.message).toErrorResponse(command.id)
        );
      }
    }
  };
}
