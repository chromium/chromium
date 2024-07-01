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

import type {CdpClient} from '../../../cdp/CdpClient.js';
import type {BidiPlusChannel} from '../../../protocol/chromium-bidi.js';
import type {
  ChromiumBidi,
  EmptyResult,
  Session,
} from '../../../protocol/protocol.js';

import type {EventManager} from './EventManager.js';

export class SessionProcessor {
  #eventManager: EventManager;
  #browserCdpClient: CdpClient;

  constructor(eventManager: EventManager, browserCdpClient: CdpClient) {
    this.#eventManager = eventManager;
    this.#browserCdpClient = browserCdpClient;
  }

  status(): Session.StatusResult {
    return {ready: false, message: 'already connected'};
  }

  async new(_params: Session.NewParameters): Promise<Session.NewResult> {
    // Since mapper exists, there is a session already.
    // Still the mapper can handle capabilities for us.
    // Currently, only Puppeteer calls here but, eventually, every client
    // should delegrate capability processing here.
    const version =
      await this.#browserCdpClient.sendCommand('Browser.getVersion');
    return {
      sessionId: 'unknown',
      capabilities: {
        acceptInsecureCerts: false,
        browserName: version.product,
        browserVersion: version.revision,
        platformName: '',
        setWindowRect: false,
        webSocketUrl: '',
        userAgent: version.userAgent,
      },
    };
  }

  async subscribe(
    params: Session.SubscriptionRequest,
    channel: BidiPlusChannel = null
  ): Promise<EmptyResult> {
    await this.#eventManager.subscribe(
      params.events as ChromiumBidi.EventNames[],
      params.contexts ?? [null],
      channel
    );
    return {};
  }

  async unsubscribe(
    params: Session.SubscriptionRequest,
    channel: BidiPlusChannel = null
  ): Promise<EmptyResult> {
    await this.#eventManager.unsubscribe(
      params.events as ChromiumBidi.EventNames[],
      params.contexts ?? [null],
      channel
    );
    return {};
  }
}
