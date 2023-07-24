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
import type Protocol from 'devtools-protocol';

import type {ICdpClient} from '../../../cdp/cdpClient.js';
import type {IEventManager} from '../events/EventManager.js';
import {DefaultMap} from '../../../utils/DefaultMap.js';
import type {Network} from '../../../protocol/protocol.js';

import {NetworkRequest} from './networkRequest.js';

export class NetworkManager {
  readonly #eventManager: IEventManager;

  /**
   * Map of request ID to NetworkRequest objects. Needed as long as information
   * about requests comes from different events.
   */
  readonly #requestMap: DefaultMap<Network.Request, NetworkRequest>;

  private constructor(eventManager: IEventManager) {
    this.#eventManager = eventManager;

    this.#requestMap = new DefaultMap(
      (requestId) => new NetworkRequest(requestId, this.#eventManager)
    );
  }

  static create(
    cdpClient: ICdpClient,
    eventManager: IEventManager
  ): NetworkManager {
    const networkProcessor = new NetworkManager(eventManager);

    cdpClient
      .browserClient()
      .on(
        'Target.detachedFromTarget',
        (params: Protocol.Target.DetachedFromTargetEvent) => {
          if (cdpClient.sessionId === params.sessionId) {
            networkProcessor.dispose();
          }
        }
      );

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

    cdpClient.on(
      'Network.requestServedFromCache',
      (params: Protocol.Network.RequestServedFromCacheEvent) => {
        networkProcessor
          .#getOrCreateNetworkRequest(params.requestId)
          .onServedFromCache();
      }
    );

    cdpClient.on(
      'Network.loadingFailed',
      (params: Protocol.Network.LoadingFailedEvent) => {
        networkProcessor
          .#getOrCreateNetworkRequest(params.requestId)
          .onLoadingFailedEvent(params);
        networkProcessor.#forgetRequest(params.requestId);
      }
    );

    cdpClient.on(
      'Network.loadingFinished',
      (params: Protocol.Network.RequestServedFromCacheEvent) => {
        networkProcessor.#forgetRequest(params.requestId);
      }
    );

    return networkProcessor;
  }

  dispose() {
    for (const request of this.#requestMap.values()) {
      request.dispose();
    }

    this.#requestMap.clear();
  }

  #getOrCreateNetworkRequest(requestId: Network.Request): NetworkRequest {
    return this.#requestMap.get(requestId);
  }

  #forgetRequest(requestId: Network.Request): void {
    const request = this.#requestMap.get(requestId);
    if (request) {
      request.dispose();
      this.#requestMap.delete(requestId);
    }
  }
}
