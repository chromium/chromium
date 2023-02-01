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

import {BrowsingContext, Message} from '../../../protocol/protocol.js';
import {BrowsingContextStorage} from './browsingContextStorage.js';
import {CdpClient} from '../../CdpConnection.js';
import {Deferred} from '../../../utils/deferred.js';
import {IEventManager} from '../events/EventManager.js';
import {LogManager} from '../log/logManager.js';
import {Protocol} from 'devtools-protocol';
import {Realm} from '../script/realm.js';
import {RealmStorage} from '../script/realmStorage.js';

export class BrowsingContextImpl {
  readonly #targetDefers = {
    documentInitialized: new Deferred<void>(),
    targetUnblocked: new Deferred<void>(),
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
  readonly #cdpBrowserContextId: string | null;
  readonly #eventManager: IEventManager;
  readonly #children: Map<string, BrowsingContextImpl> = new Map();
  readonly #realmStorage: RealmStorage;

  #url = 'about:blank';
  #loaderId: string | null = null;
  #cdpSessionId: string;
  #cdpClient: CdpClient;
  #maybeDefaultRealm: Realm | undefined;
  #browsingContextStorage: BrowsingContextStorage;

  get #defaultRealm(): Realm {
    if (this.#maybeDefaultRealm === undefined) {
      throw new Error(
        `No default realm for browsing context ${this.#contextId}`
      );
    }
    return this.#maybeDefaultRealm;
  }

  private constructor(
    realmStorage: RealmStorage,
    contextId: string,
    parentId: string | null,
    cdpClient: CdpClient,
    cdpSessionId: string,
    cdpBrowserContextId: string | null,
    eventManager: IEventManager,
    browsingContextStorage: BrowsingContextStorage
  ) {
    this.#realmStorage = realmStorage;
    this.#contextId = contextId;
    this.#parentId = parentId;
    this.#cdpClient = cdpClient;
    this.#cdpBrowserContextId = cdpBrowserContextId;
    this.#eventManager = eventManager;
    this.#cdpSessionId = cdpSessionId;
    this.#browsingContextStorage = browsingContextStorage;

    this.#initListeners();

    this.#browsingContextStorage.addContext(this);
  }

  public static async createFrameContext(
    realmStorage: RealmStorage,
    contextId: string,
    parentId: string | null,
    cdpClient: CdpClient,
    cdpSessionId: string,
    eventManager: IEventManager,
    browsingContextStorage: BrowsingContextStorage
  ): Promise<void> {
    const context = new BrowsingContextImpl(
      realmStorage,
      contextId,
      parentId,
      cdpClient,
      cdpSessionId,
      null,
      eventManager,
      browsingContextStorage
    );
    context.#targetDefers.targetUnblocked.resolve();

    await eventManager.registerEvent(
      {
        method: BrowsingContext.EventNames.ContextCreatedEvent,
        params: context.serializeToBidiValue(),
      },
      context.contextId
    );
  }

  public static async createTargetContext(
    realmStorage: RealmStorage,
    contextId: string,
    parentId: string | null,
    cdpClient: CdpClient,
    cdpSessionId: string,
    cdpBrowserContextId: string | null,
    eventManager: IEventManager,
    browsingContextStorage: BrowsingContextStorage
  ): Promise<void> {
    const context = new BrowsingContextImpl(
      realmStorage,
      contextId,
      parentId,
      cdpClient,
      cdpSessionId,
      cdpBrowserContextId,
      eventManager,
      browsingContextStorage
    );

    // No need in waiting for target to be unblocked.
    // noinspection ES6MissingAwait
    context.#unblockAttachedTarget();

    await eventManager.registerEvent(
      {
        method: BrowsingContext.EventNames.ContextCreatedEvent,
        params: context.serializeToBidiValue(),
      },
      context.contextId
    );
  }

  get cdpBrowserContextId(): string | null {
    return this.#cdpBrowserContextId;
  }

  // https://html.spec.whatwg.org/multipage/document-sequences.html#navigable
  get navigableId(): string | null {
    return this.#loaderId;
  }

  public convertFrameToTargetContext(
    cdpClient: CdpClient,
    cdpSessionId: string
  ) {
    this.#updateConnection(cdpClient, cdpSessionId);
    // No need in waiting for target to be unblocked.
    // noinspection JSIgnoredPromiseFromCall
    this.#unblockAttachedTarget();
  }

  public async delete() {
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

    await this.#eventManager.registerEvent(
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

  #updateConnection(cdpClient: CdpClient, cdpSessionId: string) {
    if (!this.#targetDefers.targetUnblocked.isFinished) {
      this.#targetDefers.targetUnblocked.reject('OOPiF');
    }
    this.#targetDefers.targetUnblocked = new Deferred<void>();

    this.#cdpClient = cdpClient;
    this.#cdpSessionId = cdpSessionId;

    this.#initListeners();
  }

  async #unblockAttachedTarget() {
    LogManager.create(
      this.#realmStorage,
      this.#cdpClient,
      this.#cdpSessionId,
      this.#eventManager
    );
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
    this.#targetDefers.targetUnblocked.resolve();
  }

  get contextId(): string {
    return this.#contextId;
  }

  get parentId(): string | null {
    return this.#parentId;
  }

  get cdpSessionId(): string {
    return this.#cdpSessionId;
  }

  get children(): BrowsingContextImpl[] {
    return Array.from(this.#children.values());
  }

  get url(): string {
    return this.#url;
  }

  addChild(child: BrowsingContextImpl): void {
    this.#children.set(child.contextId, child);
  }

  async awaitLoaded(): Promise<void> {
    await this.#targetDefers.Page.lifecycleEvent.load;
  }

  async awaitUnblocked(): Promise<void> {
    await this.#targetDefers.targetUnblocked;
  }

  public serializeToBidiValue(
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
    this.#cdpClient.on(
      'Target.targetInfoChanged',
      (params: Protocol.Target.TargetInfoChangedEvent) => {
        if (this.contextId !== params.targetInfo.targetId) {
          return;
        }
        this.#url = params.targetInfo.url;
      }
    );

    this.#cdpClient.on(
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

    this.#cdpClient.on(
      'Page.navigatedWithinDocument',
      (params: Protocol.Page.NavigatedWithinDocumentEvent) => {
        if (this.contextId !== params.frameId) {
          return;
        }

        this.#url = params.url;
        this.#targetDefers.Page.navigatedWithinDocument.resolve(params);
      }
    );

    this.#cdpClient.on(
      'Page.lifecycleEvent',
      async (params: Protocol.Page.LifecycleEventEvent) => {
        if (this.contextId !== params.frameId) {
          return;
        }

        if (params.name === 'init') {
          this.#documentChanged(params.loaderId);
          this.#targetDefers.documentInitialized.resolve();
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
            this.#targetDefers.Page.lifecycleEvent.DOMContentLoaded.resolve(
              params
            );
            await this.#eventManager.registerEvent(
              {
                method: BrowsingContext.EventNames.DomContentLoadedEvent,
                params: {
                  context: this.contextId,
                  navigation: this.#loaderId,
                  url: this.#url,
                },
              },
              this.contextId
            );
            break;

          case 'load':
            this.#targetDefers.Page.lifecycleEvent.load.resolve(params);
            await this.#eventManager.registerEvent(
              {
                method: BrowsingContext.EventNames.LoadEvent,
                params: {
                  context: this.contextId,
                  navigation: this.#loaderId,
                  url: this.#url,
                },
              },
              this.contextId
            );
            break;
        }
      }
    );

    this.#cdpClient.on(
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
          params.context.uniqueId,
          this.contextId,
          this.navigableId ?? 'UNKNOWN',
          params.context.id,
          this.#getOrigin(params),
          // TODO: differentiate types.
          'window',
          // Sandbox name for isolated world.
          params.context.auxData.type === 'isolated'
            ? params.context.name
            : undefined,
          this.#cdpSessionId,
          this.#cdpClient
        );

        if (params.context.auxData.isDefault) {
          this.#maybeDefaultRealm = realm;
        }
      }
    );

    this.#cdpClient.on(
      'Runtime.executionContextDestroyed',
      (params: Protocol.Runtime.ExecutionContextDestroyedEvent) => {
        this.#realmStorage.deleteRealms({
          cdpSessionId: this.#cdpSessionId,
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

  #documentChanged(loaderId: string) {
    if (this.#loaderId === loaderId) {
      return;
    }

    if (!this.#targetDefers.documentInitialized.isFinished) {
      this.#targetDefers.documentInitialized.reject('Document changed');
    }
    this.#targetDefers.documentInitialized = new Deferred<void>();

    if (!this.#targetDefers.Page.navigatedWithinDocument.isFinished) {
      this.#targetDefers.Page.navigatedWithinDocument.reject(
        'Document changed'
      );
    }
    this.#targetDefers.Page.navigatedWithinDocument =
      new Deferred<Protocol.Page.NavigatedWithinDocumentEvent>();

    if (!this.#targetDefers.Page.lifecycleEvent.DOMContentLoaded.isFinished) {
      this.#targetDefers.Page.lifecycleEvent.DOMContentLoaded.reject(
        'Document changed'
      );
    }
    this.#targetDefers.Page.lifecycleEvent.DOMContentLoaded =
      new Deferred<Protocol.Page.LifecycleEventEvent>();

    if (!this.#targetDefers.Page.lifecycleEvent.load.isFinished) {
      this.#targetDefers.Page.lifecycleEvent.load.reject('Document changed');
    }
    this.#targetDefers.Page.lifecycleEvent.load =
      new Deferred<Protocol.Page.LifecycleEventEvent>();

    this.#loaderId = loaderId;
  }

  async navigate(
    url: string,
    wait: BrowsingContext.ReadinessState
  ): Promise<BrowsingContext.NavigateResult> {
    await this.#targetDefers.targetUnblocked;

    // TODO: handle loading errors.
    const cdpNavigateResult = await this.#cdpClient.sendCommand(
      'Page.navigate',
      {
        url,
        frameId: this.contextId,
      }
    );

    if (cdpNavigateResult.errorText) {
      throw new Message.UnknownException(cdpNavigateResult.errorText);
    }

    if (
      cdpNavigateResult.loaderId !== undefined &&
      cdpNavigateResult.loaderId !== this.#loaderId
    ) {
      this.#documentChanged(cdpNavigateResult.loaderId);
    }

    // Wait for `wait` condition.
    switch (wait) {
      case 'none':
        break;

      case 'interactive':
        // No `loaderId` means same-document navigation.
        if (cdpNavigateResult.loaderId === undefined) {
          await this.#targetDefers.Page.navigatedWithinDocument;
        } else {
          await this.#targetDefers.Page.lifecycleEvent.DOMContentLoaded;
        }
        break;

      case 'complete':
        // No `loaderId` means same-document navigation.
        if (cdpNavigateResult.loaderId === undefined) {
          await this.#targetDefers.Page.navigatedWithinDocument;
        } else {
          await this.#targetDefers.Page.lifecycleEvent.load;
        }
        break;

      default:
        throw new Error(`Not implemented wait '${wait}'`);
    }

    return {
      result: {
        navigation: cdpNavigateResult.loaderId || null,
        url: url,
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
      await this.#cdpClient.sendCommand('Page.createIsolatedWorld', {
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
}
