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
  type EmptyResult,
  type Browser,
  InvalidArgumentException,
  NoSuchUserContextException,
} from '../../../protocol/protocol.js';
import type {CdpClient} from '../../BidiMapper.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage';

export class BrowserProcessor {
  readonly #browserCdpClient: CdpClient;
  readonly #browsingContextStorage: BrowsingContextStorage;

  constructor(
    browserCdpClient: CdpClient,
    browsingContextStorage: BrowsingContextStorage,
  ) {
    this.#browserCdpClient = browserCdpClient;
    this.#browsingContextStorage = browsingContextStorage;
  }

  close(): EmptyResult {
    // Ensure that it is put at the end of the event loop.
    // This way we send back the response before closing the tab.
    setTimeout(() => this.#browserCdpClient.sendCommand('Browser.close'), 0);

    return {};
  }

  async createUserContext(
    params: Record<string, any>,
  ): Promise<Browser.CreateUserContextResult> {
    const request: Protocol.Target.CreateBrowserContextRequest = {
      proxyServer: params['goog:proxyServer'] ?? undefined,
    };
    const proxyBypassList: string[] | undefined =
      params['goog:proxyBypassList'] ?? undefined;
    if (proxyBypassList) {
      request.proxyBypassList = proxyBypassList.join(',');
    }
    const context = await this.#browserCdpClient.sendCommand(
      'Target.createBrowserContext',
      request,
    );
    return {
      userContext: context.browserContextId,
    };
  }

  async removeUserContext(
    params: Browser.RemoveUserContextParameters,
  ): Promise<EmptyResult> {
    const userContext = params.userContext;
    if (userContext === 'default') {
      throw new InvalidArgumentException(
        '`default` user context cannot be removed',
      );
    }
    try {
      await this.#browserCdpClient.sendCommand('Target.disposeBrowserContext', {
        browserContextId: userContext,
      });
    } catch (err) {
      // https://source.chromium.org/chromium/chromium/src/+/main:content/browser/devtools/protocol/target_handler.cc;l=1424;drc=c686e8f4fd379312469fe018f5c390e9c8f20d0d
      if ((err as Error).message.startsWith('Failed to find context with id')) {
        throw new NoSuchUserContextException((err as Error).message);
      }
      throw err;
    }
    return {};
  }

  async getUserContexts(): Promise<Browser.GetUserContextsResult> {
    const result = await this.#browserCdpClient.sendCommand(
      'Target.getBrowserContexts',
    );
    return {
      userContexts: [
        {
          userContext: 'default',
        },
        ...result.browserContextIds.map((id) => {
          return {
            userContext: id,
          };
        }),
      ],
    };
  }

  async #getWindowInfo(targetId: string): Promise<Browser.ClientWindowInfo> {
    const windowInfo = await this.#browserCdpClient.sendCommand(
      'Browser.getWindowForTarget',
      {targetId},
    );
    return {
      // Is not supported in CDP yet.
      active: false,
      clientWindow: `${windowInfo.windowId}`,
      state: windowInfo.bounds.windowState ?? 'normal',
      height: windowInfo.bounds.height ?? 0,
      width: windowInfo.bounds.width ?? 0,
      x: windowInfo.bounds.left ?? 0,
      y: windowInfo.bounds.top ?? 0,
    };
  }

  async getClientWindows(): Promise<Browser.GetClientWindowsResult> {
    const topLevelTargetIds = this.#browsingContextStorage
      .getTopLevelContexts()
      .map((b) => b.cdpTarget.id);

    const clientWindows = await Promise.all(
      topLevelTargetIds.map(
        async (targetId) => await this.#getWindowInfo(targetId),
      ),
    );
    return {clientWindows};
  }
}
