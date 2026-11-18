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

import {
  UnsupportedOperationException,
  type BrowsingContext,
  type CommandData,
} from '../../../protocol/protocol.js';
import type {BrowsingContextImpl} from '../context/BrowsingContextImpl.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';

export interface AclEntry {
  commands?: string[];
}

export interface AclConfig {
  default?: AclEntry;
  perNavigable?: Record<BrowsingContext.BrowsingContext, AclEntry>;
}

/** ACL rules for a command's unvalidated `params`. */
interface AclCommandRule {
  /** Target browsing context ID, or `undefined` when only `default` applies. */
  getTargetContext(params: object): BrowsingContext.BrowsingContext | undefined;
  /**
   * Extra parameter check for restricted sessions. Throws an exception with a
   * detailed message if the command is not allowed with `params`.
   */
  assertParamsAllowed?(params: unknown): void;
}

/** For commands with `params.context`. */
const CONTEXT_TARGET_RULE: AclCommandRule = {
  getTargetContext(params) {
    if ('context' in params && typeof params.context === 'string') {
      return params.context;
    }
    return undefined;
  },
};

/** For commands without a target browsing context. */
const NO_TARGET_RULE: AclCommandRule = {
  getTargetContext() {
    return undefined;
  },
};

/** For `script` commands, with `params.target` and a required `sandbox`. */
const SANDBOXED_SCRIPT_TARGET_RULE: AclCommandRule = {
  getTargetContext(params) {
    const context = (params as {target?: {context?: unknown}}).target?.context;
    if (typeof context === 'string') {
      return context;
    }
    return undefined;
  },
  assertParamsAllowed(params) {
    // An empty or missing `sandbox` runs in the page's default realm, while a
    // named `sandbox` runs in an isolated world.
    const target = (
      params as {target?: {context?: unknown; sandbox?: unknown}} | null
    )?.target;
    if (
      typeof target?.context !== 'string' ||
      typeof target?.sandbox !== 'string' ||
      target.sandbox === ''
    ) {
      throw new UnsupportedOperationException(
        "Command 'script.evaluate' in restricted sessions requires a browsing context target with a non-empty 'sandbox'.",
      );
    }
  },
};

/**
 * Commands supported in an ACL. Each rule must match how the command's
 * processor reads the target from `params`.
 */
const SUPPORTED_ACL_COMMAND_RULES: ReadonlyMap<
  CommandData['method'],
  AclCommandRule
> = new Map<CommandData['method'], AclCommandRule>([
  // keep-sorted start
  ['browsingContext.captureScreenshot', CONTEXT_TARGET_RULE],
  ['browsingContext.close', CONTEXT_TARGET_RULE],
  // Creates a new tab; `referenceContext` only positions it.
  ['browsingContext.create', NO_TARGET_RULE],
  // Lists all tabs; `root` only filters the result.
  ['browsingContext.getTree', NO_TARGET_RULE],
  ['browsingContext.handleUserPrompt', CONTEXT_TARGET_RULE],
  // TODO: Also check `locator.value.context` for `{type: 'context'}` locators
  // when `perNavigable` grants become settable.
  ['browsingContext.locateNodes', CONTEXT_TARGET_RULE],
  ['browsingContext.navigate', CONTEXT_TARGET_RULE],
  ['browsingContext.reload', CONTEXT_TARGET_RULE],
  ['input.performActions', CONTEXT_TARGET_RULE],
  ['script.evaluate', SANDBOXED_SCRIPT_TARGET_RULE],
  // keep-sorted end
]);

/** Commands that can appear in an ACL. */
export const SUPPORTED_ACL_COMMANDS: ReadonlySet<string> = new Set<string>(
  SUPPORTED_ACL_COMMAND_RULES.keys(),
);

/** Session commands that are always allowed without an ACL grant. */
export const ACL_EXEMPT_COMMANDS: ReadonlySet<string> = new Set([
  // keep-sorted start
  'session.end',
  'session.new',
  'session.status',
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
    this.#defaultCommands = this.#extractSupportedCommands(aclConfig?.default);

    for (const [contextId, entry] of Object.entries(
      aclConfig?.perNavigable ?? {},
    )) {
      const context = this.#browsingContextStorage.findContext(contextId);
      if (context?.isTopLevelContext()) {
        this.#navigableCommands.set(
          context,
          this.#extractSupportedCommands(entry),
        );
      }
    }

    return this.getAcl();
  }

  #extractSupportedCommands(entry: AclEntry | undefined | null): Set<string> {
    const commands = new Set<string>();
    if (Array.isArray(entry?.commands)) {
      for (const command of entry.commands) {
        if (SUPPORTED_ACL_COMMANDS.has(command)) {
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

  /** Throws if the ACL disallows `command` with the given unvalidated `params`. */
  assertCommandAllowed(command: string, params: unknown): void {
    if (!this.#enabled || ACL_EXEMPT_COMMANDS.has(command)) {
      return;
    }
    const rule = SUPPORTED_ACL_COMMAND_RULES.get(
      command as CommandData['method'],
    );
    if (rule === undefined) {
      throw new UnsupportedOperationException(
        `Command '${command}' is not supported in restricted sessions.`,
      );
    }
    // A `default` grant covers every target, so skip resolving it.
    if (!this.isCommandAllowed(command)) {
      let contextId: BrowsingContext.BrowsingContext | undefined;
      if (typeof params === 'object' && params !== null) {
        contextId = rule.getTargetContext(params);
      }
      if (contextId === undefined) {
        throw new UnsupportedOperationException(
          `Command '${command}' is not allowed by the session ACL.`,
        );
      }
      if (!this.isCommandAllowed(command, contextId)) {
        throw new UnsupportedOperationException(
          `Command '${command}' is not allowed by the session ACL for browsing context '${contextId}'.`,
        );
      }
    }
    rule.assertParamsAllowed?.(params);
  }
}
