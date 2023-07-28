/**
 * Copyright 2021 Google LLC.
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

import type {ChromiumBidi} from '../protocol/protocol.js';
import type {Result} from '../utils/result.js';

export class OutgoingBidiMessage {
  readonly #message: ChromiumBidi.Message;
  readonly #channel: string | null;

  private constructor(
    message: ChromiumBidi.Message,
    channel: string | null = null
  ) {
    this.#message = message;
    this.#channel = channel;
  }

  static createFromPromise(
    messagePromise: Promise<Result<ChromiumBidi.Message>>,
    channel: string | null
  ): Promise<Result<OutgoingBidiMessage>> {
    return messagePromise.then((message) => {
      if (message.kind === 'success') {
        return {
          kind: 'success',
          value: new OutgoingBidiMessage(message.value, channel),
        };
      }
      return message;
    });
  }

  static createResolved(
    message: ChromiumBidi.Message,
    channel?: string | null
  ): Promise<Result<OutgoingBidiMessage>> {
    return Promise.resolve({
      kind: 'success',
      value: new OutgoingBidiMessage(message, channel),
    });
  }

  get message(): ChromiumBidi.Message {
    return this.#message;
  }

  get channel(): string | null {
    return this.#channel;
  }
}
