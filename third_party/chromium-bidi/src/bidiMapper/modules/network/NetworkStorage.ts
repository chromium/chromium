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
import {CdpErrorConstants} from '../../../utils/cdpErrorConstants.js';
import type {LoggerFn} from '../../../utils/log.js';
import {uuidv4} from '../../../utils/uuid.js';
import type {CdpClient} from '../../BidiMapper.js';
import type {CdpTarget} from '../cdp/CdpTarget.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';
import type {EventManager} from '../session/EventManager.js';

import {CollectorsStorage} from './CollectorsStorage.js';
import {NetworkRequest} from './NetworkRequest.js';
import {matchUrlPattern, type ParsedUrlPattern} from './NetworkUtils.js';

// The default total data size limit in CDP.
// https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/inspector/inspector_network_agent.cc;drc=da1f749634c9a401cc756f36c2e6ce233e1c9b4d;l=133
export const MAX_TOTAL_COLLECTED_SIZE = 200_000_000;

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
    this.#collectorsStorage = new CollectorsStorage(
      MAX_TOTAL_COLLECTED_SIZE,
      logger,
    );

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
    if (redirectCount === undefined && request) {
      // Force re-creating requests for redirects.
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
          const request = this.getRequestById(params.requestId);
          request?.updateCdpTarget(cdpTarget);
          request?.onDataReceivedEvent(params);
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
        'Network.loadingFinished',
        (params: Protocol.Network.LoadingFinishedEvent) => {
          const request = this.getRequestById(params.requestId);
          request?.updateCdpTarget(cdpTarget);
          request?.onLoadingFinishedEvent(params);
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
    if (
      !this.#collectorsStorage.isCollected(
        params.request,
        params.dataType,
        params.collector,
      )
    ) {
      throw new NoSuchNetworkDataException(
        params.collector === undefined
          ? `No collected ${params.dataType} data`
          : `Collector ${params.collector} didn't collect ${params.dataType} data`,
      );
    }

    if (params.disown && params.collector === undefined) {
      throw new InvalidArgumentException(
        'Cannot disown collected data without collector ID',
      );
    }

    const request = this.getRequestById(params.request);
    if (request === undefined) {
      throw new NoSuchNetworkDataException(`No data for ${params.request}`);
    }

    let result: Network.GetDataResult | undefined = undefined;
    switch (params.dataType) {
      case Network.DataType.Response:
        result = await this.#getCollectedResponseData(request);
        break;
      case Network.DataType.Request:
        result = await this.#getCollectedRequestData(request);
        break;
      default:
        throw new UnsupportedOperationException(
          `Unsupported data type ${params.dataType}`,
        );
    }

    if (params.disown && params.collector !== undefined) {
      this.#collectorsStorage.disownData(
        request.id,
        params.dataType,
        params.collector,
      );
      // `disposeRequest` disposes request only if no other collectors for it are left.
      this.disposeRequest(request.id);
    }

    return result;
  }

  async #getCollectedResponseData(
    request: NetworkRequest,
  ): Promise<Network.GetDataResult> {
    try {
      const responseBody = await request.cdpClient.sendCommand(
        'Network.getResponseBody',
        {requestId: request.id},
      );

      return {
        bytes: {
          type: responseBody.base64Encoded ? 'base64' : 'string',
          value: responseBody.body,
        },
      };
    } catch (error: any) {
      if (
        error.code === CdpErrorConstants.GENERIC_ERROR &&
        error.message === 'No resource with given identifier found'
      ) {
        // The data has be gone for whatever reason.
        throw new NoSuchNetworkDataException(`Response data was disposed`);
      }
      if (error.code === CdpErrorConstants.CONNECTION_CLOSED) {
        // The request's CDP session is gone. http://b/450771615.
        throw new NoSuchNetworkDataException(
          `Response data is disposed after the related page`,
        );
      }
      throw error;
    }
  }

  async #getCollectedRequestData(
    request: NetworkRequest,
  ): Promise<Network.GetDataResult> {
    // TODO: handle CDP error in case of the renderer is gone.
    const requestPostData = await request.cdpClient.sendCommand(
      'Network.getRequestPostData',
      {requestId: request.id},
    );

    return {
      bytes: {
        type: 'string',
        value: requestPostData.postData,
      },
    };
  }

  collectIfNeeded(request: NetworkRequest, dataType: Network.DataType) {
    this.#collectorsStorage.collectIfNeeded(
      request,
      dataType,
      request.cdpTarget.topLevelId,
      request.cdpTarget.userContext,
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

  /**
   * Disposes the given request, if no collectors targeting it are left.
   */
  disposeRequest(id: Network.Request) {
    if (this.#collectorsStorage.isCollected(id)) {
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
    if (
      !this.#collectorsStorage.isCollected(
        params.request,
        params.dataType,
        params.collector,
      )
    ) {
      throw new NoSuchNetworkDataException(
        `Collector ${params.collector} didn't collect ${params.dataType} data`,
      );
    }

    this.#collectorsStorage.disownData(
      params.request,
      params.dataType,
      params.collector,
    );
    // `disposeRequest` disposes request only if no other collectors for it are left.
    this.disposeRequest(params.request);
  }
}
