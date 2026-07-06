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
  InvalidArgumentException,
  UnsupportedOperationException,
  type EmptyResult,
  DigitalCredentials,
} from '../../../protocol/protocol.js';
import type {ContextConfigStorage} from '../browser/ContextConfigStorage.js';
import type {CdpTarget} from '../cdp/CdpTarget.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';

type Behavior = Omit<
  DigitalCredentials.SetVirtualWalletBehaviorParameters,
  'context'
>;

export class DigitalCredentialsProcessor {
  readonly #browsingContextStorage: BrowsingContextStorage;
  readonly #contextConfigStorage: ContextConfigStorage;

  constructor(
    browsingContextStorage: BrowsingContextStorage,
    contextConfigStorage: ContextConfigStorage,
  ) {
    this.#browsingContextStorage = browsingContextStorage;
    this.#contextConfigStorage = contextConfigStorage;
  }

  async setVirtualWalletBehavior(
    params: DigitalCredentials.SetVirtualWalletBehaviorParameters,
  ): Promise<EmptyResult> {
    const {context, action, protocol, response} = params;

    if (action === DigitalCredentials.VirtualWalletAction.Respond) {
      if (protocol === undefined || response === undefined) {
        throw new InvalidArgumentException(
          "Protocol and response are required when action is 'respond'",
        );
      }
    } else {
      if (protocol !== undefined || response !== undefined) {
        throw new InvalidArgumentException(
          "Protocol and response are only allowed when action is 'respond'",
        );
      }
    }

    if (context === undefined) {
      if (action === DigitalCredentials.VirtualWalletAction.Clear) {
        this.#contextConfigStorage.updateGlobalConfig({
          digitalCredentialsBehavior: null,
        });
      } else {
        this.#contextConfigStorage.updateGlobalConfig({
          digitalCredentialsBehavior: {action, protocol, response},
        });
      }
    } else {
      const browsingContext = this.#browsingContextStorage.getContext(context);
      if (browsingContext.parentId !== null) {
        throw new UnsupportedOperationException(
          'Only top-level contexts are supported',
        );
      }
      if (action === DigitalCredentials.VirtualWalletAction.Clear) {
        this.#contextConfigStorage.updateBrowsingContextConfig(context, {
          digitalCredentialsBehavior: null,
        });
      } else {
        this.#contextConfigStorage.updateBrowsingContextConfig(context, {
          digitalCredentialsBehavior: {action, protocol, response},
        });
      }
    }

    await this.#applyToAllTargets();

    return {};
  }

  async #applyToAllTargets() {
    const contexts = this.#browsingContextStorage.getAllContexts();
    const targets = new Set<CdpTarget>();
    for (const c of contexts) {
      targets.add(c.cdpTarget);
    }

    await Promise.all(
      Array.from(targets).map((target) => this.#applyBehaviorToTarget(target)),
    );
  }

  async #applyBehaviorToTarget(target: CdpTarget) {
    if (target.id !== target.topLevelId) {
      return;
    }
    const config = this.#contextConfigStorage.getActiveConfig(
      target.topLevelId,
      target.userContext,
    );

    const behavior = config.digitalCredentialsBehavior;

    if (behavior === null || behavior === undefined) {
      await this.#sendCdpCommand(target, {
        action: DigitalCredentials.VirtualWalletAction.Clear,
      });
      return;
    }

    await this.#sendCdpCommand(target, behavior);
  }

  async #sendCdpCommand(cdpTarget: CdpTarget, behavior: Behavior) {
    await cdpTarget.cdpClient.sendCommand(
      'DigitalCredentials.setVirtualWalletBehavior',
      {
        // @ts-expect-error action is kept for backward compatibility with older Chromium CDP versions
        action: behavior.action,
        behavior: behavior.action,
        protocol: behavior.protocol,
        response: behavior.response,
      },
    );
  }
}
