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

import {EventEmitter} from 'events';
import {CdpConnection} from './cdpConnection';

import type {ProtocolMapping} from 'devtools-protocol/types/protocol-mapping.js';

export interface CdpClient {
  on<K extends keyof ProtocolMapping.Events>(
    eventName: K,
    handler: (...params: ProtocolMapping.Events[K]) => void
  ): EventEmitter;
  on(
    eventName: 'event',
    handler: (method: keyof ProtocolMapping.Events, ...params: any) => void
  ): EventEmitter;
  removeListener<K extends keyof ProtocolMapping.Events>(
    eventName: K,
    handler: (...params: ProtocolMapping.Events[K]) => void
  ): EventEmitter;
  removeListener(
    eventName: 'event',
    handler: (method: keyof ProtocolMapping.Events, ...params: any) => void
  ): EventEmitter;
  emit<K extends keyof ProtocolMapping.Events>(
    eventName: K,
    ...args: ProtocolMapping.Events[K]
  ): void;
  emit<K extends keyof ProtocolMapping.Events>(
    eventName: 'event',
    methodName: K,
    ...args: ProtocolMapping.Events[K]
  ): void;
  sendCommand<T extends keyof ProtocolMapping.Commands>(
    method: T,
    ...params: ProtocolMapping.Commands[T]['paramsType']
  ): Promise<ProtocolMapping.Commands[T]['returnType']>;
}

class CdpClientImpl extends EventEmitter implements CdpClient {
  constructor(
    private _cdpConnection: CdpConnection,
    private _sessionId: string | null
  ) {
    super();
  }

  /**
   * Returns command promise, which will be resolved wth the command result after receiving CDP result.
   * @param method Name of the CDP command to call.
   * @param params Parameters to pass to the CDP command.
   */
  sendCommand<T extends keyof ProtocolMapping.Commands>(
    method: T,
    ...params: ProtocolMapping.Commands[T]['paramsType']
  ): Promise<ProtocolMapping.Commands[T]['returnType']> {
    const param = params[0];
    return this._cdpConnection.sendCommand(method, param, this._sessionId);
  }
}

/**
 * Creates a new CDP client object that communicates with the browser using a given
 * transport mechanism.
 * @param transport A transport object that will be used to send and receive raw CDP messages.
 * @returns A connected CDP client object.
 */
export function createClient(
  cdpConnection: CdpConnection,
  sessionId: string | null
): CdpClient {
  return new CdpClientImpl(cdpConnection, sessionId);
}
