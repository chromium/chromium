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

import type {BidiPlusChannel} from '../../../protocol/chromium-bidi.js';
import type {
  ChromiumBidi,
  EmptyResult,
  Session,
} from '../../../protocol/protocol.js';

import type {EventManager} from './EventManager.js';

export class SessionProcessor {
  #eventManager: EventManager;

  constructor(eventManager: EventManager) {
    this.#eventManager = eventManager;
  }

  status(): Session.StatusResult {
    return {ready: false, message: 'already connected'};
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
