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

import {
  BrowsingContext,
  CDP,
  Message,
  Script,
  Session,
} from '../protocol/protocol.js';
import {LogType, LoggerFn} from '../utils/log.js';
import {EventEmitter} from '../utils/EventEmitter.js';

import {BrowsingContextProcessor} from './domains/context/browsingContextProcessor.js';
import {BrowsingContextStorage} from './domains/context/browsingContextStorage.js';
import {CdpConnection} from './CdpConnection.js';
import {IEventManager} from './domains/events/EventManager.js';
import {OutgoingBidiMessage} from './OutgoingBidiMessage.js';
import {RealmStorage} from './domains/script/realmStorage.js';

type CommandProcessorEvents = {
  response: Promise<OutgoingBidiMessage>;
};

export interface BidiParser {
  parseAddPreloadScriptParams(
    params: object
  ): Script.AddPreloadScriptParameters;
  parseRemovePreloadScriptParams(
    params: object
  ): Script.RemovePreloadScriptParameters;
  parseGetRealmsParams(params: object): Script.GetRealmsParameters;
  parseCallFunctionParams(params: object): Script.CallFunctionParameters;
  parseEvaluateParams(params: object): Script.EvaluateParameters;
  parseDisownParams(params: object): Script.DisownParameters;
  parseSendCommandParams(params: object): CDP.SendCommandParams;
  parseGetSessionParams(params: object): CDP.GetSessionParams;
  parseSubscribeParams(params: object): Session.SubscriptionRequest;
  parseNavigateParams(params: object): BrowsingContext.NavigateParameters;
  parseReloadParams(params: object): BrowsingContext.ReloadParameters;
  parseGetTreeParams(params: object): BrowsingContext.GetTreeParameters;
  parseCreateParams(params: object): BrowsingContext.CreateParameters;
  parseCloseParams(params: object): BrowsingContext.CloseParameters;
  parseCaptureScreenshotParams(
    params: object
  ): BrowsingContext.CaptureScreenshotParameters;
  parsePrintParams(params: object): BrowsingContext.PrintParameters;
}

class BidiNoOpParser implements BidiParser {
  parseAddPreloadScriptParams(
    params: object
  ): Script.AddPreloadScriptParameters {
    return params as Script.AddPreloadScriptParameters;
  }

  parseRemovePreloadScriptParams(
    params: object
  ): Script.RemovePreloadScriptParameters {
    return params as Script.RemovePreloadScriptParameters;
  }

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
  parseSubscribeParams(params: object): Session.SubscriptionRequest {
    return params as Session.SubscriptionRequest;
  }
  parseNavigateParams(params: object): BrowsingContext.NavigateParameters {
    return params as BrowsingContext.NavigateParameters;
  }
  parseReloadParams(params: object): BrowsingContext.ReloadParameters {
    return params as BrowsingContext.ReloadParameters;
  }
  parseGetTreeParams(params: object): BrowsingContext.GetTreeParameters {
    return params as BrowsingContext.GetTreeParameters;
  }
  parseCreateParams(params: object): BrowsingContext.CreateParameters {
    return params as BrowsingContext.CreateParameters;
  }
  parseCloseParams(params: object): BrowsingContext.CloseParameters {
    return params as BrowsingContext.CloseParameters;
  }
  parseCaptureScreenshotParams(
    params: object
  ): BrowsingContext.CaptureScreenshotParameters {
    return params as BrowsingContext.CaptureScreenshotParameters;
  }
  parsePrintParams(params: object): BrowsingContext.PrintParameters {
    return params as BrowsingContext.PrintParameters;
  }
}

export class CommandProcessor extends EventEmitter<CommandProcessorEvents> {
  #contextProcessor: BrowsingContextProcessor;
  #eventManager: IEventManager;
  #parser: BidiParser;
  #logger?: LoggerFn;

  constructor(
    realmStorage: RealmStorage,
    cdpConnection: CdpConnection,
    eventManager: IEventManager,
    selfTargetId: string,
    parser: BidiParser = new BidiNoOpParser(),
    browsingContextStorage: BrowsingContextStorage,
    logger?: LoggerFn
  ) {
    super();
    this.#eventManager = eventManager;
    this.#logger = logger;
    this.#contextProcessor = new BrowsingContextProcessor(
      realmStorage,
      cdpConnection,
      selfTargetId,
      eventManager,
      browsingContextStorage,
      logger
    );
    this.#parser = parser;
  }

  static #process_session_status(): Session.StatusResult {
    return {result: {ready: false, message: 'already connected'}};
  }

  async #process_session_subscribe(
    params: Session.SubscriptionRequest,
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
    params: Session.SubscriptionRequest,
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
  ): Promise<Message.ResultData> {
    switch (commandData.method) {
      case 'session.status':
        return CommandProcessor.#process_session_status();
      case 'session.subscribe':
        return this.#process_session_subscribe(
          this.#parser.parseSubscribeParams(commandData.params),
          commandData.channel ?? null
        );
      case 'session.unsubscribe':
        return this.#process_session_unsubscribe(
          this.#parser.parseSubscribeParams(commandData.params),
          commandData.channel ?? null
        );

      case 'browsingContext.create':
        return this.#contextProcessor.process_browsingContext_create(
          this.#parser.parseCreateParams(commandData.params)
        );
      case 'browsingContext.close':
        return this.#contextProcessor.process_browsingContext_close(
          this.#parser.parseCloseParams(commandData.params)
        );
      case 'browsingContext.getTree':
        return this.#contextProcessor.process_browsingContext_getTree(
          this.#parser.parseGetTreeParams(commandData.params)
        );
      case 'browsingContext.navigate':
        return this.#contextProcessor.process_browsingContext_navigate(
          this.#parser.parseNavigateParams(commandData.params)
        );
      case 'browsingContext.captureScreenshot':
        return this.#contextProcessor.process_browsingContext_captureScreenshot(
          this.#parser.parseCaptureScreenshotParams(commandData.params)
        );
      case 'browsingContext.print':
        return this.#contextProcessor.process_browsingContext_print(
          this.#parser.parsePrintParams(commandData.params)
        );
      case 'browsingContext.reload':
        return this.#contextProcessor.process_browsingContext_reload(
          this.#parser.parseReloadParams(commandData.params)
        );

      case 'script.addPreloadScript':
        return this.#contextProcessor.process_script_addPreloadScript(
          this.#parser.parseAddPreloadScriptParams(commandData.params)
        );
      case 'script.removePreloadScript':
        return this.#contextProcessor.process_script_removePreloadScript(
          this.#parser.parseRemovePreloadScriptParams(commandData.params)
        );
      case 'script.getRealms':
        return this.#contextProcessor.process_script_getRealms(
          this.#parser.parseGetRealmsParams(commandData.params)
        );
      case 'script.callFunction':
        return this.#contextProcessor.process_script_callFunction(
          this.#parser.parseCallFunctionParams(commandData.params)
        );
      case 'script.evaluate':
        return this.#contextProcessor.process_script_evaluate(
          this.#parser.parseEvaluateParams(commandData.params)
        );
      case 'script.disown':
        return this.#contextProcessor.process_script_disown(
          this.#parser.parseDisownParams(commandData.params)
        );

      case 'cdp.sendCommand':
        return this.#contextProcessor.process_cdp_sendCommand(
          this.#parser.parseSendCommandParams(commandData.params)
        );
      case 'cdp.getSession':
        return this.#contextProcessor.process_cdp_getSession(
          this.#parser.parseGetSessionParams(commandData.params)
        );
    }

    // Intentionally kept outside of the switch statement to ensure that
    // ESLint @typescript-eslint/switch-exhaustiveness-check triggers if a new
    // command is added.
    throw new Message.UnknownCommandException(
      `Unknown command '${commandData.method as string}'.`
    );
  }

  async processCommand(command: Message.RawCommandRequest): Promise<void> {
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
      if (e instanceof Message.ErrorResponse) {
        const errorResponse = e;
        this.emit(
          'response',
          OutgoingBidiMessage.createResolved(
            errorResponse.toErrorResponse(command.id),
            command.channel ?? null
          )
        );
      } else {
        const error = e as Error;
        this.#logger?.(LogType.bidi, error);
        this.emit(
          'response',
          OutgoingBidiMessage.createResolved(
            new Message.UnknownErrorException(error.message).toErrorResponse(
              command.id
            ),
            command.channel ?? null
          )
        );
      }
    }
  }
}
