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

import {Message, type Session} from '../protocol/protocol.js';
import {LogType, type LoggerFn} from '../utils/log.js';
import {EventEmitter} from '../utils/EventEmitter.js';
import type {ICdpConnection} from '../cdp/cdpConnection.js';

import type {IBidiParser} from './BidiParser.js';
import {BidiNoOpParser} from './BidiNoOpParser.js';
import {BrowsingContextProcessor} from './domains/context/browsingContextProcessor.js';
import type {BrowsingContextStorage} from './domains/context/browsingContextStorage.js';
import type {IEventManager} from './domains/events/EventManager.js';
import {OutgoingBidiMessage} from './OutgoingBidiMessage.js';
import type {RealmStorage} from './domains/script/realmStorage.js';

type CommandProcessorEvents = {
  response: Promise<OutgoingBidiMessage>;
};

export class CommandProcessor extends EventEmitter<CommandProcessorEvents> {
  #contextProcessor: BrowsingContextProcessor;
  #eventManager: IEventManager;
  #parser: IBidiParser;
  #logger?: LoggerFn;

  constructor(
    cdpConnection: ICdpConnection,
    eventManager: IEventManager,
    selfTargetId: string,
    parser: IBidiParser = new BidiNoOpParser(),
    browsingContextStorage: BrowsingContextStorage,
    realmStorage: RealmStorage,
    logger?: LoggerFn
  ) {
    super();
    this.#eventManager = eventManager;
    this.#logger = logger;
    this.#contextProcessor = new BrowsingContextProcessor(
      cdpConnection,
      selfTargetId,
      eventManager,
      browsingContextStorage,
      realmStorage,
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
  ): Promise<Message.EmptyResult> {
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
  ): Promise<Message.EmptyResult> {
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
      case 'browsingContext.setViewport':
        return this.#contextProcessor.process_browsingContext_setViewport(
          this.#parser.parseSetViewportParams(commandData.params)
        );

      case 'cdp.sendCommand':
        return this.#contextProcessor.process_cdp_sendCommand(
          this.#parser.parseSendCommandParams(commandData.params)
        );
      case 'cdp.getSession':
        return this.#contextProcessor.process_cdp_getSession(
          this.#parser.parseGetSessionParams(commandData.params)
        );

      case 'input.performActions':
        return this.#contextProcessor.process_input_performActions(
          this.#parser.parsePerformActionsParams(commandData.params)
        );
      case 'input.releaseActions':
        return this.#contextProcessor.process_input_releaseActions(
          this.#parser.parseReleaseActionsParams(commandData.params)
        );

      case 'network.addIntercept':
        return this.#contextProcessor.process_network_addIntercept(
          this.#parser.parseAddInterceptParams(commandData.params)
        );
      case 'network.continueRequest':
        return this.#contextProcessor.process_network_continueRequest(
          this.#parser.parseContinueRequestParams(commandData.params)
        );
      case 'network.continueResponse':
        return this.#contextProcessor.process_network_continueResponse(
          this.#parser.parseContinueResponseParams(commandData.params)
        );
      case 'network.continueWithAuth':
        return this.#contextProcessor.process_network_continueWithAuth(
          this.#parser.parseContinueWithAuthParams(commandData.params)
        );
      case 'network.failRequest':
        return this.#contextProcessor.process_network_failRequest(
          this.#parser.parseFailRequestParams(commandData.params)
        );
      case 'network.provideResponse':
        return this.#contextProcessor.process_network_provideResponse(
          this.#parser.parseProvideResponseParams(commandData.params)
        );
      case 'network.removeIntercept':
        return this.#contextProcessor.process_network_removeIntercept(
          this.#parser.parseRemoveInterceptParams(commandData.params)
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
            new Message.UnknownErrorException(error).toErrorResponse(
              command.id
            ),
            command.channel ?? null
          )
        );
      }
    }
  }
}
