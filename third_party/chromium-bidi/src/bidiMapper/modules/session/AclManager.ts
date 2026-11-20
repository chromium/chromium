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
  ChromiumBidi,
  UnsupportedOperationException,
  type BrowsingContext,
  type CommandData,
  type GoogSession,
} from '../../../protocol/protocol.js';
import {Mutex} from '../../../utils/Mutex.js';
import type {BrowsingContextImpl} from '../context/BrowsingContextImpl.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';

// Aliases of the generated types, so `goog.cddl` stays the source of truth.
type AclEntry = GoogSession.AclEntry;
export type AclConfig = GoogSession.Acl;

/**
 * Lets the embedder (e.g. the browser) decide on the requested ACL, e.g. by
 * asking the user. `reason` is untrusted client text. Resolves with the ACL the
 * embedder allows, which becomes the session ACL, and with what it refused, if
 * anything. Called on every ACL change, so the embedder always knows the session
 * ACL.
 */
export type UpdateAclHandler = (
  requestedAcl: AclConfig,
  reason?: string,
) => Promise<GoogSession.SetAclResult>;

/**
 * Allows the requested ACL as is. Used until the embedder provides its own
 * handler.
 */
export const allowAllUpdateAclHandler: UpdateAclHandler = (requestedAcl) => {
  return Promise.resolve({acl: requestedAcl});
};

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
        "Restricted sessions require a browsing context target with a non-empty 'sandbox'.",
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
  // `{type: 'context'}` locators require `locator.value.context` to be a direct
  // child of `params.context`, so they stay in the same top-level navigable.
  ['browsingContext.locateNodes', CONTEXT_TARGET_RULE],
  ['browsingContext.navigate', CONTEXT_TARGET_RULE],
  ['browsingContext.reload', CONTEXT_TARGET_RULE],
  ['browsingContext.traverseHistory', CONTEXT_TARGET_RULE],
  ['input.performActions', CONTEXT_TARGET_RULE],
  ['input.releaseActions', CONTEXT_TARGET_RULE],
  // Require a named sandbox for `script.callFunction` too, because default or
  // `{realm}` targets share globals with the page.
  ['script.callFunction', SANDBOXED_SCRIPT_TARGET_RULE],
  ['script.evaluate', SANDBOXED_SCRIPT_TARGET_RULE],
  // keep-sorted end
]);

/** Commands that can appear in an ACL. */
export const SUPPORTED_ACL_COMMANDS: ReadonlySet<string> = new Set<string>(
  SUPPORTED_ACL_COMMAND_RULES.keys(),
);

/**
 * Session commands that are always allowed without an ACL grant. Clients can
 * subscribe to any event, but get only the events the ACL allows.
 */
export const ACL_EXEMPT_COMMANDS: ReadonlySet<string> = new Set([
  // keep-sorted start
  'goog:session.setAcl',
  'session.end',
  'session.new',
  'session.status',
  'session.subscribe',
  'session.unsubscribe',
  // keep-sorted end
] satisfies readonly ChromiumBidi.Command['method'][]);

/**
 * Events that can appear in an ACL. Each event has to be reviewed for data
 * leaks (e.g. URLs of other tabs, download file names, dialog text) before it's
 * added.
 */
export const SUPPORTED_ACL_EVENTS: ReadonlySet<string> = new Set<string>([
  // keep-sorted start
  ChromiumBidi.BrowsingContext.EventNames.ContextCreated,
  ChromiumBidi.BrowsingContext.EventNames.ContextDestroyed,
  ChromiumBidi.BrowsingContext.EventNames.FragmentNavigated,
  ChromiumBidi.BrowsingContext.EventNames.HistoryUpdated,
  ChromiumBidi.BrowsingContext.EventNames.NavigationCommitted,
  ChromiumBidi.BrowsingContext.EventNames.UserPromptClosed,
  ChromiumBidi.BrowsingContext.EventNames.UserPromptOpened,
  ChromiumBidi.Log.EventNames.LogEntryAdded,
  // keep-sorted end
]);

/**
 * Manages session ACL configuration (global default and per top-level navigable),
 * and checks whether commands are allowed and events can be delivered. ACL
 * changes requested by the client go through the embedder's handler, one at a
 * time.
 */
export class AclManager {
  readonly #browsingContextStorage: BrowsingContextStorage;
  readonly #updateAclHandler: UpdateAclHandler;
  readonly #updateAclMutex = new Mutex();
  #enabled = false;
  #defaultCommands = new Set<string>();
  #navigableCommands = new WeakMap<BrowsingContextImpl, Set<string>>();
  #defaultEvents = new Set<string>();
  #navigableEvents = new WeakMap<BrowsingContextImpl, Set<string>>();

  constructor(
    browsingContextStorage: BrowsingContextStorage,
    // TODO(crbug.com/586824814): let the embedder provide the handler.
    updateAclHandler: UpdateAclHandler = allowAllUpdateAclHandler,
  ) {
    this.#browsingContextStorage = browsingContextStorage;
    this.#updateAclHandler = updateAclHandler;
  }

  /**
   * Asks the embedder for `requestedAcl`, the full desired ACL, and stores the
   * ACL it allows. Returns the ACL now in effect with what the embedder
   * refused, if anything. Requests are processed one at a time, in order.
   */
  async updateAcl(
    requestedAcl: unknown,
    reason?: string,
  ): Promise<GoogSession.SetAclResult> {
    return await this.#updateAclMutex.run(async () => {
      // If the embedder fails, so does the request, and the ACL doesn't change.
      const decision = await this.#updateAclHandler(
        this.#normalizeAcl(requestedAcl),
        reason,
      );
      return {...decision, acl: this.#setAcl(decision.acl)};
    });
  }

  /**
   * Whether ACL enforcement is enabled, i.e. the session is restricted.
   */
  isEnabled(): boolean {
    return this.#enabled;
  }

  /**
   * Enables ACL enforcement for the session. Once enabled, it cannot be
   * disabled.
   */
  enable(): void {
    this.#enabled = true;
  }

  /**
   * Stores the session ACL. Unsupported commands and events, invalid entries,
   * and unknown or non-top-level navigables are ignored (granting no access).
   * Private, so that every ACL change goes through `updateAcl` and the
   * embedder's handler.
   *
   * @returns The actually saved ACL configuration.
   */
  #setAcl(acl: unknown): AclConfig {
    // Reuse the normalization so the stored and the normalized ACLs can't
    // drift apart.
    const normalizedAcl = this.#normalizeAcl(acl);
    this.#defaultCommands = new Set(normalizedAcl.default?.commands);
    this.#navigableCommands = new WeakMap();
    this.#defaultEvents = new Set(normalizedAcl.default?.events);
    this.#navigableEvents = new WeakMap();
    for (const [contextId, entry] of Object.entries(
      normalizedAcl.perNavigable ?? {},
    )) {
      // `#normalizeAcl` keeps only known top-level navigables.
      const context = this.#browsingContextStorage.getContext(contextId);
      this.#navigableCommands.set(context, new Set(entry.commands));
      this.#navigableEvents.set(context, new Set(entry.events));
    }

    return this.getAcl();
  }

  /**
   * Returns `acl` without unsupported commands and events, invalid entries, and
   * unknown or non-top-level navigables, i.e. what `#setAcl` would store,
   * without storing it.
   */
  #normalizeAcl(acl: unknown): AclConfig {
    const aclConfig = acl as AclConfig | null | undefined;
    const perNavigable: Record<BrowsingContext.BrowsingContext, AclEntry> = {};
    for (const [contextId, entry] of Object.entries(
      aclConfig?.perNavigable ?? {},
    )) {
      const context = this.#browsingContextStorage.findContext(contextId);
      if (context?.isTopLevelContext()) {
        perNavigable[contextId] = {
          commands: Array.from(this.#extractSupportedCommands(entry)),
          events: Array.from(this.#extractSupportedEvents(entry)),
        };
      }
    }
    return {
      default: {
        commands: Array.from(
          this.#extractSupportedCommands(aclConfig?.default),
        ),
        events: Array.from(this.#extractSupportedEvents(aclConfig?.default)),
      },
      perNavigable,
    };
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

  #extractSupportedEvents(entry: AclEntry | undefined | null): Set<string> {
    const events = new Set<string>();
    if (Array.isArray(entry?.events)) {
      for (const event of entry.events) {
        if (SUPPORTED_ACL_EVENTS.has(event)) {
          events.add(event);
        }
      }
    }
    return events;
  }

  /**
   * Returns the current full ACL configuration (including `default` and
   * `perNavigable`).
   */
  getAcl(): AclConfig {
    const entries: [BrowsingContext.BrowsingContext, AclEntry][] = [];
    for (const context of this.#browsingContextStorage.getTopLevelContexts()) {
      const commands = this.#navigableCommands.get(context);
      const events = this.#navigableEvents.get(context);
      if (commands !== undefined || events !== undefined) {
        entries.push([
          context.id,
          {
            commands: Array.from(commands ?? []),
            events: Array.from(events ?? []),
          },
        ]);
      }
    }

    return {
      default: {
        commands: Array.from(this.#defaultCommands),
        events: Array.from(this.#defaultEvents),
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

  /**
   * Checks whether `eventName` can be delivered for `contextId`'s top-level
   * navigable, or session-wide for events without a browsing context (`null`).
   * Always returns `true` when ACL enforcement is disabled.
   */
  isEventAllowed(
    eventName: string,
    contextId: BrowsingContext.BrowsingContext | null,
  ): boolean {
    if (!this.#enabled) {
      return true;
    }
    if (this.#defaultEvents.has(eventName)) {
      return true;
    }
    if (contextId === null) {
      // Only `default` covers events that don't belong to a navigable.
      return false;
    }
    const topLevelContextId =
      this.#browsingContextStorage.findTopLevelContextId(contextId);
    if (topLevelContextId === null) {
      return false;
    }
    const topLevelContext =
      this.#browsingContextStorage.findContext(topLevelContextId);
    if (topLevelContext === undefined) {
      return false;
    }
    return this.#navigableEvents.get(topLevelContext)?.has(eventName) ?? false;
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
