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
  Network,
  type EmptyResult,
  NoSuchRequestException,
  InvalidArgumentException,
} from '../../../protocol/protocol.js';
import {assert} from '../../../utils/assert.js';

import type {NetworkRequest} from './NetworkRequest.js';
import type {NetworkStorage} from './NetworkStorage.js';
import {
  cdpFetchHeadersFromBidiNetworkHeaders,
  cdpAuthChallengeResponseFromBidiAuthContinueWithAuthAction,
  buildUrlPatternString,
} from './NetworkUtils.js';

/** Dispatches Network domain commands. */
export class NetworkProcessor {
  readonly #networkStorage: NetworkStorage;

  constructor(networkStorage: NetworkStorage) {
    this.#networkStorage = networkStorage;
  }

  async addIntercept(
    params: Network.AddInterceptParameters
  ): Promise<Network.AddInterceptResult> {
    const urlPatterns: Network.UrlPattern[] = params.urlPatterns ?? [];
    const parsedUrlPatterns: Network.UrlPattern[] =
      NetworkProcessor.parseUrlPatterns(urlPatterns);

    const intercept: Network.Intercept =
      await this.#networkStorage.addIntercept({
        urlPatterns: parsedUrlPatterns,
        phases: params.phases,
      });

    return {
      intercept,
    };
  }

  async continueRequest(
    params: Network.ContinueRequestParameters
  ): Promise<EmptyResult> {
    const networkId = params.request;

    if (params.url !== undefined) {
      NetworkProcessor.parseUrlString(params.url);
    }

    const request = this.#getBlockedRequestOrFail(
      networkId,
      Network.InterceptPhase.BeforeRequestSent
    );

    const {url, method, headers} = params;
    // TODO: Set / expand.
    // ; Step 9. cookies
    // ; Step 10. body

    const requestHeaders: Protocol.Fetch.HeaderEntry[] | undefined =
      cdpFetchHeadersFromBidiNetworkHeaders(headers);

    await request.continueRequest(url, method, requestHeaders);

    return {};
  }

  async continueResponse(
    params: Network.ContinueResponseParameters
  ): Promise<EmptyResult> {
    const networkId = params.request;
    const {statusCode, reasonPhrase, headers} = params;
    const request = this.#getBlockedRequestOrFail(
      networkId,
      Network.InterceptPhase.ResponseStarted
    );

    const responseHeaders: Protocol.Fetch.HeaderEntry[] | undefined =
      cdpFetchHeadersFromBidiNetworkHeaders(headers);

    // TODO: Set / expand.
    // ; Step 10. cookies
    // ; Step 11. credentials

    await request.continueResponse({
      responseCode: statusCode,
      responsePhrase: reasonPhrase,
      responseHeaders,
    });

    return {};
  }

  async continueWithAuth(
    params: Network.ContinueWithAuthParameters
  ): Promise<EmptyResult> {
    const networkId = params.request;
    const request = this.#getBlockedRequestOrFail(
      networkId,
      Network.InterceptPhase.AuthRequired
    );

    let username: string | undefined;
    let password: string | undefined;

    if (params.action === 'provideCredentials') {
      const {credentials} = params;

      username = credentials.username;
      password = credentials.password;
      // TODO: This should be invalid argument exception.
      // Spec may need to be updated.
      assert(
        credentials.type === 'password',
        `Credentials type ${credentials.type} must be 'password'`
      );
    }

    const response = cdpAuthChallengeResponseFromBidiAuthContinueWithAuthAction(
      params.action
    );

    await request.continueWithAuth({
      response,
      username,
      password,
    });

    return {};
  }

  async failRequest({
    request: networkId,
  }: Network.FailRequestParameters): Promise<EmptyResult> {
    const request = this.#getRequestOrFail(networkId);
    if (request.phase === Network.InterceptPhase.AuthRequired) {
      throw new InvalidArgumentException(
        `Request '${networkId}' in 'authRequired' phase cannot be failed`
      );
    }
    if (!request.blocked) {
      throw new NoSuchRequestException(
        `No blocked request found for network id '${networkId}'`
      );
    }

    await request.failRequest('Failed');

    return {};
  }

  async provideResponse(
    params: Network.ProvideResponseParameters
  ): Promise<EmptyResult> {
    const {
      statusCode,
      reasonPhrase,
      headers,
      body,
      request: networkId,
    } = params;

    // TODO: Step 6
    // https://w3c.github.io/webdriver-bidi/#command-network-continueResponse

    const responseHeaders: Protocol.Fetch.HeaderEntry[] | undefined =
      cdpFetchHeadersFromBidiNetworkHeaders(headers);

    // TODO: Set / expand.
    // ; Step 10. cookies
    // ; Step 11. credentials
    const request = this.#getBlockedRequestOrFail(networkId, null);
    await request.provideResponse({
      responseCode: statusCode ?? request.statusCode,
      responsePhrase: reasonPhrase,
      responseHeaders,
      body: body?.value, // TODO: Differ base64 / string
    });

    return {};
  }

  async removeIntercept(
    params: Network.RemoveInterceptParameters
  ): Promise<EmptyResult> {
    await this.#networkStorage.removeIntercept(params.intercept);

    return {};
  }

  #getRequestOrFail(id: Network.Request): NetworkRequest {
    const request = this.#networkStorage.getRequestById(id);
    if (!request) {
      throw new NoSuchRequestException(
        `Network request with ID '${id}' doesn't exist`
      );
    }
    return request;
  }

  #getBlockedRequestOrFail(
    id: Network.Request,
    phase: Network.InterceptPhase | null
  ): NetworkRequest {
    const request = this.#getRequestOrFail(id);
    if (!request.blocked) {
      throw new NoSuchRequestException(
        `No blocked request found for network id '${id}'`
      );
    }
    if (phase && request.phase !== phase) {
      throw new InvalidArgumentException(
        `Blocked request for network id '${id}' is in '${request.phase}' phase`
      );
    }

    return request;
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
          // No params signifies intercept all
          if (
            urlPattern.protocol === undefined &&
            urlPattern.hostname === undefined &&
            urlPattern.port === undefined &&
            urlPattern.pathname === undefined &&
            urlPattern.search === undefined
          ) {
            return urlPattern;
          }

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
            new URL(buildUrlPatternString(urlPattern));
          } catch (error) {
            throw new InvalidArgumentException(`${error}`);
          }
          return urlPattern;
      }
    });
  }
}
