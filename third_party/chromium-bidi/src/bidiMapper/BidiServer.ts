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
import type {ChromiumBidi} from '../protocol/protocol.js';
import {EventEmitter} from '../utils/EventEmitter.js';
import {LogType, type LoggerFn} from '../utils/log.js';
import {ProcessingQueue} from '../utils/processingQueue.js';
import type {Result} from '../utils/result.js';

import type {IBidiParser} from './BidiParser.js';
import type {IBidiTransport} from './BidiTransport.js';
import {CommandProcessor} from './CommandProcessor.js';
import type {OutgoingBidiMessage} from './OutgoingBidiMessage.js';
import {BrowsingContextStorage} from './domains/context/browsingContextStorage.js';
import {EventManager} from './domains/events/EventManager.js';
import {RealmStorage} from './domains/script/realmStorage.js';

type BidiServerEvent = {
  message: ChromiumBidi.Command;
};

export class BidiServer extends EventEmitter<BidiServerEvent> {
  #messageQueue: ProcessingQueue<OutgoingBidiMessage>;
  #transport: IBidiTransport;
  #commandProcessor: CommandProcessor;
  #browsingContextStorage = new BrowsingContextStorage();
  #logger?: LoggerFn;

  #handleIncomingMessage = (message: ChromiumBidi.Command) => {
    void this.#commandProcessor.processCommand(message).catch((error) => {
      this.#logger?.(LogType.debug, error);
    });
  };

  #processOutgoingMessage = async (messageEntry: OutgoingBidiMessage) => {
    const message = messageEntry.message;

    if (messageEntry.channel !== null) {
      message['channel'] = messageEntry.channel;
    }

    await this.#transport.sendMessage(message);
  };

  private constructor(
    bidiTransport: IBidiTransport,
    cdpConnection: ICdpConnection,
    selfTargetId: string,
    parser?: IBidiParser,
    logger?: LoggerFn
  ) {
    super();
    this.#logger = logger;
    this.#messageQueue = new ProcessingQueue<OutgoingBidiMessage>(
      this.#processOutgoingMessage,
      this.#logger
    );
    this.#transport = bidiTransport;
    this.#transport.setOnMessage(this.#handleIncomingMessage);
    this.#commandProcessor = new CommandProcessor(
      cdpConnection,
      new EventManager(this),
      selfTargetId,
      this.#browsingContextStorage,
      new RealmStorage(),
      parser,
      this.#logger
    );
    this.#commandProcessor.on(
      'response',
      (response: Promise<Result<OutgoingBidiMessage>>) => {
        this.emitOutgoingMessage(response);
      }
    );
  }

  static async createAndStart(
    bidiTransport: IBidiTransport,
    cdpConnection: ICdpConnection,
    selfTargetId: string,
    parser?: IBidiParser,
    logger?: LoggerFn
  ): Promise<BidiServer> {
    const server = new BidiServer(
      bidiTransport,
      cdpConnection,
      selfTargetId,
      parser,
      logger
    );
    const cdpClient = cdpConnection.browserClient();

    // Needed to get events about new targets.
    await cdpClient.sendCommand('Target.setDiscoverTargets', {discover: true});

    // Needed to automatically attach to new targets.
    await cdpClient.sendCommand('Target.setAutoAttach', {
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
    messageEntry: Promise<Result<OutgoingBidiMessage>>
  ): void {
    this.#messageQueue.add(messageEntry);
  }

  close() {
    this.#transport.close();
  }

  getBrowsingContextStorage(): BrowsingContextStorage {
    return this.#browsingContextStorage;
  }

  async #topLevelContextsLoaded() {
    await Promise.all(
      this.#browsingContextStorage
        .getTopLevelContexts()
        .map((c) => c.lifecycleLoaded())
    );
  }
}
