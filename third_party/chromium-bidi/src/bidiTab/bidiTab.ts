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

import {
  BidiServer,
  OutgoingBidiMessage,
  type BidiTransport,
} from '../bidiMapper/bidiMapper.js';
import {CdpConnection} from '../cdp/cdpConnection.js';
import {
  ErrorCode,
  type ChromiumBidi,
  type ErrorResponse,
} from '../protocol/protocol.js';
import {LogType} from '../utils/log.js';
import type {ITransport} from '../utils/transport.js';

import {BidiParserImpl} from './BidiParserImpl';
import {generatePage, log} from './mapperTabPage.js';

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

    // Set from the server side if verbose logging is required.
    sendDebugMessage?: ((message: string) => void) | null;

    // `window.setSelfTargetId` is called via `Runtime.evaluate` from the server side.
    setSelfTargetId: (targetId: string) => void;
  }
}

// Initiate `setSelfTargetId` as soon as possible to prevent race condition.
const waitSelfTargetIdPromise = waitSelfTargetId();

void (async () => {
  generatePage();

  // Needed to filter out info related to BiDi target.
  const selfTargetId = await waitSelfTargetIdPromise;

  const bidiServer = await createBidiServer(selfTargetId);

  log(LogType.debug, 'Launched');

  bidiServer.emitOutgoingMessage(
    OutgoingBidiMessage.createResolved({
      launched: true,
    })
  );
})();

function createCdpConnection() {
  /**
   * A CdpTransport implementation that uses the window.cdp bindings
   * injected by Target.exposeDevToolsProtocol.
   */
  class WindowCdpTransport implements ITransport {
    #onMessage: ((message: string) => void) | null = null;

    constructor() {
      window.cdp.onmessage = (message: string) => {
        this.#onMessage?.call(null, message);
      };
    }

    setOnMessage(onMessage: Parameters<ITransport['setOnMessage']>[0]) {
      this.#onMessage = onMessage;
    }

    sendMessage(message: string) {
      window.cdp.send(message);
    }

    close() {
      this.#onMessage = null;
      window.cdp.onmessage = null;
    }
  }

  return new CdpConnection(new WindowCdpTransport(), log);
}

function createBidiServer(selfTargetId: string) {
  class WindowBidiTransport implements BidiTransport {
    #onMessage: ((message: ChromiumBidi.Command) => void) | null = null;

    constructor() {
      window.onBidiMessage = (messageStr: string) => {
        log(`${LogType.bidi}:RECV ◂`, messageStr);
        let messageObject: ChromiumBidi.Command;
        try {
          messageObject = WindowBidiTransport.#parseBidiMessage(messageStr);
        } catch (e: any) {
          // Transport-level error does not provide channel.
          this.#respondWithError(
            messageStr,
            ErrorCode.InvalidArgument,
            e.message,
            null
          );
          return;
        }
        this.#onMessage?.call(null, messageObject);
      };
    }

    setOnMessage(onMessage: Parameters<BidiTransport['setOnMessage']>[0]) {
      this.#onMessage = onMessage;
    }

    sendMessage(message: ChromiumBidi.Message) {
      const messageStr = JSON.stringify(message);
      window.sendBidiResponse(messageStr);
      log(`${LogType.bidi}:SEND ▸`, messageStr);
    }

    close() {
      this.#onMessage = null;
      window.onBidiMessage = null;
    }

    #respondWithError(
      plainCommandData: string,
      errorCode: ErrorCode,
      errorMessage: string,
      channel: string | null
    ) {
      const errorResponse = WindowBidiTransport.#getErrorResponse(
        plainCommandData,
        errorCode,
        errorMessage
      );

      if (channel) {
        // XXX: get rid of any, same code existed in BidiServer.
        this.sendMessage({
          ...errorResponse,
          channel,
        });
      } else {
        this.sendMessage(errorResponse);
      }
    }

    static #getJsonType(value: unknown) {
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
    ): ErrorResponse {
      // XXX: this is bizarre per spec. We reparse the payload and
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
        type: 'error',
        id: messageId,
        error: errorCode,
        message: errorMessage,
        // XXX: optional stacktrace field.
      };
    }

    static #parseBidiMessage(messageStr: string): ChromiumBidi.Command {
      let messageObject: ChromiumBidi.Command;
      try {
        messageObject = JSON.parse(messageStr);
      } catch {
        throw new Error('Cannot parse data as JSON');
      }

      const parsedType = WindowBidiTransport.#getJsonType(messageObject);
      if (parsedType !== 'object') {
        throw new Error(`Expected JSON object but got ${parsedType}`);
      }

      // Extract and validate id, method and params.
      const {id, method, params} = messageObject;

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

      let channel = messageObject.channel;
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

      return {id, method, params, channel} as ChromiumBidi.Command;
    }
  }

  return BidiServer.createAndStart(
    new WindowBidiTransport(),
    createCdpConnection(),
    selfTargetId,
    new BidiParserImpl(),
    log
  );
}

// Needed to filter out info related to BiDi target.
async function waitSelfTargetId(): Promise<string> {
  return new Promise((resolve) => {
    window.setSelfTargetId = (targetId) => {
      log(LogType.debug, 'Current target ID:', targetId);
      resolve(targetId);
    };
  });
}
