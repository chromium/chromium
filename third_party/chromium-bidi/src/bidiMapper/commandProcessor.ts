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
  async #process_session_status(): Promise<Session.StatusResult> {
    return { result: { ready: false, message: 'already connected' } };
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
        return await this.#process_session_status();
      case 'session.subscribe':
        return await this.#process_session_subscribe(
          Session.parseSubscribeParams(commandData.params)
        );
      case 'session.unsubscribe':
        return await this.#process_session_unsubscribe(
          Session.parseSubscribeParams(commandData.params)
        );

      case 'browsingContext.create':
        return await this.#contextProcessor.process_browsingContext_create(
          BrowsingContext.parseCreateParams(commandData.params)
        );
      case 'browsingContext.close':
        return await this.#contextProcessor.process_browsingContext_close(
          BrowsingContext.parseCloseParams(commandData.params)
        );
      case 'browsingContext.getTree':
        return await this.#contextProcessor.process_browsingContext_getTree(
          BrowsingContext.parseGetTreeParams(commandData.params)
        );
      case 'browsingContext.navigate':
        return await this.#contextProcessor.process_browsingContext_navigate(
          BrowsingContext.parseNavigateParams(commandData.params)
        );

      case 'script.callFunction':
        return await this.#contextProcessor.process_script_callFunction(
          Script.parseCallFunctionParams(commandData.params)
        );
      case 'script.evaluate':
        return await this.#contextProcessor.process_script_evaluate(
          Script.parseEvaluateParams(commandData.params)
        );

      case 'PROTO.browsingContext.findElement':
        return await this.#contextProcessor.process_PROTO_browsingContext_findElement(
          BrowsingContext.PROTO.parseFindElementParams(commandData.params)
        );

      case 'PROTO.cdp.sendCommand':
        return await this.#contextProcessor.process_PROTO_cdp_sendCommand(
          CDP.PROTO.parseSendCommandParams(commandData.params)
        );
      case 'PROTO.cdp.getSession':
        return await this.#contextProcessor.process_PROTO_cdp_getSession(
          CDP.PROTO.parseGetSessionParams(commandData.params)
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
