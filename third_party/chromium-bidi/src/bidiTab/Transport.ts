/**
 * Copyright 2023 Google LLC.
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
 * limitations under the License. *
 */
import type {BidiTransport} from '../bidiMapper/BidiMapper.js';
import type {BidiPlusChannel} from '../protocol/chromium-bidi.js';
import {
  type ChromiumBidi,
  ErrorCode,
  type ErrorResponse,
} from '../protocol/protocol.js';
import {LogType} from '../utils/log.js';
import type {Transport} from '../utils/transport.js';

import {log} from './mapperTabPage.js';

export class WindowBidiTransport implements BidiTransport {
  static readonly LOGGER_PREFIX_RECV = `${LogType.bidi}:RECV ◂` as const;
  static readonly LOGGER_PREFIX_SEND = `${LogType.bidi}:SEND ▸` as const;
  static readonly LOGGER_PREFIX_WARN = LogType.debugWarn;

  #onMessage: ((message: ChromiumBidi.Command) => void) | null = null;

  constructor() {
    window.onBidiMessage = (message: string) => {
      log(WindowBidiTransport.LOGGER_PREFIX_RECV, message);
      try {
        const command = WindowBidiTransport.#parseBidiMessage(message);
        this.#onMessage?.call(null, command);
      } catch (e: unknown) {
        const error = e instanceof Error ? e : new Error(e as string);
        // Transport-level error does not provide channel.
        this.#respondWithError(message, ErrorCode.InvalidArgument, error, {});
      }
    };
  }

  setOnMessage(onMessage: Parameters<BidiTransport['setOnMessage']>[0]) {
    this.#onMessage = onMessage;
  }

  sendMessage(message: ChromiumBidi.Message) {
    log(WindowBidiTransport.LOGGER_PREFIX_SEND, message);
    const json = JSON.stringify(message);
    window.sendBidiResponse(json);
  }

  close() {
    this.#onMessage = null;
    window.onBidiMessage = null;
  }

  #respondWithError(
    plainCommandData: string,
    errorCode: ErrorCode,
    error: Error,
    channel: BidiPlusChannel,
  ) {
    const errorResponse = WindowBidiTransport.#getErrorResponse(
      plainCommandData,
      errorCode,
      error,
    );

    this.sendMessage({
      ...errorResponse,
      ...(channel ?? {}),
    });
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
    message: string,
    errorCode: ErrorCode,
    error: Error,
  ): ErrorResponse {
    // XXX: this is bizarre per spec. We reparse the payload and
    // extract the ID, regardless of what kind of value it was.
    let messageId;
    try {
      const command = JSON.parse(message);
      if (
        WindowBidiTransport.#getJsonType(command) === 'object' &&
        'id' in command
      ) {
        messageId = command.id;
      }
    } catch {}

    return {
      type: 'error',
      id: messageId,
      error: errorCode,
      message: error.message,
    };
  }

  static #parseBidiMessage(message: string): ChromiumBidi.Command {
    let command: {
      id?: unknown;
      method?: unknown;
      params?: unknown;
      channel?: unknown;
      'goog:channel'?: unknown;
    };
    try {
      command = JSON.parse(message);
    } catch {
      throw new Error('Cannot parse data as JSON');
    }

    const type = WindowBidiTransport.#getJsonType(command);
    if (type !== 'object') {
      throw new Error(`Expected JSON object but got ${type}`);
    }

    // Extract and validate id, method and params.
    const {id, method, params} = command;

    const idType = WindowBidiTransport.#getJsonType(id);
    if (idType !== 'number' || !Number.isInteger(id) || (id as number) < 0) {
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

    let channel: BidiPlusChannel = {};
    if (command['goog:channel'] !== undefined) {
      const channelType = WindowBidiTransport.#getJsonType(
        command['goog:channel'],
      );
      if (channelType !== 'string') {
        throw new Error(
          `Expected string value of 'goog:channel' but got ${channelType}`,
        );
      }
      if (command['goog:channel'] !== '') {
        channel = {'goog:channel': command['goog:channel'] as string};
      }
    } else if (command.channel !== undefined) {
      log(
        WindowBidiTransport.LOGGER_PREFIX_WARN,
        'Legacy `channel` parameter is deprecated and will not supported soon. Use `goog:channel` instead.',
      );
      const channelType = WindowBidiTransport.#getJsonType(command.channel);
      if (channelType !== 'string') {
        throw new Error(`Expected string 'channel' but got ${channelType}`);
      }
      if (command.channel !== '') {
        channel = {channel: command.channel as string};
      }
    }

    return {id, method, params, channel} as ChromiumBidi.Command;
  }
}

export class WindowCdpTransport implements Transport {
  #onMessage: ((message: string) => void) | null = null;
  #cdpSend: typeof window.cdp.send;

  constructor() {
    this.#cdpSend = window.cdp.send;
    // @ts-expect-error removing cdp
    window.cdp.send = undefined;
    window.cdp.onmessage = (message: string) => {
      this.#onMessage?.call(null, message);
    };
  }

  setOnMessage(onMessage: Parameters<Transport['setOnMessage']>[0]) {
    this.#onMessage = onMessage;
  }

  sendMessage(message: string) {
    this.#cdpSend(message);
  }

  close() {
    this.#onMessage = null;
    window.cdp.onmessage = null;
  }
}
