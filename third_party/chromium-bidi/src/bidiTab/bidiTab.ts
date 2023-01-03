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
 *
 * @license
 */

import {CdpConnection} from '../cdp/index.js';
import {BidiServer} from '../bidiMapper/BidiServer.js';
import {BidiTransport} from '../bidiMapper/bidiMapper.js';

import {log, LogType} from '../utils/log.js';
import {MapperTabPage} from './mapperTabPage.js';
import {OutgoingBidiMessage} from '../bidiMapper/OutgoindBidiMessage.js';
import type {
  BrowsingContext,
  CDP,
  Message,
  Script,
  Session,
} from '../protocol/protocol';
import * as Parser from '../protocol-parser/protocol-parser.js';
import {ITransport} from '../utils/transport.js';
import {BidiParser} from '../bidiMapper/CommandProcessor.js';

const logSystem = log(LogType.system);
const logBidi = log(LogType.bidi);

declare global {
  interface Window {
    // `window.cdp` is exposed by `Target.exposeDevToolsProtocol` from the server side.
    // https://chromedevtools.github.io/devtools-protocol/tot/Target/#method-exposeDevToolsProtocol
    cdp: {
      send: (message: string) => void;
      onmessage: ((message: string) => void) | null;
    };

    // `window.sendBidiResponse` is exposed by `Runtime.addBinding` from the server side.
    sendBidiResponse: (response: string) => void;

    // `window.onBidiMessage` is called via `Runtime.evaluate` from the server side.
    onBidiMessage: ((message: string) => void) | null;

    // `window.setSelfTargetId` is called via `Runtime.evaluate` from the server side.
    setSelfTargetId: (targetId: string) => void;
  }
}

// Initiate `setSelfTargetId` as soon as possible to prevent race condition.
const _waitSelfTargetIdPromise = _waitSelfTargetId();

(async () => {
  MapperTabPage.generatePage();

  // Needed to filter out info related to BiDi target.
  const selfTargetId = await _waitSelfTargetIdPromise;

  const bidiServer = await _createBidiServer(selfTargetId);

  logSystem('launched');

  bidiServer.emitOutgoingMessage(
    OutgoingBidiMessage.createResolved({launched: true}, null)
  );
})();

function _createCdpConnection() {
  // A CdpTransport implementation that uses the window.cdp bindings
  // injected by Target.exposeDevToolsProtocol.
  class WindowCdpTransport implements ITransport {
    private _onMessage: ((message: string) => void) | null = null;

    constructor() {
      window.cdp.onmessage = (message: string) => {
        if (this._onMessage) {
          this._onMessage.call(null, message);
        }
      };
    }

    setOnMessage(onMessage: (message: string) => Promise<void>): void {
      this._onMessage = onMessage;
    }

    async sendMessage(message: string): Promise<void> {
      window.cdp.send(message);
    }

    close() {
      this._onMessage = null;
      window.cdp.onmessage = null;
    }
  }

  return new CdpConnection(new WindowCdpTransport(), log(LogType.cdp));
}

async function _createBidiServer(selfTargetId: string) {
  class WindowBidiTransport implements BidiTransport {
    private _onMessage: ((message: Message.RawCommandRequest) => void) | null =
      null;

    constructor() {
      window.onBidiMessage = (messageStr: string) => {
        logBidi('received < ', messageStr);
        let messageObj;
        try {
          messageObj = WindowBidiTransport.#parseBidiMessage(messageStr);
        } catch (e: any) {
          // Transport-level error does not provide channel.
          this.#respondWithError(
            messageStr,
            'invalid argument',
            e.message,
            null
          );
          return;
        }
        if (this._onMessage) {
          this._onMessage.call(null, messageObj);
        }
      };
    }

    setOnMessage(
      onMessage: (message: Message.RawCommandRequest) => Promise<void>
    ): void {
      this._onMessage = onMessage;
    }

    async sendMessage(message: Message.OutgoingMessage): Promise<void> {
      const messageStr = JSON.stringify(message);
      logBidi('sent > ', messageStr);
      window.sendBidiResponse(messageStr);
    }

    close() {
      this._onMessage = null;
      window.onBidiMessage = null;
    }

    #respondWithError(
      plainCommandData: string,
      errorCode: Message.ErrorCode,
      errorMessage: string,
      channel: string | null
    ) {
      const errorResponse = WindowBidiTransport.#getErrorResponse(
        plainCommandData,
        errorCode,
        errorMessage
      );

      if (channel) {
        // TODO: get rid of any, same code existed in BidiServer.
        this.sendMessage({
          ...errorResponse,
          channel,
        } as any);
      } else {
        this.sendMessage(errorResponse);
      }
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
      errorCode: Message.ErrorCode,
      errorMessage: string
    ): Message.OutgoingMessage {
      // TODO: this is bizarre per spec. We reparse the payload and
      // extract the ID, regardless of what kind of value it was.
      let messageId;
      try {
        const messageObj = JSON.parse(messageStr);
        if (
          WindowBidiTransport.#getJsonType(messageObj) === 'object' &&
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

      const parsedType = WindowBidiTransport.#getJsonType(messageObj);
      if (parsedType !== 'object') {
        throw new Error(`Expected JSON object but got ${parsedType}`);
      }

      // Extract amd validate id, method and params.
      const {id, method, params} = messageObj;

      const idType = WindowBidiTransport.#getJsonType(id);
      if (idType !== 'number' || !Number.isInteger(id) || id < 0) {
        // TODO: should uint64_t be the upper limit?
        // https://tools.ietf.org/html/rfc7049#section-2.1
        throw new Error(`Expected unsigned integer but got ${idType}`);
      }

      const methodType = WindowBidiTransport.#getJsonType(method);
      if (methodType !== 'string') {
        throw new Error(`Expected string method but got ${methodType}`);
      }

      const paramsType = WindowBidiTransport.#getJsonType(params);
      if (paramsType !== 'object') {
        throw new Error(`Expected object params but got ${paramsType}`);
      }

      let channel = messageObj.channel;
      if (channel !== undefined) {
        const channelType = WindowBidiTransport.#getJsonType(channel);
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

  return await BidiServer.createAndStart(
    new WindowBidiTransport(),
    _createCdpConnection(),
    selfTargetId,
    new BidiParserImpl()
  );
}

class BidiParserImpl implements BidiParser {
  parseGetRealmsParams(params: object): Script.GetRealmsParameters {
    return Parser.Script.parseGetRealmsParams(params);
  }
  parseCallFunctionParams(params: object): Script.CallFunctionParameters {
    return Parser.Script.parseCallFunctionParams(params);
  }
  parseEvaluateParams(params: object): Script.EvaluateParameters {
    return Parser.Script.parseEvaluateParams(params);
  }
  parseDisownParams(params: object): Script.DisownParameters {
    return Parser.Script.parseDisownParams(params);
  }
  parseSendCommandParams(params: object): CDP.SendCommandParams {
    return Parser.CDP.parseSendCommandParams(params);
  }
  parseGetSessionParams(params: object): CDP.GetSessionParams {
    return Parser.CDP.parseGetSessionParams(params);
  }
  parseNavigateParams(params: object): BrowsingContext.NavigateParameters {
    return Parser.BrowsingContext.parseNavigateParams(params);
  }
  parseGetTreeParams(params: object): BrowsingContext.GetTreeParameters {
    return Parser.BrowsingContext.parseGetTreeParams(params);
  }
  parseSubscribeParams(params: object): Session.SubscribeParameters {
    return Parser.Session.parseSubscribeParams(params);
  }
  parseCreateParams(params: object): BrowsingContext.CreateParameters {
    return Parser.BrowsingContext.parseCreateParams(params);
  }
  parseCloseParams(params: object): BrowsingContext.CloseParameters {
    return Parser.BrowsingContext.parseCloseParams(params);
  }
}

// Needed to filter out info related to BiDi target.
async function _waitSelfTargetId(): Promise<string> {
  return await new Promise((resolve) => {
    window.setSelfTargetId = (targetId) => {
      logSystem('current target ID: ' + targetId);
      resolve(targetId);
    };
  });
}
