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
 * limitations under the License.
 */
import type {Protocol} from 'devtools-protocol';

import {Network, NoSuchInterceptException} from '../../../protocol/protocol.js';
import {uuidv4} from '../../../utils/uuid.js';
import type {CdpClient} from '../../BidiMapper.js';
import type {CdpTarget} from '../context/CdpTarget.js';
import type {EventManager} from '../session/EventManager.js';

import {NetworkRequest} from './NetworkRequest.js';
import {matchUrlPattern} from './NetworkUtils.js';

interface NetworkInterception {
  urlPatterns: Network.UrlPattern[];
  phases: Network.AddInterceptParameters['phases'];
}

/** Stores network and intercept maps. */
export class NetworkStorage {
  #eventManager: EventManager;

  readonly #targets = new Set<CdpTarget>();
  /**
   * A map from network request ID to Network Request objects.
   * Needed as long as information about requests comes from different events.
   */
  readonly #requests = new Map<Network.Request, NetworkRequest>();

  /** A map from intercept ID to track active network intercepts. */
  readonly #intercepts = new Map<Network.Intercept, NetworkInterception>();

  #interceptionStages = {
    request: false,
    response: false,
    auth: false,
  };

  constructor(eventManager: EventManager, browserClient: CdpClient) {
    this.#eventManager = eventManager;

    browserClient.on(
      'Target.detachedFromTarget',
      ({sessionId}: Protocol.Target.DetachedFromTargetEvent) => {
        this.disposeRequestMap(sessionId);
      }
    );
  }

  /**
   * Gets the network request with the given ID, if any.
   * Otherwise, creates a new network request with the given ID and cdp target.
   */
  #getOrCreateNetworkRequest(
    id: Network.Request,
    cdpTarget: CdpTarget,
    redirectCount?: number
  ): NetworkRequest {
    let request = this.getRequestById(id);
    if (request) {
      return request;
    }

    request = new NetworkRequest(
      id,
      this.#eventManager,
      this,
      cdpTarget,
      redirectCount
    );

    this.addRequest(request);

    return request;
  }

  onCdpTargetCreated(cdpTarget: CdpTarget) {
    this.#targets.add(cdpTarget);

    const cdpClient = cdpTarget.cdpClient;

    // TODO: Wrap into object
    const listeners = [
      [
        'Network.requestWillBeSent',
        (params: Protocol.Network.RequestWillBeSentEvent) => {
          const request = this.getRequestById(params.requestId);
          if (request && request.isRedirecting()) {
            request.handleRedirect(params);
            this.deleteRequest(params.requestId);
            this.#getOrCreateNetworkRequest(
              params.requestId,
              cdpTarget,
              request.redirectCount + 1
            ).onRequestWillBeSentEvent(params);
          } else if (request) {
            request.onRequestWillBeSentEvent(params);
          } else {
            this.#getOrCreateNetworkRequest(
              params.requestId,
              cdpTarget
            ).onRequestWillBeSentEvent(params);
          }
        },
      ],
      [
        'Network.requestWillBeSentExtraInfo',
        (params: Protocol.Network.RequestWillBeSentExtraInfoEvent) => {
          this.#getOrCreateNetworkRequest(
            params.requestId,
            cdpTarget
          ).onRequestWillBeSentExtraInfoEvent(params);
        },
      ],
      [
        'Network.responseReceived',
        (params: Protocol.Network.ResponseReceivedEvent) => {
          this.#getOrCreateNetworkRequest(
            params.requestId,
            cdpTarget
          ).onResponseReceivedEvent(params);
        },
      ],
      [
        'Network.responseReceivedExtraInfo',
        (params: Protocol.Network.ResponseReceivedExtraInfoEvent) => {
          this.#getOrCreateNetworkRequest(
            params.requestId,
            cdpTarget
          ).onResponseReceivedExtraInfoEvent(params);
        },
      ],
      [
        'Network.requestServedFromCache',
        (params: Protocol.Network.RequestServedFromCacheEvent) => {
          this.#getOrCreateNetworkRequest(
            params.requestId,
            cdpTarget
          ).onServedFromCache();
        },
      ],
      [
        'Network.loadingFailed',
        (params: Protocol.Network.LoadingFailedEvent) => {
          this.#getOrCreateNetworkRequest(
            params.requestId,
            cdpTarget
          ).onLoadingFailedEvent(params);
        },
      ],
      [
        'Fetch.requestPaused',
        (event: Protocol.Fetch.RequestPausedEvent) => {
          this.#handleNetworkInterception(event, cdpTarget);
        },
      ],
      [
        'Fetch.authRequired',
        (event: Protocol.Fetch.AuthRequiredEvent) => {
          this.#handleAuthInterception(event, cdpTarget);
        },
      ],
    ] as const;

    for (const [event, listener] of listeners) {
      cdpClient.on(event, listener as any);
    }
  }

  async toggleInterception() {
    if (this.#intercepts.size) {
      const stages = {
        request: false,
        response: false,
        auth: false,
      };
      for (const intercept of this.#intercepts.values()) {
        stages.request ||= intercept.phases.includes(
          Network.InterceptPhase.BeforeRequestSent
        );
        stages.response ||= intercept.phases.includes(
          Network.InterceptPhase.ResponseStarted
        );
        stages.auth ||= intercept.phases.includes(
          Network.InterceptPhase.AuthRequired
        );
      }
      const patterns: Protocol.Fetch.EnableRequest['patterns'] = [];

      if (
        this.#interceptionStages.request === stages.request &&
        this.#interceptionStages.response === stages.response &&
        this.#interceptionStages.auth === stages.auth
      ) {
        return;
      }

      this.#interceptionStages = stages;
      // CDP quirk we need request interception when we intercept auth
      if (stages.request || stages.auth) {
        patterns.push({
          urlPattern: '*',
          requestStage: 'Request',
        });
      }
      if (stages.response) {
        patterns.push({
          urlPattern: '*',
          requestStage: 'Response',
        });
      }

      // TODO: Don't enable on start as we will have
      // no network interceptions at this time.
      // Needed to enable fetch events.

      await Promise.all(
        [...this.#targets.values()].map(async (cdpTarget) => {
          return await cdpTarget.enableFetchIfNeeded({
            patterns,
            handleAuthRequests: stages.auth,
          });
        })
      );
    } else {
      this.#interceptionStages = {
        request: false,
        response: false,
        auth: false,
      };

      await Promise.all(
        [...this.#targets.values()].map((target) => {
          return target.disableFetchIfNeeded();
        })
      );
    }
  }

  requestBlockedBy(
    request: NetworkRequest,
    phase?: Network.InterceptPhase
  ): Set<Network.Intercept> {
    if (request.url === undefined || phase === undefined) {
      return new Set();
    }

    const intercepts = new Set<Network.Intercept>();
    for (const [interceptId, intercept] of this.#intercepts.entries()) {
      if (!intercept.phases.includes(phase)) {
        continue;
      }
      if (intercept.urlPatterns.length === 0) {
        intercepts.add(interceptId);
        continue;
      }

      for (const pattern of intercept.urlPatterns) {
        if (matchUrlPattern(pattern, request.url)) {
          intercepts.add(interceptId);
          break;
        }
      }
    }

    return intercepts;
  }

  disposeRequestMap(sessionId: string) {
    const requests = [...this.#requests.values()].filter((request) => {
      return request.cdpClient.sessionId === sessionId;
    });

    for (const request of requests) {
      request.dispose();
      this.#requests.delete(request.id);
    }
  }

  #handleNetworkInterception(
    event: Protocol.Fetch.RequestPausedEvent,
    cdpTarget: CdpTarget
  ) {
    // CDP quirk if the Network domain is not present this is undefined
    this.#getOrCreateNetworkRequest(
      event.networkId ?? '',
      cdpTarget
    ).onRequestPaused(event);
  }

  #handleAuthInterception(
    event: Protocol.Fetch.AuthRequiredEvent,
    cdpTarget: CdpTarget
  ) {
    // CDP quirk if the Network domain is not present this is undefined
    const request = this.getRequestByFetchId(event.requestId ?? '');
    if (!request) {
      // CDP quirk even both request/response may be continued
      // with this command
      void cdpTarget.cdpClient
        .sendCommand('Fetch.continueWithAuth', {
          requestId: event.requestId,
          authChallengeResponse: {
            response: 'Default',
          },
        })
        .catch(() => {
          // TODO: add logging
        });
      return;
    }

    request.onAuthRequired(event);
  }

  /**
   * Adds the given entry to the intercept map.
   * URL patterns are assumed to be parsed.
   *
   * @return The intercept ID.
   */
  async addIntercept(value: NetworkInterception): Promise<Network.Intercept> {
    const interceptId: Network.Intercept = uuidv4();
    this.#intercepts.set(interceptId, value);

    await this.toggleInterception();

    return interceptId;
  }

  /**
   * Removes the given intercept from the intercept map.
   * Throws NoSuchInterceptException if the intercept does not exist.
   */
  async removeIntercept(intercept: Network.Intercept) {
    if (!this.#intercepts.has(intercept)) {
      throw new NoSuchInterceptException(
        `Intercept '${intercept}' does not exist.`
      );
    }
    this.#intercepts.delete(intercept);

    await this.toggleInterception();
  }

  getRequestById(id: Network.Request): NetworkRequest | undefined {
    return this.#requests.get(id);
  }

  getRequestByFetchId(fetchId: Network.Request): NetworkRequest | undefined {
    for (const request of this.#requests.values()) {
      if (request.fetchId === fetchId) {
        return request;
      }
    }

    return;
  }

  addRequest(request: NetworkRequest) {
    this.#requests.set(request.id, request);
  }

  deleteRequest(id: Network.Request) {
    const request = this.#requests.get(id);
    if (request) {
      request.dispose();
      this.#requests.delete(id);
    }
  }
}
