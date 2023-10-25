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
import type Protocol from 'devtools-protocol';

import {
  Network,
  type EmptyResult,
  NoSuchRequestException,
  InvalidArgumentException,
} from '../../../protocol/protocol.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';
import {assert} from '../../../utils/assert.js';

import {cdpFetchHeadersFromBidiNetworkHeaders} from './NetworkUtils.js';
import {NetworkStorage} from './NetworkStorage.js';

/** Dispatches Network domain commands. */
export class NetworkProcessor {
  readonly #browsingContextStorage: BrowsingContextStorage;
  readonly #networkStorage: NetworkStorage;

  constructor(
    browsingContextStorage: BrowsingContextStorage,
    networkStorage: NetworkStorage
  ) {
    this.#browsingContextStorage = browsingContextStorage;
    this.#networkStorage = networkStorage;
  }

  async addIntercept(
    params: Network.AddInterceptParameters
  ): Promise<Network.AddInterceptResult> {
    if (params.phases.length === 0) {
      throw new InvalidArgumentException(
        'At least one phase must be specified.'
      );
    }

    const urlPatterns: Network.UrlPattern[] = params.urlPatterns ?? [];
    const parsedUrlPatterns: Network.UrlPattern[] =
      NetworkProcessor.parseUrlPatterns(urlPatterns);

    const intercept: Network.Intercept = this.#networkStorage.addIntercept({
      urlPatterns: parsedUrlPatterns,
      phases: params.phases,
    });

    await this.#fetchApply();

    return {
      intercept,
    };
  }

  async continueRequest(
    params: Network.ContinueRequestParameters
  ): Promise<EmptyResult> {
    const networkId = params.request;
    const {request: fetchId, phase} = this.#getBlockedRequest(networkId);

    if (phase !== Network.InterceptPhase.BeforeRequestSent) {
      throw new InvalidArgumentException(
        `Blocked request for network id '${networkId}' is not in 'BeforeRequestSent' phase`
      );
    }

    if (params.url !== undefined) {
      NetworkProcessor.parseUrlString(params.url);
    }

    const {url, method, headers} = params;
    // TODO: Set / expand.
    // ; Step 9. cookies
    // ; Step 10. body

    const requestHeaders: Protocol.Fetch.HeaderEntry[] | undefined =
      cdpFetchHeadersFromBidiNetworkHeaders(headers);

    const request = this.#networkStorage.getRequest(networkId);
    assert(request, `Network request with ID ${networkId} doesn't exist`);

    await request.continueRequest(fetchId, url, method, requestHeaders);

    this.#networkStorage.removeBlockedRequest(networkId);

    return {};
  }

  async continueResponse(
    params: Network.ContinueResponseParameters
  ): Promise<EmptyResult> {
    const networkId = params.request;
    const {request: fetchId, phase} = this.#getBlockedRequest(networkId);

    if (phase === Network.InterceptPhase.BeforeRequestSent) {
      throw new InvalidArgumentException(
        `Blocked request for network id '${networkId}' is in 'BeforeRequestSent' phase`
      );
    }

    const {statusCode, reasonPhrase, headers} = params;

    const responseHeaders: Protocol.Fetch.HeaderEntry[] | undefined =
      cdpFetchHeadersFromBidiNetworkHeaders(headers);

    // TODO: Set / expand.
    // ; Step 10. cookies
    // ; Step 11. credentials
    const request = this.#networkStorage.getRequest(networkId);
    assert(request, `Network request with ID ${networkId} doesn't exist`);

    await request.continueResponse(
      fetchId,
      statusCode,
      reasonPhrase,
      responseHeaders
    );

    this.#networkStorage.removeBlockedRequest(networkId);

    return {};
  }

  continueWithAuth(params: Network.ContinueWithAuthParameters): EmptyResult {
    const networkId = params.request;
    const {phase} = this.#getBlockedRequest(networkId);

    if (phase !== Network.InterceptPhase.AuthRequired) {
      throw new InvalidArgumentException(
        `Blocked request for network id '${networkId}' is not in 'AuthRequired' phase`
      );
    }
    // TODO: https://w3c.github.io/webdriver-bidi/#command-network-continueWithAuth
    // Implement remaining steps 6-8.

    return {};
  }

  async failRequest(
    params: Network.FailRequestParameters
  ): Promise<EmptyResult> {
    const networkId = params.request;
    const blockedRequest = this.#getBlockedRequest(networkId);
    const {request: fetchId, phase} = blockedRequest;

    if (phase === Network.InterceptPhase.AuthRequired) {
      throw new InvalidArgumentException(
        `Blocked request for network id '${networkId}' is in 'AuthRequired' phase`
      );
    }

    const request = this.#networkStorage.getRequest(networkId);
    assert(request, `Network request with ID ${networkId} doesn't exist`);

    await request.failRequest(fetchId, 'Failed');

    this.#networkStorage.removeBlockedRequest(networkId);

    return {};
  }

  async provideResponse(
    params: Network.ProvideResponseParameters
  ): Promise<EmptyResult> {
    const networkId = params.request;
    const {request: fetchId} = this.#getBlockedRequest(networkId);

    const {statusCode, reasonPhrase, headers, body} = params;

    // TODO: Step 6
    // https://w3c.github.io/webdriver-bidi/#command-network-continueResponse

    const responseHeaders: Protocol.Fetch.HeaderEntry[] | undefined =
      cdpFetchHeadersFromBidiNetworkHeaders(headers);

    // TODO: Set / expand.
    // ; Step 10. cookies
    // ; Step 11. credentials
    const request = this.#networkStorage.getRequest(networkId);
    assert(request, `Network request with ID ${networkId} doesn't exist`);

    await request.provideResponse(
      fetchId,
      statusCode ?? request.statusCode,
      reasonPhrase,
      responseHeaders,
      body?.value // TODO: Differ base64 / string
    );

    this.#networkStorage.removeBlockedRequest(networkId);

    return {};
  }

  async removeIntercept(
    params: Network.RemoveInterceptParameters
  ): Promise<EmptyResult> {
    this.#networkStorage.removeIntercept(params.intercept);

    await this.#fetchApply();

    return {};
  }

  /** Applies all existing network intercepts to all CDP targets concurrently. */
  async #fetchEnable() {
    await Promise.all(
      this.#browsingContextStorage.getAllContexts().map(async (context) => {
        await context.cdpTarget.fetchEnable();
      })
    );
  }

  /** Removes all existing network intercepts from all CDP targets concurrently. */
  async #fetchDisable() {
    await Promise.all(
      this.#browsingContextStorage.getAllContexts().map(async (context) => {
        await context.cdpTarget.fetchDisable();
      })
    );
  }

  /**
   * Either enables or disables the Fetch domain.
   *
   * If enabling, applies all existing network intercepts to all CDP targets.
   * If disabling, removes all existing network intercepts from all CDP targets.
   *
   * Disabling is only performed when there are no remaining intercepts or
   * // blocked requests.
   */
  async #fetchApply() {
    if (
      this.#networkStorage.hasIntercepts() ||
      this.#networkStorage.hasBlockedRequests() ||
      this.#networkStorage.hasNetworkRequests()
    ) {
      // TODO: Add try/catch. Remove the intercept if CDP Fetch commands fail.
      await this.#fetchEnable();
    } else {
      // The last intercept has been removed, and there are no pending
      // blocked requests.
      // Disable the Fetch domain.
      await this.#fetchDisable();
    }
  }

  /**
   * Returns the blocked request associated with the given network ID.
   * If none, throws a NoSuchRequestException.
   */
  #getBlockedRequest(networkId: Network.Request): {
    request: Protocol.Fetch.RequestId;
    phase: Network.InterceptPhase;
    response: Network.ResponseData;
  } {
    const blockedRequest = this.#networkStorage.getBlockedRequest(networkId);
    if (!blockedRequest) {
      throw new NoSuchRequestException(
        `No blocked request found for network id '${networkId}'`
      );
    }
    return blockedRequest;
  }

  /**
   * Attempts to parse the given url.
   * Throws an InvalidArgumentException if the url is invalid.
   */
  static parseUrlString(url: string): URL {
    try {
      return new URL(url);
    } catch (error) {
      throw new InvalidArgumentException(`Invalid URL '${url}': ${error}`);
    }
  }

  static parseUrlPatterns(
    urlPatterns: Network.UrlPattern[]
  ): Network.UrlPattern[] {
    return urlPatterns.map((urlPattern) => {
      switch (urlPattern.type) {
        case 'string': {
          NetworkProcessor.parseUrlString(urlPattern.pattern);
          return urlPattern;
        }
        case 'pattern':
          if (urlPattern.protocol === '') {
            throw new InvalidArgumentException(
              `URL pattern must specify a protocol`
            );
          }

          if (urlPattern.hostname === '') {
            throw new InvalidArgumentException(
              `URL pattern must specify a hostname`
            );
          }

          if ((urlPattern.hostname?.length ?? 0) > 0) {
            if (urlPattern.protocol?.match(/^file/i)) {
              throw new InvalidArgumentException(
                `URL pattern protocol cannot be 'file'`
              );
            }

            if (urlPattern.hostname?.includes(':')) {
              throw new InvalidArgumentException(
                `URL pattern hostname must not contain a colon`
              );
            }
          }

          if (urlPattern.port === '') {
            throw new InvalidArgumentException(
              `URL pattern must specify a port`
            );
          }

          try {
            new URL(NetworkStorage.buildUrlPatternString(urlPattern));
          } catch (error) {
            throw new InvalidArgumentException(`${error}`);
          }
          return urlPattern;
      }
    });
  }
}
