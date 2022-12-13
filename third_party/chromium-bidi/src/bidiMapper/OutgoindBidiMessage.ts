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

import type {Message} from '../protocol/protocol';

export class OutgoingBidiMessage {
  readonly #message: Message.OutgoingMessage;
  readonly #channel: string | null;

  private constructor(
    message: Message.OutgoingMessage,
    channel: string | null
  ) {
    this.#message = message;
    this.#channel = channel;
  }

  static async createFromPromise(
    messagePromise: Promise<Message.OutgoingMessage>,
    channel: string | null
  ): Promise<OutgoingBidiMessage> {
    const message = await messagePromise;
    return new OutgoingBidiMessage(message, channel);
  }

  static createResolved(
    message: Message.OutgoingMessage,
    channel: string | null
  ): Promise<OutgoingBidiMessage> {
    return Promise.resolve(new OutgoingBidiMessage(message, channel));
  }

  get message(): Message.OutgoingMessage {
    return this.#message;
  }

  get channel(): string | null {
    return this.#channel;
  }
}
