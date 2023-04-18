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
    return this.getAllContexts().filter((c) => c.isTopLevelContext());
  }

  /** Gets all contexts. */
  getAllContexts(): BrowsingContextImpl[] {
    return Array.from(this.#contexts.values());
  }

  /** Deletes the context with the given ID. */
  deleteContext(contextId: CommonDataTypes.BrowsingContext) {
    this.#contexts.delete(contextId);
  }

  /** Adds the given context. */
  addContext(context: BrowsingContextImpl) {
    this.#contexts.set(context.contextId, context);
    if (!context.isTopLevelContext()) {
      this.getContext(context.parentId!).addChild(context);
    }
  }

  /** Returns true whether there is an existing context with the given ID. */
  hasContext(contextId: CommonDataTypes.BrowsingContext): boolean {
    return this.#contexts.has(contextId);
  }

  /** Gets the context with the given ID, if any. */
  findContext(
    contextId: CommonDataTypes.BrowsingContext
  ): BrowsingContextImpl | undefined {
    return this.#contexts.get(contextId);
  }

  /** Returns the top-level context ID of the given context, if any. */
  findTopLevelContextId(
    contextId: CommonDataTypes.BrowsingContext | null
  ): CommonDataTypes.BrowsingContext | null {
    if (contextId === null) {
      return null;
    }
    const maybeContext = this.findContext(contextId);
    const parentId = maybeContext?.parentId ?? null;
    if (parentId === null) {
      return contextId;
    }
    return this.findTopLevelContextId(parentId);
  }

  /** Gets the context with the given ID, if any, otherwise throws. */
  getContext(contextId: CommonDataTypes.BrowsingContext): BrowsingContextImpl {
    const result = this.findContext(contextId);
    if (result === undefined) {
      throw new Message.NoSuchFrameException(`Context ${contextId} not found`);
    }
    return result;
  }
}
