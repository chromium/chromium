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

import type {Cdp} from '../../../protocol/protocol.js';
import type {CdpClient, CdpConnection} from '../../BidiMapper.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';

export class CdpProcessor {
  readonly #browsingContextStorage: BrowsingContextStorage;
  readonly #cdpConnection: CdpConnection;
  readonly #browserCdpClient: CdpClient;

  constructor(
    browsingContextStorage: BrowsingContextStorage,
    cdpConnection: CdpConnection,
    browserCdpClient: CdpClient
  ) {
    this.#browsingContextStorage = browsingContextStorage;
    this.#cdpConnection = cdpConnection;
    this.#browserCdpClient = browserCdpClient;
  }

  getSession(params: Cdp.GetSessionParameters): Cdp.GetSessionResult {
    const context = params.context;
    const sessionId =
      this.#browsingContextStorage.getContext(context).cdpTarget.cdpSessionId;
    if (sessionId === undefined) {
      return {};
    }
    return {session: sessionId};
  }

  async sendCommand(
    params: Cdp.SendCommandParameters
  ): Promise<Cdp.SendCommandResult> {
    const client = params.session
      ? this.#cdpConnection.getCdpClient(params.session)
      : this.#browserCdpClient;
    const result = await client.sendCommand(params.method, params.params);
    return {
      result,
      session: params.session,
    };
  }
}
