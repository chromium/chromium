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

import type {ICdpClient} from '../../../cdp/cdpClient.js';
import {LogManager} from '../log/logManager.js';
import type {RealmStorage} from '../script/realmStorage.js';
import type {IEventManager} from '../events/EventManager.js';
import {CDP, CommonDataTypes} from '../../../protocol/protocol.js';
import {Deferred} from '../../../utils/deferred.js';
import {NetworkProcessor} from '../network/networkProcessor.js';
import type {ChannelProxy} from '../script/channelProxy.js';
import type {PreloadScriptStorage} from './PreloadScriptStorage.js';

export class CdpTarget {
  readonly #targetId: string;
  readonly #parentTargetId: string | null;
  readonly #cdpClient: ICdpClient;
  readonly #cdpSessionId: string;
  readonly #eventManager: IEventManager;
  readonly #preloadScriptStorage: PreloadScriptStorage;

  readonly #targetUnblocked: Deferred<void>;
  #networkDomainActivated: boolean;

  static create(
    targetId: string,
    parentTargetId: string | null,
    cdpClient: ICdpClient,
    cdpSessionId: string,
    realmStorage: RealmStorage,
    eventManager: IEventManager,
    preloadScriptStorage: PreloadScriptStorage
  ): CdpTarget {
    const cdpTarget = new CdpTarget(
      targetId,
      parentTargetId,
      cdpClient,
      cdpSessionId,
      eventManager,
      preloadScriptStorage
    );

    LogManager.create(cdpTarget, realmStorage, eventManager);

    cdpTarget.#setEventListeners();

    // No need to await.
    // Deferred will be resolved when the target is unblocked.
    void cdpTarget.#unblock();

    return cdpTarget;
  }

  private constructor(
    targetId: string,
    parentTargetId: string | null,
    cdpClient: ICdpClient,
    cdpSessionId: string,
    eventManager: IEventManager,
    preloadScriptStorage: PreloadScriptStorage
  ) {
    this.#targetId = targetId;
    this.#parentTargetId = parentTargetId;
    this.#cdpClient = cdpClient;
    this.#cdpSessionId = cdpSessionId;
    this.#eventManager = eventManager;
    this.#preloadScriptStorage = preloadScriptStorage;

    this.#networkDomainActivated = false;
    this.#targetUnblocked = new Deferred();
  }

  /** Returns a promise that resolves when the target is unblocked. */
  get targetUnblocked(): Deferred<void> {
    return this.#targetUnblocked;
  }

  get targetId(): string {
    return this.#targetId;
  }

  get cdpClient(): ICdpClient {
    return this.#cdpClient;
  }

  /**
   * Needed for CDP escape path.
   */
  get cdpSessionId(): string {
    return this.#cdpSessionId;
  }

  /**
   * Enables all the required CDP domains and unblocks the target.
   */
  async #unblock() {
    try {
      // Scheduled commands can be finished only after the
      // `Runtime.runIfWaitingForDebugger` is called. Collect all the commands
      // promises, and wait for them after the `Runtime.runIfWaitingForDebugger`
      // and before the command is finished.
      const promises: Promise<unknown>[] = [];

      // Enable Network domain, if it is enabled globally.
      // TODO: enable Network domain for OOPiF targets.
      if (this.#eventManager.isNetworkDomainEnabled) {
        promises.push(this.enableNetworkDomain());
      }

      // Schedule, but don't wait for the result, as the command can be finished
      // only after the `Runtime.runIfWaitingForDebugger`.
      promises.push(this.#cdpClient.sendCommand('Runtime.enable'));
      promises.push(this.#cdpClient.sendCommand('Page.enable'));
      promises.push(
        this.#cdpClient.sendCommand('Page.setLifecycleEventsEnabled', {
          enabled: true,
        })
      );
      promises.push(
        this.#cdpClient.sendCommand('Target.setAutoAttach', {
          autoAttach: true,
          waitForDebuggerOnStart: true,
          flatten: true,
        })
      );

      promises.push(this.#initAndEvaluatePreloadScripts());

      promises.push(
        this.#cdpClient.sendCommand('Runtime.runIfWaitingForDebugger')
      );
      // Wait for all the scheduled commands are finished.
      await Promise.all(promises);
    } catch (error: any) {
      // The target might have been closed before the initialization finished.
      if (!this.#cdpClient.isCloseError(error)) {
        throw error;
      }
    }

    this.#targetUnblocked.resolve();
  }

  /**
   * Enables the Network domain (creates NetworkProcessor on the target's cdp
   * client) if it is not enabled yet.
   */
  async enableNetworkDomain() {
    if (!this.#networkDomainActivated) {
      this.#networkDomainActivated = true;
      await NetworkProcessor.create(this.cdpClient, this.#eventManager);
    }
  }

  #setEventListeners() {
    this.#cdpClient.on('*', (cdpMethod, params) => {
      this.#eventManager.registerEvent(
        {
          method: CDP.EventNames.EventReceivedEvent,
          params: {
            cdpMethod: cdpMethod as keyof ProtocolMapping.Commands,
            cdpParams: params ?? {},
            cdpSession: this.#cdpSessionId,
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
  getChannels(contextId: CommonDataTypes.BrowsingContext): ChannelProxy[] {
    return this.#preloadScriptStorage
      .findPreloadScripts({
        contextIds: [null, contextId],
      })
      .flatMap((script) => script.channels);
  }

  /** Loads all top-level and parent preload scripts. */
  async #initAndEvaluatePreloadScripts() {
    for (const script of this.#preloadScriptStorage.findPreloadScripts({
      contextIds: [null, this.#parentTargetId],
    })) {
      // Upon attaching to a new target, schedule initiating target and running
      // preload scripts right after `Runtime.runIfWaitingForDebugger`, but
      // don't wait for the result, as the commands can be finished only after
      // the `Runtime.runIfWaitingForDebugger`.
      script.scheduleEvaluateInTarget(this);
      await script.initInTarget(this);
    }
  }
}
