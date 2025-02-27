/*
 * Copyright 2025 Google LLC.
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
 */
import debug from 'debug';

import {assert} from '../utils/assert.js';
import type {Transport} from '../utils/transport.js';

const debugInternal = debug('bidi:server:internal');

export class PipeTransport implements Transport {
  #pipeWrite: NodeJS.WritableStream;
  #onMessage: ((message: string) => void) | null = null;

  #isClosed = false;
  #pendingMessage = '';

  constructor(
    pipeWrite: NodeJS.WritableStream,
    pipeRead: NodeJS.ReadableStream,
  ) {
    this.#pipeWrite = pipeWrite;

    pipeRead.on('data', (chunk) => {
      return this.#dispatch(chunk);
    });
    pipeRead.on('close', () => {
      this.close();
    });
    pipeRead.on('error', (error) => {
      debugInternal('Pipe read error: ', error);
      this.close();
    });
    pipeWrite.on('error', (error) => {
      debugInternal('Pipe read error: ', error);
      this.close();
    });
  }

  setOnMessage(onMessage: (message: string) => void) {
    this.#onMessage = onMessage;
  }
  sendMessage(message: string) {
    assert(!this.#isClosed, '`PipeTransport` is closed.');

    this.#pipeWrite.write(message);
    this.#pipeWrite.write('\0');
  }

  #dispatch(buffer: Buffer): void {
    assert(!this.#isClosed, '`PipeTransport` is closed.');

    let end = buffer.indexOf('\0');
    if (end === -1) {
      this.#pendingMessage += buffer.toString();
      return;
    }
    const message = this.#pendingMessage + buffer.toString(undefined, 0, end);
    if (this.#onMessage) {
      this.#onMessage.call(null, message);
    }

    let start = end + 1;
    end = buffer.indexOf('\0', start);
    while (end !== -1) {
      if (this.#onMessage) {
        this.#onMessage.call(null, buffer.toString(undefined, start, end));
      }
      start = end + 1;
      end = buffer.indexOf('\0', start);
    }
    this.#pendingMessage = buffer.toString(undefined, start);
  }

  close(): void {
    debugInternal('Closing pipe');
    this.#isClosed = true;
  }
}
