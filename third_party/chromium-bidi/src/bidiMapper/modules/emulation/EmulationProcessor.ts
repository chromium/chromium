/**
 * Copyright 2025 Google LLC.
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

import {InvalidArgumentException} from '../../../protocol/ErrorResponse';
import type {
  EmptyResult,
  Emulation,
} from '../../../protocol/generated/webdriver-bidi';
import type {UserContextStorage} from '../browser/UserContextStorage';
import type {BrowsingContextImpl} from '../context/BrowsingContextImpl';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage';

export class EmulationProcessor {
  #userContextStorage: UserContextStorage;
  #browsingContextStorage: BrowsingContextStorage;

  constructor(
    browsingContextStorage: BrowsingContextStorage,
    userContextStorage: UserContextStorage,
  ) {
    this.#userContextStorage = userContextStorage;
    this.#browsingContextStorage = browsingContextStorage;
  }

  async setGeolocationOverride(
    params: Emulation.SetGeolocationOverrideParameters,
  ): Promise<EmptyResult> {
    if (
      (params.coordinates?.altitude ?? null) === null &&
      (params.coordinates?.altitudeAccuracy ?? null) !== null
    ) {
      throw new InvalidArgumentException(
        'Geolocation altitudeAccuracy can be set only with altitude',
      );
    }

    const browsingContexts = await this.#getRelatedTopLevelBrowsingContexts(
      params.contexts,
      params.userContexts,
    );

    for (const userContextId of params.userContexts ?? []) {
      const userContextConfig =
        this.#userContextStorage.getConfig(userContextId);
      userContextConfig.emulatedGeolocation = params.coordinates;
    }

    await Promise.all(
      browsingContexts.map(
        async (context) =>
          await context.cdpTarget.setGeolocationOverride(params.coordinates),
      ),
    );
    return {};
  }

  /**
   * Returns a list of top-level browsing contexts.
   */
  async #getRelatedTopLevelBrowsingContexts(
    browsingContextIds?: string[],
    userContextIds?: string[],
  ): Promise<BrowsingContextImpl[]> {
    if (browsingContextIds === undefined && userContextIds === undefined) {
      throw new InvalidArgumentException(
        'Either user contexts or browsing contexts must be provided',
      );
    }

    if (browsingContextIds !== undefined && userContextIds !== undefined) {
      throw new InvalidArgumentException(
        'User contexts and browsing contexts are mutually exclusive',
      );
    }

    const result = [];
    if (browsingContextIds === undefined) {
      // userContextIds !== undefined
      if (userContextIds!.length === 0) {
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
    } else {
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
}
