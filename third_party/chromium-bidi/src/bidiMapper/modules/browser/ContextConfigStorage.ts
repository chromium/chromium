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

import {ContextConfig} from './ContextConfig.js';

export class ContextConfigStorage {
  #global = new ContextConfig();
  #userContextConfigs = new Map<string, ContextConfig>();
  #browsingContextConfigs = new Map<string, ContextConfig>();

  updateGlobalConfig(config: ContextConfig) {
    this.#global = {...this.#global, ...config};
  }

  updateBrowsingContextConfig(
    browsingContextId: string,
    config: ContextConfig,
  ) {
    if (this.#browsingContextConfigs.has(browsingContextId)) {
      this.#browsingContextConfigs.set(browsingContextId, {
        ...this.#browsingContextConfigs.get(browsingContextId),
        ...config,
      });
    } else {
      this.#browsingContextConfigs.set(browsingContextId, config);
    }
  }

  updateUserContextConfig(userContext: string, config: ContextConfig) {
    if (this.#userContextConfigs.has(userContext)) {
      this.#userContextConfigs.set(userContext, {
        ...this.#userContextConfigs.get(userContext),
        ...config,
      });
    } else {
      this.#userContextConfigs.set(userContext, config);
    }
  }

  getActiveConfig(topLevelBrowsingContextId?: string, userContext?: string) {
    let result = {...this.#global};
    if (
      userContext !== undefined &&
      this.#userContextConfigs.has(userContext)
    ) {
      result = {...result, ...this.#userContextConfigs.get(userContext)};
    }
    if (
      topLevelBrowsingContextId !== undefined &&
      this.#browsingContextConfigs.has(topLevelBrowsingContextId)
    ) {
      result = {
        ...result,
        ...this.#browsingContextConfigs.get(topLevelBrowsingContextId),
      };
    }
    return result;
  }
}
