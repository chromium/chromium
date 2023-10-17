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
import type Protocol from 'devtools-protocol';

import type {ICdpClient} from '../../../cdp/CdpClient.js';
import {Deferred} from '../../../utils/Deferred.js';
import type {Result} from '../../../utils/result.js';
import type {EventManager} from '../events/EventManager.js';
import {LogManager} from '../log/LogManager.js';
import {NetworkManager} from '../network/NetworkManager.js';
import type {NetworkStorage} from '../network/NetworkStorage.js';
import type {ChannelProxy} from '../script/ChannelProxy.js';
import type {PreloadScriptStorage} from '../script/PreloadScriptStorage.js';
import type {RealmStorage} from '../script/RealmStorage.js';

export class CdpTarget {
  readonly #targetId: Protocol.Target.TargetID;
  readonly #cdpClient: ICdpClient;
  readonly #browserCdpClient: ICdpClient;
  readonly #cdpSessionId: Protocol.Target.SessionID;
  readonly #eventManager: EventManager;

  readonly #preloadScriptStorage: PreloadScriptStorage;
  readonly #networkStorage: NetworkStorage;

  readonly #targetUnblocked = new Deferred<Result<void>>();

  static create(
    targetId: Protocol.Target.TargetID,
    cdpClient: ICdpClient,
    browserCdpClient: ICdpClient,
    cdpSessionId: Protocol.Target.SessionID,
    realmStorage: RealmStorage,
    eventManager: EventManager,
    preloadScriptStorage: PreloadScriptStorage,
    networkStorage: NetworkStorage
  ): CdpTarget {
    const cdpTarget = new CdpTarget(
      targetId,
      cdpClient,
      browserCdpClient,
      cdpSessionId,
      eventManager,
      preloadScriptStorage,
      networkStorage
    );

    LogManager.create(cdpTarget, realmStorage, eventManager);
    NetworkManager.create(cdpTarget, networkStorage);

    cdpTarget.#setEventListeners();

    // No need to await.
    // Deferred will be resolved when the target is unblocked.
    void cdpTarget.#unblock();

    return cdpTarget;
  }

  constructor(
    targetId: Protocol.Target.TargetID,
    cdpClient: ICdpClient,
    browserCdpClient: ICdpClient,
    cdpSessionId: Protocol.Target.SessionID,
    eventManager: EventManager,
    preloadScriptStorage: PreloadScriptStorage,
    networkStorage: NetworkStorage
  ) {
    this.#targetId = targetId;
    this.#cdpClient = cdpClient;
    this.#cdpSessionId = cdpSessionId;
    this.#eventManager = eventManager;
    this.#preloadScriptStorage = preloadScriptStorage;
    this.#networkStorage = networkStorage;
    this.#browserCdpClient = browserCdpClient;
  }

  /** Returns a promise that resolves when the target is unblocked. */
  get targetUnblocked(): Deferred<Result<void>> {
    return this.#targetUnblocked;
  }

  get targetId(): Protocol.Target.TargetID {
    return this.#targetId;
  }

  get cdpClient(): ICdpClient {
    return this.#cdpClient;
  }

  get browserCdpClient(): ICdpClient {
    return this.#browserCdpClient;
  }

  /** Needed for CDP escape path. */
  get cdpSessionId(): Protocol.Target.SessionID {
    return this.#cdpSessionId;
  }

  /** Calls `Fetch.enable` with the added network intercepts. */
  async fetchEnable() {
    await this.#cdpClient.sendCommand(
      'Fetch.enable',
      this.#networkStorage.getFetchEnableParams()
    );
  }

  /** Calls `Fetch.disable`. */
  async fetchDisable() {
    await this.#cdpClient.sendCommand('Fetch.disable');
  }

  /**
   * Enables all the required CDP domains and unblocks the target.
   */
  async #unblock() {
    try {
      await Promise.all([
        this.#cdpClient.sendCommand('Runtime.enable'),
        this.#cdpClient.sendCommand('Page.enable'),
        this.#cdpClient.sendCommand('Page.setLifecycleEventsEnabled', {
          enabled: true,
        }),
        // XXX: #1080: Do not always enable the network domain globally.
        // TODO: enable Network domain for OOPiF targets.
        this.#cdpClient.sendCommand('Network.enable'),
        // XXX: #1080: Do not always enable the fetch domain globally.
        this.fetchEnable(),
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
        this.#targetUnblocked.resolve({
          kind: 'error',
          error,
        });
        return;
      }
    }

    this.#targetUnblocked.resolve({
      kind: 'success',
      value: undefined,
    });
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
            session: this.#cdpSessionId,
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
    for (const script of this.#preloadScriptStorage.find()) {
      await script.initInTarget(this, true);
    }
  }
}
