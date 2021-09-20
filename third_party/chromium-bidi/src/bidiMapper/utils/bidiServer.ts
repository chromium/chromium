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

import { log } from '../../utils/log';
const logBidi = log('bidi');

import { EventEmitter } from 'events';

import { ITransport } from '../../utils/transport';

export interface BidiCommandMessage {
  id: number;
  method: string;
  params: object;
}

export interface BidiResponseMessage {
  id: number;
  result?: object;
}

export interface BidiErrorMessage {
  id?: number;
}

export interface BidiEventMessage {
  method: string;
  params?: object;
}

export type BidiOutgoingMessage =
  | BidiResponseMessage
  | BidiErrorMessage
  | BidiEventMessage;

export interface IBidiServer {
  on(event: 'message', handler: (messageObj: BidiCommandMessage) => void): void;
  sendMessage: (messageObj: BidiOutgoingMessage) => Promise<void>;
  close(): void;
}

interface BidiServerEvents {
  message: BidiCommandMessage;
}

export declare interface BidiServer {
  on<U extends keyof BidiServerEvents>(
    event: U,
    listener: (params: BidiServerEvents[U]) => void
  ): this;
  emit<U extends keyof BidiServerEvents>(
    event: U,
    params: BidiServerEvents[U]
  ): boolean;
}

export class BidiServer extends EventEmitter implements IBidiServer {
  constructor(private _transport: ITransport) {
    super();

    this._transport.setOnMessage(this._onBidiMessage);
  }

  /**
   * Sends BiDi message. Returns resolved promise.
   * @param messageObj Message object to be sent. Will be automatically enriched with `id`.
   */
  async sendMessage(messageObj: object): Promise<void> {
    const messageStr = JSON.stringify(messageObj);
    logBidi('sent > ' + messageStr);
    this._transport.sendMessage(messageStr);
  }

  close(): void {
    this._transport.close();
  }

  private _onBidiMessage = async (messageStr: string) => {
    logBidi('received < ' + messageStr);

    let messageObj;
    try {
      messageObj = this._parseBidiMessage(messageStr);
    } catch (e) {
      this._respondWithError(messageStr, 'invalid argument', e.message);
      return;
    }
    this.emit('message', messageObj);
  };

  private _respondWithError(
    plainCommandData: string,
    errorCode: string,
    errorMessage: string
  ) {
    const errorResponse = this._getErrorResponse(
      plainCommandData,
      errorCode,
      errorMessage
    );
    this.sendMessage(errorResponse);
  }

  private _getJsonType(value: any) {
    if (value === null) {
      return 'null';
    }
    if (Array.isArray(value)) {
      return 'array';
    }
    return typeof value;
  }

  private _getErrorResponse(
    messageStr: string,
    errorCode: string,
    errorMessage: string
  ) {
    // TODO: this is bizarre per spec. We reparse the payload and
    // extract the ID, regardless of what kind of value it was.
    let messageId = undefined;
    try {
      const messageObj = JSON.parse(messageStr);
      if (this._getJsonType(messageObj) === 'object' && 'id' in messageObj) {
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

  private _parseBidiMessage(messageStr: string): BidiCommandMessage {
    let messageObj: any;
    try {
      messageObj = JSON.parse(messageStr);
    } catch {
      throw new Error('Cannot parse data as JSON');
    }

    const parsedType = this._getJsonType(messageObj);
    if (parsedType !== 'object') {
      throw new Error(`Expected JSON object but got ${parsedType}`);
    }

    // Extract amd validate id, method and params.
    const { id, method, params } = messageObj;

    const idType = this._getJsonType(id);
    if (idType !== 'number' || !Number.isInteger(id) || id < 0) {
      // TODO: should uint64_t be the upper limit?
      // https://tools.ietf.org/html/rfc7049#section-2.1
      throw new Error(`Expected unsigned integer but got ${idType}`);
    }

    const methodType = this._getJsonType(method);
    if (methodType !== 'string') {
      throw new Error(`Expected string method but got ${methodType}`);
    }

    const paramsType = this._getJsonType(params);
    if (paramsType !== 'object') {
      throw new Error(`Expected object params but got ${paramsType}`);
    }

    return { id, method, params };
  }
}
