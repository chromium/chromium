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

import {BrowsingContextProcessor} from './domains/context/browsingContextProcessor.js';
import {
  Message,
  Session,
  BrowsingContext,
  Script,
  CDP,
} from '../protocol/protocol.js';
import {CdpConnection} from './CdpConnection.js';
import {OutgoingBidiMessage} from './OutgoindBidiMessage.js';
import {IEventManager} from './domains/events/EventManager.js';
import {EventEmitter} from '../utils/EventEmitter.js';
import {BrowsingContextStorage} from './domains/context/browsingContextStorage.js';

type CommandProcessorEvents = {
  response: Promise<OutgoingBidiMessage>;
};

export interface BidiParser {
  parseGetRealmsParams(params: object): Script.GetRealmsParameters;
  parseCallFunctionParams(params: object): Script.CallFunctionParameters;
  parseEvaluateParams(params: object): Script.EvaluateParameters;
  parseDisownParams(params: object): Script.DisownParameters;
  parseSendCommandParams(params: object): CDP.SendCommandParams;
  parseGetSessionParams(params: object): CDP.GetSessionParams;
  parseNavigateParams(params: object): BrowsingContext.NavigateParameters;
  parseGetTreeParams(params: object): BrowsingContext.GetTreeParameters;
  parseSubscribeParams(params: object): Session.SubscribeParameters;
  parseCreateParams(params: object): BrowsingContext.CreateParameters;
  parseCloseParams(params: object): BrowsingContext.CloseParameters;
}

class BidiNoOpParser implements BidiParser {
  parseGetRealmsParams(params: object): Script.GetRealmsParameters {
    return params as Script.GetRealmsParameters;
  }
  parseCallFunctionParams(params: object): Script.CallFunctionParameters {
    return params as Script.CallFunctionParameters;
  }
  parseEvaluateParams(params: object): Script.EvaluateParameters {
    return params as Script.EvaluateParameters;
  }
  parseDisownParams(params: object): Script.DisownParameters {
    return params as Script.DisownParameters;
  }
  parseSendCommandParams(params: object): CDP.SendCommandParams {
    return params as CDP.SendCommandParams;
  }
  parseGetSessionParams(params: object): CDP.GetSessionParams {
    return params as CDP.GetSessionParams;
  }
  parseNavigateParams(params: object): BrowsingContext.NavigateParameters {
    return params as BrowsingContext.NavigateParameters;
  }
  parseGetTreeParams(params: object): BrowsingContext.GetTreeParameters {
    return params as BrowsingContext.GetTreeParameters;
  }
  parseSubscribeParams(params: object): Session.SubscribeParameters {
    return params as Session.SubscribeParameters;
  }
  parseCreateParams(params: object): BrowsingContext.CreateParameters {
    return params as BrowsingContext.CreateParameters;
  }
  parseCloseParams(params: object): BrowsingContext.CloseParameters {
    return params as BrowsingContext.CloseParameters;
  }
}

export class CommandProcessor extends EventEmitter<CommandProcessorEvents> {
  #contextProcessor: BrowsingContextProcessor;
  #eventManager: IEventManager;
  #parser: BidiParser;

  constructor(
    cdpConnection: CdpConnection,
    eventManager: IEventManager,
    selfTargetId: string,
    parser: BidiParser = new BidiNoOpParser(),
    browsingContextStorage: BrowsingContextStorage
  ) {
    super();
    this.#eventManager = eventManager;
    this.#contextProcessor = new BrowsingContextProcessor(
      cdpConnection,
      selfTargetId,
      eventManager,
      browsingContextStorage
    );
    this.#parser = parser;
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
          this.#parser.parseSubscribeParams(commandData.params),
          commandData.channel ?? null
        );
      case 'session.unsubscribe':
        return await this.#process_session_unsubscribe(
          this.#parser.parseSubscribeParams(commandData.params),
          commandData.channel ?? null
        );

      case 'browsingContext.create':
        return await this.#contextProcessor.process_browsingContext_create(
          this.#parser.parseCreateParams(commandData.params)
        );
      case 'browsingContext.close':
        return await this.#contextProcessor.process_browsingContext_close(
          this.#parser.parseCloseParams(commandData.params)
        );
      case 'browsingContext.getTree':
        return await this.#contextProcessor.process_browsingContext_getTree(
          this.#parser.parseGetTreeParams(commandData.params)
        );
      case 'browsingContext.navigate':
        return await this.#contextProcessor.process_browsingContext_navigate(
          this.#parser.parseNavigateParams(commandData.params)
        );

      case 'script.getRealms':
        return this.#contextProcessor.process_script_getRealms(
          this.#parser.parseGetRealmsParams(commandData.params)
        );
      case 'script.callFunction':
        return await this.#contextProcessor.process_script_callFunction(
          this.#parser.parseCallFunctionParams(commandData.params)
        );
      case 'script.evaluate':
        return await this.#contextProcessor.process_script_evaluate(
          this.#parser.parseEvaluateParams(commandData.params)
        );
      case 'script.disown':
        return await this.#contextProcessor.process_script_disown(
          this.#parser.parseDisownParams(commandData.params)
        );

      case 'cdp.sendCommand':
        return await this.#contextProcessor.process_cdp_sendCommand(
          this.#parser.parseSendCommandParams(commandData.params)
        );
      case 'cdp.getSession':
        return await this.#contextProcessor.process_cdp_getSession(
          this.#parser.parseGetSessionParams(commandData.params)
        );

      default:
        throw new Message.UnknownCommandException(
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
      if (e instanceof Message.ErrorResponseClass) {
        const errorResponse = e as Message.ErrorResponseClass;
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
            new Message.UnknownException(error.message).toErrorResponse(
              command.id
            ),
            command.channel ?? null
          )
        );
      }
    }
  };
}
