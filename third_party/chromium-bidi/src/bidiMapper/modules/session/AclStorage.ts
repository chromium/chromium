/**
 * Copyright 2026 Google LLC.
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

import type {BrowsingContext, CommandData} from '../../../protocol/protocol.js';
import type {BrowsingContextImpl} from '../context/BrowsingContextImpl.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';

export interface AclEntry {
  commands?: string[];
}

export interface AclConfig {
  default?: AclEntry;
  perNavigable?: Record<BrowsingContext.BrowsingContext, AclEntry>;
}

export const ALLOWED_ACL_COMMANDS: ReadonlySet<string> = new Set([
  // keep-sorted start
  'browsingContext.captureScreenshot',
  'browsingContext.close',
  'browsingContext.create',
  'browsingContext.getTree',
  'browsingContext.handleUserPrompt',
  'browsingContext.locateNodes',
  'browsingContext.navigate',
  'browsingContext.reload',
  'input.performActions',
  'script.evaluate',
  // keep-sorted end
] satisfies readonly CommandData['method'][]);

/**
 * Manages session ACL configuration (global default and per top-level navigable)
 * and checks whether commands are allowed.
 */
export class AclStorage {
  readonly #browsingContextStorage: BrowsingContextStorage;
  #enabled = false;
  #defaultCommands = new Set<string>();
  #navigableCommands = new WeakMap<BrowsingContextImpl, Set<string>>();

  constructor(browsingContextStorage: BrowsingContextStorage) {
    this.#browsingContextStorage = browsingContextStorage;
  }

  /**
   * Enables or disables ACL enforcement for the session.
   */
  setEnabled(enabled: boolean): void {
    this.#enabled = enabled;
  }

  /**
   * Configures the session ACL. Unsupported commands, invalid entries, and
   * unknown or non-top-level navigables are ignored (granting no permissions).
   *
   * @returns The actually saved ACL configuration.
   */
  setAcl(acl: unknown): AclConfig {
    this.#navigableCommands = new WeakMap();

    const aclConfig = acl as AclConfig | null | undefined;
    this.#defaultCommands = this.#extractAllowedCommands(aclConfig?.default);

    for (const [contextId, entry] of Object.entries(
      aclConfig?.perNavigable ?? {},
    )) {
      const context = this.#browsingContextStorage.findContext(contextId);
      if (context?.isTopLevelContext()) {
        this.#navigableCommands.set(
          context,
          this.#extractAllowedCommands(entry),
        );
      }
    }

    return this.getAcl();
  }

  #extractAllowedCommands(entry: AclEntry | undefined | null): Set<string> {
    const commands = new Set<string>();
    if (Array.isArray(entry?.commands)) {
      for (const command of entry.commands) {
        if (ALLOWED_ACL_COMMANDS.has(command)) {
          commands.add(command);
        }
      }
    }
    return commands;
  }

  /**
   * Returns the current full ACL configuration (including `default` and
   * `perNavigable`).
   */
  getAcl(): AclConfig {
    const entries: [BrowsingContext.BrowsingContext, AclEntry][] = [];
    for (const context of this.#browsingContextStorage.getTopLevelContexts()) {
      const commands = this.#navigableCommands.get(context);
      if (commands !== undefined) {
        entries.push([
          context.id,
          {
            commands: Array.from(commands),
          },
        ]);
      }
    }

    return {
      default: {
        commands: Array.from(this.#defaultCommands),
      },
      perNavigable: Object.fromEntries(entries),
    };
  }

  /**
   * Checks whether `command` is allowed globally (when `contextId` is
   * `undefined`) or for the top-level navigable containing `contextId`.
   * Always returns `true` when ACL enforcement is disabled.
   */
  isCommandAllowed(
    command: string,
    contextId?: BrowsingContext.BrowsingContext,
  ): boolean {
    if (!this.#enabled) {
      return true;
    }
    if (this.#defaultCommands.has(command)) {
      return true;
    }
    if (contextId !== undefined) {
      const topLevelContextId =
        this.#browsingContextStorage.findTopLevelContextId(contextId);
      if (topLevelContextId !== null) {
        const topLevelContext =
          this.#browsingContextStorage.findContext(topLevelContextId);
        if (topLevelContext !== undefined) {
          return (
            this.#navigableCommands.get(topLevelContext)?.has(command) ?? false
          );
        }
      }
    }
    return false;
  }
}
