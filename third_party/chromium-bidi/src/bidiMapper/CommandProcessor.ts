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

import {BrowsingContextProcessor} from './domains/context/browsingContextProcessor';
import {
  BrowsingContext,
  CDP,
  Message,
  Script,
  Session,
} from '../protocol/types';
import {CdpConnection} from './CdpConnection';
import {OutgoingBidiMessage} from './OutgoindBidiMessage';
import {IEventManager} from './domains/events/EventManager';
import {
  ErrorResponseClass,
  UnknownCommandException,
  UnknownException,
} from '../protocol/error';
import {EventEmitter} from '../utils/EventEmitter';

type CommandProcessorEvents = {
  response: Promise<OutgoingBidiMessage>;
};

export class CommandProcessor extends EventEmitter<CommandProcessorEvents> {
  #contextProcessor: BrowsingContextProcessor;
  #eventManager: IEventManager;

  constructor(
    cdpConnection: CdpConnection,
    eventManager: IEventManager,
    selfTargetId: string
  ) {
    super();
    this.#eventManager = eventManager;
    this.#contextProcessor = new BrowsingContextProcessor(
      cdpConnection,
      selfTargetId,
      eventManager
    );
  }

  // noinspection JSMethodCanBeStatic,JSUnusedLocalSymbols
  async #process_session_status(): Promise<Session.StatusResult> {
    return {result: {ready: false, message: 'already connected'}};
  }

  async #process_session_subscribe(
    params: Session.SubscribeParameters,
    channel: string | null
  ): Promise<Session.SubscribeResult> {
    await this.#eventManager.subscribe(
      params.events,
      params.contexts ?? [null],
      channel
    );
    return {result: {}};
  }

  async #process_session_unsubscribe(
    params: Session.SubscribeParameters,
    channel: string | null
  ): Promise<Session.UnsubscribeResult> {
    await this.#eventManager.unsubscribe(
      params.events,
      params.contexts ?? [null],
      channel
    );
    return {result: {}};
  }

  async #processCommand(
    commandData: Message.RawCommandRequest
  ): Promise<Message.CommandResponseResult> {
    switch (commandData.method) {
      case 'session.status':
        return await this.#process_session_status();
      case 'session.subscribe':
        return await this.#process_session_subscribe(
          Session.parseSubscribeParams(commandData.params),
          commandData.channel ?? null
        );
      case 'session.unsubscribe':
        return await this.#process_session_unsubscribe(
          Session.parseSubscribeParams(commandData.params),
          commandData.channel ?? null
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

      case 'script.getRealms':
        return this.#contextProcessor.process_script_getRealms(
          Script.parseGetRealmsParams(commandData.params)
        );
      case 'script.callFunction':
        return await this.#contextProcessor.process_script_callFunction(
          Script.parseCallFunctionParams(commandData.params)
        );
      case 'script.evaluate':
        return await this.#contextProcessor.process_script_evaluate(
          Script.parseEvaluateParams(commandData.params)
        );
      case 'script.disown':
        return await this.#contextProcessor.process_script_disown(
          Script.parseDisownParams(commandData.params)
        );

      case 'cdp.sendCommand':
        return await this.#contextProcessor.process_cdp_sendCommand(
          CDP.parseSendCommandParams(commandData.params)
        );
      case 'cdp.getSession':
        return await this.#contextProcessor.process_cdp_getSession(
          CDP.parseGetSessionParams(commandData.params)
        );

      default:
        throw new UnknownCommandException(
          `Unknown command '${commandData.method}'.`
        );
    }
  }

  processCommand = async (
    command: Message.RawCommandRequest
  ): Promise<void> => {
    try {
      const result = await this.#processCommand(command);

      const response = {
        id: command.id,
        ...result,
      };

      this.emit(
        'response',
        OutgoingBidiMessage.createResolved(response, command.channel ?? null)
      );
    } catch (e) {
      if (e instanceof ErrorResponseClass) {
        const errorResponse = e as ErrorResponseClass;
        this.emit(
          'response',
          OutgoingBidiMessage.createResolved(
            errorResponse.toErrorResponse(command.id),
            command.channel ?? null
          )
        );
      } else {
        const error = e as Error;
        console.error(error);
        this.emit(
          'response',
          OutgoingBidiMessage.createResolved(
            new UnknownException(error.message).toErrorResponse(command.id),
            command.channel ?? null
          )
        );
      }
    }
  };
}
