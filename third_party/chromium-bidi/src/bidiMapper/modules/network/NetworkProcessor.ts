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
  UnsupportedOperationException,
} from '../../../protocol/protocol.js';
import type {ContextConfigStorage} from '../browser/ContextConfigStorage.js';
import type {UserContextStorage} from '../browser/UserContextStorage.js';
import type {BrowsingContextImpl} from '../context/BrowsingContextImpl.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';

import type {NetworkRequest} from './NetworkRequest.js';
import type {NetworkStorage} from './NetworkStorage.js';
import {isSpecialScheme, type ParsedUrlPattern} from './NetworkUtils.js';

/** Dispatches Network module commands. */
export class NetworkProcessor {
  readonly #browsingContextStorage: BrowsingContextStorage;
  readonly #networkStorage: NetworkStorage;
  readonly #userContextStorage: UserContextStorage;
  readonly #contextConfigStorage: ContextConfigStorage;

  constructor(
    browsingContextStorage: BrowsingContextStorage,
    networkStorage: NetworkStorage,
    userContextStorage: UserContextStorage,
    contextConfigStorage: ContextConfigStorage,
  ) {
    this.#userContextStorage = userContextStorage;
    this.#browsingContextStorage = browsingContextStorage;
    this.#networkStorage = networkStorage;
    this.#contextConfigStorage = contextConfigStorage;
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

    // Adding interception may require enabling CDP Network domains.
    await this.#toggleNetwork();

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

  /**
   * In some states CDP Network and Fetch domains are not required, but in some they have
   * to be updated. Whenever potential change in these kinds of states is introduced,
   * update the states of all the CDP targets.
   */
  async #toggleNetwork() {
    await Promise.all(
      this.#browsingContextStorage.getAllContexts().map((context) => {
        return context.cdpTarget.toggleNetwork();
      }),
    );
  }

  async removeIntercept(
    params: Network.RemoveInterceptParameters,
  ): Promise<EmptyResult> {
    this.#networkStorage.removeIntercept(params.intercept);

    // Removing interception may allow for disabling CDP Network domains.
    await this.#toggleNetwork();

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
    if (
      error?.message.includes('Invalid header') ||
      error?.message.includes('Unsafe header')
    ) {
      return new InvalidArgumentException(error.message);
    }
    return error;
  }

  async addDataCollector(
    params: Network.AddDataCollectorParameters,
  ): Promise<Network.AddDataCollectorResult> {
    if (params.userContexts !== undefined && params.contexts !== undefined) {
      throw new InvalidArgumentException(
        "'contexts' and 'userContexts' are mutually exclusive",
      );
    }
    if (params.userContexts !== undefined) {
      // Assert the user contexts exist.
      await this.#userContextStorage.verifyUserContextIdList(
        params.userContexts,
      );
    }
    if (params.contexts !== undefined) {
      for (const browsingContextId of params.contexts) {
        // Assert the browsing context exists and are top-level.
        const browsingContext =
          this.#browsingContextStorage.getContext(browsingContextId);
        if (!browsingContext.isTopLevelContext()) {
          throw new InvalidArgumentException(
            `Data collectors are available only on top-level browsing contexts`,
          );
        }
      }
    }
    const collectorId = this.#networkStorage.addDataCollector(params);

    // Adding data collectors may require enabling CDP Network domains.
    await this.#toggleNetwork();

    return {collector: collectorId};
  }

  async getData(
    params: Network.GetDataParameters,
  ): Promise<Network.GetDataResult> {
    return await this.#networkStorage.getCollectedData(params);
  }

  async removeDataCollector(
    params: Network.RemoveDataCollectorParameters,
  ): Promise<EmptyResult> {
    this.#networkStorage.removeDataCollector(params);

    // Removing data collectors may allow disabling CDP Network domains.
    await this.#toggleNetwork();

    return {};
  }

  disownData(params: Network.DisownDataParameters): EmptyResult {
    this.#networkStorage.disownData(params);
    return {};
  }

  async #getRelatedTopLevelBrowsingContexts(
    browsingContextIds?: string[],
    userContextIds?: string[],
  ): Promise<BrowsingContextImpl[]> {
    // Duplicated with EmulationProcessor logic. Consider moving to ConfigStorage.
    if (browsingContextIds === undefined && userContextIds === undefined) {
      return this.#browsingContextStorage.getTopLevelContexts();
    }

    if (browsingContextIds !== undefined && userContextIds !== undefined) {
      throw new InvalidArgumentException(
        'User contexts and browsing contexts are mutually exclusive',
      );
    }

    const result = [];
    if (userContextIds !== undefined) {
      if (userContextIds.length === 0) {
        throw new InvalidArgumentException('user context should be provided');
      }

      // Verify that all user contexts exist.
      await this.#userContextStorage.verifyUserContextIdList(userContextIds!);

      for (const userContextId of userContextIds!) {
        const topLevelBrowsingContexts = this.#browsingContextStorage
          .getTopLevelContexts()
          .filter(
            (browsingContext) => browsingContext.userContext === userContextId,
          );
        result.push(...topLevelBrowsingContexts);
      }
    }
    if (browsingContextIds !== undefined) {
      if (browsingContextIds.length === 0) {
        throw new InvalidArgumentException(
          'browsing context should be provided',
        );
      }

      for (const browsingContextId of browsingContextIds) {
        const browsingContext =
          this.#browsingContextStorage.getContext(browsingContextId);
        if (!browsingContext.isTopLevelContext()) {
          throw new InvalidArgumentException(
            'The command is only supported on the top-level context',
          );
        }
        result.push(browsingContext);
      }
    }
    // Remove duplicates. Compare `BrowsingContextImpl` by reference is correct here, as
    // `browsingContextStorage` returns the same instance for the same id.
    return [...new Set(result).values()];
  }

  async setExtraHeaders(
    params: Network.SetExtraHeadersParameters,
  ): Promise<EmptyResult> {
    const affectedBrowsingContexts =
      await this.#getRelatedTopLevelBrowsingContexts(
        params.contexts,
        params.userContexts,
      );

    const cdpExtraHeaders = parseBiDiHeaders(params.headers);

    if (params.userContexts === undefined && params.contexts === undefined) {
      this.#contextConfigStorage.updateGlobalConfig({
        extraHeaders: cdpExtraHeaders,
      });
    }

    if (params.userContexts !== undefined) {
      params.userContexts.forEach((userContext) => {
        this.#contextConfigStorage.updateUserContextConfig(userContext, {
          extraHeaders: cdpExtraHeaders,
        });
      });
    }
    if (params.contexts !== undefined) {
      params.contexts.forEach((browsingContextId) => {
        this.#contextConfigStorage.updateBrowsingContextConfig(
          browsingContextId,
          {extraHeaders: cdpExtraHeaders},
        );
      });
    }

    await Promise.all(
      affectedBrowsingContexts.map(async (context) => {
        // Actual value can be different from the one in params, e.g. in case of already
        // existing setting.
        const extraHeaders =
          this.#contextConfigStorage.getActiveConfig(
            context.id,
            context.userContext,
          ).extraHeaders ?? {};

        await context.setExtraHeaders(extraHeaders);
      }),
    );

    return {};
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

// https://fetch.spec.whatwg.org/#header-name
const FORBIDDEN_HEADER_NAME_SYMBOLS = new Set([
  ' ',
  '\t',
  '\n',
  '"',
  '(',
  ')',
  ',',
  '/',
  ':',
  ';',
  '<',
  '=',
  '>',
  '?',
  '@',
  '[',
  '\\',
  ']',
  '{',
  '}',
]);

// https://fetch.spec.whatwg.org/#header-value
const FORBIDDEN_HEADER_VALUE_SYMBOLS = new Set(['\0', '\n', '\r']);

function includesChar(str: string, chars: Set<string>) {
  for (const char of str) {
    if (chars.has(char)) {
      return true;
    }
  }
  return false;
}

// Export for testing.
export function parseBiDiHeaders(
  headers: Network.Header[],
): Protocol.Network.Headers {
  const parsedHeaders: Protocol.Network.Headers = {};
  for (const bidiHeader of headers) {
    if (bidiHeader.value.type === 'string') {
      const name = bidiHeader.name;
      const value = bidiHeader.value.value;

      if (name.length === 0) {
        throw new InvalidArgumentException(`Empty header name is not allowed`);
      }

      if (includesChar(name, FORBIDDEN_HEADER_NAME_SYMBOLS)) {
        throw new InvalidArgumentException(
          `Header name '${name}' contains forbidden symbols`,
        );
      }

      if (includesChar(value, FORBIDDEN_HEADER_VALUE_SYMBOLS)) {
        throw new InvalidArgumentException(
          `Header value '${value}' contains forbidden symbols`,
        );
      }

      if (value.trim() !== value) {
        throw new InvalidArgumentException(
          `Header value should not contain trailing or ending whitespaces`,
        );
      }

      // BiDi spec does not combine but overrides the headers with the same names.
      // https://www.w3.org/TR/webdriver-bidi/#update-headers
      parsedHeaders[bidiHeader.name] = bidiHeader.value.value;
    } else {
      throw new UnsupportedOperationException(
        'Only string headers values are supported',
      );
    }
  }
  return parsedHeaders;
}
