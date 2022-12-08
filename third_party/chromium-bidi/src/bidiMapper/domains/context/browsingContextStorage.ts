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

import {BrowsingContextImpl} from './browsingContextImpl';
import {NoSuchFrameException} from '../../../protocol/error';

export class BrowsingContextStorage {
  static #contexts: Map<string, BrowsingContextImpl> = new Map();

  static getTopLevelContexts(): BrowsingContextImpl[] {
    return Array.from(BrowsingContextStorage.#contexts.values()).filter(
      (c) => c.parentId === null
    );
  }

  static removeContext(contextId: string) {
    BrowsingContextStorage.#contexts.delete(contextId);
  }

  static addContext(context: BrowsingContextImpl) {
    BrowsingContextStorage.#contexts.set(context.contextId, context);
    if (context.parentId !== null) {
      BrowsingContextStorage.getKnownContext(context.parentId).addChild(
        context
      );
    }
  }

  static hasKnownContext(contextId: string): boolean {
    return BrowsingContextStorage.#contexts.has(contextId);
  }

  static findContext(contextId: string): BrowsingContextImpl | undefined {
    return BrowsingContextStorage.#contexts.get(contextId)!;
  }

  static getKnownContext(contextId: string): BrowsingContextImpl {
    const result = BrowsingContextStorage.findContext(contextId);
    if (result === undefined) {
      throw new NoSuchFrameException(`Context ${contextId} not found`);
    }
    return result;
  }
}
