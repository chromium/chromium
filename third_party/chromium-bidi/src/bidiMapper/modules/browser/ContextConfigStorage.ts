/*
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

import {
  type Browser,
  type BrowsingContext,
  InvalidArgumentException,
} from '../../../protocol/protocol.js';
import type {BrowsingContextImpl} from '../context/BrowsingContextImpl.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';

import {ContextConfig} from './ContextConfig.js';
import type {UserContextStorage} from './UserContextStorage.js';

/**
 * Manages context-specific configurations. This class allows setting
 * configurations at three levels: global, user context, and browsing context.
 *
 * When `getActiveConfig` is called, it merges the configurations in a specific
 * order of precedence: `global -> user context -> browsing context`. For each
 * configuration property, the value from the highest-precedence level that has a
 * non-`undefined` value is used.
 *
 * `updateConfig` merges the provided configuration with the existing one at the
 * corresponding level. Properties with `undefined` values in the provided
 * configuration are ignored, preserving the existing value.
 */
export class ContextConfigStorage {
  readonly #browsingContextStorage: BrowsingContextStorage;
  readonly #userContextStorage: UserContextStorage;
  #global = new ContextConfig();
  #userContextConfigs = new Map<string, ContextConfig>();
  #browsingContextConfigs = new Map<string, ContextConfig>();

  constructor(
    browsingContextStorage: BrowsingContextStorage,
    userContextStorage: UserContextStorage,
  ) {
    this.#browsingContextStorage = browsingContextStorage;
    this.#userContextStorage = userContextStorage;
  }

  /**
   * Updates the global configuration. Properties with `undefined` values in the
   * provided `config` are ignored.
   */
  #updateGlobalConfig(config: ContextConfig): void {
    this.#global = ContextConfig.merge(this.#global, config);
  }

  /**
   * Updates the configuration for a specific browsing context. Properties with
   * `undefined` values in the provided `config` are ignored.
   */
  #updateBrowsingContextConfig(
    browsingContextId: string,
    config: ContextConfig,
  ): void {
    this.#browsingContextConfigs.set(
      browsingContextId,
      ContextConfig.merge(
        this.#browsingContextConfigs.get(browsingContextId),
        config,
      ),
    );
  }

  /**
   * Updates the configuration for a specific user context. Properties with
   * `undefined` values in the provided `config` are ignored.
   */
  #updateUserContextConfig(userContext: string, config: ContextConfig): void {
    this.#userContextConfigs.set(
      userContext,
      ContextConfig.merge(this.#userContextConfigs.get(userContext), config),
    );
  }

  /**
   * Returns the current global configuration.
   */
  getGlobalConfig(): ContextConfig {
    return this.#global;
  }

  /**
   * Extra headers is a special case. The headers from the different levels have to be
   * merged instead of being overridden.
   */
  #getExtraHeaders(
    topLevelBrowsingContextId: string | undefined,
    userContext: string,
  ) {
    const globalHeaders = this.#global.extraHeaders ?? {};
    const userContextHeaders =
      this.#userContextConfigs.get(userContext)?.extraHeaders ?? {};
    const browsingContextHeaders =
      topLevelBrowsingContextId === undefined
        ? {}
        : (this.#browsingContextConfigs.get(topLevelBrowsingContextId)
            ?.extraHeaders ?? {});

    return {...globalHeaders, ...userContextHeaders, ...browsingContextHeaders};
  }

  /**
   * Calculates the active configuration by merging global, user context, and
   * browsing context settings.
   */
  getActiveConfig(
    topLevelBrowsingContextId: string | undefined,
    userContext: string,
  ): ContextConfig {
    let result = ContextConfig.merge(
      this.#global,
      this.#userContextConfigs.get(userContext),
    );
    if (topLevelBrowsingContextId !== undefined) {
      result = ContextConfig.merge(
        result,
        this.#browsingContextConfigs.get(topLevelBrowsingContextId),
      );
    }

    // Extra headers is a special case which have to be treated in a special way.
    const extraHeaders = this.#getExtraHeaders(
      topLevelBrowsingContextId,
      userContext,
    );
    result.extraHeaders =
      Object.keys(extraHeaders).length > 0 ? extraHeaders : undefined;

    return result;
  }

  /**
   * Updates the configuration at the browsing context, user context, or global
   * level, and returns the related top-level browsing contexts.
   *
   * `browsingContextIds` and `userContextIds` are mutually exclusive:
   * - To update specific top-level browsing contexts, pass `browsingContextIds`
   *   and omit `userContextIds`: `updateConfig(config, browsingContextIds)`.
   * - To update specific user contexts, pass `undefined` for
   *   `browsingContextIds` and provide `userContextIds`:
   *   `updateConfig(config, undefined, userContextIds)`.
   * - To update the global configuration, omit both (or pass `undefined` for
   *   both): `updateConfig(config)`.
   *
   * Properties with `undefined` values in `config` are ignored, preserving
   * existing values. Properties with `null` values reset the setting at the
   * target level so it falls back to a broader level (or default).
   *
   * @param config The configuration fields to merge into the target scope.
   * @param browsingContextIds Top-level browsing context IDs to update, or
   *   `undefined`.
   * @param userContextIds User context IDs to update, or `undefined`.
   * @param allowGlobal Whether updating the global configuration (when both
   *   `browsingContextIds` and `userContextIds` are `undefined`) is allowed.
   *   Defaults to `true`. If `false`, throws `InvalidArgumentException` when
   *   neither is provided.
   * @returns The deduplicated list of related top-level browsing contexts.
   */
  async updateConfig(
    config: ContextConfig,
    browsingContextIds?: BrowsingContext.BrowsingContext[],
    userContextIds?: Browser.UserContext[],
    allowGlobal = true,
  ): Promise<BrowsingContextImpl[]> {
    const relatedBrowsingContexts =
      await this.#getRelatedTopLevelBrowsingContexts(
        browsingContextIds,
        userContextIds,
        allowGlobal,
      );

    for (const browsingContextId of browsingContextIds ?? []) {
      this.#updateBrowsingContextConfig(browsingContextId, config);
    }

    for (const userContextId of userContextIds ?? []) {
      this.#updateUserContextConfig(userContextId, config);
    }

    if (browsingContextIds === undefined && userContextIds === undefined) {
      this.#updateGlobalConfig(config);
    }

    return relatedBrowsingContexts;
  }

  /**
   * Returns a list of top-level browsing contexts related to the provided
   * parameters.
   */
  async #getRelatedTopLevelBrowsingContexts(
    browsingContextIds: BrowsingContext.BrowsingContext[] | undefined,
    userContextIds: Browser.UserContext[] | undefined,
    allowGlobal = true,
  ): Promise<BrowsingContextImpl[]> {
    if (browsingContextIds === undefined && userContextIds === undefined) {
      if (allowGlobal) {
        return this.#browsingContextStorage.getTopLevelContexts();
      }
      throw new InvalidArgumentException(
        'Either user contexts or browsing contexts must be provided',
      );
    }

    if (browsingContextIds !== undefined && userContextIds !== undefined) {
      throw new InvalidArgumentException(
        'User contexts and browsing contexts are mutually exclusive',
      );
    }

    // Use `Set` to deduplicate. Comparing `BrowsingContextImpl` by reference is
    // correct here, as `BrowsingContextStorage` returns the same instance for
    // the same id.
    const result = new Set<BrowsingContextImpl>();
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
        for (const browsingContext of topLevelBrowsingContexts) {
          result.add(browsingContext);
        }
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
        result.add(browsingContext);
      }
    }
    return Array.from(result);
  }
}
