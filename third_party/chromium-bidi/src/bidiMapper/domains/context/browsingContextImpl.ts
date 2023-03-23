/**
 * Copyright 2022 Google LLC.
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

import {Protocol} from 'devtools-protocol';

import {inchesFromCm} from '../../../utils/unitConversions.js';
import {BrowsingContext, Message} from '../../../protocol/protocol.js';
import {LoggerFn, LogType} from '../../../utils/log.js';
import {Deferred} from '../../../utils/deferred.js';
import {IEventManager} from '../events/EventManager.js';
import {Realm} from '../script/realm.js';
import {RealmStorage} from '../script/realmStorage.js';

import {BrowsingContextStorage} from './browsingContextStorage.js';
import {CdpTarget} from './cdpTarget';

export class BrowsingContextImpl {
  readonly #defers = {
    documentInitialized: new Deferred<void>(),
    Page: {
      navigatedWithinDocument:
        new Deferred<Protocol.Page.NavigatedWithinDocumentEvent>(),
      lifecycleEvent: {
        DOMContentLoaded: new Deferred<Protocol.Page.LifecycleEventEvent>(),
        load: new Deferred<Protocol.Page.LifecycleEventEvent>(),
      },
    },
  };

  readonly #contextId: string;
  readonly #parentId: string | null;
  readonly #eventManager: IEventManager;
  readonly #children: Map<string, BrowsingContextImpl> = new Map();
  readonly #realmStorage: RealmStorage;

  #url = 'about:blank';
  #loaderId: string | null = null;
  #cdpTarget: CdpTarget;
  #maybeDefaultRealm: Realm | undefined;
  readonly #browsingContextStorage: BrowsingContextStorage;
  readonly #logger?: LoggerFn;

  get #defaultRealm(): Realm {
    if (this.#maybeDefaultRealm === undefined) {
      throw new Error(
        `No default realm for browsing context ${this.#contextId}`
      );
    }
    return this.#maybeDefaultRealm;
  }

  private constructor(
    cdpTarget: CdpTarget,
    realmStorage: RealmStorage,
    contextId: string,
    parentId: string | null,
    eventManager: IEventManager,
    browsingContextStorage: BrowsingContextStorage,
    logger?: LoggerFn
  ) {
    this.#cdpTarget = cdpTarget;
    this.#realmStorage = realmStorage;
    this.#contextId = contextId;
    this.#parentId = parentId;
    this.#eventManager = eventManager;
    this.#browsingContextStorage = browsingContextStorage;
    this.#logger = logger;

    this.#initListeners();
  }

  static async create(
    cdpTarget: CdpTarget,
    realmStorage: RealmStorage,
    contextId: string,
    parentId: string | null,
    eventManager: IEventManager,
    browsingContextStorage: BrowsingContextStorage,
    logger?: LoggerFn
  ): Promise<void> {
    const context = new BrowsingContextImpl(
      cdpTarget,
      realmStorage,
      contextId,
      parentId,
      eventManager,
      browsingContextStorage,
      logger
    );

    browsingContextStorage.addContext(context);

    eventManager.registerEvent(
      {
        method: BrowsingContext.EventNames.ContextCreatedEvent,
        params: context.serializeToBidiValue(),
      },
      context.contextId
    );
  }

  // https://html.spec.whatwg.org/multipage/document-sequences.html#navigable
  get navigableId(): string | null {
    return this.#loaderId;
  }

  updateCdpTarget(cdpTarget: CdpTarget) {
    this.#cdpTarget = cdpTarget;
    this.#initListeners();
  }

  async delete() {
    await this.#removeChildContexts();

    this.#realmStorage.deleteRealms({
      browsingContextId: this.contextId,
    });

    // Remove context from the parent.
    if (this.parentId !== null) {
      const parent = this.#browsingContextStorage.getKnownContext(
        this.parentId
      );
      parent.#children.delete(this.contextId);
    }

    this.#eventManager.registerEvent(
      {
        method: BrowsingContext.EventNames.ContextDestroyedEvent,
        params: this.serializeToBidiValue(),
      },
      this.contextId
    );
    this.#browsingContextStorage.removeContext(this.contextId);
  }

  async #removeChildContexts() {
    await Promise.all(this.children.map((child) => child.delete()));
  }

  get contextId(): string {
    return this.#contextId;
  }

  get parentId(): string | null {
    return this.#parentId;
  }

  get cdpTarget(): CdpTarget {
    return this.#cdpTarget;
  }

  get children(): BrowsingContextImpl[] {
    return Array.from(this.#children.values());
  }

  get url(): string {
    return this.#url;
  }

  addChild(child: BrowsingContextImpl) {
    this.#children.set(child.contextId, child);
  }

  async awaitLoaded(): Promise<void> {
    await this.#defers.Page.lifecycleEvent.load;
  }

  async awaitUnblocked(): Promise<void> {
    return this.#cdpTarget.targetUnblocked;
  }

  serializeToBidiValue(
    maxDepth = 0,
    addParentFiled = true
  ): BrowsingContext.Info {
    return {
      context: this.#contextId,
      url: this.url,
      children:
        maxDepth > 0
          ? this.children.map((c) =>
              c.serializeToBidiValue(maxDepth - 1, false)
            )
          : null,
      ...(addParentFiled ? {parent: this.#parentId} : {}),
    };
  }

  #initListeners() {
    this.#cdpTarget.cdpClient.on(
      'Target.targetInfoChanged',
      (params: Protocol.Target.TargetInfoChangedEvent) => {
        if (this.contextId !== params.targetInfo.targetId) {
          return;
        }
        this.#url = params.targetInfo.url;
      }
    );

    this.#cdpTarget.cdpClient.on(
      'Page.frameNavigated',
      async (params: Protocol.Page.FrameNavigatedEvent) => {
        if (this.contextId !== params.frame.id) {
          return;
        }
        this.#url = params.frame.url + (params.frame.urlFragment ?? '');

        // At the point the page is initiated, all the nested iframes from the
        // previous page are detached and realms are destroyed.
        // Remove context's children.
        await this.#removeChildContexts();

        // Remove all the already created realms.
        this.#realmStorage.deleteRealms({browsingContextId: this.contextId});
      }
    );

    this.#cdpTarget.cdpClient.on(
      'Page.navigatedWithinDocument',
      (params: Protocol.Page.NavigatedWithinDocumentEvent) => {
        if (this.contextId !== params.frameId) {
          return;
        }

        this.#url = params.url;
        this.#defers.Page.navigatedWithinDocument.resolve(params);
      }
    );

    this.#cdpTarget.cdpClient.on(
      'Page.lifecycleEvent',
      async (params: Protocol.Page.LifecycleEventEvent) => {
        if (this.contextId !== params.frameId) {
          return;
        }

        // `timestamp` from the event is MonotonicTime, not real time, so
        // the best Mapper can do is to set the timestamp to the epoch time
        // of the event arrived.
        // https://chromedevtools.github.io/devtools-protocol/tot/Network/#type-MonotonicTime
        const timestamp = new Date().getTime();

        if (params.name === 'init') {
          this.#documentChanged(params.loaderId);
          this.#defers.documentInitialized.resolve();
        }

        if (params.name === 'commit') {
          this.#loaderId = params.loaderId;
          return;
        }

        if (params.loaderId !== this.#loaderId) {
          return;
        }

        switch (params.name) {
          case 'DOMContentLoaded':
            this.#defers.Page.lifecycleEvent.DOMContentLoaded.resolve(params);
            this.#eventManager.registerEvent(
              {
                method: BrowsingContext.EventNames.DomContentLoadedEvent,
                params: {
                  context: this.contextId,
                  navigation: this.#loaderId,
                  timestamp,
                  url: this.#url,
                },
              },
              this.contextId
            );
            break;

          case 'load':
            this.#defers.Page.lifecycleEvent.load.resolve(params);
            this.#eventManager.registerEvent(
              {
                method: BrowsingContext.EventNames.LoadEvent,
                params: {
                  context: this.contextId,
                  navigation: this.#loaderId,
                  timestamp,
                  url: this.#url,
                },
              },
              this.contextId
            );
            break;
        }
      }
    );

    this.#cdpTarget.cdpClient.on(
      'Runtime.executionContextCreated',
      (params: Protocol.Runtime.ExecutionContextCreatedEvent) => {
        if (params.context.auxData.frameId !== this.contextId) {
          return;
        }
        // Only this execution contexts are supported for now.
        if (!['default', 'isolated'].includes(params.context.auxData.type)) {
          return;
        }
        const realm = new Realm(
          this.#realmStorage,
          this.#browsingContextStorage,
          params.context.uniqueId,
          this.contextId,
          params.context.id,
          this.#getOrigin(params),
          // TODO: differentiate types.
          'window',
          // Sandbox name for isolated world.
          params.context.auxData.type === 'isolated'
            ? params.context.name
            : undefined,
          this.#cdpTarget.cdpSessionId,
          this.#cdpTarget.cdpClient,
          this.#eventManager
        );

        if (params.context.auxData.isDefault) {
          this.#maybeDefaultRealm = realm;
        }
      }
    );

    this.#cdpTarget.cdpClient.on(
      'Runtime.executionContextDestroyed',
      (params: Protocol.Runtime.ExecutionContextDestroyedEvent) => {
        this.#realmStorage.deleteRealms({
          cdpSessionId: this.#cdpTarget.cdpSessionId,
          executionContextId: params.executionContextId,
        });
      }
    );
  }

  #getOrigin(params: Protocol.Runtime.ExecutionContextCreatedEvent) {
    if (params.context.auxData.type === 'isolated') {
      // Sandbox should have the same origin as the context itself, but in CDP
      // it has an empty one.
      return this.#defaultRealm.origin;
    }
    // https://html.spec.whatwg.org/multipage/origin.html#ascii-serialisation-of-an-origin
    return ['://', ''].includes(params.context.origin)
      ? 'null'
      : params.context.origin;
  }

  #documentChanged(loaderId?: string) {
    // Same document navigation.
    if (loaderId === undefined || this.#loaderId === loaderId) {
      if (this.#defers.Page.navigatedWithinDocument.isFinished) {
        this.#defers.Page.navigatedWithinDocument =
          new Deferred<Protocol.Page.NavigatedWithinDocumentEvent>();
      }
      return;
    }

    if (this.#defers.documentInitialized.isFinished) {
      this.#defers.documentInitialized = new Deferred<void>();
    } else {
      this.#logger?.(LogType.browsingContexts, 'Document changed');
    }

    if (this.#defers.Page.lifecycleEvent.DOMContentLoaded.isFinished) {
      this.#defers.Page.lifecycleEvent.DOMContentLoaded =
        new Deferred<Protocol.Page.LifecycleEventEvent>();
    } else {
      this.#logger?.(LogType.browsingContexts, 'Document changed');
    }

    if (this.#defers.Page.lifecycleEvent.load.isFinished) {
      this.#defers.Page.lifecycleEvent.load =
        new Deferred<Protocol.Page.LifecycleEventEvent>();
    } else {
      this.#logger?.(LogType.browsingContexts, 'Document changed');
    }

    this.#loaderId = loaderId;
  }

  async navigate(
    url: string,
    wait: BrowsingContext.ReadinessState
  ): Promise<BrowsingContext.NavigateResult> {
    await this.awaitUnblocked();

    // TODO: handle loading errors.
    const cdpNavigateResult = await this.#cdpTarget.cdpClient.sendCommand(
      'Page.navigate',
      {
        url,
        frameId: this.contextId,
      }
    );

    if (cdpNavigateResult.errorText) {
      throw new Message.UnknownException(cdpNavigateResult.errorText);
    }

    this.#documentChanged(cdpNavigateResult.loaderId);

    // Wait for `wait` condition.
    switch (wait) {
      case 'none':
        break;

      case 'interactive':
        // No `loaderId` means same-document navigation.
        if (cdpNavigateResult.loaderId === undefined) {
          await this.#defers.Page.navigatedWithinDocument;
        } else {
          await this.#defers.Page.lifecycleEvent.DOMContentLoaded;
        }
        break;

      case 'complete':
        // No `loaderId` means same-document navigation.
        if (cdpNavigateResult.loaderId === undefined) {
          await this.#defers.Page.navigatedWithinDocument;
        } else {
          await this.#defers.Page.lifecycleEvent.load;
        }
        break;

      default:
        throw new Error(`Not implemented wait '${wait}'`);
    }

    return {
      result: {
        navigation: cdpNavigateResult.loaderId || null,
        url,
      },
    };
  }

  async getOrCreateSandbox(sandbox: string | undefined): Promise<Realm> {
    if (sandbox === undefined || sandbox === '') {
      return this.#defaultRealm;
    }

    let maybeSandboxes = this.#realmStorage.findRealms({
      browsingContextId: this.contextId,
      sandbox,
    });

    if (maybeSandboxes.length === 0) {
      await this.#cdpTarget.cdpClient.sendCommand('Page.createIsolatedWorld', {
        frameId: this.contextId,
        worldName: sandbox,
      });
      // `Runtime.executionContextCreated` should be emitted by the time the
      // previous command is done.
      maybeSandboxes = this.#realmStorage.findRealms({
        browsingContextId: this.contextId,
        sandbox,
      });
    }
    if (maybeSandboxes.length !== 1) {
      throw Error(`Sandbox ${sandbox} wasn't created.`);
    }
    return maybeSandboxes[0]!;
  }

  async captureScreenshot(): Promise<BrowsingContext.CaptureScreenshotResult> {
    const [, result] = await Promise.all([
      // TODO: Either make this a proposal in the BiDi spec, or focus the
      // original tab right after the screenshot is taken.
      // The screenshot command gets blocked until we focus the active tab.
      this.#cdpTarget.cdpClient.sendCommand('Page.bringToFront'),
      this.#cdpTarget.cdpClient.sendCommand('Page.captureScreenshot', {}),
    ]);
    return {
      result: {
        data: result.data,
      },
    };
  }

  async print(
    params: BrowsingContext.PrintParameters
  ): Promise<BrowsingContext.PrintResult> {
    const printToPdfCdpParams: Protocol.Page.PrintToPDFRequest = {
      printBackground: params.background,
      landscape: params.orientation === 'landscape',
      pageRanges: params.pageRanges?.join(',') ?? '',
      scale: params.scale,
      // TODO(#518): Use `shrinkToFit`.
    };

    if (params.margin?.bottom) {
      printToPdfCdpParams.marginBottom = inchesFromCm(params.margin.bottom);
    }
    if (params.margin?.left) {
      printToPdfCdpParams.marginLeft = inchesFromCm(params.margin.left);
    }
    if (params.margin?.right) {
      printToPdfCdpParams.marginRight = inchesFromCm(params.margin.right);
    }
    if (params.margin?.top) {
      printToPdfCdpParams.marginTop = inchesFromCm(params.margin.top);
    }
    if (params.page?.height) {
      printToPdfCdpParams.paperHeight = inchesFromCm(params.page.height);
    }
    if (params.page?.width) {
      printToPdfCdpParams.paperWidth = inchesFromCm(params.page.width);
    }

    const result = await this.#cdpTarget.cdpClient.sendCommand(
      'Page.printToPDF',
      printToPdfCdpParams
    );

    return {
      result: {
        data: result.data,
      },
    };
  }
}
