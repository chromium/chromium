/**
 * Copyright 2021 Google Inc. All rights reserved.
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

import { ServerBinding, AbstractServer } from './iServer';
import { log } from './log';
const logBidi = log('bidi');

export class BidiServer extends AbstractServer {
  private _bidiBindings: ServerBinding;

  constructor(bidiBindings: ServerBinding) {
    super(bidiBindings);
    this._bidiBindings = bidiBindings;
    this._bidiBindings.onmessage = (messageStr: string) => {
      this._onBidiMessage(messageStr);
    };
  }

  /**
   * Sends BiDi message. Returns resolved promise.
   * @param messageObj Message object to be sent. Will be automatically enriched with `id`.
   */
  sendMessage(messageObj: any): Promise<any> {
    const messageStr = JSON.stringify(messageObj);
    logBidi('sent > ' + messageStr);
    this._bidiBindings.sendMessage(messageStr);

    return Promise.resolve();
  }

  private _onBidiMessage(messageStr: string): void {
    logBidi('received < ' + messageStr);

    let messageObj;
    try {
      messageObj = this._parseBidiMessage(messageStr);
    } catch (e) {
      this._respondWithError(messageStr, 'invalid argument', e.message);
      return;
    }
    this.notifySubscribersOnMessage(messageObj);
  }

  private _respondWithError(plainCommandData, errorCode, errorMessage) {
    const errorResponse = this._getErrorResponse(
      plainCommandData,
      errorCode,
      errorMessage
    );
    this.sendMessage(errorResponse);
  }

  private _getJsonType(value) {
    if (value === null) {
      return 'null';
    }
    if (Array.isArray(value)) {
      return 'array';
    }
    return typeof value;
  }

  private _getErrorResponse(messageStr, errorCode, errorMessage) {
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

  private _parseBidiMessage(messageStr: string) {
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
