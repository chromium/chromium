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

import {
  type BrowsingContext,
  Network,
  NoSuchInterceptException,
} from '../../../protocol/protocol.js';
import type {LoggerFn} from '../../../utils/log.js';
import {uuidv4} from '../../../utils/uuid.js';
import type {CdpClient} from '../../BidiMapper.js';
import type {CdpTarget} from '../cdp/CdpTarget.js';
import type {EventManager} from '../session/EventManager.js';

import {NetworkRequest} from './NetworkRequest.js';
import {matchUrlPattern} from './NetworkUtils.js';

type NetworkInterception = Omit<
  Network.AddInterceptParameters,
  'urlPatterns'
> & {
  urlPatterns: Network.UrlPattern[];
};

/** Stores network and intercept maps. */
export class NetworkStorage {
  #eventManager: EventManager;
  #logger?: LoggerFn;

  /**
   * A map from network request ID to Network Request objects.
   * Needed as long as information about requests comes from different events.
   */
  readonly #requests = new Map<Network.Request, NetworkRequest>();

  /** A map from intercept ID to track active network intercepts. */
  readonly #intercepts = new Map<Network.Intercept, NetworkInterception>();

  constructor(
    eventManager: EventManager,
    browserClient: CdpClient,
    logger?: LoggerFn
  ) {
    this.#eventManager = eventManager;

    browserClient.on(
      'Target.detachedFromTarget',
      ({sessionId}: Protocol.Target.DetachedFromTargetEvent) => {
        this.disposeRequestMap(sessionId);
      }
    );

    this.#logger = logger;
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
      redirectCount,
      this.#logger
    );

    this.addRequest(request);

    return request;
  }

  onCdpTargetCreated(cdpTarget: CdpTarget) {
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
          this.#getOrCreateNetworkRequest(
            // CDP quirk if the Network domain is not present this is undefined
            event.networkId ?? event.requestId,
            cdpTarget
          ).onRequestPaused(event);
        },
      ],
      [
        'Fetch.authRequired',
        (event: Protocol.Fetch.AuthRequiredEvent) => {
          let request = this.getRequestByFetchId(event.requestId);
          if (!request) {
            request = this.#getOrCreateNetworkRequest(
              event.requestId,
              cdpTarget
            );
          }

          request.onAuthRequired(event);
        },
      ],
    ] as const;

    for (const [event, listener] of listeners) {
      cdpClient.on(event, listener as any);
    }
  }

  getInterceptionStages(browsingContextId: BrowsingContext.BrowsingContext) {
    const stages = {
      request: false,
      response: false,
      auth: false,
    };
    for (const intercept of this.#intercepts.values()) {
      if (
        intercept.contexts &&
        !intercept.contexts.includes(browsingContextId)
      ) {
        continue;
      }

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

    return stages;
  }

  getInterceptsForPhase(
    request: NetworkRequest,
    phase: Network.InterceptPhase
  ): Set<Network.Intercept> {
    if (request.url === NetworkRequest.unknownParameter) {
      return new Set();
    }

    const intercepts = new Set<Network.Intercept>();
    for (const [interceptId, intercept] of this.#intercepts.entries()) {
      if (
        !intercept.phases.includes(phase) ||
        (intercept.contexts &&
          !intercept.contexts.includes(request.cdpTarget.topLevelId))
      ) {
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
    for (const request of this.#requests.values()) {
      if (request.cdpClient.sessionId === sessionId) {
        this.#requests.delete(request.id);
      }
    }
  }

  /**
   * Adds the given entry to the intercept map.
   * URL patterns are assumed to be parsed.
   *
   * @return The intercept ID.
   */
  addIntercept(value: NetworkInterception): Network.Intercept {
    const interceptId: Network.Intercept = uuidv4();
    this.#intercepts.set(interceptId, value);

    return interceptId;
  }

  /**
   * Removes the given intercept from the intercept map.
   * Throws NoSuchInterceptException if the intercept does not exist.
   */
  removeIntercept(intercept: Network.Intercept) {
    if (!this.#intercepts.has(intercept)) {
      throw new NoSuchInterceptException(
        `Intercept '${intercept}' does not exist.`
      );
    }
    this.#intercepts.delete(intercept);
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
    this.#requests.delete(id);
  }
}
