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

import {log, LogType} from '../utils/log';
import {EventEmitter} from '../utils/EventEmitter';

import {ITransport} from '../utils/transport';
import {Message} from './domains/protocol/bidiProtocolTypes';
import {ProcessingQueue} from '../utils/processingQueue';
import ErrorCode = Message.ErrorCode;
import {OutgoingBidiMessage} from './OutgoindBidiMessage';
import {EventManager} from './domains/events/EventManager';
import {CommandProcessor} from './CommandProcessor';
import {CdpConnection} from './CdpConnection';
import {BrowsingContextStorage} from './domains/context/browsingContextStorage';

const logBidi = log(LogType.bidi);

type BidiServerEvents = {
  message: Message.RawCommandRequest;
};

export class BidiServer extends EventEmitter<BidiServerEvents> {
  #messageQueue: ProcessingQueue<OutgoingBidiMessage>;
  #transport: ITransport;
  #commandProcessor: CommandProcessor;

  private constructor(
    bidiTransport: ITransport,
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
    bidiTransport: ITransport,
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

    const messageStr = JSON.stringify(message);
    logBidi('sent > ' + messageStr);
    await this.#transport.sendMessage(messageStr);
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

  #handleIncomingMessage = async (messageStr: string) => {
    logBidi('received < ' + messageStr);

    let messageObj;
    try {
      messageObj = BidiServer.#parseBidiMessage(messageStr);
    } catch (e: any) {
      // Transport-level error does not provide channel.
      this.#respondWithError(messageStr, 'invalid argument', e.message, null);
      return;
    }

    this.#commandProcessor.processCommand(messageObj);
  };

  #respondWithError(
    plainCommandData: string,
    errorCode: ErrorCode,
    errorMessage: string,
    channel: string | null
  ) {
    const errorResponse = BidiServer.#getErrorResponse(
      plainCommandData,
      errorCode,
      errorMessage
    );
    this.emitOutgoingMessage(
      OutgoingBidiMessage.createResolved(errorResponse, channel)
    );
  }

  static #getJsonType(value: any) {
    if (value === null) {
      return 'null';
    }
    if (Array.isArray(value)) {
      return 'array';
    }
    return typeof value;
  }

  static #getErrorResponse(
    messageStr: string,
    errorCode: ErrorCode,
    errorMessage: string
  ): Message.OutgoingMessage {
    // TODO: this is bizarre per spec. We reparse the payload and
    // extract the ID, regardless of what kind of value it was.
    let messageId = undefined;
    try {
      const messageObj = JSON.parse(messageStr);
      if (
        BidiServer.#getJsonType(messageObj) === 'object' &&
        'id' in messageObj
      ) {
        messageId = messageObj.id;
      }
    } catch {}

    return {
      id: messageId,
      error: errorCode,
      message: errorMessage,
      // TODO: optional stacktrace field.
    };
  }

  static #parseBidiMessage(messageStr: string): Message.RawCommandRequest {
    let messageObj: any;
    try {
      messageObj = JSON.parse(messageStr);
    } catch {
      throw new Error('Cannot parse data as JSON');
    }

    const parsedType = BidiServer.#getJsonType(messageObj);
    if (parsedType !== 'object') {
      throw new Error(`Expected JSON object but got ${parsedType}`);
    }

    // Extract amd validate id, method and params.
    const {id, method, params} = messageObj;

    const idType = BidiServer.#getJsonType(id);
    if (idType !== 'number' || !Number.isInteger(id) || id < 0) {
      // TODO: should uint64_t be the upper limit?
      // https://tools.ietf.org/html/rfc7049#section-2.1
      throw new Error(`Expected unsigned integer but got ${idType}`);
    }

    const methodType = BidiServer.#getJsonType(method);
    if (methodType !== 'string') {
      throw new Error(`Expected string method but got ${methodType}`);
    }

    const paramsType = BidiServer.#getJsonType(params);
    if (paramsType !== 'object') {
      throw new Error(`Expected object params but got ${paramsType}`);
    }

    let channel = messageObj.channel;
    if (channel !== undefined) {
      const channelType = BidiServer.#getJsonType(channel);
      if (channelType !== 'string') {
        throw new Error(`Expected string channel but got ${channelType}`);
      }
      // Empty string channel is considered as no channel provided.
      if (channel === '') {
        channel = undefined;
      }
    }

    return {id, method, params, channel};
  }
}
