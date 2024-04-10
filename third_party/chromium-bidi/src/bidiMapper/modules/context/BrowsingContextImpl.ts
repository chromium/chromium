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

import type {Protocol} from 'devtools-protocol';

import {
  BrowsingContext,
  ChromiumBidi,
  InvalidArgumentException,
  InvalidSelectorException,
  NoSuchElementException,
  NoSuchHistoryEntryException,
  Script,
  UnableToCaptureScreenException,
  UnknownErrorException,
  UnsupportedOperationException,
} from '../../../protocol/protocol.js';
import {assert} from '../../../utils/assert.js';
import {Deferred} from '../../../utils/Deferred.js';
import {type LoggerFn, LogType} from '../../../utils/log.js';
import {inchesFromCm} from '../../../utils/unitConversions.js';
import type {CdpTarget} from '../cdp/CdpTarget.js';
import type {Realm} from '../script/Realm.js';
import type {RealmStorage} from '../script/RealmStorage.js';
import {WindowRealm} from '../script/WindowRealm.js';
import type {EventManager} from '../session/EventManager.js';

import type {BrowsingContextStorage} from './BrowsingContextStorage.js';

export class BrowsingContextImpl {
  static readonly LOGGER_PREFIX = `${LogType.debug}:browsingContext` as const;

  /** The ID of this browsing context. */
  readonly #id: BrowsingContext.BrowsingContext;
  readonly userContext: string;

  /**
   * The ID of the parent browsing context.
   * If null, this is a top-level context.
   */
  readonly #parentId: BrowsingContext.BrowsingContext | null;

  /** Direct children browsing contexts. */
  readonly #children = new Set<BrowsingContext.BrowsingContext>();

  readonly #browsingContextStorage: BrowsingContextStorage;

  #lifecycle = {
    DOMContentLoaded: new Deferred<void>(),
    load: new Deferred<void>(),
  };

  #navigation = {
    withinDocument: new Deferred<void>(),
  };

  #url = 'about:blank';
  readonly #eventManager: EventManager;
  readonly #realmStorage: RealmStorage;
  #loaderId?: Protocol.Network.LoaderId;
  #cdpTarget: CdpTarget;
  #maybeDefaultRealm?: Realm;
  readonly #logger?: LoggerFn;
  // Keeps track of the previously set viewport.
  #previousViewport: {width: number; height: number} = {width: 0, height: 0};

  private constructor(
    id: BrowsingContext.BrowsingContext,
    parentId: BrowsingContext.BrowsingContext | null,
    userContext: string,
    cdpTarget: CdpTarget,
    eventManager: EventManager,
    browsingContextStorage: BrowsingContextStorage,
    realmStorage: RealmStorage,
    logger?: LoggerFn
  ) {
    this.#cdpTarget = cdpTarget;
    this.#id = id;
    this.#parentId = parentId;
    this.userContext = userContext;
    this.#eventManager = eventManager;
    this.#browsingContextStorage = browsingContextStorage;
    this.#realmStorage = realmStorage;
    this.#logger = logger;
  }

  static create(
    id: BrowsingContext.BrowsingContext,
    parentId: BrowsingContext.BrowsingContext | null,
    userContext: string,
    cdpTarget: CdpTarget,
    eventManager: EventManager,
    browsingContextStorage: BrowsingContextStorage,
    realmStorage: RealmStorage,
    logger?: LoggerFn
  ): BrowsingContextImpl {
    const context = new BrowsingContextImpl(
      id,
      parentId,
      userContext,
      cdpTarget,
      eventManager,
      browsingContextStorage,
      realmStorage,
      logger
    );

    context.#initListeners();

    browsingContextStorage.addContext(context);
    if (!context.isTopLevelContext()) {
      context.parent!.addChild(context.id);
    }

    eventManager.registerEvent(
      {
        type: 'event',
        method: ChromiumBidi.BrowsingContext.EventNames.ContextCreated,
        params: context.serializeToBidiValue(),
      },
      context.id
    );

    return context;
  }

  static getTimestamp(): number {
    // `timestamp` from the event is MonotonicTime, not real time, so
    // the best Mapper can do is to set the timestamp to the epoch time
    // of the event arrived.
    // https://chromedevtools.github.io/devtools-protocol/tot/Network/#type-MonotonicTime
    return new Date().getTime();
  }

  /**
   * @see https://html.spec.whatwg.org/multipage/document-sequences.html#navigable
   */
  get navigableId(): string | undefined {
    return this.#loaderId;
  }

  dispose() {
    this.#deleteAllChildren();

    this.#realmStorage.deleteRealms({
      browsingContextId: this.id,
    });

    // Remove context from the parent.
    if (!this.isTopLevelContext()) {
      this.parent!.#children.delete(this.id);
    }

    // Fail all ongoing navigations.
    this.#failLifecycleIfNotFinished();

    this.#eventManager.registerEvent(
      {
        type: 'event',
        method: ChromiumBidi.BrowsingContext.EventNames.ContextDestroyed,
        params: this.serializeToBidiValue(),
      },
      this.id
    );
    this.#browsingContextStorage.deleteContextById(this.id);
  }

  /** Returns the ID of this context. */
  get id(): BrowsingContext.BrowsingContext {
    return this.#id;
  }

  /** Returns the parent context ID. */
  get parentId(): BrowsingContext.BrowsingContext | null {
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

  get top(): BrowsingContextImpl {
    // eslint-disable-next-line @typescript-eslint/no-this-alias
    let topContext: BrowsingContextImpl = this;
    let parent = topContext.parent;
    while (parent) {
      topContext = parent;
      parent = topContext.parent;
    }
    return topContext;
  }

  addChild(childId: BrowsingContext.BrowsingContext) {
    this.#children.add(childId);
  }

  #deleteAllChildren() {
    this.directChildren.map((child) => child.dispose());
  }

  get #defaultRealm(): Realm {
    assert(
      this.#maybeDefaultRealm,
      `No default realm for browsing context ${this.#id}`
    );
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

  async lifecycleLoaded() {
    await this.#lifecycle.load;
  }

  async targetUnblockedOrThrow(): Promise<void> {
    const result = await this.#cdpTarget.unblocked;
    if (result.kind === 'error') {
      throw result.error;
    }
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
      assert(maybeSandboxes.length !== 0);
    }
    // It's possible for more than one sandbox to be created due to provisional
    // frames. In this case, it's always the first one (i.e. the oldest one)
    // that is more relevant since the user may have set that one up already
    // through evaluation.
    return maybeSandboxes[0]!;
  }

  serializeToBidiValue(
    maxDepth = 0,
    addParentField = true
  ): BrowsingContext.Info {
    return {
      context: this.#id,
      url: this.url,
      userContext: this.userContext,
      children:
        maxDepth > 0
          ? this.directChildren.map((c) =>
              c.serializeToBidiValue(maxDepth - 1, false)
            )
          : null,
      ...(addParentField ? {parent: this.#parentId} : {}),
    };
  }

  onTargetInfoChanged(params: Protocol.Target.TargetInfoChangedEvent) {
    this.#url = params.targetInfo.url;
  }

  #initListeners() {
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
        const timestamp = BrowsingContextImpl.getTimestamp();
        this.#url = params.url;
        this.#navigation.withinDocument.resolve();

        this.#eventManager.registerEvent(
          {
            type: 'event',
            method: ChromiumBidi.BrowsingContext.EventNames.FragmentNavigated,
            params: {
              context: this.id,
              navigation: null,
              timestamp,
              url: this.#url,
            },
          },
          this.id
        );
      }
    );

    this.#cdpTarget.cdpClient.on(
      'Page.frameStartedLoading',
      (params: Protocol.Page.FrameStartedLoadingEvent) => {
        if (this.id !== params.frameId) {
          return;
        }
        this.#eventManager.registerEvent(
          {
            type: 'event',
            method: ChromiumBidi.BrowsingContext.EventNames.NavigationStarted,
            params: {
              context: this.id,
              navigation: null,
              timestamp: BrowsingContextImpl.getTimestamp(),
              url: '',
            },
          },
          this.id
        );
      }
    );

    this.#cdpTarget.cdpClient.on(
      'Page.lifecycleEvent',
      (params: Protocol.Page.LifecycleEventEvent) => {
        if (this.id !== params.frameId) {
          return;
        }

        if (params.name === 'init') {
          this.#documentChanged(params.loaderId);
          return;
        }

        if (params.name === 'commit') {
          this.#loaderId = params.loaderId;
          return;
        }

        // If mapper attached to the page late, it might miss init and
        // commit events. In that case, save the first loaderId for this
        // frameId.
        if (!this.#loaderId) {
          this.#loaderId = params.loaderId;
        }

        // Ignore event from not current navigation.
        if (params.loaderId !== this.#loaderId) {
          return;
        }

        const timestamp = BrowsingContextImpl.getTimestamp();

        switch (params.name) {
          case 'DOMContentLoaded':
            this.#eventManager.registerEvent(
              {
                type: 'event',
                method:
                  ChromiumBidi.BrowsingContext.EventNames.DomContentLoaded,
                params: {
                  context: this.id,
                  navigation: this.#loaderId ?? null,
                  timestamp,
                  url: this.#url,
                },
              },
              this.id
            );
            this.#lifecycle.DOMContentLoaded.resolve();
            break;

          case 'load':
            this.#eventManager.registerEvent(
              {
                type: 'event',
                method: ChromiumBidi.BrowsingContext.EventNames.Load,
                params: {
                  context: this.id,
                  navigation: this.#loaderId ?? null,
                  timestamp,
                  url: this.#url,
                },
              },
              this.id
            );
            this.#lifecycle.load.resolve();
            break;
        }
      }
    );

    this.#cdpTarget.cdpClient.on(
      'Runtime.executionContextCreated',
      (params: Protocol.Runtime.ExecutionContextCreatedEvent) => {
        const {auxData, name, uniqueId, id} = params.context;
        if (!auxData || auxData.frameId !== this.id) {
          return;
        }

        let origin: string;
        let sandbox: string | undefined;
        // Only these execution contexts are supported for now.
        switch (auxData.type) {
          case 'isolated':
            sandbox = name;
            // Sandbox should have the same origin as the context itself, but in CDP
            // it has an empty one.
            origin = this.#defaultRealm.origin;
            break;
          case 'default':
            origin = serializeOrigin(params.context.origin);
            break;
          default:
            return;
        }
        const realm = new WindowRealm(
          this.id,
          this.#browsingContextStorage,
          this.#cdpTarget.cdpClient,
          this.#eventManager,
          id,
          this.#logger,
          origin,
          uniqueId,
          this.#realmStorage,
          sandbox
        );

        if (auxData.isDefault) {
          this.#maybeDefaultRealm = realm;

          // Initialize ChannelProxy listeners for all the channels of all the
          // preload scripts related to this BrowsingContext.
          // TODO: extend for not default realms by the sandbox name.
          void Promise.all(
            this.#cdpTarget
              .getChannels()
              .map((channel) =>
                channel.startListenerFromWindow(realm, this.#eventManager)
              )
          );
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

    this.#cdpTarget.cdpClient.on('Page.javascriptDialogClosed', (params) => {
      const accepted = params.result;

      this.#eventManager.registerEvent(
        {
          type: 'event',
          method: ChromiumBidi.BrowsingContext.EventNames.UserPromptClosed,
          params: {
            context: this.id,
            accepted,
            userText:
              accepted && params.userInput ? params.userInput : undefined,
          },
        },
        this.id
      );
    });

    this.#cdpTarget.cdpClient.on('Page.javascriptDialogOpening', (params) => {
      this.#eventManager.registerEvent(
        {
          type: 'event',
          method: ChromiumBidi.BrowsingContext.EventNames.UserPromptOpened,
          params: {
            context: this.id,
            type: params.type,
            message: params.message,
            // Don't set the value if empty string
            defaultValue: params.defaultPrompt || undefined,
          },
        },
        this.id
      );
    });
  }

  #documentChanged(loaderId?: Protocol.Network.LoaderId) {
    // Same document navigation.
    if (loaderId === undefined || this.#loaderId === loaderId) {
      if (this.#navigation.withinDocument.isFinished) {
        this.#navigation.withinDocument = new Deferred();
      } else {
        this.#logger?.(
          BrowsingContextImpl.LOGGER_PREFIX,
          'Document changed (navigatedWithinDocument)'
        );
      }
      return;
    }

    this.#resetLifecycleIfFinished();

    this.#loaderId = loaderId;
  }

  #resetLifecycleIfFinished() {
    if (this.#lifecycle.DOMContentLoaded.isFinished) {
      this.#lifecycle.DOMContentLoaded = new Deferred();
    } else {
      this.#logger?.(
        BrowsingContextImpl.LOGGER_PREFIX,
        'Document changed (DOMContentLoaded)'
      );
    }

    if (this.#lifecycle.load.isFinished) {
      this.#lifecycle.load = new Deferred();
    } else {
      this.#logger?.(
        BrowsingContextImpl.LOGGER_PREFIX,
        'Document changed (load)'
      );
    }
  }

  #failLifecycleIfNotFinished() {
    if (!this.#lifecycle.DOMContentLoaded.isFinished) {
      this.#lifecycle.DOMContentLoaded.reject(
        new UnknownErrorException('navigation canceled')
      );
    }

    if (!this.#lifecycle.load.isFinished) {
      this.#lifecycle.load.reject(
        new UnknownErrorException('navigation canceled')
      );
    }
  }

  async navigate(
    url: string,
    wait: BrowsingContext.ReadinessState
  ): Promise<BrowsingContext.NavigateResult> {
    try {
      new URL(url);
    } catch {
      throw new InvalidArgumentException(`Invalid URL: ${url}`);
    }

    await this.targetUnblockedOrThrow();

    // TODO: handle loading errors.
    const cdpNavigateResult = await this.#cdpTarget.cdpClient.sendCommand(
      'Page.navigate',
      {
        url,
        frameId: this.id,
      }
    );

    if (cdpNavigateResult.errorText) {
      this.#eventManager.registerEvent(
        {
          type: 'event',
          method: ChromiumBidi.BrowsingContext.EventNames.NavigationFailed,
          params: {
            context: this.id,
            navigation: cdpNavigateResult.loaderId ?? null,
            timestamp: BrowsingContextImpl.getTimestamp(),
            url,
          },
        },
        this.id
      );

      throw new UnknownErrorException(cdpNavigateResult.errorText);
    }

    this.#documentChanged(cdpNavigateResult.loaderId);

    switch (wait) {
      case BrowsingContext.ReadinessState.None:
        break;
      case BrowsingContext.ReadinessState.Interactive:
        // No `loaderId` means same-document navigation.
        if (cdpNavigateResult.loaderId === undefined) {
          await this.#navigation.withinDocument;
        } else {
          await this.#lifecycle.DOMContentLoaded;
        }
        break;
      case BrowsingContext.ReadinessState.Complete:
        // No `loaderId` means same-document navigation.
        if (cdpNavigateResult.loaderId === undefined) {
          await this.#navigation.withinDocument;
        } else {
          await this.#lifecycle.load;
        }
        break;
    }

    return {
      navigation: cdpNavigateResult.loaderId ?? null,
      // Url can change due to redirect get the latest one.
      url: wait === BrowsingContext.ReadinessState.None ? url : this.#url,
    };
  }

  async reload(
    ignoreCache: boolean,
    wait: BrowsingContext.ReadinessState
  ): Promise<BrowsingContext.NavigateResult> {
    await this.targetUnblockedOrThrow();

    this.#resetLifecycleIfFinished();

    await this.#cdpTarget.cdpClient.sendCommand('Page.reload', {
      ignoreCache,
    });

    switch (wait) {
      case BrowsingContext.ReadinessState.None:
        break;
      case BrowsingContext.ReadinessState.Interactive:
        await this.#lifecycle.DOMContentLoaded;
        break;
      case BrowsingContext.ReadinessState.Complete:
        await this.#lifecycle.load;
        break;
    }

    return {
      navigation:
        wait === BrowsingContext.ReadinessState.None
          ? null
          : this.navigableId ?? null,
      url: this.url,
    };
  }

  async setViewport(
    viewport?: BrowsingContext.Viewport | null,
    devicePixelRatio?: number | null
  ) {
    if (viewport === null && devicePixelRatio === null) {
      await this.#cdpTarget.cdpClient.sendCommand(
        'Emulation.clearDeviceMetricsOverride'
      );
    } else {
      try {
        let appliedViewport;
        if (viewport === undefined) {
          appliedViewport = this.#previousViewport;
        } else if (viewport === null) {
          appliedViewport = {
            width: 0,
            height: 0,
          };
        } else {
          appliedViewport = viewport;
        }
        this.#previousViewport = appliedViewport;
        await this.#cdpTarget.cdpClient.sendCommand(
          'Emulation.setDeviceMetricsOverride',
          {
            width: this.#previousViewport.width,
            height: this.#previousViewport.height,
            deviceScaleFactor: devicePixelRatio ? devicePixelRatio : 0,
            mobile: false,
            dontSetVisibleSize: true,
          }
        );
      } catch (err) {
        if (
          (err as Error).message.startsWith(
            // https://crsrc.org/c/content/browser/devtools/protocol/emulation_handler.cc;l=257;drc=2f6eee84cf98d4227e7c41718dd71b82f26d90ff
            'Width and height values must be positive'
          )
        ) {
          throw new UnsupportedOperationException(
            'Provided viewport dimensions are not supported'
          );
        }
        throw err;
      }
    }
  }

  async handleUserPrompt(
    params: BrowsingContext.HandleUserPromptParameters
  ): Promise<void> {
    await this.#cdpTarget.cdpClient.sendCommand('Page.handleJavaScriptDialog', {
      accept: params.accept ?? true,
      promptText: params.userText,
    });
  }

  async activate(): Promise<void> {
    await this.#cdpTarget.cdpClient.sendCommand('Page.bringToFront');
  }

  async captureScreenshot(
    params: BrowsingContext.CaptureScreenshotParameters
  ): Promise<BrowsingContext.CaptureScreenshotResult> {
    if (!this.isTopLevelContext()) {
      throw new UnsupportedOperationException(
        `Non-top-level 'context' (${params.context}) is currently not supported`
      );
    }
    const formatParameters = getImageFormatParameters(params);

    // XXX: Focus the original tab after the screenshot is taken.
    // This is needed because the screenshot gets blocked until the active tab gets focus.
    await this.#cdpTarget.cdpClient.sendCommand('Page.bringToFront');

    let captureBeyondViewport = false;
    let script: string;
    params.origin ??= 'viewport';
    switch (params.origin) {
      case 'document': {
        script = String(() => {
          const element = document.documentElement;
          return {
            x: 0,
            y: 0,
            width: element.scrollWidth,
            height: element.scrollHeight,
          };
        });
        captureBeyondViewport = true;
        break;
      }
      case 'viewport': {
        script = String(() => {
          const viewport = window.visualViewport!;
          return {
            x: viewport.pageLeft,
            y: viewport.pageTop,
            width: viewport.width,
            height: viewport.height,
          };
        });
        break;
      }
    }
    const realm = await this.getOrCreateSandbox(undefined);
    const originResult = await realm.callFunction(script, false);
    assert(originResult.type === 'success');
    const origin = deserializeDOMRect(originResult.result);
    assert(origin);

    const rect = params.clip
      ? getIntersectionRect(await this.#parseRect(params.clip), origin)
      : origin;

    if (rect.width === 0 || rect.height === 0) {
      throw new UnableToCaptureScreenException(
        `Unable to capture screenshot with zero dimensions: width=${rect.width}, height=${rect.height}`
      );
    }

    return await this.#cdpTarget.cdpClient.sendCommand(
      'Page.captureScreenshot',
      {
        clip: {...rect, scale: 1.0},
        ...formatParameters,
        captureBeyondViewport,
      }
    );
  }

  async print(
    params: BrowsingContext.PrintParameters
  ): Promise<BrowsingContext.PrintResult> {
    const cdpParams: Protocol.Page.PrintToPDFRequest = {};

    if (params.background !== undefined) {
      cdpParams.printBackground = params.background;
    }
    if (params.margin?.bottom !== undefined) {
      cdpParams.marginBottom = inchesFromCm(params.margin.bottom);
    }
    if (params.margin?.left !== undefined) {
      cdpParams.marginLeft = inchesFromCm(params.margin.left);
    }
    if (params.margin?.right !== undefined) {
      cdpParams.marginRight = inchesFromCm(params.margin.right);
    }
    if (params.margin?.top !== undefined) {
      cdpParams.marginTop = inchesFromCm(params.margin.top);
    }
    if (params.orientation !== undefined) {
      cdpParams.landscape = params.orientation === 'landscape';
    }
    if (params.page?.height !== undefined) {
      cdpParams.paperHeight = inchesFromCm(params.page.height);
    }
    if (params.page?.width !== undefined) {
      cdpParams.paperWidth = inchesFromCm(params.page.width);
    }
    if (params.pageRanges !== undefined) {
      for (const range of params.pageRanges) {
        if (typeof range === 'number') {
          continue;
        }
        const rangeParts = range.split('-');
        if (rangeParts.length < 1 || rangeParts.length > 2) {
          throw new InvalidArgumentException(
            `Invalid page range: ${range} is not a valid integer range.`
          );
        }
        if (rangeParts.length === 1) {
          void parseInteger(rangeParts[0] ?? '');
          continue;
        }
        let lowerBound: number;
        let upperBound: number;
        const [rangeLowerPart = '', rangeUpperPart = ''] = rangeParts;
        if (rangeLowerPart === '') {
          lowerBound = 1;
        } else {
          lowerBound = parseInteger(rangeLowerPart);
        }
        if (rangeUpperPart === '') {
          upperBound = Number.MAX_SAFE_INTEGER;
        } else {
          upperBound = parseInteger(rangeUpperPart);
        }
        if (lowerBound > upperBound) {
          throw new InvalidArgumentException(
            `Invalid page range: ${rangeLowerPart} > ${rangeUpperPart}`
          );
        }
      }
      cdpParams.pageRanges = params.pageRanges.join(',');
    }
    if (params.scale !== undefined) {
      cdpParams.scale = params.scale;
    }
    if (params.shrinkToFit !== undefined) {
      cdpParams.preferCSSPageSize = !params.shrinkToFit;
    }

    try {
      const result = await this.#cdpTarget.cdpClient.sendCommand(
        'Page.printToPDF',
        cdpParams
      );
      return {
        data: result.data,
      };
    } catch (error) {
      // Effectively zero dimensions.
      if (
        (error as Error).message ===
        'invalid print parameters: content area is empty'
      ) {
        throw new UnsupportedOperationException((error as Error).message);
      }
      throw error;
    }
  }

  /**
   * See
   * https://w3c.github.io/webdriver-bidi/#:~:text=If%20command%20parameters%20contains%20%22clip%22%3A
   */
  async #parseRect(clip: BrowsingContext.ClipRectangle) {
    switch (clip.type) {
      case 'box':
        return {x: clip.x, y: clip.y, width: clip.width, height: clip.height};
      case 'element': {
        // TODO: #1213: Use custom sandbox specifically for Chromium BiDi
        const sandbox = await this.getOrCreateSandbox(undefined);
        const result = await sandbox.callFunction(
          String((element: unknown) => {
            return element instanceof Element;
          }),
          false,
          {type: 'undefined'},
          [clip.element]
        );
        if (result.type === 'exception') {
          throw new NoSuchElementException(
            `Element '${clip.element.sharedId}' was not found`
          );
        }
        assert(result.result.type === 'boolean');
        if (!result.result.value) {
          throw new NoSuchElementException(
            `Node '${clip.element.sharedId}' is not an Element`
          );
        }
        {
          const result = await sandbox.callFunction(
            String((element: Element) => {
              const rect = element.getBoundingClientRect();
              return {
                x: rect.x,
                y: rect.y,
                height: rect.height,
                width: rect.width,
              };
            }),
            false,
            {type: 'undefined'},
            [clip.element]
          );
          assert(result.type === 'success');
          const rect = deserializeDOMRect(result.result);
          if (!rect) {
            throw new UnableToCaptureScreenException(
              `Could not get bounding box for Element '${clip.element.sharedId}'`
            );
          }
          return rect;
        }
      }
    }
  }

  async close(): Promise<void> {
    await this.#cdpTarget.cdpClient.sendCommand('Page.close');
  }

  async traverseHistory(delta: number): Promise<void> {
    if (delta === 0) {
      return;
    }

    const history = await this.#cdpTarget.cdpClient.sendCommand(
      'Page.getNavigationHistory'
    );
    const entry = history.entries[history.currentIndex + delta];
    if (!entry) {
      throw new NoSuchHistoryEntryException(
        `No history entry at delta ${delta}`
      );
    }
    await this.#cdpTarget.cdpClient.sendCommand('Page.navigateToHistoryEntry', {
      entryId: entry.id,
    });
  }

  async toggleModulesIfNeeded(): Promise<void> {
    await this.#cdpTarget.toggleNetworkIfNeeded();
  }

  async locateNodes(
    params: BrowsingContext.LocateNodesParameters
  ): Promise<BrowsingContext.LocateNodesResult> {
    // TODO: create a dedicated sandbox instead of `#defaultRealm`.
    return await this.#locateNodesByLocator(
      this.#defaultRealm,
      params.locator,
      params.startNodes ?? [],
      params.maxNodeCount,
      params.serializationOptions
    );
  }

  #getLocatorDelegate(
    locator: BrowsingContext.Locator,
    maxNodeCount: number | undefined,
    startNodes: Script.SharedReference[]
  ): {
    functionDeclaration: string;
    argumentsLocalValues: Script.LocalValue[];
  } {
    switch (locator.type) {
      case 'css':
        return {
          functionDeclaration: String(
            (
              cssSelector: string,
              maxNodeCount: number,
              ...startNodes: HTMLElement[]
            ) => {
              const locateNodesUsingCss = (element: HTMLElement) => {
                if (!(element instanceof HTMLElement)) {
                  throw new Error(
                    'startNodes in css selector should be HTMLElement'
                  );
                }
                return [...element.querySelectorAll(cssSelector)];
              };

              startNodes = startNodes.length > 0 ? startNodes : [document.body];
              const returnedNodes = startNodes
                .map((startNode) =>
                  // TODO: stop search early if `maxNodeCount` is reached.
                  locateNodesUsingCss(startNode)
                )
                .flat(1);
              return maxNodeCount === 0
                ? returnedNodes
                : returnedNodes.slice(0, maxNodeCount);
            }
          ),
          argumentsLocalValues: [
            // `cssSelector`
            {type: 'string', value: locator.value},
            // `maxNodeCount` with `0` means no limit.
            {type: 'number', value: maxNodeCount ?? 0},
            // `startNodes`
            ...startNodes,
          ],
        };
      case 'xpath':
        return {
          functionDeclaration: String(
            (
              xPathSelector: string,
              maxNodeCount: number,
              ...startNodes: HTMLElement[]
            ) => {
              // https://w3c.github.io/webdriver-bidi/#locate-nodes-using-xpath
              const evaluator = new XPathEvaluator();
              const expression = evaluator.createExpression(xPathSelector);
              const locateNodesUsingXpath = (element: HTMLElement) => {
                const xPathResult = expression.evaluate(
                  element,
                  XPathResult.ORDERED_NODE_SNAPSHOT_TYPE
                );
                const returnedNodes = [];
                for (let i = 0; i < xPathResult.snapshotLength; i++) {
                  returnedNodes.push(xPathResult.snapshotItem(i));
                }
                return returnedNodes;
              };
              startNodes = startNodes.length > 0 ? startNodes : [document.body];
              const returnedNodes = startNodes
                .map((startNode) =>
                  // TODO: stop search early if `maxNodeCount` is reached.
                  locateNodesUsingXpath(startNode)
                )
                .flat(1);
              return maxNodeCount === 0
                ? returnedNodes
                : returnedNodes.slice(0, maxNodeCount);
            }
          ),
          argumentsLocalValues: [
            // `xPathSelector`
            {type: 'string', value: locator.value},
            // `maxNodeCount` with `0` means no limit.
            {type: 'number', value: maxNodeCount ?? 0},
            // `startNodes`
            ...startNodes,
          ],
        };
      case 'innerText':
        // https://w3c.github.io/webdriver-bidi/#locate-nodes-using-inner-text
        if (locator.value === '') {
          throw new InvalidSelectorException(
            'innerText locator cannot be empty'
          );
        }
        return {
          functionDeclaration: String(
            (
              innerTextSelector: string,
              fullMatch: boolean,
              ignoreCase: boolean,
              maxNodeCount: number,
              maxDepth: number,
              ...startNodes: HTMLElement[]
            ) => {
              const searchText = ignoreCase
                ? innerTextSelector.toUpperCase()
                : innerTextSelector;
              const locateNodesUsingInnerText = (
                element: HTMLElement,
                currentMaxDepth: number
              ) => {
                const returnedNodes: HTMLElement[] = [];
                const nodeInnerText = ignoreCase
                  ? element.innerText?.toUpperCase()
                  : element.innerText;
                if (!nodeInnerText.includes(searchText)) {
                  return [];
                }
                const childNodes = [];
                for (const child of element.children) {
                  if (child instanceof HTMLElement) {
                    childNodes.push(child);
                  }
                }
                if (childNodes.length === 0) {
                  if (fullMatch && nodeInnerText === searchText) {
                    returnedNodes.push(element);
                  } else {
                    if (!fullMatch) {
                      // Note: `nodeInnerText.includes(searchText)` is already checked
                      returnedNodes.push(element);
                    }
                  }
                } else {
                  const childNodeMatches =
                    // Don't search deeper if `maxDepth` is reached.
                    currentMaxDepth === 0
                      ? []
                      : childNodes
                          .map((child) =>
                            locateNodesUsingInnerText(
                              child,
                              currentMaxDepth - 1
                            )
                          )
                          .flat(1);
                  if (childNodeMatches.length === 0) {
                    // Note: `nodeInnerText.includes(searchText)` is already checked
                    if (!fullMatch || nodeInnerText === searchText) {
                      returnedNodes.push(element);
                    }
                  } else {
                    returnedNodes.push(...childNodeMatches);
                  }
                }
                // TODO: stop search early if `maxNodeCount` is reached.
                return returnedNodes;
              };
              // TODO: add maxDepth.
              // TODO: stop search early if `maxNodeCount` is reached.
              startNodes = startNodes.length > 0 ? startNodes : [document.body];
              const returnedNodes = startNodes
                .map((startNode) =>
                  // TODO: stop search early if `maxNodeCount` is reached.
                  locateNodesUsingInnerText(startNode, maxDepth)
                )
                .flat(1);
              return maxNodeCount === 0
                ? returnedNodes
                : returnedNodes.slice(0, maxNodeCount);
            }
          ),
          argumentsLocalValues: [
            // `innerTextSelector`
            {type: 'string', value: locator.value},
            // `fullMatch` with default `true`.
            {type: 'boolean', value: locator.matchType !== 'partial'},
            // `ignoreCase` with default `false`.
            {type: 'boolean', value: locator.ignoreCase === true},
            // `maxNodeCount` with `0` means no limit.
            {type: 'number', value: maxNodeCount ?? 0},
            // `maxDepth` with default `1000` (same as default full serialization depth).
            {type: 'number', value: locator.maxDepth ?? 1000},
            // `startNodes`
            ...startNodes,
          ],
        };
    }
  }

  async #locateNodesByLocator(
    realm: Realm,
    locator: BrowsingContext.Locator,
    startNodes: Script.SharedReference[],
    maxNodeCount: number | undefined,
    serializationOptions: Script.SerializationOptions | undefined
  ): Promise<BrowsingContext.LocateNodesResult> {
    const locatorDelegate = this.#getLocatorDelegate(
      locator,
      maxNodeCount,
      startNodes
    );

    serializationOptions = {
      ...serializationOptions,
      // The returned object is an array of nodes, so no need in deeper JS serialization.
      maxObjectDepth: 1,
    };

    const locatorResult = await realm.callFunction(
      locatorDelegate.functionDeclaration,
      false,
      {type: 'undefined'},
      locatorDelegate.argumentsLocalValues,
      Script.ResultOwnership.None,
      serializationOptions
    );

    if (locatorResult.type !== 'success') {
      this.#logger?.(
        BrowsingContextImpl.LOGGER_PREFIX,
        'Failed locateNodesByLocator',
        locatorResult
      );

      // Heuristic to detect invalid selector for different types of selectors.
      if (
        // CSS selector.
        locatorResult.exceptionDetails.text?.endsWith(
          'is not a valid selector.'
        ) ||
        // XPath selector.
        locatorResult.exceptionDetails.text?.endsWith(
          'is not a valid XPath expression.'
        )
      ) {
        throw new InvalidSelectorException(
          `Not valid selector ${locator.value}`
        );
      }
      // Heuristic to detect if the `startNode` is not an `HTMLElement` in css selector.
      if (
        locatorResult.exceptionDetails.text ===
        'Error: startNodes in css selector should be HTMLElement'
      ) {
        throw new InvalidArgumentException(
          `startNodes in css selector should be HTMLElement`
        );
      }
      throw new UnknownErrorException(
        `Unexpected error in selector script: ${locatorResult.exceptionDetails.text}`
      );
    }

    if (locatorResult.result.type !== 'array') {
      throw new UnknownErrorException(
        `Unexpected selector script result type: ${locatorResult.result.type}`
      );
    }

    // Check there are no non-node elements in the result.
    const nodes = locatorResult.result.value!.map(
      (value): Script.NodeRemoteValue => {
        if (value.type !== 'node') {
          throw new UnknownErrorException(
            `Unexpected selector script result element: ${value.type}`
          );
        }
        return value;
      }
    );

    return {nodes};
  }
}

export function serializeOrigin(origin: string) {
  // https://html.spec.whatwg.org/multipage/origin.html#ascii-serialisation-of-an-origin
  if (['://', ''].includes(origin)) {
    origin = 'null';
  }
  return origin;
}

function getImageFormatParameters(
  params: Readonly<BrowsingContext.CaptureScreenshotParameters>
) {
  const {quality, type} = params.format ?? {
    type: 'image/png',
  };
  switch (type) {
    case 'image/png': {
      return {format: 'png'} as const;
    }
    case 'image/jpeg': {
      return {
        format: 'jpeg',
        ...(quality === undefined ? {} : {quality: Math.round(quality * 100)}),
      } as const;
    }
    case 'image/webp': {
      return {
        format: 'webp',
        ...(quality === undefined ? {} : {quality: Math.round(quality * 100)}),
      } as const;
    }
  }
  throw new InvalidArgumentException(
    `Image format '${type}' is not a supported format`
  );
}

function deserializeDOMRect(
  result: Script.RemoteValue
): Protocol.DOM.Rect | undefined {
  if (result.type !== 'object' || result.value === undefined) {
    return;
  }
  const x = result.value.find(([key]) => {
    return key === 'x';
  })?.[1];
  const y = result.value.find(([key]) => {
    return key === 'y';
  })?.[1];
  const height = result.value.find(([key]) => {
    return key === 'height';
  })?.[1];
  const width = result.value.find(([key]) => {
    return key === 'width';
  })?.[1];
  if (
    x?.type !== 'number' ||
    y?.type !== 'number' ||
    height?.type !== 'number' ||
    width?.type !== 'number'
  ) {
    return;
  }
  return {
    x: x.value,
    y: y.value,
    width: width.value,
    height: height.value,
  } as Protocol.DOM.Rect;
}

/** @see https://w3c.github.io/webdriver-bidi/#normalize-rect */
function normalizeRect(box: Readonly<Protocol.DOM.Rect>): Protocol.DOM.Rect {
  return {
    ...(box.width < 0
      ? {
          x: box.x + box.width,
          width: -box.width,
        }
      : {
          x: box.x,
          width: box.width,
        }),
    ...(box.height < 0
      ? {
          y: box.y + box.height,
          height: -box.height,
        }
      : {
          y: box.y,
          height: box.height,
        }),
  };
}

/** @see https://w3c.github.io/webdriver-bidi/#rectangle-intersection */
function getIntersectionRect(
  first: Readonly<Protocol.DOM.Rect>,
  second: Readonly<Protocol.DOM.Rect>
): Protocol.DOM.Rect {
  first = normalizeRect(first);
  second = normalizeRect(second);
  const x = Math.max(first.x, second.x);
  const y = Math.max(first.y, second.y);
  return {
    x,
    y,
    width: Math.max(
      Math.min(first.x + first.width, second.x + second.width) - x,
      0
    ),
    height: Math.max(
      Math.min(first.y + first.height, second.y + second.height) - y,
      0
    ),
  };
}

function parseInteger(value: string) {
  value = value.trim();
  if (!/^[0-9]+$/.test(value)) {
    throw new InvalidArgumentException(`Invalid integer: ${value}`);
  }
  return parseInt(value);
}
