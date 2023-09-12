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
  type Network,
  type EmptyResult,
  UnknownCommandException,
  InvalidArgumentException,
} from '../../../protocol/protocol.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';

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

    // TODO: Add try/catch. Remove the intercept if CDP Fetch commands fail.
    await this.#applyIntercepts();

    return {
      intercept,
    };
  }

  continueRequest(_params: Network.ContinueRequestParameters): EmptyResult {
    throw new UnknownCommandException('Not implemented yet.');
  }

  continueResponse(_params: Network.ContinueResponseParameters): EmptyResult {
    throw new UnknownCommandException('Not implemented yet.');
  }

  continueWithAuth(_params: Network.ContinueWithAuthParameters): EmptyResult {
    throw new UnknownCommandException('Not implemented yet.');
  }

  failRequest(_params: Network.FailRequestParameters): EmptyResult {
    throw new UnknownCommandException('Not implemented yet.');
  }

  provideResponse(_params: Network.ProvideResponseParameters): EmptyResult {
    throw new UnknownCommandException('Not implemented yet.');
  }

  async removeIntercept(
    params: Network.RemoveInterceptParameters
  ): Promise<EmptyResult> {
    this.#networkStorage.removeIntercept(params.intercept);

    // TODO: Add try/catch. Remove the intercept if CDP Fetch commands fail.
    await this.#applyIntercepts();

    return {};
  }

  /** Applies all existing network intercepts to all CDP targets concurrently. */
  async #applyIntercepts() {
    await Promise.all(
      this.#browsingContextStorage.getAllContexts().map(async (context) => {
        await context.cdpTarget.fetchApply();
      })
    );
  }

  static parseUrlPatterns(
    urlPatterns: Network.UrlPattern[]
  ): Network.UrlPattern[] {
    return urlPatterns.map((urlPattern) => {
      switch (urlPattern.type) {
        case 'string': {
          try {
            new URL(urlPattern.pattern);
          } catch (error) {
            throw new InvalidArgumentException(
              `Invalid URL '${urlPattern.pattern}': ${error}`
            );
          }
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
