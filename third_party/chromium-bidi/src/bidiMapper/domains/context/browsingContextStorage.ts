/**
 * Copyright 2022 Google LLC.
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

import {BrowsingContextImpl} from './browsingContextImpl.js';
import {Message} from '../../../protocol/protocol.js';

export class BrowsingContextStorage {
  readonly #contexts = new Map<string, BrowsingContextImpl>();

  getTopLevelContexts(): BrowsingContextImpl[] {
    return Array.from(this.#contexts.values()).filter(
      (c) => c.parentId === null
    );
  }

  removeContext(contextId: string) {
    this.#contexts.delete(contextId);
  }

  addContext(context: BrowsingContextImpl) {
    this.#contexts.set(context.contextId, context);
    if (context.parentId !== null) {
      this.getKnownContext(context.parentId).addChild(context);
    }
  }

  hasKnownContext(contextId: string): boolean {
    return this.#contexts.has(contextId);
  }

  findContext(contextId: string): BrowsingContextImpl {
    return this.#contexts.get(contextId)!;
  }

  getKnownContext(contextId: string): BrowsingContextImpl {
    const result = this.findContext(contextId);
    if (result === undefined) {
      throw new Message.NoSuchFrameException(`Context ${contextId} not found`);
    }
    return result;
  }
}
