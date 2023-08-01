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
} from '../protocol/protocol.js';
import {EventEmitter} from '../utils/EventEmitter.js';
import {LogType, type LoggerFn} from '../utils/log.js';
import type {Result} from '../utils/result.js';

import {BidiNoOpParser} from './BidiNoOpParser.js';
import type {IBidiParser} from './BidiParser.js';
import {OutgoingBidiMessage} from './OutgoingBidiMessage.js';
import {BrowserProcessor} from './domains/browser/BrowserProcessor.js';
import {CdpProcessor} from './domains/cdp/CdpProcessor.js';
import {BrowsingContextProcessor} from './domains/context/browsingContextProcessor.js';
import type {BrowsingContextStorage} from './domains/context/browsingContextStorage.js';
import type {IEventManager} from './domains/events/EventManager.js';
import {InputProcessor} from './domains/input/InputProcessor.js';
import {PreloadScriptStorage} from './domains/script/PreloadScriptStorage.js';
import {ScriptProcessor} from './domains/script/ScriptProcessor.js';
import type {RealmStorage} from './domains/script/realmStorage.js';
import {SessionProcessor} from './domains/session/SessionProcessor.js';

type CommandProcessorEvents = {
  response: Promise<Result<OutgoingBidiMessage>>;
};

export class CommandProcessor extends EventEmitter<CommandProcessorEvents> {
  #browserProcessor: BrowserProcessor;
  #browsingContextProcessor: BrowsingContextProcessor;
  #inputProcessor: InputProcessor;
  #scriptProcessor: ScriptProcessor;
  #sessionProcessor: SessionProcessor;
  #cdpProcessor: CdpProcessor;

  #parser: IBidiParser;
  #logger?: LoggerFn;

  constructor(
    cdpConnection: ICdpConnection,
    eventManager: IEventManager,
    selfTargetId: string,
    browsingContextStorage: BrowsingContextStorage,
    realmStorage: RealmStorage,
    parser: IBidiParser = new BidiNoOpParser(),
    logger?: LoggerFn
  ) {
    super();
    this.#parser = parser;
    this.#logger = logger;
    const preloadScriptStorage = new PreloadScriptStorage();
    this.#browserProcessor = new BrowserProcessor(cdpConnection);
    this.#browsingContextProcessor = new BrowsingContextProcessor(
      cdpConnection,
      selfTargetId,
      eventManager,
      browsingContextStorage,
      realmStorage,
      preloadScriptStorage,
      logger
    );
    this.#inputProcessor = InputProcessor.create(browsingContextStorage);
    this.#scriptProcessor = new ScriptProcessor(
      browsingContextStorage,
      realmStorage,
      preloadScriptStorage
    );
    this.#sessionProcessor = new SessionProcessor(eventManager);
    this.#cdpProcessor = new CdpProcessor(
      browsingContextStorage,
      cdpConnection
    );
  }

  async #processCommand(
    command: ChromiumBidi.Command
  ): Promise<ChromiumBidi.ResultData> {
    switch (command.method) {
      case 'session.end':
      case 'session.new':
        // TODO: Implement.
        break;

      // Browser domain
      // keep-sorted start block=yes
      case 'browser.close':
        return this.#browserProcessor.close();
      // keep-sorted end

      // Browsing Context domain
      // keep-sorted start block=yes
      case 'browsingContext.activate':
        return this.#browsingContextProcessor.activate(
          this.#parser.parseActivateParams(command.params)
        );
      case 'browsingContext.captureScreenshot':
        return this.#browsingContextProcessor.captureScreenshot(
          this.#parser.parseCaptureScreenshotParams(command.params)
        );
      case 'browsingContext.close':
        return this.#browsingContextProcessor.close(
          this.#parser.parseCloseParams(command.params)
        );
      case 'browsingContext.create':
        return this.#browsingContextProcessor.create(
          this.#parser.parseCreateParams(command.params)
        );
      case 'browsingContext.getTree':
        return this.#browsingContextProcessor.getTree(
          this.#parser.parseGetTreeParams(command.params)
        );
      case 'browsingContext.handleUserPrompt':
        return this.#browsingContextProcessor.handleUserPrompt(
          this.#parser.parseHandleUserPromptParams(command.params)
        );
      case 'browsingContext.navigate':
        return this.#browsingContextProcessor.navigate(
          this.#parser.parseNavigateParams(command.params)
        );
      case 'browsingContext.print':
        return this.#browsingContextProcessor.print(
          this.#parser.parsePrintParams(command.params)
        );
      case 'browsingContext.reload':
        return this.#browsingContextProcessor.reload(
          this.#parser.parseReloadParams(command.params)
        );
      case 'browsingContext.setViewport':
        return this.#browsingContextProcessor.setViewport(
          this.#parser.parseSetViewportParams(command.params)
        );
      // keep-sorted end

      // CDP domain
      // keep-sorted start block=yes
      case 'cdp.getSession':
        return this.#cdpProcessor.getSession(
          this.#parser.parseGetSessionParams(command.params)
        );
      case 'cdp.sendCommand':
        return this.#cdpProcessor.sendCommand(
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
        return this.#scriptProcessor.addPreloadScript(
          this.#parser.parseAddPreloadScriptParams(command.params)
        );
      case 'script.callFunction':
        return this.#scriptProcessor.callFunction(
          this.#parser.parseCallFunctionParams(command.params)
        );
      case 'script.disown':
        return this.#scriptProcessor.disown(
          this.#parser.parseDisownParams(command.params)
        );
      case 'script.evaluate':
        return this.#scriptProcessor.evaluate(
          this.#parser.parseEvaluateParams(command.params)
        );
      case 'script.getRealms':
        return this.#scriptProcessor.getRealms(
          this.#parser.parseGetRealmsParams(command.params)
        );
      case 'script.removePreloadScript':
        return this.#scriptProcessor.removePreloadScript(
          this.#parser.parseRemovePreloadScriptParams(command.params)
        );
      // keep-sorted end

      // Session domain
      // keep-sorted start block=yes
      case 'session.status':
        return this.#sessionProcessor.status();
      case 'session.subscribe':
        return this.#sessionProcessor.subscribe(
          this.#parser.parseSubscribeParams(command.params),
          command.channel
        );
      case 'session.unsubscribe':
        return this.#sessionProcessor.unsubscribe(
          this.#parser.parseSubscribeParams(command.params),
          command.channel
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
        OutgoingBidiMessage.createResolved(response, command.channel)
      );
    } catch (e) {
      if (e instanceof Exception) {
        const errorResponse = e;
        this.emit(
          'response',
          OutgoingBidiMessage.createResolved(
            errorResponse.toErrorResponse(command.id),
            command.channel
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
            command.channel
          )
        );
      }
    }
  }
}
