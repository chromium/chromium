/**
 * Copyright 2024 Google LLC.
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
import type Protocol from 'devtools-protocol';

import type {CdpClient} from '../../../cdp/CdpClient.js';
import type {CdpConnection} from '../../../cdp/CdpConnection.js';
import type {Browser} from '../../../protocol/protocol.js';
import {LogType, type LoggerFn} from '../../../utils/log.js';
import {
  BrowsingContextImpl,
  serializeOrigin,
} from '../context/BrowsingContextImpl.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';
import type {NetworkStorage} from '../network/NetworkStorage.js';
import type {PreloadScriptStorage} from '../script/PreloadScriptStorage.js';
import type {Realm} from '../script/Realm.js';
import type {RealmStorage} from '../script/RealmStorage.js';
import {WorkerRealm, type WorkerRealmType} from '../script/WorkerRealm.js';
import type {EventManager} from '../session/EventManager.js';

import {CdpTarget} from './CdpTarget.js';

const cdpToBidiTargetTypes = {
  service_worker: 'service-worker',
  shared_worker: 'shared-worker',
  worker: 'dedicated-worker',
} as const;

export class CdpTargetManager {
  readonly #browserCdpClient: CdpClient;
  readonly #cdpConnection: CdpConnection;
  readonly #selfTargetId: string;
  readonly #eventManager: EventManager;

  readonly #browsingContextStorage: BrowsingContextStorage;
  readonly #networkStorage: NetworkStorage;
  readonly #acceptInsecureCerts: boolean;
  readonly #preloadScriptStorage: PreloadScriptStorage;
  readonly #realmStorage: RealmStorage;

  readonly #defaultUserContextId: Browser.UserContext;
  readonly #logger?: LoggerFn;

  constructor(
    cdpConnection: CdpConnection,
    browserCdpClient: CdpClient,
    selfTargetId: string,
    eventManager: EventManager,
    browsingContextStorage: BrowsingContextStorage,
    realmStorage: RealmStorage,
    networkStorage: NetworkStorage,
    preloadScriptStorage: PreloadScriptStorage,
    acceptInsecureCerts: boolean,
    defaultUserContextId: Browser.UserContext,
    logger?: LoggerFn
  ) {
    this.#acceptInsecureCerts = acceptInsecureCerts;
    this.#cdpConnection = cdpConnection;
    this.#browserCdpClient = browserCdpClient;
    this.#selfTargetId = selfTargetId;
    this.#eventManager = eventManager;
    this.#browsingContextStorage = browsingContextStorage;
    this.#preloadScriptStorage = preloadScriptStorage;
    this.#networkStorage = networkStorage;
    this.#realmStorage = realmStorage;
    this.#defaultUserContextId = defaultUserContextId;
    this.#logger = logger;

    this.#setEventListeners(browserCdpClient);
  }

  /**
   * This method is called for each CDP session, since this class is responsible
   * for creating and destroying all targets and browsing contexts.
   */
  #setEventListeners(cdpClient: CdpClient) {
    cdpClient.on('Target.attachedToTarget', (params) => {
      this.#handleAttachedToTargetEvent(params, cdpClient);
    });
    cdpClient.on(
      'Target.detachedFromTarget',
      this.#handleDetachedFromTargetEvent.bind(this)
    );
    cdpClient.on(
      'Target.targetInfoChanged',
      this.#handleTargetInfoChangedEvent.bind(this)
    );
    cdpClient.on('Inspector.targetCrashed', () => {
      this.#handleTargetCrashedEvent(cdpClient);
    });

    cdpClient.on(
      'Page.frameAttached',
      this.#handleFrameAttachedEvent.bind(this)
    );
    cdpClient.on(
      'Page.frameDetached',
      this.#handleFrameDetachedEvent.bind(this)
    );
  }

  #handleFrameAttachedEvent(params: Protocol.Page.FrameAttachedEvent) {
    const parentBrowsingContext = this.#browsingContextStorage.findContext(
      params.parentFrameId
    );
    if (parentBrowsingContext !== undefined) {
      BrowsingContextImpl.create(
        params.frameId,
        params.parentFrameId,
        parentBrowsingContext.userContext,
        parentBrowsingContext.cdpTarget,
        this.#eventManager,
        this.#browsingContextStorage,
        this.#realmStorage,
        this.#logger
      );
    }
  }

  #handleFrameDetachedEvent(params: Protocol.Page.FrameDetachedEvent) {
    // In case of OOPiF no need in deleting BrowsingContext.
    if (params.reason === 'swap') {
      return;
    }
    this.#browsingContextStorage.findContext(params.frameId)?.dispose();
  }

  #handleAttachedToTargetEvent(
    params: Protocol.Target.AttachedToTargetEvent,
    parentSessionCdpClient: CdpClient
  ) {
    const {sessionId, targetInfo} = params;
    const targetCdpClient = this.#cdpConnection.getCdpClient(sessionId);

    switch (targetInfo.type) {
      case 'page':
      case 'iframe': {
        if (targetInfo.targetId === this.#selfTargetId) {
          break;
        }

        const cdpTarget = this.#createCdpTarget(targetCdpClient, targetInfo);
        const maybeContext = this.#browsingContextStorage.findContext(
          targetInfo.targetId
        );
        if (maybeContext) {
          // OOPiF.
          maybeContext.updateCdpTarget(cdpTarget);
        } else {
          const userContext =
            targetInfo.browserContextId &&
            targetInfo.browserContextId !== this.#defaultUserContextId
              ? targetInfo.browserContextId
              : 'default';
          // New context.
          BrowsingContextImpl.create(
            targetInfo.targetId,
            null,
            userContext,
            cdpTarget,
            this.#eventManager,
            this.#browsingContextStorage,
            this.#realmStorage,
            this.#logger
          );
        }
        return;
      }
      case 'service_worker':
      case 'worker': {
        const realm = this.#realmStorage.findRealm({
          cdpSessionId: parentSessionCdpClient.sessionId,
        });
        // If there is no browsing context, this worker is already terminated.
        if (!realm) {
          break;
        }

        const cdpTarget = this.#createCdpTarget(targetCdpClient, targetInfo);
        this.#handleWorkerTarget(
          cdpToBidiTargetTypes[targetInfo.type],
          cdpTarget,
          realm
        );
        return;
      }
      // In CDP, we only emit shared workers on the browser and not the set of
      // frames that use the shared worker. If we change this in the future to
      // behave like service workers (emits on both browser and frame targets),
      // we can remove this block and merge service workers with the above one.
      case 'shared_worker': {
        const cdpTarget = this.#createCdpTarget(targetCdpClient, targetInfo);
        this.#handleWorkerTarget(
          cdpToBidiTargetTypes[targetInfo.type],
          cdpTarget
        );
        return;
      }
    }

    // DevTools or some other not supported by BiDi target. Just release
    // debugger and ignore them.
    targetCdpClient
      .sendCommand('Runtime.runIfWaitingForDebugger')
      .then(() =>
        parentSessionCdpClient.sendCommand('Target.detachFromTarget', params)
      )
      .catch((error) => this.#logger?.(LogType.debugError, error));
  }

  #createCdpTarget(
    targetCdpClient: CdpClient,
    targetInfo: Protocol.Target.TargetInfo
  ) {
    this.#setEventListeners(targetCdpClient);

    const target = CdpTarget.create(
      targetInfo.targetId,
      targetCdpClient,
      this.#browserCdpClient,
      this.#realmStorage,
      this.#eventManager,
      this.#preloadScriptStorage,
      this.#networkStorage,
      this.#acceptInsecureCerts,
      this.#logger
    );

    this.#networkStorage.onCdpTargetCreated(target);

    return target;
  }

  #workers = new Map<string, Realm>();
  #handleWorkerTarget(
    realmType: WorkerRealmType,
    cdpTarget: CdpTarget,
    ownerRealm?: Realm
  ) {
    cdpTarget.cdpClient.on('Runtime.executionContextCreated', (params) => {
      const {uniqueId, id, origin} = params.context;
      const workerRealm = new WorkerRealm(
        cdpTarget.cdpClient,
        this.#eventManager,
        id,
        this.#logger,
        serializeOrigin(origin),
        ownerRealm ? [ownerRealm] : [],
        uniqueId,
        this.#realmStorage,
        realmType
      );
      this.#workers.set(cdpTarget.cdpSessionId, workerRealm);
    });
  }

  #handleDetachedFromTargetEvent(
    params: Protocol.Target.DetachedFromTargetEvent
  ) {
    const context = this.#browsingContextStorage.findContextBySession(
      params.sessionId
    );
    if (context) {
      context.dispose();
      this.#preloadScriptStorage
        .find({targetId: context.id})
        .map((preloadScript) => preloadScript.dispose(context.id));
      return;
    }

    const worker = this.#workers.get(params.sessionId);
    if (worker) {
      this.#realmStorage.deleteRealms({
        cdpSessionId: worker.cdpClient.sessionId,
      });
    }
  }

  #handleTargetInfoChangedEvent(
    params: Protocol.Target.TargetInfoChangedEvent
  ) {
    const context = this.#browsingContextStorage.findContext(
      params.targetInfo.targetId
    );
    if (context) {
      context.onTargetInfoChanged(params);
    }
  }

  #handleTargetCrashedEvent(cdpClient: CdpClient) {
    // This is primarily used for service and shared workers. CDP tends to not
    // signal they closed gracefully and instead says they crashed to signal
    // they are closed.
    const realms = this.#realmStorage.findRealms({
      cdpSessionId: cdpClient.sessionId,
    });
    for (const realm of realms) {
      realm.dispose();
    }
  }
}
