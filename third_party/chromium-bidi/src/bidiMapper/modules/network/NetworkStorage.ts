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
  InvalidArgumentException,
  Network,
  NoSuchInterceptException,
  NoSuchNetworkDataException,
  UnsupportedOperationException,
} from '../../../protocol/protocol.js';
import type {LoggerFn} from '../../../utils/log.js';
import {uuidv4} from '../../../utils/uuid.js';
import type {CdpClient} from '../../BidiMapper.js';
import type {CdpTarget} from '../cdp/CdpTarget.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';
import type {EventManager} from '../session/EventManager.js';

import {CollectorsStorage} from './CollectorsStorage.js';
import {NetworkRequest} from './NetworkRequest.js';
import {matchUrlPattern, type ParsedUrlPattern} from './NetworkUtils.js';

type NetworkInterception = Omit<
  Network.AddInterceptParameters,
  'urlPatterns'
> & {
  urlPatterns: ParsedUrlPattern[];
};

/** Stores network and intercept maps. */
export class NetworkStorage {
  readonly #browsingContextStorage: BrowsingContextStorage;
  readonly #eventManager: EventManager;
  readonly #collectorsStorage: CollectorsStorage;

  readonly #logger?: LoggerFn;

  /**
   * A map from network request ID to Network Request objects.
   * Needed as long as information about requests comes from different events.
   */
  readonly #requests = new Map<Network.Request, NetworkRequest>();

  /** A map from intercept ID to track active network intercepts. */
  readonly #intercepts = new Map<Network.Intercept, NetworkInterception>();

  #defaultCacheBehavior: Network.SetCacheBehaviorParameters['cacheBehavior'] =
    'default';

  constructor(
    eventManager: EventManager,
    browsingContextStorage: BrowsingContextStorage,
    browserClient: CdpClient,
    logger?: LoggerFn,
  ) {
    this.#browsingContextStorage = browsingContextStorage;
    this.#eventManager = eventManager;
    this.#collectorsStorage = new CollectorsStorage(logger);

    browserClient.on('Target.detachedFromTarget', ({sessionId}) => {
      this.disposeRequestMap(sessionId);
    });

    this.#logger = logger;
  }

  /**
   * Gets the network request with the given ID, if any.
   * Otherwise, creates a new network request with the given ID and cdp target.
   */
  #getOrCreateNetworkRequest(
    id: Network.Request,
    cdpTarget: CdpTarget,
    redirectCount?: number,
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
      this.#logger,
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
          request?.updateCdpTarget(cdpTarget);
          if (request && request.isRedirecting()) {
            request.handleRedirect(params);
            this.disposeRequest(params.requestId);
            this.#getOrCreateNetworkRequest(
              params.requestId,
              cdpTarget,
              request.redirectCount + 1,
            ).onRequestWillBeSentEvent(params);
          } else {
            this.#getOrCreateNetworkRequest(
              params.requestId,
              cdpTarget,
            ).onRequestWillBeSentEvent(params);
          }
        },
      ],
      [
        'Network.requestWillBeSentExtraInfo',
        (params: Protocol.Network.RequestWillBeSentExtraInfoEvent) => {
          const request = this.#getOrCreateNetworkRequest(
            params.requestId,
            cdpTarget,
          );
          request.updateCdpTarget(cdpTarget);
          request.onRequestWillBeSentExtraInfoEvent(params);
        },
      ],
      [
        'Network.responseReceived',
        (params: Protocol.Network.ResponseReceivedEvent) => {
          const request = this.#getOrCreateNetworkRequest(
            params.requestId,
            cdpTarget,
          );
          request.updateCdpTarget(cdpTarget);
          request.onResponseReceivedEvent(params);
        },
      ],
      [
        'Network.responseReceivedExtraInfo',
        (params: Protocol.Network.ResponseReceivedExtraInfoEvent) => {
          const request = this.#getOrCreateNetworkRequest(
            params.requestId,
            cdpTarget,
          );
          request.updateCdpTarget(cdpTarget);
          request.onResponseReceivedExtraInfoEvent(params);
        },
      ],
      [
        'Network.requestServedFromCache',
        (params: Protocol.Network.RequestServedFromCacheEvent) => {
          const request = this.#getOrCreateNetworkRequest(
            params.requestId,
            cdpTarget,
          );
          request.updateCdpTarget(cdpTarget);
          request.onServedFromCache();
        },
      ],
      [
        'Network.loadingFailed',
        (params: Protocol.Network.LoadingFailedEvent) => {
          const request = this.#getOrCreateNetworkRequest(
            params.requestId,
            cdpTarget,
          );
          request.updateCdpTarget(cdpTarget);
          request.onLoadingFailedEvent(params);
        },
      ],
      [
        'Fetch.requestPaused',
        (event: Protocol.Fetch.RequestPausedEvent) => {
          const request = this.#getOrCreateNetworkRequest(
            // CDP quirk if the Network domain is not present this is undefined
            event.networkId ?? event.requestId,
            cdpTarget,
          );
          request.updateCdpTarget(cdpTarget);
          request.onRequestPaused(event);
        },
      ],
      [
        'Fetch.authRequired',
        (event: Protocol.Fetch.AuthRequiredEvent) => {
          let request = this.getRequestByFetchId(event.requestId);
          if (!request) {
            request = this.#getOrCreateNetworkRequest(
              event.requestId,
              cdpTarget,
            );
          }
          request.updateCdpTarget(cdpTarget);
          request.onAuthRequired(event);
        },
      ],
      [
        'Network.dataReceived',
        (params: Protocol.Network.DataReceivedEvent) => {
          this.getRequestById(params.requestId)?.updateCdpTarget(cdpTarget);
        },
      ],
      [
        'Network.loadingFinished',
        (params: Protocol.Network.LoadingFinishedEvent) => {
          this.getRequestById(params.requestId)?.updateCdpTarget(cdpTarget);
        },
      ],
    ] as const;

    for (const [event, listener] of listeners) {
      cdpClient.on(event, listener as any);
    }
  }

  async getCollectedData(
    params: Network.GetDataParameters,
  ): Promise<Network.GetDataResult> {
    switch (params.dataType) {
      case Network.DataType.Response:
        return await this.#getCollectedResponseData(params);
      case Network.DataType.Request:
      default:
        throw new UnsupportedOperationException(
          `Unsupported data type ${params.dataType}`,
        );
    }
  }
  async #getCollectedResponseData(
    params: Network.GetDataParameters,
  ): Promise<Network.GetDataResult> {
    if (
      !this.#collectorsStorage.isResponseCollected(
        params.request,
        params.collector,
      )
    ) {
      if (params.collector !== undefined) {
        throw new NoSuchNetworkDataException(
          `Collector ${params.collector} didn't collect data for request ${params.request}`,
        );
      }
      throw new NoSuchNetworkDataException(
        `No collected data for request ${params.request}`,
      );
    }

    if (params.disown && params.collector === undefined) {
      throw new InvalidArgumentException(
        'Cannot disown collected data without collector ID',
      );
    }

    const request = this.getRequestById(params.request)!;
    if (request === undefined) {
      throw new NoSuchNetworkDataException(
        `No data for request ${params.request}`,
      );
    }

    // TODO: handle CDP error in case of the renderer is gone.
    const responseBody = await request.cdpClient.sendCommand(
      'Network.getResponseBody',
      {requestId: request.id},
    );

    if (params.disown && params.collector !== undefined) {
      this.#collectorsStorage.disownResponse(params.request, params.collector);
      // `disposeRequest` disposes request only if no other collectors for it are left.
      this.disposeRequest(request.id);
    }

    return {
      bytes: {
        type: responseBody.base64Encoded ? 'base64' : 'string',
        value: responseBody.body,
      },
    };
  }

  collectIfNeeded(request: NetworkRequest) {
    this.#collectorsStorage.collectIfNeeded(
      request,
      request.cdpTarget.topLevelId,
      this.#browsingContextStorage.getContext(request.cdpTarget.topLevelId)
        .userContext,
    );
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
        Network.InterceptPhase.BeforeRequestSent,
      );
      stages.response ||= intercept.phases.includes(
        Network.InterceptPhase.ResponseStarted,
      );
      stages.auth ||= intercept.phases.includes(
        Network.InterceptPhase.AuthRequired,
      );
    }

    return stages;
  }

  getInterceptsForPhase(
    request: NetworkRequest,
    phase: Network.InterceptPhase,
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
        request.dispose();
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
        `Intercept '${intercept}' does not exist.`,
      );
    }
    this.#intercepts.delete(intercept);
  }

  getRequestsByTarget(target: CdpTarget): NetworkRequest[] {
    const requests: NetworkRequest[] = [];
    for (const request of this.#requests.values()) {
      if (request.cdpTarget === target) {
        requests.push(request);
      }
    }
    return requests;
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

  disposeRequest(id: Network.Request) {
    if (
      this.#collectorsStorage.isResponseCollected(id) ||
      this.#collectorsStorage.isRequestCollected(id)
    ) {
      // Keep request, as it's data can be accessed later.
      return;
    }
    // TODO: dispose Network data from Chromium once there is a CDP command for that.
    this.#requests.delete(id);
  }

  /**
   * Gets the virtual navigation ID for the given navigable ID.
   */
  getNavigationId(contextId: string | undefined): string | null {
    if (contextId === undefined) {
      return null;
    }
    return (
      this.#browsingContextStorage.findContext(contextId)?.navigationId ?? null
    );
  }

  set defaultCacheBehavior(
    behavior: Network.SetCacheBehaviorParameters['cacheBehavior'],
  ) {
    this.#defaultCacheBehavior = behavior;
  }

  get defaultCacheBehavior() {
    return this.#defaultCacheBehavior;
  }

  addDataCollector(params: Network.AddDataCollectorParameters): string {
    return this.#collectorsStorage.addDataCollector(params);
  }

  removeDataCollector(params: Network.RemoveDataCollectorParameters) {
    const releasedRequests = this.#collectorsStorage.removeDataCollector(
      params.collector,
    );
    releasedRequests.map((request) => this.disposeRequest(request));
  }

  disownData(params: Network.DisownDataParameters) {
    switch (params.dataType) {
      case Network.DataType.Response:
        if (
          !this.#collectorsStorage.isResponseCollected(
            params.request,
            params.collector,
          )
        ) {
          throw new NoSuchNetworkDataException(
            `Collector ${params.collector} didn't collect data for request ${params.request}`,
          );
        }

        this.#collectorsStorage.disownResponse(
          params.request,
          params.collector,
        );
        // `disposeRequest` disposes request only if no other collectors for it are left.
        this.disposeRequest(params.request);
        break;
      case Network.DataType.Request:
      default:
        throw new UnsupportedOperationException(
          `Unsupported data type ${params.dataType}`,
        );
    }
    this.disposeRequest(params.request);
  }
}
