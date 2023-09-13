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
import type Protocol from 'devtools-protocol';

import type {ITransport} from '../utils/transport.js';
import {LogType} from '../utils/log.js';
import type {LoggerFn} from '../utils/log.js';

import {CloseError, CdpClient, type ICdpClient} from './CdpClient.js';
import type {CdpMessage} from './cdpMessage.js';

interface CdpCallbacks {
  resolve: (result: CdpMessage<any>['result']) => void;
  reject: (error: object) => void;
  error: Error;
}

export interface ICdpConnection {
  browserClient(): ICdpClient;
  getCdpClient(sessionId: Protocol.Target.SessionID): ICdpClient;
}

/**
 * Represents a high-level CDP connection to the browser backend.
 *
 * Manages all CdpClients (each backed by a Session ID) instance for each active
 * CDP session.
 */
export class CdpConnection implements ICdpConnection {
  static readonly LOGGER_PREFIX_RECV = `${LogType.cdp}:RECV ◂` as const;
  static readonly LOGGER_PREFIX_SEND = `${LogType.cdp}:SEND ▸` as const;

  readonly #transport: ITransport;

  /** The CdpClient object attached to the root browser session. */
  readonly #browserCdpClient: CdpClient;

  /** Map from session ID to CdpClient. */
  readonly #sessionCdpClients = new Map<Protocol.Target.SessionID, CdpClient>();

  readonly #commandCallbacks = new Map<number, CdpCallbacks>();
  readonly #logger?: LoggerFn;
  #nextId = 0;

  constructor(transport: ITransport, logger?: LoggerFn) {
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
  getCdpClient(sessionId: Protocol.Target.SessionID): CdpClient {
    const cdpClient = this.#sessionCdpClients.get(sessionId);
    if (!cdpClient) {
      throw new Error(`Unknown CDP session ID: ${sessionId}`);
    }
    return cdpClient;
  }

  sendCommand<CdpMethod extends keyof ProtocolMapping.Commands>(
    method: CdpMethod,
    params?: ProtocolMapping.Commands[CdpMethod]['paramsType'][0],
    sessionId?: Protocol.Target.SessionID
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

      void this.#transport
        .sendMessage(JSON.stringify(cdpMessage))
        ?.catch((error) => {
          this.#logger?.(LogType.debugError, error);
          this.#transport.close();
        });
      this.#logger?.(CdpConnection.LOGGER_PREFIX_SEND, cdpMessage);
    });
  }

  #onMessage = (json: string) => {
    const message: CdpMessage<any> = JSON.parse(json);
    this.#logger?.(CdpConnection.LOGGER_PREFIX_RECV, message);

    // Update client map if a session is attached
    // Listen for these events on every session.
    if (message.method === 'Target.attachedToTarget') {
      const {sessionId} = message.params;
      this.#sessionCdpClients.set(sessionId, new CdpClient(this, sessionId));
    }

    if (message.id !== undefined) {
      // Handle command response.
      const callbacks = this.#commandCallbacks.get(message.id);
      this.#commandCallbacks.delete(message.id);
      if (callbacks) {
        if (message.result) {
          callbacks.resolve(message.result);
        } else if (message.error) {
          callbacks.reject(message.error);
        }
      }
    } else if (message.method) {
      const client = message.sessionId
        ? this.#sessionCdpClients.get(message.sessionId)
        : this.#browserCdpClient;
      client?.emit(message.method, message.params || {});

      // Update client map if a session is detached
      // But emit on that session
      if (message.method === 'Target.detachedFromTarget') {
        const {sessionId} = message.params;
        const client = this.#sessionCdpClients.get(sessionId);
        if (client) {
          this.#sessionCdpClients.delete(sessionId);
          client.removeAllListeners();
        }
      }
    }
  };
}
