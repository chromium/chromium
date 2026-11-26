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
  readonly #browserOnlyTargetKeys = new Set<string>();
  readonly #rendererTargetKeys = new Set<string>();
  readonly #tabSessionIds = new Set<Protocol.Target.SessionID>();
  readonly #deferredRendererPageDetaches = new Map<
    Protocol.Target.TargetID,
    Map<
      Protocol.Target.SessionID,
      {
        parentSessionId: Protocol.Target.SessionID | undefined;
        detach: () => void;
      }
    >
  >();
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
    this.#browserOnlyTargetKeys.add(selfTargetId);
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
    // Forward browser-level CDP events to BiDi.
    this.#setCdpEventListeners(browserCdpClient);
  }

  #isBrowserOnlySession(cdpClient: CdpClient): boolean {
    return (
      cdpClient === this.#browserCdpClient ||
      (cdpClient.sessionId !== undefined &&
        this.#tabSessionIds.has(cdpClient.sessionId))
    );
  }

  /**
   * Forward CDP events to BiDi at the browser level (global events) for
   * sessions not associated with a BrowsingContext (e.g. the browser session
   * and custom CDP sessions).
   */
  #setCdpEventListeners(cdpClient: CdpClient) {
    cdpClient.on('*', (event, params) => {
      // Skip non-CDP EventEmitter events.
      if (typeof event !== 'string') {
        return;
      }
      this.#eventManager.registerGlobalEvent({
        type: 'event',
        method: `goog:cdp.${event}`,
        params: {
          event,
          params,
          session: cdpClient.sessionId,
        },
      });
    });
  }

  /**
   * This method is called for each CDP session, since this class is responsible
   * for creating and destroying all targets and browsing contexts.
   */
  #setEventListeners(cdpClient: CdpClient) {
    // The browser session reports info changes for all targets (see
    // `Target.setDiscoverTargets` in `BidiServer`). Other sessions can carry
    // renderer-originated events, so only trust the browser session here.
    if (cdpClient === this.#browserCdpClient) {
      cdpClient.on(
        'Target.targetInfoChanged',
        this.#handleTargetInfoChangedEvent.bind(this),
      );
    }
    cdpClient.on('Target.attachedToTarget', (params) => {
      this.#handleAttachedToTargetEvent(params, cdpClient);
    });
    cdpClient.on(
      'Target.detachedFromTarget',
      this.#handleDetachedFromTargetEvent.bind(this),
    );
    cdpClient.on('Inspector.targetCrashed', () => {
      this.#handleTargetCrashedEvent(cdpClient);
    });

    // Pass the emitting session, so that frame events are only applied to
    // frames hosted by that session.
    cdpClient.on('Page.frameAttached', (params) => {
      this.#handleFrameAttachedEvent(params, cdpClient);
    });
    cdpClient.on('Page.frameSubtreeWillBeDetached', (params) => {
      this.#handleFrameSubtreeWillBeDetached(params, cdpClient);
    });
  }

  #handleFrameAttachedEvent(
    params: Protocol.Page.FrameAttachedEvent,
    cdpClient: CdpClient,
  ) {
    const parentBrowsingContext = this.#browsingContextStorage.findContext(
      params.parentFrameId,
    );
    if (parentBrowsingContext === undefined) {
      return;
    }
    // A renderer reports frames attached to the frames it hosts. A session
    // that does not host the parent frame cannot attach frames to it.
    if (parentBrowsingContext.cdpTarget.cdpClient !== cdpClient) {
      this.#logger?.(LogType.debugError)?.(
        `Ignoring frameAttached for ${params.frameId}: the session does not host parent ${params.parentFrameId}`,
      );
      return;
    }
    // A known frame is either already hosted by this session, or by another
    // target which only the browser can release (`Target.detachedFromTarget`).
    // Never re-create or re-bind it here. This also prevents parent cycles,
    // as every ancestor of the parent is a known frame.
    if (this.#browsingContextStorage.hasContext(params.frameId)) {
      return;
    }
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

  #handleFrameSubtreeWillBeDetached(
    params: Protocol.Page.FrameSubtreeWillBeDetachedEvent,
    cdpClient: CdpClient,
  ) {
    const context = this.#browsingContextStorage.findContext(params.frameId);
    // Only the session hosting the frame can remove it here. A frame with its
    // own target (OOPIF) is disposed when the browser reports that target
    // detached, as the embedder's session does not host it.
    if (context?.cdpTarget.cdpClient !== cdpClient) {
      return;
    }
    context.dispose(true);
  }

  #handleAttachedToTargetEvent(
    params: Protocol.Target.AttachedToTargetEvent,
    parentSessionCdpClient: CdpClient,
  ) {
    const {sessionId, targetInfo} = params;
    const targetCdpClient = this.#cdpConnection.getCdpClient(sessionId);

    const detach = async () => {
      // Detaches and resumes the target suppressing errors.
      await Promise.allSettled([
        targetCdpClient.sendCommand('Runtime.runIfWaitingForDebugger'),
        // `tab` targets are left paused to be resumed by their child target
        // (`CdpTarget.unblock`). When a `tab`'s child is detached instead
        // (e.g. a non-DevTools `other` target), resume the parent `tab` too.
        ...(parentSessionCdpClient.sessionId !== undefined &&
        this.#tabSessionIds.has(parentSessionCdpClient.sessionId)
          ? [
              parentSessionCdpClient.sendCommand(
                'Runtime.runIfWaitingForDebugger',
              ),
            ]
          : []),
      ])
        .then(() =>
          parentSessionCdpClient.sendCommand('Target.detachFromTarget', params),
        )
        .catch((error) => this.#logger?.(LogType.debugError)?.(error));
    };

    const fromBrowserOnlySession = this.#isBrowserOnlySession(
      parentSessionCdpClient,
    );

    // Do not attach to the Mapper target.
    if (this.#selfTargetId === targetInfo.targetId) {
      if (fromBrowserOnlySession) {
        void detach();
      }
      return;
    }

    // `tab` and `shared_worker` targets are only attached on the browser
    // session, and `page`/`other` targets are only attached on a browser-only
    // session (`browser` or `tab`). Reject renderer-emitted attaches before
    // recording any target state.
    if (
      ((targetInfo.type === 'tab' || targetInfo.type === 'shared_worker') &&
        parentSessionCdpClient !== this.#browserCdpClient) ||
      ((targetInfo.type === 'page' || targetInfo.type === 'other') &&
        !fromBrowserOnlySession)
    ) {
      this.#logger?.(LogType.debugError)?.(
        `Ignoring ${targetInfo.type} attach for ${targetInfo.targetId} on non-browser session ${parentSessionCdpClient.sessionId}`,
      );
      if (targetInfo.type === 'page' && params.waitingForDebugger) {
        const existingContext = this.#browsingContextStorage.findContext(
          targetInfo.targetId,
        );
        if (
          existingContext !== undefined &&
          this.#browserOnlyTargetKeys.has(targetInfo.targetId) &&
          existingContext.cdpTarget.id === targetInfo.targetId
        ) {
          void (async () => {
            await existingContext.cdpTarget.resumed;
            void detach();
          })();
        } else {
          let deferred = this.#deferredRendererPageDetaches.get(
            targetInfo.targetId,
          );
          if (deferred === undefined) {
            deferred = new Map();
            this.#deferredRendererPageDetaches.set(
              targetInfo.targetId,
              deferred,
            );
          }
          deferred.set(sessionId, {
            parentSessionId: parentSessionCdpClient.sessionId,
            detach: () => {
              void detach();
            },
          });
        }
      }
      return;
    }

    // Service workers and dedicated workers scope their key by type and parent
    // session: service workers attach on both browser and page targets, and a
    // worker key on a renderer session must never collide with a frame ID.
    const targetKey =
      targetInfo.type === 'service_worker' || targetInfo.type === 'worker'
        ? `${targetInfo.type}_${parentSessionCdpClient.sessionId}_${targetInfo.targetId}`
        : targetInfo.targetId;

    // Mapper generally only needs one session per target. Additional sessions
    // attached on a browser-only session for an already-tracked target come
    // from custom CDP sessions.
    if (fromBrowserOnlySession) {
      if (
        this.#browserOnlyTargetKeys.has(targetKey) ||
        (targetInfo.type === 'iframe' &&
          this.#rendererTargetKeys.has(targetKey))
      ) {
        // Forward CDP events from custom sessions before leaving them untouched.
        this.#setCdpEventListeners(targetCdpClient);
        return;
      }
      this.#rendererTargetKeys.delete(targetKey);
      this.#browserOnlyTargetKeys.add(targetKey);
    } else {
      if (
        this.#browserOnlyTargetKeys.has(targetKey) ||
        this.#rendererTargetKeys.has(targetKey)
      ) {
        return;
      }
      this.#rendererTargetKeys.add(targetKey);
    }

    const userContext =
      targetInfo.browserContextId &&
      targetInfo.browserContextId !== this.#defaultUserContextId
        ? targetInfo.browserContextId
        : 'default';

    switch (targetInfo.type) {
      case 'tab': {
        this.#tabSessionIds.add(sessionId);
        // Tab targets are required only to handle page targets beneath them.
        this.#setEventListeners(targetCdpClient);

        // Auto-attach to the page target. No need in resuming tab target debugger, as it
        // should preserve the page target debugger state, and will be resumed by the page
        // target.
        void targetCdpClient
          .sendCommand('Target.setAutoAttach', {
            autoAttach: true,
            waitForDebuggerOnStart: true,
            flatten: true,
          })
          .catch((error) => {
            if (!targetCdpClient.isCloseError(error)) {
              this.#logger?.(LogType.debugError)?.(error);
            }
          });
        return;
      }
      case 'page':
      case 'iframe':
      case 'other': {
        // DevTools windows are exposed in CDP with type `other` and a `devtools://` URL.
        // Ignore all other `other` targets.
        if (
          targetInfo.type === 'other' &&
          !targetInfo.url.startsWith('devtools://')
        ) {
          void detach();
          return;
        }

        const maybeContext = this.#browsingContextStorage.findContext(
          targetInfo.targetId,
        );
        if (maybeContext !== undefined) {
          if (targetInfo.type === 'iframe') {
            // Only an iframe target attached through the session that hosts the
            // frame (or its parent) can take over an existing browsing context.
            if (
              !fromBrowserOnlySession &&
              maybeContext.cdpTarget.cdpClient !== parentSessionCdpClient &&
              maybeContext.parent?.cdpTarget.cdpClient !==
                parentSessionCdpClient
            ) {
              // Allow the owning session to attach the target later.
              this.#rendererTargetKeys.delete(targetKey);
              void detach();
              return;
            }
            const cdpTarget = this.#createCdpTarget(
              targetCdpClient,
              parentSessionCdpClient,
              targetInfo,
              userContext,
            );
            // OOPiF.
            maybeContext.updateCdpTarget(cdpTarget);
            void cdpTarget.unblock();
            return;
          }
          // A `page` or `other` target is attached on a browser-only session.
          // Replace any conflicting context created from renderer-reported data.
          maybeContext.dispose(true);
        }

        const cdpTarget = this.#createCdpTarget(
          targetCdpClient,
          parentSessionCdpClient,
          targetInfo,
          userContext,
        );
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
        void cdpTarget.unblock();
        if (targetInfo.type === 'page') {
          const deferredDetaches = this.#deferredRendererPageDetaches.get(
            targetInfo.targetId,
          );
          if (deferredDetaches !== undefined) {
            this.#deferredRendererPageDetaches.delete(targetInfo.targetId);
            void (async () => {
              await cdpTarget.resumed;
              for (const {
                detach: deferredDetach,
              } of deferredDetaches.values()) {
                deferredDetach();
              }
            })();
          }
        }
        return;
      }
      case 'worker': {
        // In CDP, `Target.attachedToTarget` for a dedicated worker spawned
        // during navigation can arrive before the parent frame's
        // `Runtime.executionContextCreated` event (or after
        // `Runtime.executionContextsCleared` deletes the previous document's
        // realm). Check `BrowsingContextStorage` so the worker target is not
        // prematurely detached while its parent browsing context's default
        // realm is still pending.
        let browsingContext: BrowsingContextImpl | undefined;
        if (parentSessionCdpClient.sessionId) {
          browsingContext = this.#browsingContextStorage.findContextBySession(
            parentSessionCdpClient.sessionId,
          );
        }
        const realm = this.#realmStorage.findRealm({
          cdpSessionId: parentSessionCdpClient.sessionId,
          sandbox: null, // Non-sandboxed realms.
        });
        // If there is no browsing context, this worker is already terminated.
        if (!realm && !browsingContext) {
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
          browsingContext,
        );
        void cdpTarget.unblock();
        return;
      }
      case 'service_worker': {
        const realm = this.#realmStorage.findRealm({
          cdpSessionId: parentSessionCdpClient.sessionId,
          sandbox: null, // Non-sandboxed realms.
        });
        // If there is no realm, this worker is already terminated.
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
        void cdpTarget.unblock();
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
        void cdpTarget.unblock();
        return;
      }
    }

    // Target not supported by BiDi. Just release debugger and ignore it.
    void detach();
  }

  /** Try to find the parent browsing context ID for the given attached target. */
  #findFrameParentId(
    targetInfo: Protocol.Target.TargetInfo,
    parentSessionId: Protocol.Target.SessionID | undefined,
  ): string | null {
    if (
      targetInfo.type !== 'iframe' ||
      targetInfo.parentFrameId === undefined
    ) {
      return null;
    }
    const parentContext = this.#browsingContextStorage.findContext(
      targetInfo.parentFrameId,
    );
    // Only accept a parent frame that exists and is hosted by the attaching session.
    if (
      parentContext !== undefined &&
      parentContext.cdpTarget.cdpSessionId === parentSessionId
    ) {
      return targetInfo.parentFrameId;
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
    const isFrameTarget = CdpTarget.isFrameTarget(targetInfo.type);
    if (isFrameTarget) {
      // Preload scripts only apply to window realms.
      this.#preloadScriptStorage.onCdpTargetCreated(
        targetInfo.targetId,
        userContext,
      );
    }

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
      targetInfo.type,
      this.#logger,
      targetInfo.parentFrameId,
    );

    this.#networkStorage.onCdpTargetCreated(target);
    if (isFrameTarget) {
      this.#bluetoothProcessor.onCdpTargetCreated(target);
      this.#speculationProcessor.onCdpTargetCreated(target);
    }

    return target;
  }

  #workers = new Map<string, Realm>();
  #handleWorkerTarget(
    realmType: WorkerRealmType,
    cdpTarget: CdpTarget,
    ownerRealm?: Realm,
    browsingContext?: BrowsingContextImpl,
  ) {
    cdpTarget.cdpClient.on(
      'Runtime.executionContextCreated',
      async (params) => {
        const {uniqueId, id, origin} = params.context;
        // Prefer the parent session's current default realm, or await it on the
        // parent browsing context if navigation has not created it yet.
        let resolvedOwnerRealm = browsingContext
          ? this.#realmStorage.findRealm({
              cdpSessionId: cdpTarget.parentCdpClient.sessionId,
              sandbox: null,
            })
          : ownerRealm;
        if (!resolvedOwnerRealm && browsingContext) {
          let detached = false;
          const onDetached = (
            params: Protocol.Target.DetachedFromTargetEvent,
          ) => {
            if (params.sessionId === cdpTarget.cdpSessionId) {
              detached = true;
            }
          };
          cdpTarget.parentCdpClient.on('Target.detachedFromTarget', onDetached);
          try {
            resolvedOwnerRealm =
              await browsingContext.getOrCreateUserSandbox(undefined);
          } catch (error) {
            // The context was destroyed before its default realm was created.
            this.#logger?.(LogType.debugError)?.(error);
            return;
          } finally {
            cdpTarget.parentCdpClient.off(
              'Target.detachedFromTarget',
              onDetached,
            );
          }
          // Do not register a realm if the worker detached while awaiting its owner.
          if (detached) {
            return;
          }
        }
        const workerRealm = new WorkerRealm(
          cdpTarget.cdpClient,
          this.#eventManager,
          id,
          this.#logger,
          serializeOrigin(origin),
          resolvedOwnerRealm ? [resolvedOwnerRealm] : [],
          uniqueId,
          this.#realmStorage,
          realmType,
        );
        this.#workers.set(cdpTarget.cdpSessionId, workerRealm);
      },
    );
  }

  #handleDetachedFromTargetEvent({
    sessionId,
    targetId,
  }: Protocol.Target.DetachedFromTargetEvent) {
    this.#tabSessionIds.delete(sessionId);
    for (const [
      deferredTargetId,
      sessions,
    ] of this.#deferredRendererPageDetaches.entries()) {
      for (const [deferredSessionId, entry] of sessions.entries()) {
        if (
          deferredSessionId === sessionId ||
          entry.parentSessionId === sessionId
        ) {
          sessions.delete(deferredSessionId);
        }
      }
      if (sessions.size === 0) {
        this.#deferredRendererPageDetaches.delete(deferredTargetId);
      }
    }

    const context =
      this.#browsingContextStorage.findContextBySession(sessionId);
    const preloadTargetId =
      context?.id ??
      (targetId && !this.#browsingContextStorage.hasContext(targetId)
        ? targetId
        : undefined);
    if (preloadTargetId) {
      this.#preloadScriptStorage
        .find({targetId: preloadTargetId})
        .map((preloadScript) => {
          preloadScript.dispose(preloadTargetId);
        });
    }
    if (context) {
      this.#rendererTargetKeys.delete(context.id);
      this.#browserOnlyTargetKeys.delete(context.id);
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
    // Delete (rather than just dispose) the realms so a subsequent
    // `Target.detachedFromTarget` event does not dispose them a second time.
    this.#realmStorage.deleteRealms({
      cdpSessionId: cdpClient.sessionId,
    });
  }
}
