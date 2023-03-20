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

import WebSocket from 'ws';

import {ITransport} from './transport.js';

export class WebSocketTransport implements ITransport {
  #onMessage: ((message: string) => void) | null = null;

  #websocket: WebSocket;

  constructor(websocket: WebSocket) {
    this.#websocket = websocket;

    this.#websocket.on('message', (message: string) => {
      this.#onMessage?.(message);
    });
  }

  setOnMessage(onMessage: (message: string) => void) {
    this.#onMessage = onMessage;
  }

  async sendMessage(message: string): Promise<void> {
    this.#websocket.send(message);
  }

  close() {
    this.#onMessage = null;
    this.#websocket.close();
  }
}
