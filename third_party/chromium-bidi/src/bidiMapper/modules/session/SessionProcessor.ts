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

import type {CdpClient} from '../../../cdp/CdpClient.js';
import type {GoogChannel} from '../../../protocol/chromium-bidi.js';
import {
  type ChromiumBidi,
  type EmptyResult,
  type GoogSession,
  InvalidArgumentException,
  Session,
  SessionNotCreatedException,
  UnsupportedOperationException,
} from '../../../protocol/protocol.js';
import type {MapperOptions} from '../../MapperOptions.js';

import type {AclManager} from './AclManager.js';
import type {EventManager} from './EventManager.js';

// Predicates returning true when a `MapperOptions` capability is allowed in a
// restricted `session.new`. Typed against `keyof MapperOptions` so any new
// Mapper option triggers a compile error until classified here.
const RESTRICTED_CAPABILITY_RULES = {
  acceptInsecureCerts(value: unknown): boolean {
    return value !== true;
  },
  unhandledPromptBehavior(value: unknown): boolean {
    return value === undefined || value === null;
  },
  'goog:prerenderingDisabled'(value: unknown): boolean {
    return value !== true;
  },
  'goog:disableNetworkDurableMessages'(value: unknown): boolean {
    return value !== true;
  },
} satisfies Record<keyof MapperOptions, (value: unknown) => boolean>;

export class SessionProcessor {
  #eventManager: EventManager;
  #browserCdpClient: CdpClient;
  #initConnection: (opts: MapperOptions) => Promise<void>;
  #aclManager: AclManager;
  // When true, the Mapper starts with an empty ACL and `session.new` requires
  // `'goog:sessionType': 'restricted'` (preferring restricted candidates and
  // throwing `SessionNotCreatedException` otherwise). When false, standard
  // sessions are allowed unless `'goog:sessionType': 'restricted'` is
  // requested.
  #restrictedMode: boolean;
  // `#creating` blocks concurrent or duplicate `session.new` calls once
  // `#initConnection` starts, while `#created` (returned by `isCreated()`)
  // stays false until initialization completes so other commands still fail
  // with `invalid session id` while `session.new` is in flight.
  #creating = false;
  #created = false;

  constructor(
    eventManager: EventManager,
    browserCdpClient: CdpClient,
    initConnection: (opts: MapperOptions) => Promise<void>,
    aclManager: AclManager,
    restrictedMode = false,
  ) {
    this.#eventManager = eventManager;
    this.#browserCdpClient = browserCdpClient;
    this.#initConnection = initConnection;
    this.#aclManager = aclManager;
    this.#restrictedMode = restrictedMode;
  }

  status(): Session.StatusResult {
    return {ready: false, message: 'already connected'};
  }

  /** Whether `session.new` succeeded and created the session. */
  isCreated(): boolean {
    return this.#created;
  }

  #mergeCapabilities(
    capabilitiesRequest: Session.CapabilitiesRequest,
  ): Session.CapabilityRequest {
    // Roughly following https://www.w3.org/TR/webdriver2/#dfn-capabilities-processing.
    // Validations should already be done by the parser.

    const mergedCapabilities = [];

    for (const first of capabilitiesRequest.firstMatch ?? [{}]) {
      const result = {
        ...capabilitiesRequest.alwaysMatch,
      };
      for (const key of Object.keys(first)) {
        if (result[key] !== undefined) {
          throw new InvalidArgumentException(
            `Capability ${key} in firstMatch is already defined in alwaysMatch`,
          );
        }
        result[key] = first[key];
      }

      mergedCapabilities.push(result);
    }

    // In restricted mode, prefer candidates requesting a restricted session.
    const candidates = this.#restrictedMode
      ? mergedCapabilities.filter((c) => c['goog:sessionType'] === 'restricted')
      : mergedCapabilities;

    const match =
      candidates.find((c) => c.browserName === 'chrome') ??
      candidates[0] ??
      mergedCapabilities[0] ??
      {};

    match.unhandledPromptBehavior = this.#getUnhandledPromptBehavior(
      match.unhandledPromptBehavior,
    );

    // Other values of `goog:sessionType` are ignored: the session is not
    // restricted, and the result omits the value.
    if (match['goog:sessionType'] !== 'restricted') {
      delete match['goog:sessionType'];
    }

    return match;
  }

  #getUnhandledPromptBehavior(
    capabilityValue: unknown,
  ): Session.UserPromptHandler | undefined {
    if (capabilityValue === undefined) {
      return undefined;
    }
    if (typeof capabilityValue === 'object') {
      // Do not validate capabilities. Incorrect ones will be ignored by Mapper.
      return capabilityValue as Session.UserPromptHandler;
    }
    if (typeof capabilityValue !== 'string') {
      throw new InvalidArgumentException(
        `Unexpected 'unhandledPromptBehavior' type: ${typeof capabilityValue}`,
      );
    }
    switch (capabilityValue) {
      // `beforeUnload: accept` has higher priority over string capability, as the latest
      // one is set to "fallbackDefault".
      // https://w3c.github.io/webdriver/#dfn-deserialize-as-an-unhandled-prompt-behavior
      // https://w3c.github.io/webdriver/#dfn-get-the-prompt-handler
      case 'accept':
      case 'accept and notify':
        return {
          default: Session.UserPromptHandlerType.Accept,
          beforeUnload: Session.UserPromptHandlerType.Accept,
        };
      case 'dismiss':
      case 'dismiss and notify':
        return {
          default: Session.UserPromptHandlerType.Dismiss,
          beforeUnload: Session.UserPromptHandlerType.Accept,
        };
      case 'ignore':
        return {
          default: Session.UserPromptHandlerType.Ignore,
          beforeUnload: Session.UserPromptHandlerType.Accept,
        };
      default:
        throw new InvalidArgumentException(
          `Unexpected 'unhandledPromptBehavior' value: ${capabilityValue}`,
        );
    }
  }

  async new(params: Session.NewParameters): Promise<Session.NewResult> {
    if (this.#created || this.#creating) {
      throw new SessionNotCreatedException('Session has been already created.');
    }

    const matchedCapabilities = this.#mergeCapabilities(params.capabilities);

    const isRestricted =
      matchedCapabilities['goog:sessionType'] === 'restricted';

    if (this.#restrictedMode && !isRestricted) {
      // In restricted mode, only restricted sessions can be created.
      throw new SessionNotCreatedException(
        'Only restricted sessions are allowed. Request capability "goog:sessionType": "restricted".',
      );
    }

    if (isRestricted) {
      // Reject restricted capabilities before `#creating` is set, so the client
      // can retry.
      for (const [name, isAllowedInRestrictedMode] of Object.entries(
        RESTRICTED_CAPABILITY_RULES,
      )) {
        if (!isAllowedInRestrictedMode(matchedCapabilities[name])) {
          throw new SessionNotCreatedException(
            `Capability '${name}' is not allowed in restricted sessions.`,
          );
        }
      }
    }

    this.#creating = true;

    if (isRestricted) {
      // The ACL is still empty: `goog:session.setAcl` fails before the session
      // is created.
      this.#aclManager.enable();
    }
    await this.#initConnection(isRestricted ? {} : matchedCapabilities);

    const version =
      await this.#browserCdpClient.sendCommand('Browser.getVersion');
    const slashIndex = version.product.indexOf('/');
    const browserName =
      slashIndex === -1
        ? version.product
        : version.product.slice(0, slashIndex);
    const browserVersion =
      (slashIndex === -1 ? undefined : version.product.slice(slashIndex + 1)) ||
      version.revision;
    this.#created = true;

    return {
      sessionId: 'unknown',
      capabilities: {
        ...matchedCapabilities,
        acceptInsecureCerts: matchedCapabilities.acceptInsecureCerts ?? false,
        browserName,
        browserVersion,
        platformName: '',
        setWindowRect: false,
        webSocketUrl: '',
        userAgent: version.userAgent,
      },
    };
  }

  async setAcl(
    params: GoogSession.SetAclParameters,
  ): Promise<GoogSession.SetAclResult> {
    // Only `session.new` decides whether the session is restricted.
    if (!this.#aclManager.isEnabled()) {
      throw new UnsupportedOperationException(
        `Command 'goog:session.setAcl' requires a restricted session. Request capability "goog:sessionType": "restricted" in session.new.`,
      );
    }
    return await this.#aclManager.updateAcl(params.acl, params.reason);
  }

  async subscribe(
    params: Session.SubscribeParameters,
    googChannel: GoogChannel = null,
  ): Promise<Session.SubscribeResult> {
    const subscription = await this.#eventManager.subscribe(
      params.events as ChromiumBidi.EventNames[],
      params.contexts ?? [],
      params.userContexts ?? [],
      googChannel,
    );
    return {
      subscription,
    };
  }

  async unsubscribe(
    params: Session.UnsubscribeParameters,
    googChannel: GoogChannel = null,
  ): Promise<EmptyResult> {
    if ('subscriptions' in params) {
      await this.#eventManager.unsubscribeByIds(params.subscriptions);
      return {};
    }
    await this.#eventManager.unsubscribe(
      params.events as ChromiumBidi.EventNames[],
      googChannel,
    );
    return {};
  }
}
