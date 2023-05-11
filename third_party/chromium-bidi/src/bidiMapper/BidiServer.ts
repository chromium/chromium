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

import {EventEmitter} from '../utils/EventEmitter.js';
import {LogType, LoggerFn} from '../utils/log.js';
import type {Message} from '../protocol/protocol.js';
import {ProcessingQueue} from '../utils/processingQueue.js';

import {BidiParser, CommandProcessor} from './CommandProcessor.js';
import {BidiTransport} from './BidiTransport.js';
import {BrowsingContextStorage} from './domains/context/browsingContextStorage.js';
import {CdpConnection} from './CdpConnection.js';
import {EventManager} from './domains/events/EventManager.js';
import {OutgoingBidiMessage} from './OutgoingBidiMessage.js';
import {RealmStorage} from './domains/script/realmStorage.js';

type BidiServerEvents = {
  message: Message.RawCommandRequest;
};

export class BidiServer extends EventEmitter<BidiServerEvents> {
  #messageQueue: ProcessingQueue<OutgoingBidiMessage>;
  #transport: BidiTransport;
  #commandProcessor: CommandProcessor;
  #browsingContextStorage: BrowsingContextStorage;
  #realmStorage: RealmStorage;
  #logger?: LoggerFn;

  #handleIncomingMessage = (message: Message.RawCommandRequest) => {
    void this.#commandProcessor.processCommand(message).catch((error) => {
      this.#logger?.(LogType.system, error);
    });
  };

  #processOutgoingMessage = async (messageEntry: OutgoingBidiMessage) => {
    const message = messageEntry.message as any;

    if (messageEntry.channel !== null) {
      message['channel'] = messageEntry.channel;
    }

    await this.#transport.sendMessage(message);
  };

  private constructor(
    bidiTransport: BidiTransport,
    cdpConnection: CdpConnection,
    selfTargetId: string,
    parser?: BidiParser,
    logger?: LoggerFn
  ) {
    super();
    this.#logger = logger;
    this.#browsingContextStorage = new BrowsingContextStorage();
    this.#realmStorage = new RealmStorage();
    this.#messageQueue = new ProcessingQueue<OutgoingBidiMessage>(
      this.#processOutgoingMessage,
      this.#logger
    );
    this.#transport = bidiTransport;
    this.#transport.setOnMessage(this.#handleIncomingMessage);
    this.#commandProcessor = new CommandProcessor(
      this.#realmStorage,
      cdpConnection,
      new EventManager(this),
      selfTargetId,
      parser,
      this.#browsingContextStorage,
      this.#logger
    );
    this.#commandProcessor.on(
      'response',
      (response: Promise<OutgoingBidiMessage>) => {
        this.emitOutgoingMessage(response);
      }
    );
  }

  static async createAndStart(
    bidiTransport: BidiTransport,
    cdpConnection: CdpConnection,
    selfTargetId: string,
    parser?: BidiParser,
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

    await server.topLevelContextsLoaded();
    return server;
  }

  async topLevelContextsLoaded() {
    await Promise.all(
      this.#browsingContextStorage
        .getTopLevelContexts()
        .map((c) => c.awaitLoaded())
    );
  }

  /**
   * Sends BiDi message.
   */
  emitOutgoingMessage(messageEntry: Promise<OutgoingBidiMessage>): void {
    this.#messageQueue.add(messageEntry);
  }

  close() {
    this.#transport.close();
  }

  getBrowsingContextStorage(): BrowsingContextStorage {
    return this.#browsingContextStorage;
  }
}
