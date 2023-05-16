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

import {EventEmitter} from '../utils/EventEmitter.js';

import {CdpConnection, CloseError} from './cdpConnection.js';

type Mapping = {
  [Property in keyof ProtocolMapping.Events]: ProtocolMapping.Events[Property][0];
};

export class CdpClient extends EventEmitter<Mapping> {
  #cdpConnection: CdpConnection;
  #sessionId: string | null;

  constructor(cdpConnection: CdpConnection, sessionId: string | null) {
    super();
    this.#cdpConnection = cdpConnection;
    this.#sessionId = sessionId;
  }

  /**
   * Creates a new CDP client object that communicates with the browser using a given
   * transport mechanism.
   * @param transport A transport object that will be used to send and receive raw CDP messages.
   * @return A connected CDP client object.
   */
  static create(
    cdpConnection: CdpConnection,
    sessionId: string | null
  ): CdpClient {
    return new CdpClient(cdpConnection, sessionId);
  }

  /**
   * Returns command promise, which will be resolved with the command result after receiving CDP result.
   * @param method Name of the CDP command to call.
   * @param params Parameters to pass to the CDP command.
   */
  sendCommand<CdpMethod extends keyof ProtocolMapping.Commands>(
    method: CdpMethod,
    ...params: ProtocolMapping.Commands[CdpMethod]['paramsType']
  ): Promise<ProtocolMapping.Commands[CdpMethod]['returnType']> {
    const param = params[0];
    return this.#cdpConnection.sendCommand(method, param, this.#sessionId);
  }

  isCloseError(error: unknown): boolean {
    return error instanceof CloseError;
  }
}
