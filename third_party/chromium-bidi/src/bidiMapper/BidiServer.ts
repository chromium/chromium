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

import type {CdpClient} from '../cdp/CdpClient';
import type {CdpConnection} from '../cdp/CdpConnection.js';
import type {Browser, ChromiumBidi} from '../protocol/protocol.js';
import {EventEmitter} from '../utils/EventEmitter.js';
import {type LoggerFn, LogType} from '../utils/log.js';
import {ProcessingQueue} from '../utils/ProcessingQueue.js';
import type {Result} from '../utils/result.js';

import type {BidiCommandParameterParser} from './BidiParser.js';
import type {BidiTransport} from './BidiTransport.js';
import {CommandProcessor, CommandProcessorEvents} from './CommandProcessor.js';
import {BrowsingContextStorage} from './domains/context/BrowsingContextStorage.js';
import {RealmStorage} from './domains/script/RealmStorage.js';
import {
  EventManager,
  EventManagerEvents,
} from './domains/session/EventManager.js';
import type {OutgoingMessage} from './OutgoingMessage.js';

type BidiServerEvent = {
  message: ChromiumBidi.Command;
};

export type MapperOptions = {
  acceptInsecureCerts: boolean;
};

export class BidiServer extends EventEmitter<BidiServerEvent> {
  #messageQueue: ProcessingQueue<OutgoingMessage>;
  #transport: BidiTransport;
  #commandProcessor: CommandProcessor;
  #eventManager: EventManager;
  #browsingContextStorage = new BrowsingContextStorage();
  #logger?: LoggerFn;

  #handleIncomingMessage = (message: ChromiumBidi.Command) => {
    void this.#commandProcessor.processCommand(message).catch((error) => {
      this.#logger?.(LogType.debugError, error);
    });
  };

  #processOutgoingMessage = async (messageEntry: OutgoingMessage) => {
    const message = messageEntry.message;

    if (messageEntry.channel !== null) {
      message['channel'] = messageEntry.channel;
    }

    await this.#transport.sendMessage(message);
  };

  private constructor(
    bidiTransport: BidiTransport,
    cdpConnection: CdpConnection,
    browserCdpClient: CdpClient,
    selfTargetId: string,
    defaultUserContextId: Browser.UserContext,
    options?: MapperOptions,
    parser?: BidiCommandParameterParser,
    logger?: LoggerFn
  ) {
    super();
    this.#logger = logger;
    this.#messageQueue = new ProcessingQueue<OutgoingMessage>(
      this.#processOutgoingMessage,
      this.#logger
    );
    this.#transport = bidiTransport;
    this.#transport.setOnMessage(this.#handleIncomingMessage);
    this.#eventManager = new EventManager(this.#browsingContextStorage);
    this.#commandProcessor = new CommandProcessor(
      cdpConnection,
      browserCdpClient,
      this.#eventManager,
      selfTargetId,
      defaultUserContextId,
      this.#browsingContextStorage,
      new RealmStorage(),
      options?.acceptInsecureCerts ?? false,
      parser,
      this.#logger
    );
    this.#eventManager.on(EventManagerEvents.Event, ({message, event}) => {
      this.emitOutgoingMessage(message, event);
    });
    this.#commandProcessor.on(
      CommandProcessorEvents.Response,
      ({message, event}) => {
        this.emitOutgoingMessage(message, event);
      }
    );
  }

  /**
   * Creates and starts BiDi Mapper instance.
   */
  static async createAndStart(
    bidiTransport: BidiTransport,
    cdpConnection: CdpConnection,
    browserCdpClient: CdpClient,
    selfTargetId: string,
    options?: MapperOptions,
    parser?: BidiCommandParameterParser,
    logger?: LoggerFn
  ): Promise<BidiServer> {
    // The default context is not exposed in Target.getBrowserContexts but can
    // be observed via Target.getTargets. To determine the default browser
    // context, we check which one is mentioned in Target.getTargets and not in
    // Target.getBrowserContexts.
    const [{browserContextIds}, {targetInfos}] = await Promise.all([
      browserCdpClient.sendCommand('Target.getBrowserContexts'),
      browserCdpClient.sendCommand('Target.getTargets'),
    ]);
    let defaultUserContextId = 'default';
    for (const info of targetInfos) {
      if (
        info.browserContextId &&
        !browserContextIds.includes(info.browserContextId)
      ) {
        defaultUserContextId = info.browserContextId;
        break;
      }
    }

    const server = new BidiServer(
      bidiTransport,
      cdpConnection,
      browserCdpClient,
      selfTargetId,
      defaultUserContextId,
      options,
      parser,
      logger
    );
    // Needed to get events about new targets.
    await browserCdpClient.sendCommand('Target.setDiscoverTargets', {
      discover: true,
    });

    // Needed to automatically attach to new targets.
    await browserCdpClient.sendCommand('Target.setAutoAttach', {
      autoAttach: true,
      waitForDebuggerOnStart: true,
      flatten: true,
    });

    await server.#topLevelContextsLoaded();
    return server;
  }

  /**
   * Sends BiDi message.
   */
  emitOutgoingMessage(
    messageEntry: Promise<Result<OutgoingMessage>>,
    event: string
  ): void {
    this.#messageQueue.add(messageEntry, event);
  }

  close() {
    this.#transport.close();
  }

  async #topLevelContextsLoaded() {
    await Promise.all(
      this.#browsingContextStorage
        .getTopLevelContexts()
        .map((c) => c.lifecycleLoaded())
    );
  }
}
