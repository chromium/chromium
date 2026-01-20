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
import type {BluetoothProcessor} from '../bluetooth/BluetoothProcessor.js';
import type {ContextConfigStorage} from '../browser/ContextConfigStorage.js';
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
import type {SpeculationProcessor} from '../speculation/SpeculationProcessor.js';

import {CdpTarget} from './CdpTarget.js';

const cdpToBidiTargetTypes = {
  service_worker: 'service-worker',
  shared_worker: 'shared-worker',
  worker: 'dedicated-worker',
} as const;

export class CdpTargetManager {
  readonly #browserCdpClient: CdpClient;
  readonly #cdpConnection: CdpConnection;
  readonly #targetKeysToBeIgnoredByAutoAttach = new Set<string>();
  readonly #selfTargetId: string;
  readonly #eventManager: EventManager;

  readonly #browsingContextStorage: BrowsingContextStorage;
  readonly #networkStorage: NetworkStorage;
  readonly #bluetoothProcessor: BluetoothProcessor;
  readonly #preloadScriptStorage: PreloadScriptStorage;
  readonly #realmStorage: RealmStorage;
  readonly #configStorage: ContextConfigStorage;
  readonly #speculationProcessor: SpeculationProcessor;

  readonly #defaultUserContextId: Browser.UserContext;
  readonly #defaultUserAgent: string;
  readonly #logger?: LoggerFn;

  constructor(
    cdpConnection: CdpConnection,
    browserCdpClient: CdpClient,
    selfTargetId: string,
    eventManager: EventManager,
    browsingContextStorage: BrowsingContextStorage,
    realmStorage: RealmStorage,
    networkStorage: NetworkStorage,
    configStorage: ContextConfigStorage,
    bluetoothProcessor: BluetoothProcessor,
    speculationProcessor: SpeculationProcessor,
    preloadScriptStorage: PreloadScriptStorage,
    defaultUserContextId: Browser.UserContext,
    defaultUserAgent: string,
    logger?: LoggerFn,
  ) {
    this.#cdpConnection = cdpConnection;
    this.#browserCdpClient = browserCdpClient;
    this.#targetKeysToBeIgnoredByAutoAttach.add(selfTargetId);
    this.#selfTargetId = selfTargetId;
    this.#eventManager = eventManager;
    this.#browsingContextStorage = browsingContextStorage;
    this.#preloadScriptStorage = preloadScriptStorage;
    this.#networkStorage = networkStorage;
    this.#configStorage = configStorage;
    this.#bluetoothProcessor = bluetoothProcessor;
    this.#speculationProcessor = speculationProcessor;
    this.#realmStorage = realmStorage;
    this.#defaultUserContextId = defaultUserContextId;
    this.#defaultUserAgent = defaultUserAgent;
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
      this.#handleDetachedFromTargetEvent.bind(this),
    );
    cdpClient.on(
      'Target.targetInfoChanged',
      this.#handleTargetInfoChangedEvent.bind(this),
    );
    cdpClient.on('Inspector.targetCrashed', () => {
      this.#handleTargetCrashedEvent(cdpClient);
    });

    cdpClient.on(
      'Page.frameAttached',
      this.#handleFrameAttachedEvent.bind(this),
    );
    cdpClient.on(
      'Page.frameSubtreeWillBeDetached',
      this.#handleFrameSubtreeWillBeDetached.bind(this),
    );
  }

  #handleFrameAttachedEvent(params: Protocol.Page.FrameAttachedEvent) {
    const parentBrowsingContext = this.#browsingContextStorage.findContext(
      params.parentFrameId,
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
        this.#configStorage,
        // At this point, we don't know the URL of the frame yet, so it will be updated
        // later.
        'about:blank',
        undefined,
        this.#logger,
      );
    }
  }

  #handleFrameSubtreeWillBeDetached(
    params: Protocol.Page.FrameSubtreeWillBeDetachedEvent,
  ) {
    this.#browsingContextStorage.findContext(params.frameId)?.dispose(true);
  }

  #handleAttachedToTargetEvent(
    params: Protocol.Target.AttachedToTargetEvent,
    parentSessionCdpClient: CdpClient,
  ) {
    const {sessionId, targetInfo} = params;
    const targetCdpClient = this.#cdpConnection.getCdpClient(sessionId);

    const detach = async () => {
      // Detaches and resumes the target suppressing errors.
      await targetCdpClient
        .sendCommand('Runtime.runIfWaitingForDebugger')
        .then(() =>
          parentSessionCdpClient.sendCommand('Target.detachFromTarget', params),
        )
        .catch((error) => this.#logger?.(LogType.debugError, error));
    };

    // Do not attach to the Mapper target.
    if (this.#selfTargetId === targetInfo.targetId) {
      void detach();
      return;
    }
    // Service workers are special case because they attach to the
    // browser target and the page target (so twice per worker) during
    // the regular auto-attach and might hang if the CDP session on
    // the browser level is not detached. The logic to detach the
    // right session is handled in the switch below.
    const targetKey =
      targetInfo.type === 'service_worker'
        ? `${parentSessionCdpClient.sessionId}_${targetInfo.targetId}`
        : targetInfo.targetId;

    // Mapper generally only needs one session per target. If we
    // receive additional auto-attached sessions, that is very likely
    // coming from custom CDP sessions.
    if (this.#targetKeysToBeIgnoredByAutoAttach.has(targetKey)) {
      // Return to leave the session untouched.
      return;
    }
    this.#targetKeysToBeIgnoredByAutoAttach.add(targetKey);

    const userContext =
      targetInfo.browserContextId &&
      targetInfo.browserContextId !== this.#defaultUserContextId
        ? targetInfo.browserContextId
        : 'default';

    switch (targetInfo.type) {
      case 'tab': {
        // Tab targets are required only to handle page targets beneath them.
        this.#setEventListeners(targetCdpClient);

        // Auto-attach to the page target. No need in resuming tab target debugger, as it
        // should preserve the page target debugger state, and will be resumed by the page
        // target.
        void (async () => {
          await targetCdpClient.sendCommand('Target.setAutoAttach', {
            autoAttach: true,
            waitForDebuggerOnStart: true,
            flatten: true,
          });
        })();
        return;
      }
      case 'page':
      case 'iframe': {
        const cdpTarget = this.#createCdpTarget(
          targetCdpClient,
          parentSessionCdpClient,
          targetInfo,
          userContext,
        );
        const maybeContext = this.#browsingContextStorage.findContext(
          targetInfo.targetId,
        );
        if (maybeContext && targetInfo.type === 'iframe') {
          // OOPiF.
          maybeContext.updateCdpTarget(cdpTarget);
        } else {
          // If attaching to existing browser instance, there could be OOPiF targets. This
          // case is handled by the `findFrameParentId` method.
          const parentId = this.#findFrameParentId(
            targetInfo,
            parentSessionCdpClient.sessionId,
          );
          // New context.
          BrowsingContextImpl.create(
            targetInfo.targetId,
            parentId,
            userContext,
            cdpTarget,
            this.#eventManager,
            this.#browsingContextStorage,
            this.#realmStorage,
            this.#configStorage,
            // Hack: when a new target created, CDP emits targetInfoChanged with an empty
            // url, and navigates it to about:blank later. When the event is emitted for
            // an existing target (reconnect), the url is already known, and navigation
            // events will not be emitted anymore. Replacing empty url with `about:blank`
            // allows to handle both cases in the same way.
            // "7.3.2.1 Creating browsing contexts".
            // https://html.spec.whatwg.org/multipage/document-sequences.html#creating-browsing-contexts
            // TODO: check who to deal with non-null creator and its `creatorOrigin`.
            targetInfo.url === '' ? 'about:blank' : targetInfo.url,
            targetInfo.openerFrameId ?? targetInfo.openerId,
            this.#logger,
          );
        }
        return;
      }
      case 'service_worker':
      case 'worker': {
        const realm = this.#realmStorage.findRealm({
          cdpSessionId: parentSessionCdpClient.sessionId,
          sandbox: null, // Non-sandboxed realms.
        });
        // If there is no browsing context, this worker is already terminated.
        if (!realm) {
          void detach();
          return;
        }

        const cdpTarget = this.#createCdpTarget(
          targetCdpClient,
          parentSessionCdpClient,
          targetInfo,
          userContext,
        );
        this.#handleWorkerTarget(
          cdpToBidiTargetTypes[targetInfo.type],
          cdpTarget,
          realm,
        );
        return;
      }
      // In CDP, we only emit shared workers on the browser and not the set of
      // frames that use the shared worker. If we change this in the future to
      // behave like service workers (emits on both browser and frame targets),
      // we can remove this block and merge service workers with the above one.
      case 'shared_worker': {
        const cdpTarget = this.#createCdpTarget(
          targetCdpClient,
          parentSessionCdpClient,
          targetInfo,
          userContext,
        );
        this.#handleWorkerTarget(
          cdpToBidiTargetTypes[targetInfo.type],
          cdpTarget,
        );
        return;
      }
    }

    // DevTools or some other not supported by BiDi target. Just release
    // debugger and ignore them.
    void detach();
  }

  /** Try to find the parent browsing context ID for the given attached target. */
  #findFrameParentId(
    targetInfo: Protocol.Target.TargetInfo,
    parentSessionId: Protocol.Target.SessionID | undefined,
  ): string | null {
    if (targetInfo.type !== 'iframe') {
      return null;
    }
    const parentId = targetInfo.openerFrameId ?? targetInfo.openerId;
    if (parentId !== undefined) {
      return parentId;
    }
    if (parentSessionId !== undefined) {
      return (
        this.#browsingContextStorage.findContextBySession(parentSessionId)
          ?.id ?? null
      );
    }
    return null;
  }

  #createCdpTarget(
    targetCdpClient: CdpClient,
    parentCdpClient: CdpClient,
    targetInfo: Protocol.Target.TargetInfo,
    userContext: Browser.UserContext,
  ) {
    this.#setEventListeners(targetCdpClient);
    this.#preloadScriptStorage.onCdpTargetCreated(
      targetInfo.targetId,
      userContext,
    );

    const target = CdpTarget.create(
      targetInfo.targetId,
      targetCdpClient,
      this.#browserCdpClient,
      parentCdpClient,
      this.#realmStorage,
      this.#eventManager,
      this.#preloadScriptStorage,
      this.#browsingContextStorage,
      this.#networkStorage,
      this.#configStorage,
      userContext,
      // Pass the cached default User Agent to the new target.
      this.#defaultUserAgent,
      this.#logger,
    );

    this.#networkStorage.onCdpTargetCreated(target);
    this.#bluetoothProcessor.onCdpTargetCreated(target);
    this.#speculationProcessor.onCdpTargetCreated(target);

    return target;
  }

  #workers = new Map<string, Realm>();
  #handleWorkerTarget(
    realmType: WorkerRealmType,
    cdpTarget: CdpTarget,
    ownerRealm?: Realm,
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
        realmType,
      );
      this.#workers.set(cdpTarget.cdpSessionId, workerRealm);
    });
  }

  #handleDetachedFromTargetEvent({
    sessionId,
    targetId,
  }: Protocol.Target.DetachedFromTargetEvent) {
    if (targetId) {
      this.#preloadScriptStorage.find({targetId}).map((preloadScript) => {
        preloadScript.dispose(targetId);
      });
    }
    const context =
      this.#browsingContextStorage.findContextBySession(sessionId);
    if (context) {
      context.dispose(true);
      return;
    }

    const worker = this.#workers.get(sessionId);
    if (worker) {
      this.#realmStorage.deleteRealms({
        cdpSessionId: worker.cdpClient.sessionId,
      });
    }
  }

  #handleTargetInfoChangedEvent(
    params: Protocol.Target.TargetInfoChangedEvent,
  ) {
    const context = this.#browsingContextStorage.findContext(
      params.targetInfo.targetId,
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
