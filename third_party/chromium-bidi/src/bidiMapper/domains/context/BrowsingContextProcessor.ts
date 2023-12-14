/**
 * Copyright 2021 Google LLC.
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
import type {Protocol} from 'devtools-protocol';

import type {ICdpClient} from '../../../cdp/CdpClient.js';
import type {ICdpConnection} from '../../../cdp/CdpConnection.js';
import {
  BrowsingContext,
  InvalidArgumentException,
  type EmptyResult,
} from '../../../protocol/protocol.js';
import {CdpErrorConstants} from '../../../utils/CdpErrorConstants.js';
import {LogType, type LoggerFn} from '../../../utils/log.js';
import type {EventManager} from '../events/EventManager.js';
import type {NetworkStorage} from '../network/NetworkStorage.js';
import type {PreloadScriptStorage} from '../script/PreloadScriptStorage.js';
import {Realm} from '../script/Realm.js';
import type {RealmStorage} from '../script/RealmStorage.js';

import {BrowsingContextImpl, serializeOrigin} from './BrowsingContextImpl.js';
import type {BrowsingContextStorage} from './BrowsingContextStorage.js';
import {CdpTarget} from './CdpTarget.js';

export class BrowsingContextProcessor {
  readonly #browserCdpClient: ICdpClient;
  readonly #cdpConnection: ICdpConnection;
  readonly #selfTargetId: string;
  readonly #eventManager: EventManager;

  readonly #browsingContextStorage: BrowsingContextStorage;
  readonly #networkStorage: NetworkStorage;
  readonly #acceptInsecureCerts: boolean;
  readonly #preloadScriptStorage: PreloadScriptStorage;
  readonly #realmStorage: RealmStorage;

  readonly #logger?: LoggerFn;

  constructor(
    cdpConnection: ICdpConnection,
    browserCdpClient: ICdpClient,
    selfTargetId: string,
    eventManager: EventManager,
    browsingContextStorage: BrowsingContextStorage,
    realmStorage: RealmStorage,
    networkStorage: NetworkStorage,
    preloadScriptStorage: PreloadScriptStorage,
    acceptInsecureCerts: boolean,
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
    this.#logger = logger;

    this.#setEventListeners(browserCdpClient);
  }

  getTree(
    params: BrowsingContext.GetTreeParameters
  ): BrowsingContext.GetTreeResult {
    const resultContexts =
      params.root === undefined
        ? this.#browsingContextStorage.getTopLevelContexts()
        : [this.#browsingContextStorage.getContext(params.root)];

    return {
      contexts: resultContexts.map((c) =>
        c.serializeToBidiValue(params.maxDepth ?? Number.MAX_VALUE)
      ),
    };
  }

  async create(
    params: BrowsingContext.CreateParameters
  ): Promise<BrowsingContext.CreateResult> {
    let referenceContext: BrowsingContextImpl | undefined;
    if (params.referenceContext !== undefined) {
      referenceContext = this.#browsingContextStorage.getContext(
        params.referenceContext
      );
      if (!referenceContext.isTopLevelContext()) {
        throw new InvalidArgumentException(
          `referenceContext should be a top-level context`
        );
      }
    }

    let result: Protocol.Target.CreateTargetResponse;

    switch (params.type) {
      case BrowsingContext.CreateType.Tab:
        result = await this.#browserCdpClient.sendCommand(
          'Target.createTarget',
          {
            url: 'about:blank',
            newWindow: false,
          }
        );
        break;
      case BrowsingContext.CreateType.Window:
        result = await this.#browserCdpClient.sendCommand(
          'Target.createTarget',
          {
            url: 'about:blank',
            newWindow: true,
          }
        );
        break;
    }

    // Wait for the new tab to be loaded to avoid race conditions in the
    // `browsingContext` events, when the `browsingContext.domContentLoaded` and
    // `browsingContext.load` events from the initial `about:blank` navigation
    // are emitted after the next navigation is started.
    // Details: https://github.com/web-platform-tests/wpt/issues/35846
    const contextId = result.targetId;
    const context = this.#browsingContextStorage.getContext(contextId);
    await context.lifecycleLoaded();

    return {context: context.id};
  }

  navigate(
    params: BrowsingContext.NavigateParameters
  ): Promise<BrowsingContext.NavigateResult> {
    const context = this.#browsingContextStorage.getContext(params.context);

    return context.navigate(
      params.url,
      params.wait ?? BrowsingContext.ReadinessState.None
    );
  }

  reload(params: BrowsingContext.ReloadParameters): Promise<EmptyResult> {
    const context = this.#browsingContextStorage.getContext(params.context);

    return context.reload(
      params.ignoreCache ?? false,
      params.wait ?? BrowsingContext.ReadinessState.None
    );
  }

  async activate(
    params: BrowsingContext.ActivateParameters
  ): Promise<EmptyResult> {
    const context = this.#browsingContextStorage.getContext(params.context);
    if (!context.isTopLevelContext()) {
      throw new InvalidArgumentException(
        'Activation is only supported on the top-level context'
      );
    }
    await context.activate();
    return {};
  }

  async captureScreenshot(
    params: BrowsingContext.CaptureScreenshotParameters
  ): Promise<BrowsingContext.CaptureScreenshotResult> {
    const context = this.#browsingContextStorage.getContext(params.context);
    return await context.captureScreenshot(params);
  }

  async print(
    params: BrowsingContext.PrintParameters
  ): Promise<BrowsingContext.PrintResult> {
    const context = this.#browsingContextStorage.getContext(params.context);
    return await context.print(params);
  }

  async setViewport(
    params: BrowsingContext.SetViewportParameters
  ): Promise<EmptyResult> {
    const context = this.#browsingContextStorage.getContext(params.context);
    if (!context.isTopLevelContext()) {
      throw new InvalidArgumentException(
        'Emulating viewport is only supported on the top-level context'
      );
    }
    await context.setViewport(params.viewport, params.devicePixelRatio);
    return {};
  }

  async traverseHistory(
    params: BrowsingContext.TraverseHistoryParameters
  ): Promise<BrowsingContext.TraverseHistoryResult> {
    const context = this.#browsingContextStorage.getContext(params.context);
    if (!context) {
      throw new InvalidArgumentException(
        `No browsing context with id ${params.context}`
      );
    }
    await context.traverseHistory(params.delta);
    return {};
  }

  async handleUserPrompt(
    params: BrowsingContext.HandleUserPromptParameters
  ): Promise<EmptyResult> {
    const context = this.#browsingContextStorage.getContext(params.context);
    await context.handleUserPrompt(params);
    return {};
  }

  async close(params: BrowsingContext.CloseParameters): Promise<EmptyResult> {
    const context = this.#browsingContextStorage.getContext(params.context);

    if (!context.isTopLevelContext()) {
      throw new InvalidArgumentException(
        `Non top-level browsing context ${context.id} cannot be closed.`
      );
    }

    try {
      const detachedFromTargetPromise = new Promise<void>((resolve) => {
        const onContextDestroyed = (
          event: Protocol.Target.DetachedFromTargetEvent
        ) => {
          if (event.targetId === params.context) {
            this.#browserCdpClient.off(
              'Target.detachedFromTarget',
              onContextDestroyed
            );
            resolve();
          }
        };
        this.#browserCdpClient.on(
          'Target.detachedFromTarget',
          onContextDestroyed
        );
      });

      if (params.promptUnload) {
        await context.close();
      } else {
        await this.#browserCdpClient.sendCommand('Target.closeTarget', {
          targetId: params.context,
        });
      }

      // Sometimes CDP command finishes before `detachedFromTarget` event,
      // sometimes after. Wait for the CDP command to be finished, and then wait
      // for `detachedFromTarget` if it hasn't emitted.
      await detachedFromTargetPromise;
    } catch (error: any) {
      // Swallow error that arise from the page being destroyed
      // Example is navigating to faulty SSL certificate
      if (
        !(
          error.code === CdpErrorConstants.GENERIC_ERROR &&
          error.message === 'Not attached to an active page'
        )
      ) {
        throw error;
      }
    }

    return {};
  }

  /**
   * This method is called for each CDP session, since this class is responsible
   * for creating and destroying all targets and browsing contexts.
   */
  #setEventListeners(cdpClient: ICdpClient) {
    cdpClient.on('Target.attachedToTarget', (params) => {
      this.#handleAttachedToTargetEvent(params, cdpClient);
    });
    cdpClient.on('Target.detachedFromTarget', (params) => {
      this.#handleDetachedFromTargetEvent(params);
    });
    cdpClient.on('Target.targetInfoChanged', (params) => {
      this.#handleTargetInfoChangedEvent(params);
    });

    cdpClient.on(
      'Page.frameAttached',
      (params: Protocol.Page.FrameAttachedEvent) => {
        this.#handleFrameAttachedEvent(params);
      }
    );
    cdpClient.on(
      'Page.frameDetached',
      (params: Protocol.Page.FrameDetachedEvent) => {
        this.#handleFrameDetachedEvent(params);
      }
    );
  }

  #handleFrameAttachedEvent(params: Protocol.Page.FrameAttachedEvent) {
    const parentBrowsingContext = this.#browsingContextStorage.findContext(
      params.parentFrameId
    );
    if (parentBrowsingContext !== undefined) {
      BrowsingContextImpl.create(
        parentBrowsingContext.cdpTarget,
        this.#realmStorage,
        params.frameId,
        params.parentFrameId,
        this.#eventManager,
        this.#browsingContextStorage,
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
    parentSessionCdpClient: ICdpClient
  ) {
    const {sessionId, targetInfo} = params;
    const targetCdpClient = this.#cdpConnection.getCdpClient(sessionId);

    this.#logger?.(
      LogType.debugInfo,
      'AttachedToTarget event received:',
      params
    );

    switch (targetInfo.type) {
      case 'page':
      case 'iframe': {
        if (targetInfo.targetId === this.#selfTargetId) {
          break;
        }

        this.#setEventListeners(targetCdpClient);

        const cdpTarget = CdpTarget.create(
          targetInfo.targetId,
          targetCdpClient,
          this.#browserCdpClient,
          sessionId,
          this.#realmStorage,
          this.#eventManager,
          this.#preloadScriptStorage,
          this.#networkStorage,
          this.#acceptInsecureCerts
        );

        const maybeContext = this.#browsingContextStorage.findContext(
          targetInfo.targetId
        );
        if (maybeContext) {
          // OOPiF.
          maybeContext.updateCdpTarget(cdpTarget);
        } else {
          // New context.
          BrowsingContextImpl.create(
            cdpTarget,
            this.#realmStorage,
            targetInfo.targetId,
            null,
            this.#eventManager,
            this.#browsingContextStorage,
            this.#logger
          );
        }
        return;
      }
      case 'worker': {
        this.#setEventListeners(targetCdpClient);

        const cdpTarget = CdpTarget.create(
          targetInfo.targetId,
          targetCdpClient,
          this.#browserCdpClient,
          sessionId,
          this.#realmStorage,
          this.#eventManager,
          this.#preloadScriptStorage,
          this.#networkStorage,
          this.#acceptInsecureCerts
        );

        const browsingContext =
          parentSessionCdpClient.sessionId &&
          this.#browsingContextStorage.findContextBySession(
            parentSessionCdpClient.sessionId
          );
        // If there is no browsing context, this worker is already terminated.
        if (!browsingContext) {
          break;
        }

        this.#handleWorkerTarget(cdpTarget, browsingContext.id);
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

  #workers = new Map<string, Realm>();
  #handleWorkerTarget(cdpTarget: CdpTarget, browsingContextId: string) {
    cdpTarget.cdpClient.on('Runtime.executionContextCreated', (params) => {
      const {uniqueId, id, origin} = params.context;
      const realm = new Realm(
        this.#realmStorage,
        this.#browsingContextStorage,
        uniqueId,
        browsingContextId,
        id,
        serializeOrigin(origin),
        'dedicated-worker',
        undefined,
        cdpTarget.cdpClient,
        this.#eventManager,
        this.#logger
      );
      this.#workers.set(cdpTarget.cdpSessionId, realm);
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
}
