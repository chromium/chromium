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

import {
  Network,
  type EmptyResult,
  NoSuchRequestException,
  InvalidArgumentException,
} from '../../../protocol/protocol.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';

import type {NetworkRequest} from './NetworkRequest.js';
import type {NetworkStorage} from './NetworkStorage.js';
import {isSpecialScheme, type ParsedUrlPattern} from './NetworkUtils.js';

/** Dispatches Network domain commands. */
export class NetworkProcessor {
  readonly #browsingContextStorage: BrowsingContextStorage;
  readonly #networkStorage: NetworkStorage;

  constructor(
    browsingContextStorage: BrowsingContextStorage,
    networkStorage: NetworkStorage,
  ) {
    this.#browsingContextStorage = browsingContextStorage;
    this.#networkStorage = networkStorage;
  }

  async addIntercept(
    params: Network.AddInterceptParameters,
  ): Promise<Network.AddInterceptResult> {
    this.#browsingContextStorage.verifyTopLevelContextsList(params.contexts);

    const urlPatterns: Network.UrlPattern[] = params.urlPatterns ?? [];
    const parsedUrlPatterns: ParsedUrlPattern[] =
      NetworkProcessor.parseUrlPatterns(urlPatterns);

    const intercept: Network.Intercept = this.#networkStorage.addIntercept({
      urlPatterns: parsedUrlPatterns,
      phases: params.phases,
      contexts: params.contexts,
    });

    await Promise.all(
      this.#browsingContextStorage.getAllContexts().map((context) => {
        return context.cdpTarget.toggleNetwork();
      }),
    );

    return {
      intercept,
    };
  }

  async continueRequest(
    params: Network.ContinueRequestParameters,
  ): Promise<EmptyResult> {
    if (params.url !== undefined) {
      NetworkProcessor.parseUrlString(params.url);
    }

    if (params.method !== undefined) {
      if (!NetworkProcessor.isMethodValid(params.method)) {
        throw new InvalidArgumentException(
          `Method '${params.method}' is invalid.`,
        );
      }
    }

    if (params.headers) {
      NetworkProcessor.validateHeaders(params.headers);
    }

    const request = this.#getBlockedRequestOrFail(params.request, [
      Network.InterceptPhase.BeforeRequestSent,
    ]);

    try {
      await request.continueRequest(params);
    } catch (error) {
      throw NetworkProcessor.wrapInterceptionError(error);
    }

    return {};
  }

  async continueResponse(
    params: Network.ContinueResponseParameters,
  ): Promise<EmptyResult> {
    if (params.headers) {
      NetworkProcessor.validateHeaders(params.headers);
    }

    const request = this.#getBlockedRequestOrFail(params.request, [
      Network.InterceptPhase.AuthRequired,
      Network.InterceptPhase.ResponseStarted,
    ]);

    try {
      await request.continueResponse(params);
    } catch (error) {
      throw NetworkProcessor.wrapInterceptionError(error);
    }

    return {};
  }

  async continueWithAuth(
    params: Network.ContinueWithAuthParameters,
  ): Promise<EmptyResult> {
    const networkId = params.request;
    const request = this.#getBlockedRequestOrFail(networkId, [
      Network.InterceptPhase.AuthRequired,
    ]);

    await request.continueWithAuth(params);

    return {};
  }

  async failRequest({
    request: networkId,
  }: Network.FailRequestParameters): Promise<EmptyResult> {
    const request = this.#getRequestOrFail(networkId);
    if (request.interceptPhase === Network.InterceptPhase.AuthRequired) {
      throw new InvalidArgumentException(
        `Request '${networkId}' in 'authRequired' phase cannot be failed`,
      );
    }
    if (!request.interceptPhase) {
      throw new NoSuchRequestException(
        `No blocked request found for network id '${networkId}'`,
      );
    }

    await request.failRequest('Failed');

    return {};
  }

  async provideResponse(
    params: Network.ProvideResponseParameters,
  ): Promise<EmptyResult> {
    if (params.headers) {
      NetworkProcessor.validateHeaders(params.headers);
    }

    const request = this.#getBlockedRequestOrFail(params.request, [
      Network.InterceptPhase.BeforeRequestSent,
      Network.InterceptPhase.ResponseStarted,
      Network.InterceptPhase.AuthRequired,
    ]);

    try {
      await request.provideResponse(params);
    } catch (error) {
      throw NetworkProcessor.wrapInterceptionError(error);
    }

    return {};
  }

  async removeIntercept(
    params: Network.RemoveInterceptParameters,
  ): Promise<EmptyResult> {
    this.#networkStorage.removeIntercept(params.intercept);

    await Promise.all(
      this.#browsingContextStorage.getAllContexts().map((context) => {
        return context.cdpTarget.toggleNetwork();
      }),
    );

    return {};
  }

  async setCacheBehavior(
    params: Network.SetCacheBehaviorParameters,
  ): Promise<EmptyResult> {
    const contexts = this.#browsingContextStorage.verifyTopLevelContextsList(
      params.contexts,
    );

    // Change all targets
    if (contexts.size === 0) {
      this.#networkStorage.defaultCacheBehavior = params.cacheBehavior;

      await Promise.all(
        this.#browsingContextStorage.getAllContexts().map((context) => {
          return context.cdpTarget.toggleSetCacheDisabled();
        }),
      );

      return {};
    }

    const cacheDisabled = params.cacheBehavior === 'bypass';

    await Promise.all(
      [...contexts.values()].map((context) => {
        return context.cdpTarget.toggleSetCacheDisabled(cacheDisabled);
      }),
    );

    return {};
  }

  #getRequestOrFail(id: Network.Request): NetworkRequest {
    const request = this.#networkStorage.getRequestById(id);
    if (!request) {
      throw new NoSuchRequestException(
        `Network request with ID '${id}' doesn't exist`,
      );
    }
    return request;
  }

  #getBlockedRequestOrFail(
    id: Network.Request,
    phases: Network.InterceptPhase[],
  ): NetworkRequest {
    const request = this.#getRequestOrFail(id);
    if (!request.interceptPhase) {
      throw new NoSuchRequestException(
        `No blocked request found for network id '${id}'`,
      );
    }
    if (request.interceptPhase && !phases.includes(request.interceptPhase)) {
      throw new InvalidArgumentException(
        `Blocked request for network id '${id}' is in '${request.interceptPhase}' phase`,
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
          `Header value '${headerValue}' is not acceptable value`,
        );
      }
    }
  }

  static isMethodValid(method: string) {
    // https://httpwg.org/specs/rfc9110.html#method.overview
    return /^[!#$%&'*+\-.^_`|~a-zA-Z\d]+$/.test(method);
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
    urlPatterns: Network.UrlPattern[],
  ): ParsedUrlPattern[] {
    return urlPatterns.map((urlPattern) => {
      let patternUrl = '';
      let hasProtocol = true;
      let hasHostname = true;
      let hasPort = true;
      let hasPathname = true;
      let hasSearch = true;

      switch (urlPattern.type) {
        case 'string': {
          patternUrl = unescapeURLPattern(urlPattern.pattern);
          break;
        }
        case 'pattern': {
          if (urlPattern.protocol === undefined) {
            hasProtocol = false;
            patternUrl += 'http';
          } else {
            if (urlPattern.protocol === '') {
              throw new InvalidArgumentException(
                'URL pattern must specify a protocol',
              );
            }
            urlPattern.protocol = unescapeURLPattern(urlPattern.protocol);
            if (!urlPattern.protocol.match(/^[a-zA-Z+-.]+$/)) {
              throw new InvalidArgumentException('Forbidden characters');
            }
            patternUrl += urlPattern.protocol;
          }
          const scheme = patternUrl.toLocaleLowerCase();
          patternUrl += ':';
          if (isSpecialScheme(scheme)) {
            patternUrl += '//';
          }
          if (urlPattern.hostname === undefined) {
            if (scheme !== 'file') {
              patternUrl += 'placeholder';
            }
            hasHostname = false;
          } else {
            if (urlPattern.hostname === '') {
              throw new InvalidArgumentException(
                'URL pattern must specify a hostname',
              );
            }
            if (urlPattern.protocol === 'file') {
              throw new InvalidArgumentException(
                `URL pattern protocol cannot be 'file'`,
              );
            }

            urlPattern.hostname = unescapeURLPattern(urlPattern.hostname);

            let insideBrackets = false;

            for (const c of urlPattern.hostname) {
              if (c === '/' || c === '?' || c === '#') {
                throw new InvalidArgumentException(
                  `'/', '?', '#' are forbidden in hostname`,
                );
              }
              if (!insideBrackets && c === ':') {
                throw new InvalidArgumentException(
                  `':' is only allowed inside brackets in hostname`,
                );
              }
              if (c === '[') {
                insideBrackets = true;
              }
              if (c === ']') {
                insideBrackets = false;
              }
            }

            patternUrl += urlPattern.hostname;
          }
          if (urlPattern.port === undefined) {
            hasPort = false;
          } else {
            if (urlPattern.port === '') {
              throw new InvalidArgumentException(
                `URL pattern must specify a port`,
              );
            }
            urlPattern.port = unescapeURLPattern(urlPattern.port);

            patternUrl += ':';

            if (!urlPattern.port.match(/^\d+$/)) {
              throw new InvalidArgumentException('Forbidden characters');
            }

            patternUrl += urlPattern.port;
          }

          if (urlPattern.pathname === undefined) {
            hasPathname = false;
          } else {
            urlPattern.pathname = unescapeURLPattern(urlPattern.pathname);
            if (urlPattern.pathname[0] !== '/') {
              patternUrl += '/';
            }
            if (
              urlPattern.pathname.includes('#') ||
              urlPattern.pathname.includes('?')
            ) {
              throw new InvalidArgumentException('Forbidden characters');
            }
            patternUrl += urlPattern.pathname;
          }

          if (urlPattern.search === undefined) {
            hasSearch = false;
          } else {
            urlPattern.search = unescapeURLPattern(urlPattern.search);
            if (urlPattern.search[0] !== '?') {
              patternUrl += '?';
            }
            if (urlPattern.search.includes('#')) {
              throw new InvalidArgumentException('Forbidden characters');
            }
            patternUrl += urlPattern.search;
          }
          break;
        }
      }

      const serializePort = (url: URL) => {
        const defaultPorts: {[key: string]: null | number} = {
          'ftp:': 21,
          'file:': null,
          'http:': 80,
          'https:': 443,
          'ws:': 80,
          'wss:': 443,
        };
        if (
          isSpecialScheme(url.protocol) &&
          defaultPorts[url.protocol] !== null &&
          (!url.port || String(defaultPorts[url.protocol]) === url.port)
        ) {
          return '';
        } else if (url.port) {
          return url.port;
        }
        return undefined;
      };

      try {
        const url = new URL(patternUrl);
        return {
          protocol: hasProtocol ? url.protocol.replace(/:$/, '') : undefined,
          hostname: hasHostname ? url.hostname : undefined,
          port: hasPort ? serializePort(url) : undefined,
          pathname: hasPathname && url.pathname ? url.pathname : undefined,
          search: hasSearch ? url.search : undefined,
        };
      } catch (err) {
        throw new InvalidArgumentException(
          `${(err as Error).message} '${patternUrl}'`,
        );
      }
    });
  }

  static wrapInterceptionError(error: any) {
    // https://source.chromium.org/chromium/chromium/src/+/main:content/browser/devtools/protocol/fetch_handler.cc;l=169
    if (error?.message.includes('Invalid header')) {
      return new InvalidArgumentException('Invalid header');
    }
    return error;
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
