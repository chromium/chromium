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
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';

import type {NetworkRequest} from './NetworkRequest.js';
import type {NetworkStorage} from './NetworkStorage.js';
import {
  cdpFetchHeadersFromBidiNetworkHeaders,
  cdpAuthChallengeResponseFromBidiAuthContinueWithAuthAction,
} from './NetworkUtils.js';

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
    this.#browsingContextStorage.verifyTopLevelContextsList(params.contexts);

    const urlPatterns: Network.UrlPattern[] = params.urlPatterns ?? [];
    const parsedUrlPatterns: Network.UrlPattern[] =
      NetworkProcessor.parseUrlPatterns(urlPatterns);

    const intercept: Network.Intercept = this.#networkStorage.addIntercept({
      urlPatterns: parsedUrlPatterns,
      phases: params.phases,
      contexts: params.contexts,
    });

    await Promise.all(
      this.#browsingContextStorage.getAllContexts().map((context) => {
        return context.cdpTarget.toggleFetchIfNeeded();
      })
    );

    return {
      intercept,
    };
  }

  async continueRequest(
    params: Network.ContinueRequestParameters
  ): Promise<EmptyResult> {
    const {
      url,
      method,
      headers: commandHeaders,
      body,
      request: networkId,
    } = params;

    if (params.url !== undefined) {
      NetworkProcessor.parseUrlString(params.url);
    }

    if (params.headers) {
      NetworkProcessor.validateHeaders(params.headers);
    }

    const request = this.#getBlockedRequestOrFail(networkId, [
      Network.InterceptPhase.BeforeRequestSent,
    ]);

    const headers: Protocol.Fetch.HeaderEntry[] | undefined =
      cdpFetchHeadersFromBidiNetworkHeaders(commandHeaders);

    // TODO: Set / expand.
    // ; Step 9. cookies
    try {
      await request.continueRequest({
        url,
        method,
        headers,
        postData: getCdpBodyFromBiDiBytesValue(body),
      });
    } catch (error) {
      // https://source.chromium.org/chromium/chromium/src/+/main:content/browser/devtools/protocol/fetch_handler.cc;l=169
      if ((error as any)?.message.includes('Invalid header')) {
        throw new InvalidArgumentException('Tried setting invalid header');
      }
      throw error;
    }

    return {};
  }

  async continueResponse(
    params: Network.ContinueResponseParameters
  ): Promise<EmptyResult> {
    const {request: networkId, statusCode, reasonPhrase, headers} = params;

    if (params.headers) {
      NetworkProcessor.validateHeaders(params.headers);
    }

    const responseHeaders: Protocol.Fetch.HeaderEntry[] | undefined =
      cdpFetchHeadersFromBidiNetworkHeaders(headers);

    const request = this.#getBlockedRequestOrFail(networkId, [
      Network.InterceptPhase.AuthRequired,
      Network.InterceptPhase.ResponseStarted,
    ]);

    if (request.interceptPhase === Network.InterceptPhase.AuthRequired) {
      if (params.credentials) {
        await Promise.all([
          request.waitNextPhase,
          request.continueWithAuth({
            response: 'ProvideCredentials',
            username: params.credentials.username,
            password: params.credentials.password,
          }),
        ]);
      } else {
        // We need to use `ProvideCredentials`
        // As `Default` may cancel the request
        await request.continueWithAuth({
          response: 'ProvideCredentials',
        });
        return {};
      }
    }

    if (request.interceptPhase === Network.InterceptPhase.ResponseStarted) {
      // TODO: Set / expand.
      // ; Step 10. cookies
      await request.continueResponse({
        responseCode: statusCode,
        responsePhrase: reasonPhrase,
        responseHeaders,
      });
    }

    return {};
  }

  async continueWithAuth(
    params: Network.ContinueWithAuthParameters
  ): Promise<EmptyResult> {
    const networkId = params.request;
    const request = this.#getBlockedRequestOrFail(networkId, [
      Network.InterceptPhase.AuthRequired,
    ]);

    let username: string | undefined;
    let password: string | undefined;

    if (params.action === 'provideCredentials') {
      const {credentials} = params;

      username = credentials.username;
      password = credentials.password;
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
    if (request.interceptPhase === Network.InterceptPhase.AuthRequired) {
      throw new InvalidArgumentException(
        `Request '${networkId}' in 'authRequired' phase cannot be failed`
      );
    }
    if (!request.interceptPhase) {
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
      reasonPhrase: responsePhrase,
      headers,
      body,
      request: networkId,
    } = params;

    if (params.headers) {
      NetworkProcessor.validateHeaders(params.headers);
    }

    // TODO: Step 6
    // https://w3c.github.io/webdriver-bidi/#command-network-continueResponse
    const responseHeaders: Protocol.Fetch.HeaderEntry[] | undefined =
      cdpFetchHeadersFromBidiNetworkHeaders(headers);

    // TODO: Set / expand.
    // ; Step 10. cookies
    const request = this.#getBlockedRequestOrFail(networkId, [
      Network.InterceptPhase.BeforeRequestSent,
      Network.InterceptPhase.ResponseStarted,
      Network.InterceptPhase.AuthRequired,
    ]);

    // We need to pass through if the request is already in
    // AuthRequired phase
    if (request.interceptPhase === Network.InterceptPhase.AuthRequired) {
      // We need to use `ProvideCredentials`
      // As `Default` may cancel the request
      await request.continueWithAuth({
        response: 'ProvideCredentials',
      });
      return {};
    }

    // If we don't modify the response
    // just continue the request
    if (!body && !headers) {
      await request.continueRequest();
      return {};
    }

    const responseCode = statusCode ?? request.statusCode ?? 200;

    try {
      await request.provideResponse({
        responseCode,
        responsePhrase,
        responseHeaders,
        body: getCdpBodyFromBiDiBytesValue(body),
      });
    } catch (error) {
      // https://source.chromium.org/chromium/chromium/src/+/main:content/browser/devtools/protocol/fetch_handler.cc;l=169
      if ((error as any)?.message.includes('Invalid header')) {
        throw new InvalidArgumentException('Tried setting invalid header');
      }
      throw error;
    }
    return {};
  }

  async removeIntercept(
    params: Network.RemoveInterceptParameters
  ): Promise<EmptyResult> {
    this.#networkStorage.removeIntercept(params.intercept);

    await Promise.all(
      this.#browsingContextStorage.getAllContexts().map((context) => {
        return context.cdpTarget.toggleFetchIfNeeded();
      })
    );

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
    phases: Network.InterceptPhase[]
  ): NetworkRequest {
    const request = this.#getRequestOrFail(id);
    if (!request.interceptPhase) {
      throw new NoSuchRequestException(
        `No blocked request found for network id '${id}'`
      );
    }
    if (request.interceptPhase && !phases.includes(request.interceptPhase)) {
      throw new InvalidArgumentException(
        `Blocked request for network id '${id}' is in '${request.interceptPhase}' phase`
      );
    }

    return request;
  }

  /**
   * Validate https://fetch.spec.whatwg.org/#header-value
   */
  static validateHeaders(headers: Network.Header[]) {
    for (const header of headers) {
      let headerValue: string;
      if (header.value.type === 'string') {
        headerValue = header.value.value;
      } else {
        headerValue = atob(header.value.value);
      }

      if (
        headerValue !== headerValue.trim() ||
        headerValue.includes('\n') ||
        headerValue.includes('\0')
      ) {
        throw new InvalidArgumentException(
          `Header value '${headerValue}' is not acceptable value`
        );
      }
    }
  }

  /**
   * Attempts to parse the given url.
   * Throws an InvalidArgumentException if the url is invalid.
   */
  static parseUrlString(url: string) {
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

          if (urlPattern.protocol) {
            urlPattern.protocol = unescapeURLPattern(urlPattern.protocol);
            if (!urlPattern.protocol.match(/^[a-zA-Z+-.]+$/)) {
              throw new InvalidArgumentException('Forbidden characters');
            }
          }
          if (urlPattern.hostname) {
            urlPattern.hostname = unescapeURLPattern(urlPattern.hostname);
          }
          if (urlPattern.port) {
            urlPattern.port = unescapeURLPattern(urlPattern.port);
          }
          if (urlPattern.pathname) {
            urlPattern.pathname = unescapeURLPattern(urlPattern.pathname);
            if (urlPattern.pathname[0] !== '/') {
              urlPattern.pathname = `/${urlPattern.pathname}`;
            }
            if (
              urlPattern.pathname.includes('#') ||
              urlPattern.pathname.includes('?')
            ) {
              throw new InvalidArgumentException('Forbidden characters');
            }
          } else if (urlPattern.pathname === '') {
            urlPattern.pathname = '/';
          }

          if (urlPattern.search) {
            urlPattern.search = unescapeURLPattern(urlPattern.search);
            if (urlPattern.search[0] !== '?') {
              urlPattern.search = `?${urlPattern.search}`;
            }
            if (urlPattern.search.includes('#')) {
              throw new InvalidArgumentException('Forbidden characters');
            }
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
            new URLPattern(urlPattern);
          } catch (error) {
            throw new InvalidArgumentException(`${error}`);
          }

          return urlPattern;
      }
    });
  }
}

/**
 * See https://w3c.github.io/webdriver-bidi/#unescape-url-pattern
 */
function unescapeURLPattern(pattern: string) {
  const forbidden = new Set(['(', ')', '*', '{', '}']);
  let result = '';
  let isEscaped = false;
  for (const c of pattern) {
    if (!isEscaped) {
      if (forbidden.has(c)) {
        throw new InvalidArgumentException('Forbidden characters');
      }
      if (c === '\\') {
        isEscaped = true;
        continue;
      }
    }
    result += c;
    isEscaped = false;
  }
  return result;
}

function getCdpBodyFromBiDiBytesValue(
  body?: Network.BytesValue
): string | undefined {
  let parsedBody: string | undefined;
  if (body?.type === 'string') {
    parsedBody = btoa(body.value);
  } else if (body?.type === 'base64') {
    parsedBody = body.value;
  }
  return parsedBody;
}
