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

import {CommonDataTypes, Message} from '../../../protocol/protocol.js';

import {BrowsingContextImpl} from './browsingContextImpl.js';

/** Container class for browsing contexts. */
export class BrowsingContextStorage {
  /** Map from context ID to context implementation. */
  readonly #contexts = new Map<
    CommonDataTypes.BrowsingContext,
    BrowsingContextImpl
  >();

  /** Gets all top-level contexts, i.e. those with no parent. */
  getTopLevelContexts(): BrowsingContextImpl[] {
    return this.getAllContexts().filter((context) =>
      context.isTopLevelContext()
    );
  }

  /** Gets all contexts. */
  getAllContexts(): BrowsingContextImpl[] {
    return Array.from(this.#contexts.values());
  }

  /** Deletes the context with the given ID. */
  deleteContextById(id: CommonDataTypes.BrowsingContext) {
    this.#contexts.delete(id);
  }

  /** Deletes the given context. */
  deleteContext(context: BrowsingContextImpl) {
    this.#contexts.delete(context.id);
  }

  /** Tracks the given context. */
  addContext(context: BrowsingContextImpl) {
    this.#contexts.set(context.id, context);
  }

  /** Returns true whether there is an existing context with the given ID. */
  hasContext(id: CommonDataTypes.BrowsingContext): boolean {
    return this.#contexts.has(id);
  }

  /** Gets the context with the given ID, if any. */
  findContext(
    id: CommonDataTypes.BrowsingContext
  ): BrowsingContextImpl | undefined {
    return this.#contexts.get(id);
  }

  /** Returns the top-level context ID of the given context, if any. */
  findTopLevelContextId(
    id: CommonDataTypes.BrowsingContext | null
  ): CommonDataTypes.BrowsingContext | null {
    if (id === null) {
      return null;
    }
    const maybeContext = this.findContext(id);
    const parentId = maybeContext?.parentId ?? null;
    if (parentId === null) {
      return id;
    }
    return this.findTopLevelContextId(parentId);
  }

  /** Returns the top-level context of the given context, if any. */
  findTopLevelContext(
    id: CommonDataTypes.BrowsingContext | null
  ): BrowsingContextImpl | null | undefined {
    const topLevelContextId = this.findTopLevelContextId(id);
    if (topLevelContextId === null) {
      return null;
    }
    return this.findContext(topLevelContextId);
  }

  /** Gets the context with the given ID, if any, otherwise throws. */
  getContext(id: CommonDataTypes.BrowsingContext): BrowsingContextImpl {
    const result = this.findContext(id);
    if (result === undefined) {
      throw new Message.NoSuchFrameException(`Context ${id} not found`);
    }
    return result;
  }
}
