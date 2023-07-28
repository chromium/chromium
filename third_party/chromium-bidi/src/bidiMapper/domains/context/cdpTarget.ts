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
import type {ProtocolMapping} from 'devtools-protocol/types/protocol-mapping.js';
import type Protocol from 'devtools-protocol';

import type {ICdpClient} from '../../../cdp/cdpClient.js';
import {Deferred} from '../../../utils/deferred.js';
import type {IEventManager} from '../events/EventManager.js';
import {LogManager} from '../log/logManager.js';
import {NetworkManager} from '../network/NetworkManager.js';
import type {ChannelProxy} from '../script/channelProxy.js';
import type {RealmStorage} from '../script/realmStorage.js';
import type {PreloadScriptStorage} from '../script/PreloadScriptStorage.js';
import type {Result} from '../../../utils/result.js';

export class CdpTarget {
  readonly #targetId: Protocol.Target.TargetID;
  readonly #cdpClient: ICdpClient;
  readonly #cdpSessionId: Protocol.Target.SessionID;
  readonly #eventManager: IEventManager;
  readonly #preloadScriptStorage: PreloadScriptStorage;

  readonly #targetUnblocked = new Deferred<Result<void>>();

  static create(
    targetId: Protocol.Target.TargetID,
    cdpClient: ICdpClient,
    cdpSessionId: Protocol.Target.SessionID,
    realmStorage: RealmStorage,
    eventManager: IEventManager,
    preloadScriptStorage: PreloadScriptStorage
  ): CdpTarget {
    const cdpTarget = new CdpTarget(
      targetId,
      cdpClient,
      cdpSessionId,
      eventManager,
      preloadScriptStorage
    );

    LogManager.create(cdpTarget, realmStorage, eventManager);
    NetworkManager.create(cdpClient, eventManager);

    cdpTarget.#setEventListeners();

    // No need to await.
    // Deferred will be resolved when the target is unblocked.
    void cdpTarget.#unblock();

    return cdpTarget;
  }

  private constructor(
    targetId: Protocol.Target.TargetID,
    cdpClient: ICdpClient,
    cdpSessionId: Protocol.Target.SessionID,
    eventManager: IEventManager,
    preloadScriptStorage: PreloadScriptStorage
  ) {
    this.#targetId = targetId;
    this.#cdpClient = cdpClient;
    this.#cdpSessionId = cdpSessionId;
    this.#eventManager = eventManager;
    this.#preloadScriptStorage = preloadScriptStorage;
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

  /**
   * Needed for CDP escape path.
   */
  get cdpSessionId(): Protocol.Target.SessionID {
    return this.#cdpSessionId;
  }

  /**
   * Enables all the required CDP domains and unblocks the target.
   */
  async #unblock() {
    try {
      // Collect all command promises and wait for them after
      // `Runtime.runIfWaitingForDebugger`.
      const promises: Promise<unknown>[] = [];

      promises.push(
        this.#cdpClient.sendCommand('Runtime.enable'),
        this.#cdpClient.sendCommand('Page.enable'),
        this.#cdpClient.sendCommand('Page.setLifecycleEventsEnabled', {
          enabled: true,
        }),
        // XXX: #1080: Do not always enable the network domain globally.
        // TODO: enable Network domain for OOPiF targets.
        this.#cdpClient.sendCommand('Network.enable'),
        this.#cdpClient.sendCommand('Target.setAutoAttach', {
          autoAttach: true,
          waitForDebuggerOnStart: true,
          flatten: true,
        }),
        this.#initAndEvaluatePreloadScripts()
      );

      await this.#cdpClient.sendCommand('Runtime.runIfWaitingForDebugger');

      await Promise.all(promises);
    } catch (error: any) {
      // The target might have been closed before the initialization finished.
      if (!this.#cdpClient.isCloseError(error)) {
        throw error;
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
            params: params as ProtocolMapping.Events[typeof event],
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
      .findPreloadScripts()
      .flatMap((script) => script.channels);
  }

  /** Loads all top-level preload scripts. */
  async #initAndEvaluatePreloadScripts() {
    for (const script of this.#preloadScriptStorage.findPreloadScripts()) {
      await script.initInTarget(this, true);
    }
  }
}
