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

import {EventEmitter} from '../utils/EventEmitter';

import {BidiTransport} from './BidiTransport';
import type {Message} from '../protocol/types';
import {ProcessingQueue} from '../utils/processingQueue';
import {OutgoingBidiMessage} from './OutgoindBidiMessage';
import {EventManager} from './domains/events/EventManager';
import {CommandProcessor} from './CommandProcessor';
import {CdpConnection} from './CdpConnection';
import {BrowsingContextStorage} from './domains/context/browsingContextStorage';

type BidiServerEvents = {
  message: Message.RawCommandRequest;
};

export class BidiServer extends EventEmitter<BidiServerEvents> {
  #messageQueue: ProcessingQueue<OutgoingBidiMessage>;
  #transport: BidiTransport;
  #commandProcessor: CommandProcessor;

  private constructor(
    bidiTransport: BidiTransport,
    cdpConnection: CdpConnection,
    selfTargetId: string
  ) {
    super();
    this.#messageQueue = new ProcessingQueue<OutgoingBidiMessage>(
      this.#processOutgoingMessage
    );
    this.#transport = bidiTransport;
    this.#transport.setOnMessage(this.#handleIncomingMessage);
    this.#commandProcessor = new CommandProcessor(
      cdpConnection,
      new EventManager(this),
      selfTargetId
    );
    this.#commandProcessor.on(
      'response',
      (response: Promise<OutgoingBidiMessage>) => {
        this.emitOutgoingMessage(response);
      }
    );
  }

  public static async createAndStart(
    bidiTransport: BidiTransport,
    cdpConnection: CdpConnection,
    selfTargetId: string
  ): Promise<BidiServer> {
    const server = new BidiServer(bidiTransport, cdpConnection, selfTargetId);
    const cdpClient = cdpConnection.browserClient();

    // Needed to get events about new targets.
    await cdpClient.sendCommand('Target.setDiscoverTargets', {discover: true});

    // Needed to automatically attach to new targets.
    await cdpClient.sendCommand('Target.setAutoAttach', {
      autoAttach: true,
      waitForDebuggerOnStart: true,
      flatten: true,
    });

    await Promise.all(
      BrowsingContextStorage.getTopLevelContexts().map((c) => c.awaitLoaded())
    );
    return server;
  }

  #processOutgoingMessage = async (messageEntry: OutgoingBidiMessage) => {
    const message = messageEntry.message as any;

    if (messageEntry.channel !== null) {
      message['channel'] = messageEntry.channel;
    }

    await this.#transport.sendMessage(message);
  };

  /**
   * Sends BiDi message.
   */
  emitOutgoingMessage(messageEntry: Promise<OutgoingBidiMessage>): void {
    this.#messageQueue.add(messageEntry);
  }

  close(): void {
    this.#transport.close();
  }

  #handleIncomingMessage = async (message: Message.RawCommandRequest) => {
    this.#commandProcessor.processCommand(message);
  };
}
