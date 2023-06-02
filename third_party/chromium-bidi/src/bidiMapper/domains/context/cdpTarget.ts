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
import {RealmStorage} from '../script/realmStorage.js';
import type {IEventManager} from '../events/EventManager.js';
import {CDP, Script} from '../../../protocol/protocol.js';
import {Deferred} from '../../../utils/deferred.js';
import {NetworkProcessor} from '../network/networkProcessor.js';
import {LoggerFn, LogType} from '../../../utils/log.js';

import {PreloadScriptStorage} from './PreloadScriptStorage.js';
import {BrowsingContextStorage} from './browsingContextStorage';

export class CdpTarget {
  readonly #targetId: string;
  readonly #parentTargetId: string | null;
  readonly #cdpClient: ICdpClient;
  readonly #cdpSessionId: string;
  readonly #eventManager: IEventManager;
  readonly #preloadScriptStorage: PreloadScriptStorage;
  readonly #logger?: LoggerFn;

  readonly #targetUnblocked: Deferred<void>;
  #networkDomainActivated: boolean;
  #browsingContextStorage: BrowsingContextStorage;

  static create(
    targetId: string,
    parentTargetId: string | null,
    cdpClient: ICdpClient,
    cdpSessionId: string,
    realmStorage: RealmStorage,
    eventManager: IEventManager,
    preloadScriptStorage: PreloadScriptStorage,
    browsingContextStorage: BrowsingContextStorage,
    logger?: LoggerFn
  ): CdpTarget {
    const cdpTarget = new CdpTarget(
      targetId,
      parentTargetId,
      cdpClient,
      cdpSessionId,
      eventManager,
      preloadScriptStorage,
      browsingContextStorage,
      logger
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
    preloadScriptStorage: PreloadScriptStorage,
    browsingContextStorage: BrowsingContextStorage,
    logger?: LoggerFn
  ) {
    this.#targetId = targetId;
    this.#parentTargetId = parentTargetId;
    this.#cdpClient = cdpClient;
    this.#cdpSessionId = cdpSessionId;
    this.#eventManager = eventManager;
    this.#preloadScriptStorage = preloadScriptStorage;
    this.#browsingContextStorage = browsingContextStorage;
    this.#logger = logger;

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

      await this.#loadPreloadScripts();

      await this.#cdpClient.sendCommand('Runtime.runIfWaitingForDebugger');
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

  /** Loads all top-level and parent preload scripts. */
  async #loadPreloadScripts() {
    for (const script of this.#preloadScriptStorage.findPreloadScripts({
      contextIds: [null, this.#parentTargetId],
    })) {
      const {functionDeclaration, sandbox} = script;

      // The spec provides a function, and CDP expects an evaluation.
      const cdpPreloadScriptId = await this.addPreloadScript(
        `(${functionDeclaration})();`,
        sandbox
      );

      // Upon attaching to a new target, run preload scripts on each execution
      // context before `Runtime.runIfWaitingForDebugger`.
      //
      // Otherwise a browsing context might be created without the evaluation of
      // preload scripts.
      await Promise.all(
        this.#browsingContextStorage
          .getAllContexts()
          .filter((context) => context.cdpTarget === this)
          .map((context) =>
            context
              .getOrCreateSandbox(sandbox)
              .then((realm) =>
                this.cdpClient.sendCommand('Runtime.evaluate', {
                  expression: `(${functionDeclaration})();`,
                  contextId: realm.executionContextId,
                })
              )
              .catch((error) => {
                this.#logger?.(
                  LogType.cdp,
                  'Could not evaluate preload script',
                  error
                );
              })
          )
      );

      this.#preloadScriptStorage.appendCdpPreloadScript(script, {
        target: this,
        preloadScriptId: cdpPreloadScriptId,
      });
    }
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
