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

import type {ICdpConnection} from '../cdp/cdpConnection.js';
import {
  Exception,
  UnknownCommandException,
  UnknownErrorException,
  type ChromiumBidi,
  type EmptyResult,
  type Session,
} from '../protocol/protocol.js';
import {EventEmitter} from '../utils/EventEmitter.js';
import {LogType, type LoggerFn} from '../utils/log.js';

import {BidiNoOpParser} from './BidiNoOpParser.js';
import type {IBidiParser} from './BidiParser.js';
import {OutgoingBidiMessage} from './OutgoingBidiMessage.js';
import {BrowsingContextProcessor} from './domains/context/browsingContextProcessor.js';
import type {BrowsingContextStorage} from './domains/context/browsingContextStorage.js';
import type {IEventManager} from './domains/events/EventManager.js';
import {InputProcessor} from './domains/input/InputProcessor.js';
import type {RealmStorage} from './domains/script/realmStorage.js';

type CommandProcessorEvents = {
  response: Promise<OutgoingBidiMessage>;
};

export class CommandProcessor extends EventEmitter<CommandProcessorEvents> {
  #browsingContextProcessor: BrowsingContextProcessor;
  #inputProcessor: InputProcessor;

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
    this.#parser = parser;
    this.#logger = logger;
    this.#browsingContextProcessor = new BrowsingContextProcessor(
      cdpConnection,
      selfTargetId,
      eventManager,
      browsingContextStorage,
      realmStorage,
      logger
    );
    this.#inputProcessor = InputProcessor.create(browsingContextStorage);
  }

  static #process_session_status(): Session.StatusResult {
    return {ready: false, message: 'already connected'};
  }

  async #process_session_subscribe(
    params: Session.SubscriptionRequest,
    channel: string | null
  ): Promise<EmptyResult> {
    await this.#eventManager.subscribe(
      params.events as ChromiumBidi.EventNames[],
      params.contexts ?? [null],
      channel
    );
    return {};
  }

  async #process_session_unsubscribe(
    params: Session.SubscriptionRequest,
    channel: string | null
  ): Promise<EmptyResult> {
    await this.#eventManager.unsubscribe(
      params.events as ChromiumBidi.EventNames[],
      params.contexts ?? [null],
      channel
    );
    return {};
  }

  async #processCommand(
    command: ChromiumBidi.Command
  ): Promise<ChromiumBidi.ResultData> {
    switch (command.method) {
      case 'browser.close':
      case 'session.end':
      case 'session.new':
        // TODO: Implement.
        break;

      // Browsing Context domain
      // keep-sorted start block=yes
      case 'browsingContext.activate':
        return this.#browsingContextProcessor.process_browsingContext_activate(
          this.#parser.parseActivateParams(command.params)
        );
      case 'browsingContext.captureScreenshot':
        return this.#browsingContextProcessor.process_browsingContext_captureScreenshot(
          this.#parser.parseCaptureScreenshotParams(command.params)
        );
      case 'browsingContext.close':
        return this.#browsingContextProcessor.process_browsingContext_close(
          this.#parser.parseCloseParams(command.params)
        );
      case 'browsingContext.create':
        return this.#browsingContextProcessor.process_browsingContext_create(
          this.#parser.parseCreateParams(command.params)
        );
      case 'browsingContext.getTree':
        return this.#browsingContextProcessor.process_browsingContext_getTree(
          this.#parser.parseGetTreeParams(command.params)
        );
      case 'browsingContext.handleUserPrompt':
        return this.#browsingContextProcessor.process_browsingContext_handleUserPrompt(
          this.#parser.parseHandleUserPromptParams(command.params)
        );
      case 'browsingContext.navigate':
        return this.#browsingContextProcessor.process_browsingContext_navigate(
          this.#parser.parseNavigateParams(command.params)
        );
      case 'browsingContext.print':
        return this.#browsingContextProcessor.process_browsingContext_print(
          this.#parser.parsePrintParams(command.params)
        );
      case 'browsingContext.reload':
        return this.#browsingContextProcessor.process_browsingContext_reload(
          this.#parser.parseReloadParams(command.params)
        );
      case 'browsingContext.setViewport':
        return this.#browsingContextProcessor.process_browsingContext_setViewport(
          this.#parser.parseSetViewportParams(command.params)
        );
      // keep-sorted end

      // CDP domain
      // keep-sorted start block=yes
      case 'cdp.getSession':
        return this.#browsingContextProcessor.process_cdp_getSession(
          this.#parser.parseGetSessionParams(command.params)
        );
      case 'cdp.sendCommand':
        return this.#browsingContextProcessor.process_cdp_sendCommand(
          this.#parser.parseSendCommandParams(command.params)
        );
      // keep-sorted end

      // Input domain
      // keep-sorted start block=yes
      case 'input.performActions':
        return this.#inputProcessor.performActions(
          this.#parser.parsePerformActionsParams(command.params)
        );
      case 'input.releaseActions':
        return this.#inputProcessor.releaseActions(
          this.#parser.parseReleaseActionsParams(command.params)
        );
      // keep-sorted end

      // Script domain
      // keep-sorted start block=yes
      case 'script.addPreloadScript':
        return this.#browsingContextProcessor.process_script_addPreloadScript(
          this.#parser.parseAddPreloadScriptParams(command.params)
        );
      case 'script.callFunction':
        return this.#browsingContextProcessor.process_script_callFunction(
          this.#parser.parseCallFunctionParams(command.params)
        );
      case 'script.disown':
        return this.#browsingContextProcessor.process_script_disown(
          this.#parser.parseDisownParams(command.params)
        );
      case 'script.evaluate':
        return this.#browsingContextProcessor.process_script_evaluate(
          this.#parser.parseEvaluateParams(command.params)
        );
      case 'script.getRealms':
        return this.#browsingContextProcessor.process_script_getRealms(
          this.#parser.parseGetRealmsParams(command.params)
        );
      case 'script.removePreloadScript':
        return this.#browsingContextProcessor.process_script_removePreloadScript(
          this.#parser.parseRemovePreloadScriptParams(command.params)
        );
      // keep-sorted end

      // Session domain
      // keep-sorted start block=yes
      case 'session.status':
        return CommandProcessor.#process_session_status();
      case 'session.subscribe':
        return this.#process_session_subscribe(
          this.#parser.parseSubscribeParams(command.params),
          command.channel ?? null
        );
      case 'session.unsubscribe':
        return this.#process_session_unsubscribe(
          this.#parser.parseSubscribeParams(command.params),
          command.channel ?? null
        );
      // keep-sorted end
    }

    // Intentionally kept outside of the switch statement to ensure that
    // ESLint @typescript-eslint/switch-exhaustiveness-check triggers if a new
    // command is added.
    throw new UnknownCommandException(`Unknown command '${command.method}'.`);
  }

  async processCommand(command: ChromiumBidi.Command): Promise<void> {
    try {
      const result = await this.#processCommand(command);

      const response = {
        type: 'success',
        id: command.id,
        result,
      } satisfies ChromiumBidi.CommandResponse;

      this.emit(
        'response',
        OutgoingBidiMessage.createResolved(response, command.channel ?? null)
      );
    } catch (e) {
      if (e instanceof Exception) {
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
            new UnknownErrorException(
              error.message,
              error.stack
            ).toErrorResponse(command.id),
            command.channel ?? null
          )
        );
      }
    }
  }
}
