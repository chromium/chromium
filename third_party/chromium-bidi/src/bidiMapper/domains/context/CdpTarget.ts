/*
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
 *
 */
import type {Protocol} from 'devtools-protocol';

import type {CdpClient} from '../../../cdp/CdpClient.js';
import {BiDiModule} from '../../../protocol/chromium-bidi.js';
import {Deferred} from '../../../utils/Deferred.js';
import type {LoggerFn} from '../../../utils/log.js';
import type {Result} from '../../../utils/result.js';
import {LogManager} from '../log/LogManager.js';
import type {NetworkStorage} from '../network/NetworkStorage.js';
import type {ChannelProxy} from '../script/ChannelProxy.js';
import type {PreloadScriptStorage} from '../script/PreloadScriptStorage.js';
import type {RealmStorage} from '../script/RealmStorage.js';
import type {EventManager} from '../session/EventManager.js';

export class CdpTarget {
  readonly #id: Protocol.Target.TargetID;
  readonly #cdpClient: CdpClient;
  readonly #browserCdpClient: CdpClient;
  readonly #eventManager: EventManager;

  readonly #preloadScriptStorage: PreloadScriptStorage;

  readonly #unblocked = new Deferred<Result<void>>();
  readonly #acceptInsecureCerts: boolean;
  #networkDomainEnabled = false;
  #fetchDomainEnabled = false;

  static create(
    targetId: Protocol.Target.TargetID,
    cdpClient: CdpClient,
    browserCdpClient: CdpClient,
    realmStorage: RealmStorage,
    eventManager: EventManager,
    preloadScriptStorage: PreloadScriptStorage,
    networkStorage: NetworkStorage,
    acceptInsecureCerts: boolean,
    logger?: LoggerFn
  ): CdpTarget {
    const cdpTarget = new CdpTarget(
      targetId,
      cdpClient,
      browserCdpClient,
      eventManager,
      preloadScriptStorage,
      acceptInsecureCerts
    );

    LogManager.create(cdpTarget, realmStorage, eventManager, logger);

    cdpTarget.#setEventListeners();

    // No need to await.
    // Deferred will be resolved when the target is unblocked.
    void cdpTarget.#unblock();

    return cdpTarget;
  }

  constructor(
    targetId: Protocol.Target.TargetID,
    cdpClient: CdpClient,
    browserCdpClient: CdpClient,
    eventManager: EventManager,
    preloadScriptStorage: PreloadScriptStorage,
    acceptInsecureCerts: boolean
  ) {
    this.#id = targetId;
    this.#cdpClient = cdpClient;
    this.#eventManager = eventManager;
    this.#preloadScriptStorage = preloadScriptStorage;
    this.#browserCdpClient = browserCdpClient;
    this.#acceptInsecureCerts = acceptInsecureCerts;
  }

  /** Returns a deferred that resolves when the target is unblocked. */
  get unblocked(): Deferred<Result<void>> {
    return this.#unblocked;
  }

  get id(): Protocol.Target.TargetID {
    return this.#id;
  }

  get cdpClient(): CdpClient {
    return this.#cdpClient;
  }

  get browserCdpClient(): CdpClient {
    return this.#browserCdpClient;
  }

  /** Needed for CDP escape path. */
  get cdpSessionId(): Protocol.Target.SessionID {
    // SAFETY we got the client by it's id for creating
    return this.#cdpClient.sessionId!;
  }

  /**
   * Enables all the required CDP domains and unblocks the target.
   */
  async #unblock() {
    // Check if the network domain is enabled globally.
    const enabledNetwork =
      this.#eventManager.subscriptionManager.isSubscribedToModule(
        BiDiModule.Network,
        this.#id
      );

    try {
      await Promise.all([
        this.#cdpClient.sendCommand('Runtime.enable'),
        this.#cdpClient.sendCommand('Page.enable'),
        this.#cdpClient.sendCommand('Page.setLifecycleEventsEnabled', {
          enabled: true,
        }),
        // Set ignore certificate errors for each target.
        this.#cdpClient.sendCommand('Security.setIgnoreCertificateErrors', {
          ignore: this.#acceptInsecureCerts,
        }),
        // TODO: enable Network domain for OOPiF targets.
        enabledNetwork
          ? this.#cdpClient.sendCommand('Network.enable')
          : undefined,
        this.#cdpClient.sendCommand('Target.setAutoAttach', {
          autoAttach: true,
          waitForDebuggerOnStart: true,
          flatten: true,
        }),
        this.#initAndEvaluatePreloadScripts(),
        this.#cdpClient.sendCommand('Runtime.runIfWaitingForDebugger'),
      ]);
    } catch (error: any) {
      // The target might have been closed before the initialization finished.
      if (!this.#cdpClient.isCloseError(error)) {
        this.#unblocked.resolve({
          kind: 'error',
          error,
        });
        return;
      }
    }

    this.#unblocked.resolve({
      kind: 'success',
      value: undefined,
    });
  }

  async enableFetchIfNeeded(params: Protocol.Fetch.EnableRequest) {
    if (!this.#networkDomainEnabled || this.#fetchDomainEnabled) {
      return;
    }

    this.#fetchDomainEnabled = true;
    try {
      await this.#cdpClient.sendCommand('Fetch.enable', params);
    } catch (err) {
      this.#fetchDomainEnabled = false;
    }
  }

  async disableFetchIfNeeded() {
    if (!this.#fetchDomainEnabled) {
      return;
    }

    this.#fetchDomainEnabled = false;
    try {
      await this.#cdpClient.sendCommand('Fetch.disable');
    } catch (err) {
      this.#fetchDomainEnabled = true;
    }
  }

  async toggleNetworkIfNeeded(enabled: boolean): Promise<void> {
    if (enabled === this.#networkDomainEnabled) {
      return;
    }

    this.#networkDomainEnabled = enabled;
    try {
      await this.#cdpClient.sendCommand(
        this.#networkDomainEnabled ? 'Network.enable' : 'Network.disable'
      );
    } catch (err) {
      this.#networkDomainEnabled = !enabled;
    }
  }

  #setEventListeners() {
    this.#cdpClient.on('*', (event, params) => {
      // We may encounter uses for EventEmitter other than CDP events,
      // which we want to skip.
      if (typeof event !== 'string') {
        return;
      }
      this.#eventManager.registerEvent(
        {
          type: 'event',
          method: `cdp.${event}`,
          params: {
            event,
            params,
            session: this.cdpSessionId,
          },
        },
        null
      );
    });
  }

  /**
   * All the ProxyChannels from all the preload scripts of the given
   * BrowsingContext.
   */
  getChannels(): ChannelProxy[] {
    return this.#preloadScriptStorage
      .find()
      .flatMap((script) => script.channels);
  }

  /** Loads all top-level preload scripts. */
  async #initAndEvaluatePreloadScripts() {
    for (const script of this.#preloadScriptStorage.find({
      global: true,
    })) {
      await script.initInTarget(this, true);
    }
  }
}
