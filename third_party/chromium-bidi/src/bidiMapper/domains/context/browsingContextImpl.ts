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
import {
  BrowsingContext,
  CommonDataTypes,
  Message,
} from '../../../protocol/protocol.js';
import {LoggerFn, LogType} from '../../../utils/log.js';
import {Deferred} from '../../../utils/deferred.js';
import {IEventManager} from '../events/EventManager.js';
import {Realm} from '../script/realm.js';
import {RealmStorage} from '../script/realmStorage.js';

import {BrowsingContextStorage} from './browsingContextStorage.js';
import {CdpTarget} from './cdpTarget.js';

export class BrowsingContextImpl {
  /** The ID of this browsing context. */
  readonly #id: CommonDataTypes.BrowsingContext;

  /**
   * The ID of the parent browsing context.
   * If null, this is a top-level context.
   */
  readonly #parentId: CommonDataTypes.BrowsingContext | null;

  /** Direct children browsing contexts. */
  readonly #children = new Set<CommonDataTypes.BrowsingContext>();

  readonly #browsingContextStorage: BrowsingContextStorage;

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

  #url = 'about:blank';
  readonly #eventManager: IEventManager;
  readonly #realmStorage: RealmStorage;
  #loaderId: string | null = null;
  #cdpTarget: CdpTarget;
  #maybeDefaultRealm: Realm | undefined;
  readonly #logger?: LoggerFn;

  private constructor(
    cdpTarget: CdpTarget,
    realmStorage: RealmStorage,
    id: CommonDataTypes.BrowsingContext,
    parentId: CommonDataTypes.BrowsingContext | null,
    eventManager: IEventManager,
    browsingContextStorage: BrowsingContextStorage,
    logger?: LoggerFn
  ) {
    this.#cdpTarget = cdpTarget;
    this.#realmStorage = realmStorage;
    this.#id = id;
    this.#parentId = parentId;
    this.#eventManager = eventManager;
    this.#browsingContextStorage = browsingContextStorage;
    this.#logger = logger;
  }

  static create(
    cdpTarget: CdpTarget,
    realmStorage: RealmStorage,
    id: CommonDataTypes.BrowsingContext,
    parentId: CommonDataTypes.BrowsingContext | null,
    eventManager: IEventManager,
    browsingContextStorage: BrowsingContextStorage,
    logger?: LoggerFn
  ): BrowsingContextImpl {
    const context = new BrowsingContextImpl(
      cdpTarget,
      realmStorage,
      id,
      parentId,
      eventManager,
      browsingContextStorage,
      logger
    );

    context.#initListeners();

    browsingContextStorage.addContext(context);
    if (!context.isTopLevelContext()) {
      context.parent!.addChild(context.id);
    }

    eventManager.registerEvent(
      {
        method: BrowsingContext.EventNames.ContextCreatedEvent,
        params: context.serializeToBidiValue(),
      },
      context.id
    );

    return context;
  }

  /**
   * @see https://html.spec.whatwg.org/multipage/document-sequences.html#navigable
   */
  get navigableId(): string | null {
    return this.#loaderId;
  }

  delete() {
    this.#deleteAllChildren();

    this.#realmStorage.deleteRealms({
      browsingContextId: this.id,
    });

    // Remove context from the parent.
    if (!this.isTopLevelContext()) {
      this.parent!.#children.delete(this.id);
    }

    this.#eventManager.registerEvent(
      {
        method: BrowsingContext.EventNames.ContextDestroyedEvent,
        params: this.serializeToBidiValue(),
      },
      this.id
    );
    this.#browsingContextStorage.deleteContextById(this.id);
  }

  /** Returns the ID of this context. */
  get id(): CommonDataTypes.BrowsingContext {
    return this.#id;
  }

  /** Returns the parent context ID. */
  get parentId(): CommonDataTypes.BrowsingContext | null {
    return this.#parentId;
  }

  /** Returns the parent context. */
  get parent(): BrowsingContextImpl | null {
    if (this.parentId === null) {
      return null;
    }
    return this.#browsingContextStorage.getContext(this.parentId);
  }

  /** Returns all direct children contexts. */
  get directChildren(): BrowsingContextImpl[] {
    return [...this.#children].map((id) =>
      this.#browsingContextStorage.getContext(id)
    );
  }

  /** Returns all children contexts, flattened. */
  get allChildren(): BrowsingContextImpl[] {
    const children = this.directChildren;
    return children.concat(...children.map((child) => child.allChildren));
  }

  /**
   * Returns true if this is a top-level context.
   * This is the case whenever the parent context ID is null.
   */
  isTopLevelContext(): boolean {
    return this.#parentId === null;
  }

  addChild(childId: CommonDataTypes.BrowsingContext) {
    this.#children.add(childId);
  }

  #deleteAllChildren() {
    this.directChildren.map((child) => child.delete());
  }

  get #defaultRealm(): Realm {
    if (this.#maybeDefaultRealm === undefined) {
      throw new Error(`No default realm for browsing context ${this.#id}`);
    }
    return this.#maybeDefaultRealm;
  }

  get cdpTarget(): CdpTarget {
    return this.#cdpTarget;
  }

  updateCdpTarget(cdpTarget: CdpTarget) {
    this.#cdpTarget = cdpTarget;
    this.#initListeners();
  }

  get url(): string {
    return this.#url;
  }

  async awaitLoaded() {
    await this.#defers.Page.lifecycleEvent.load;
  }

  awaitUnblocked(): Promise<void> {
    return this.#cdpTarget.targetUnblocked;
  }

  async getOrCreateSandbox(sandbox: string | undefined): Promise<Realm> {
    if (sandbox === undefined || sandbox === '') {
      return this.#defaultRealm;
    }

    let maybeSandboxes = this.#realmStorage.findRealms({
      browsingContextId: this.id,
      sandbox,
    });

    if (maybeSandboxes.length === 0) {
      await this.#cdpTarget.cdpClient.sendCommand('Page.createIsolatedWorld', {
        frameId: this.id,
        worldName: sandbox,
      });
      // `Runtime.executionContextCreated` should be emitted by the time the
      // previous command is done.
      maybeSandboxes = this.#realmStorage.findRealms({
        browsingContextId: this.id,
        sandbox,
      });
    }
    if (maybeSandboxes.length !== 1) {
      throw Error(`Sandbox ${sandbox} wasn't created.`);
    }
    return maybeSandboxes[0]!;
  }

  serializeToBidiValue(
    maxDepth = 0,
    addParentFiled = true
  ): BrowsingContext.Info {
    return {
      context: this.#id,
      url: this.url,
      children:
        maxDepth > 0
          ? this.directChildren.map((c) =>
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
        if (this.id !== params.targetInfo.targetId) {
          return;
        }
        this.#url = params.targetInfo.url;
      }
    );

    this.#cdpTarget.cdpClient.on(
      'Page.frameNavigated',
      (params: Protocol.Page.FrameNavigatedEvent) => {
        if (this.id !== params.frame.id) {
          return;
        }
        this.#url = params.frame.url + (params.frame.urlFragment ?? '');

        // At the point the page is initialized, all the nested iframes from the
        // previous page are detached and realms are destroyed.
        // Remove children from context.
        this.#deleteAllChildren();
      }
    );

    this.#cdpTarget.cdpClient.on(
      'Page.navigatedWithinDocument',
      (params: Protocol.Page.NavigatedWithinDocumentEvent) => {
        if (this.id !== params.frameId) {
          return;
        }

        this.#url = params.url;
        this.#defers.Page.navigatedWithinDocument.resolve(params);
      }
    );

    this.#cdpTarget.cdpClient.on(
      'Page.lifecycleEvent',
      (params: Protocol.Page.LifecycleEventEvent) => {
        if (this.id !== params.frameId) {
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
                  context: this.id,
                  navigation: this.#loaderId,
                  timestamp,
                  url: this.#url,
                },
              },
              this.id
            );
            break;

          case 'load':
            this.#defers.Page.lifecycleEvent.load.resolve(params);
            this.#eventManager.registerEvent(
              {
                method: BrowsingContext.EventNames.LoadEvent,
                params: {
                  context: this.id,
                  navigation: this.#loaderId,
                  timestamp,
                  url: this.#url,
                },
              },
              this.id
            );
            break;
        }
      }
    );

    this.#cdpTarget.cdpClient.on(
      'Runtime.executionContextCreated',
      (params: Protocol.Runtime.ExecutionContextCreatedEvent) => {
        if (params.context.auxData.frameId !== this.id) {
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
          this.id,
          params.context.id,
          this.#getOrigin(params),
          // XXX: differentiate types.
          'window',
          // Sandbox name for isolated world.
          params.context.auxData.type === 'isolated'
            ? params.context.name
            : undefined,
          this.#cdpTarget.cdpSessionId,
          this.#cdpTarget.cdpClient,
          this.#eventManager,
          this.#logger
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

    this.#cdpTarget.cdpClient.on('Runtime.executionContextsCleared', () => {
      this.#realmStorage.deleteRealms({
        cdpSessionId: this.#cdpTarget.cdpSessionId,
      });
    });
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
    const cdpNavigateResult: Protocol.Page.NavigateResponse =
      await this.#cdpTarget.cdpClient.sendCommand('Page.navigate', {
        url,
        frameId: this.id,
      });

    if (cdpNavigateResult.errorText) {
      throw new Message.UnknownErrorException(cdpNavigateResult.errorText);
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
    }

    return {
      result: {
        navigation: cdpNavigateResult.loaderId || null,
        url,
      },
    };
  }

  async captureScreenshot(): Promise<BrowsingContext.CaptureScreenshotResult> {
    const [, result] = await Promise.all([
      // XXX: Either make this a proposal in the BiDi spec, or focus the
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
      preferCSSPageSize: !params.shrinkToFit,
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
