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
import type {ProtocolMapping} from 'devtools-protocol/types/protocol-mapping.js';

import {ITransport} from '../utils/transport.js';

import {CdpClient} from './cdpClient.js';
import {CdpMessage} from './cdpMessage.js';

interface CdpCallbacks {
  resolve: (messageObj: object) => void;
  reject: (errorObj: object) => void;
  error: Error;
}

/**
 * A error that will be thrown if when the connection is closed
 */
export class CloseError extends Error {}

/**
 * Represents a high-level CDP connection to the browser backend.
 * Manages a CdpClient instance for each active CDP session.
 */
export class CdpConnection {
  readonly #transport: ITransport;
  readonly #browserCdpClient: CdpClient;
  /** Map from session ID to CdpClient. */
  readonly #sessionCdpClients = new Map<string, CdpClient>();
  readonly #commandCallbacks = new Map<number, CdpCallbacks>();
  readonly #log: (...messages: unknown[]) => void;
  #nextId = 0;

  constructor(
    transport: ITransport,
    log: (...messages: unknown[]) => void = () => {}
  ) {
    this.#transport = transport;
    this.#log = log;
    this.#transport.setOnMessage(this.#onMessage);
    this.#browserCdpClient = CdpClient.create(this, null);
  }

  /**
   * Closes the connection to the browser.
   */
  close() {
    this.#transport.close();
    for (const [, {reject, error}] of this.#commandCallbacks) {
      reject(error);
    }
    this.#commandCallbacks.clear();
    this.#sessionCdpClients.clear();
  }

  /**
   * @return The CdpClient object attached to the root browser session.
   */
  browserClient(): CdpClient {
    return this.#browserCdpClient;
  }

  /**
   * Gets a CdpClient instance by sessionId.
   * @param sessionId The sessionId of the CdpClient to retrieve.
   * @return The CdpClient object attached to the given session, or null if the session is not attached.
   */
  getCdpClient(sessionId: string): CdpClient {
    const cdpClient = this.#sessionCdpClients.get(sessionId);
    if (!cdpClient) {
      throw new Error('Unknown CDP session ID');
    }
    return cdpClient;
  }

  sendCommand<CdpMethod extends keyof ProtocolMapping.Commands>(
    method: CdpMethod,
    params: ProtocolMapping.Commands[CdpMethod]['paramsType'][0] | undefined,
    sessionId: string | null
  ): Promise<object> {
    return new Promise((resolve, reject) => {
      const id = this.#nextId++;
      this.#commandCallbacks.set(id, {
        resolve,
        reject,
        error: new CloseError(
          `${method} ${JSON.stringify(params)} ${
            sessionId ?? ''
          } call rejected because the connection has been closed.`
        ),
      });
      const messageObj: CdpMessage<CdpMethod> = {id, method, params};
      if (sessionId) {
        messageObj.sessionId = sessionId;
      }

      const messageStr = JSON.stringify(messageObj);
      const messagePretty = JSON.stringify(messageObj, null, 2);
      void this.#transport.sendMessage(messageStr)?.catch((error) => {
        this.#log('error', error);
        this.#transport.close();
      });
      this.#log('sent ▸', messagePretty);
    });
  }

  #onMessage = (message: string) => {
    const parsed = JSON.parse(message);
    const messagePretty = JSON.stringify(parsed, null, 2);
    this.#log('received ◂', messagePretty);

    // Update client map if a session is attached or detached.
    // Listen for these events on every session.
    if (parsed.method === 'Target.attachedToTarget') {
      const {sessionId} = parsed.params;
      this.#sessionCdpClients.set(sessionId, CdpClient.create(this, sessionId));
    } else if (parsed.method === 'Target.detachedFromTarget') {
      const {sessionId} = parsed.params;
      const client = this.#sessionCdpClients.get(sessionId);
      if (client) {
        this.#sessionCdpClients.delete(sessionId);
      }
    }

    if (parsed.id !== undefined) {
      // Handle command response.
      const callbacks = this.#commandCallbacks.get(parsed.id);
      this.#commandCallbacks.delete(parsed.id);
      if (callbacks) {
        if (parsed.result) {
          callbacks.resolve(parsed.result);
        } else if (parsed.error) {
          callbacks.reject(parsed.error);
        }
      }
    } else if (parsed.method) {
      const client = parsed.sessionId
        ? this.#sessionCdpClients.get(parsed.sessionId)
        : this.#browserCdpClient;
      if (client) {
        client.emit(parsed.method, parsed.params || {});
      }
    }
  };
}
