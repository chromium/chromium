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

import type {ITransport} from '../utils/transport.js';

import {CloseError, CdpClient, ICdpClient} from './cdpClient.js';
import {CdpMessage} from './cdpMessage.js';

interface CdpCallbacks {
  resolve: (result: CdpMessage<any>['result']) => void;
  reject: (error: object) => void;
  error: Error;
}

export interface ICdpConnection {
  browserClient(): ICdpClient;
  getCdpClient(sessionId: string): ICdpClient;
}

/**
 * Represents a high-level CDP connection to the browser backend.
 * Manages a CdpClient instance for each active CDP session.
 */
export class CdpConnection implements ICdpConnection {
  readonly #transport: ITransport;

  /** The CdpClient object attached to the root browser session. */
  readonly #browserCdpClient: CdpClient;
  /** Map from session ID to CdpClient. */
  readonly #sessionCdpClients = new Map<string, CdpClient>();
  readonly #commandCallbacks = new Map<number, CdpCallbacks>();
  readonly #logger?: (...messages: unknown[]) => void;
  #nextId = 0;

  constructor(
    transport: ITransport,
    logger?: (...messages: unknown[]) => void
  ) {
    this.#transport = transport;
    this.#logger = logger;
    this.#transport.setOnMessage(this.#onMessage);
    this.#browserCdpClient = new CdpClient(this, undefined);
  }

  /** Closes the connection to the browser. */
  close() {
    this.#transport.close();
    for (const [, {reject, error}] of this.#commandCallbacks) {
      reject(error);
    }
    this.#commandCallbacks.clear();
    this.#sessionCdpClients.clear();
  }

  /** The CdpClient object attached to the root browser session. */
  browserClient(): CdpClient {
    return this.#browserCdpClient;
  }

  /**
   * Gets a CdpClient instance attached to the given session ID,
   * or null if the session is not attached.
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
    params?: ProtocolMapping.Commands[CdpMethod]['paramsType'][0],
    sessionId?: string
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
      const cdpMessage: CdpMessage<CdpMethod> = {id, method, params};
      if (sessionId) {
        cdpMessage.sessionId = sessionId;
      }

      const cdpMessageStr = JSON.stringify(cdpMessage);
      void this.#transport.sendMessage(cdpMessageStr)?.catch((error) => {
        this.#logger?.('error', error);
        this.#transport.close();
      });
      this.#logger?.('sent ▸', JSON.stringify(cdpMessage, null, 2));
    });
  }

  #onMessage = (message: string) => {
    const messageParsed: CdpMessage<any> = JSON.parse(message);
    const messagePretty = JSON.stringify(messageParsed, null, 2);
    this.#logger?.('received ◂', messagePretty);

    // Update client map if a session is attached or detached.
    // Listen for these events on every session.
    if (messageParsed.method === 'Target.attachedToTarget') {
      const {sessionId} = messageParsed.params;
      this.#sessionCdpClients.set(sessionId, new CdpClient(this, sessionId));
    } else if (messageParsed.method === 'Target.detachedFromTarget') {
      const {sessionId} = messageParsed.params;
      const client = this.#sessionCdpClients.get(sessionId);
      if (client) {
        this.#sessionCdpClients.delete(sessionId);
      }
    }

    if (messageParsed.id !== undefined) {
      // Handle command response.
      const callbacks = this.#commandCallbacks.get(messageParsed.id);
      this.#commandCallbacks.delete(messageParsed.id);
      if (callbacks) {
        if (messageParsed.result) {
          callbacks.resolve(messageParsed.result);
        } else if (messageParsed.error) {
          callbacks.reject(messageParsed.error);
        }
      }
    } else if (messageParsed.method) {
      const client = messageParsed.sessionId
        ? this.#sessionCdpClients.get(messageParsed.sessionId)
        : this.#browserCdpClient;
      client?.emit(messageParsed.method, messageParsed.params || {});
    }
  };
}
