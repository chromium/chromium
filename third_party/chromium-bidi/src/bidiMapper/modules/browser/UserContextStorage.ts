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

import type {CdpClient} from '../../../cdp/CdpClient.js';
import {
  NoSuchUserContextException,
  type Browser,
} from '../../../protocol/protocol.js';

export class UserContextStorage {
  #browserClient: CdpClient;
  constructor(browserClient: CdpClient) {
    this.#browserClient = browserClient;
  }

  async getUserContexts(): Promise<
    [Browser.UserContextInfo, ...Browser.UserContextInfo[]]
  > {
    const result = await this.#browserClient.sendCommand(
      'Target.getBrowserContexts',
    );
    return [
      {
        userContext: 'default',
      },
      ...result.browserContextIds.map((id) => {
        return {
          userContext: id,
        };
      }),
    ];
  }

  async verifyUserContextIdList(userContextIds: Browser.UserContext[]) {
    const foundContexts = new Set<Browser.UserContext>();
    if (!userContextIds.length) {
      return foundContexts;
    }

    const userContexts = await this.getUserContexts();
    const knownUserContextIds = new Set(
      userContexts.map((userContext) => userContext.userContext),
    );
    for (const userContextId of userContextIds) {
      if (!knownUserContextIds.has(userContextId)) {
        throw new NoSuchUserContextException(
          `User context ${userContextId} not found`,
        );
      }
      foundContexts.add(userContextId);
    }

    return foundContexts;
  }
}
