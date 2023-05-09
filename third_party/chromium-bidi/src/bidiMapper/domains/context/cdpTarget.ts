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

import {CdpClient} from '../../CdpConnection.js';
import {LogManager} from '../log/logManager.js';
import {RealmStorage} from '../script/realmStorage.js';
import {IEventManager} from '../events/EventManager.js';
import {CDP, Script} from '../../../protocol/protocol.js';
import {Deferred} from '../../../utils/deferred.js';
import {NetworkProcessor} from '../network/networkProcessor.js';

export class CdpTarget {
  readonly #targetId: string;
  readonly #cdpClient: CdpClient;
  readonly #cdpSessionId: string;
  readonly #eventManager: IEventManager;

  readonly #targetUnblocked: Deferred<void>;
  #networkDomainActivated: boolean;

  static create(
    targetId: string,
    cdpClient: CdpClient,
    cdpSessionId: string,
    realmStorage: RealmStorage,
    eventManager: IEventManager
  ): CdpTarget {
    const cdpTarget = new CdpTarget(
      targetId,
      cdpClient,
      cdpSessionId,
      eventManager
    );

    LogManager.create(cdpTarget, realmStorage, eventManager);

    cdpTarget.#setEventListeners();

    // No need in waiting. Deferred will be resolved when the target is unblocked.
    void cdpTarget.#unblock();
    return cdpTarget;
  }

  private constructor(
    targetId: string,
    cdpClient: CdpClient,
    cdpSessionId: string,
    eventManager: IEventManager
  ) {
    this.#targetId = targetId;
    this.#cdpClient = cdpClient;
    this.#cdpSessionId = cdpSessionId;
    this.#eventManager = eventManager;

    this.#networkDomainActivated = false;
    this.#targetUnblocked = new Deferred();
  }

  /**
   * Returns a promise that resolves when the target is unblocked.
   */
  get targetUnblocked(): Deferred<void> {
    return this.#targetUnblocked;
  }

  get targetId(): string {
    return this.#targetId;
  }

  get cdpClient(): CdpClient {
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
    // Enable Network domain, if it is enabled globally.
    // TODO: enable Network domain for OOPiF targets.
    if (this.#eventManager.isNetworkDomainEnabled) {
      await this.enableNetworkDomain();
    }

    await this.#cdpClient.sendCommand('Runtime.enable');
    await this.#cdpClient.sendCommand('Page.enable');
    await this.#cdpClient.sendCommand('Page.setLifecycleEventsEnabled', {
      enabled: true,
    });
    await this.#cdpClient.sendCommand('Target.setAutoAttach', {
      autoAttach: true,
      waitForDebuggerOnStart: true,
      flatten: true,
    });

    await this.#cdpClient.sendCommand('Runtime.runIfWaitingForDebugger');
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
   * Issues `Page.addScriptToEvaluateOnNewDocument` CDP command with the given
   * script source in evaluated form and world name / sandbox.
   *
   * @return The CDP preload script ID.
   */
  async addPreloadScript(
    scriptSource: string,
    sandbox?: string
  ): Promise<Script.PreloadScript> {
    const result = await this.cdpClient.sendCommand(
      'Page.addScriptToEvaluateOnNewDocument',
      {
        source: scriptSource,
        worldName: sandbox,
      }
    );

    return result.identifier;
  }

  /**
   * Issues `Page.removeScriptToEvaluateOnNewDocument` CDP command with the
   * given CDP preload script ID.
   */
  async removePreloadScript(cdpPreloadScriptId: string): Promise<void> {
    await this.cdpClient.sendCommand(
      'Page.removeScriptToEvaluateOnNewDocument',
      {
        identifier: cdpPreloadScriptId,
      }
    );
  }
}
