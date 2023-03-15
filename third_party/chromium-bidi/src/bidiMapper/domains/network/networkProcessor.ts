/*
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
 * limitations under the License.
 */

/**
 * @fileoverview This file implements the Network domain processor which is
 * responsible for processing Network domain events and redirecting events to
 * related `NetworkRequest`.
 */
import Protocol from 'devtools-protocol';

import {CdpClient} from '../../CdpConnection';
import {IEventManager} from '../events/EventManager';

import {NetworkRequest} from './networkRequest';

export class NetworkProcessor {
  readonly #eventManager: IEventManager;

  /**
   * Map of request ID to NetworkRequest objects. Needed as long as information
   * about requests comes from different events.
   */
  readonly #requestMap = new Map<string, NetworkRequest>();

  private constructor(eventManager: IEventManager) {
    this.#eventManager = eventManager;
  }

  static async create(
    cdpClient: CdpClient,
    eventManager: IEventManager
  ): Promise<NetworkProcessor> {
    const networkProcessor = new NetworkProcessor(eventManager);

    cdpClient.on(
      'Network.requestWillBeSent',
      (params: Protocol.Network.RequestWillBeSentEvent) => {
        networkProcessor
          .#getOrCreateNetworkRequest(params.requestId)
          .onRequestWillBeSentEvent(params);
      }
    );

    cdpClient.on(
      'Network.requestWillBeSentExtraInfo',
      (params: Protocol.Network.RequestWillBeSentExtraInfoEvent) => {
        networkProcessor
          .#getOrCreateNetworkRequest(params.requestId)
          .onRequestWillBeSentExtraInfoEvent(params);
      }
    );

    cdpClient.on(
      'Network.responseReceived',
      (params: Protocol.Network.ResponseReceivedEvent) => {
        networkProcessor
          .#getOrCreateNetworkRequest(params.requestId)
          .onResponseReceivedEvent(params);
      }
    );

    cdpClient.on(
      'Network.responseReceivedExtraInfo',
      (params: Protocol.Network.ResponseReceivedExtraInfoEvent) => {
        networkProcessor
          .#getOrCreateNetworkRequest(params.requestId)
          .onResponseReceivedEventExtraInfo(params);
      }
    );

    await cdpClient.sendCommand('Network.enable');

    return networkProcessor;
  }

  #getOrCreateNetworkRequest(requestId: string): NetworkRequest {
    if (!this.#requestMap.has(requestId)) {
      const networkRequest = new NetworkRequest(requestId, this.#eventManager);
      this.#requestMap.set(requestId, networkRequest);
    }
    return this.#requestMap.get(requestId)!;
  }
}
