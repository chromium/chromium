/**
 * Copyright 2026 Google LLC.
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

import {beforeEach, describe, it} from 'node:test';

import {assert} from 'chai';
import type {Protocol} from 'devtools-protocol';
import type {ProtocolMapping} from 'devtools-protocol/types/protocol-mapping.js';
import sinon from 'sinon';

import {
  CloseError,
  type CdpClient,
  type CdpEvents,
} from '../../../cdp/CdpClient.js';
import type {CdpConnection} from '../../../cdp/CdpConnection.js';
import {ChromiumBidi, Network} from '../../../protocol/protocol.js';
import {Deferred} from '../../../utils/Deferred.js';
import {EventEmitter} from '../../../utils/EventEmitter.js';
import {type LoggerFn, LogType} from '../../../utils/log.js';
import type {BluetoothProcessor} from '../bluetooth/BluetoothProcessor.js';
import {ContextConfigStorage} from '../browser/ContextConfigStorage.js';
import {UserContextStorage} from '../browser/UserContextStorage.js';
import {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';
import {NetworkStorage} from '../network/NetworkStorage.js';
import {PreloadScript} from '../script/PreloadScript.js';
import {PreloadScriptStorage} from '../script/PreloadScriptStorage.js';
import {RealmStorage} from '../script/RealmStorage.js';
import {AclManager} from '../session/AclManager.js';
import {EventManager} from '../session/EventManager.js';
import type {SpeculationProcessor} from '../speculation/SpeculationProcessor.js';

import {CdpTargetManager} from './CdpTargetManager.js';

const SELF_TARGET_ID = 'MAPPER_TARGET';
const DEFAULT_BROWSER_CONTEXT_ID = 'DEFAULT_BROWSER_CONTEXT';

const TAB_TARGET_ID = 'TAB_TARGET';
const TAB_SESSION_ID = 'TAB_SESSION';
// Top-level frame of the first tab, hosted by `PAGE_SESSION_ID`.
const TOP_FRAME_ID = 'TOP_FRAME';
const PAGE_SESSION_ID = 'PAGE_SESSION';
// Same-process child frame of `TOP_FRAME_ID`.
const CHILD_FRAME_ID = 'CHILD_FRAME';
// Out-of-process child frame of `TOP_FRAME_ID`, hosted by `OOPIF_SESSION_ID`.
const OOPIF_FRAME_ID = 'OOPIF_FRAME';
const OOPIF_SESSION_ID = 'OOPIF_SESSION';
// Out-of-process grandchild frame, hosted by `NESTED_OOPIF_SESSION_ID`.
const NESTED_OOPIF_FRAME_ID = 'NESTED_OOPIF_FRAME';
const NESTED_OOPIF_SESSION_ID = 'NESTED_OOPIF_SESSION';

const OTHER_TAB_TARGET_ID = 'OTHER_TAB_TARGET';
const OTHER_TAB_SESSION_ID = 'OTHER_TAB_SESSION';
// Top-level frame of an unrelated tab, hosted by `OTHER_PAGE_SESSION_ID`.
const OTHER_TOP_FRAME_ID = 'OTHER_TOP_FRAME';
const OTHER_PAGE_SESSION_ID = 'OTHER_PAGE_SESSION';

const OOPIF_URL = 'http://oopif.test/';

/** Stub for the processors which only observe new targets. */
const targetCreatedObserverStub = {
  onCdpTargetCreated(): void {
    // Not relevant for these tests.
  },
};

/** Returns the main frame reported by `Page.getFrameTree` for a session. */
function frameTreeForSession(
  sessionId: Protocol.Target.SessionID | undefined,
): object {
  switch (sessionId) {
    case PAGE_SESSION_ID:
      return {frame: {id: TOP_FRAME_ID, loaderId: 'TOP_LOADER', url: ''}};
    case OTHER_PAGE_SESSION_ID:
      return {
        frame: {id: OTHER_TOP_FRAME_ID, loaderId: 'OTHER_LOADER', url: ''},
      };
    case OOPIF_SESSION_ID:
      return {
        frame: {
          id: OOPIF_FRAME_ID,
          parentId: TOP_FRAME_ID,
          loaderId: 'OOPIF_LOADER',
          url: OOPIF_URL,
        },
      };
    case NESTED_OOPIF_SESSION_ID:
      return {
        frame: {
          id: NESTED_OOPIF_FRAME_ID,
          loaderId: 'NESTED_OOPIF_LOADER',
          url: OOPIF_URL,
        },
      };
    default:
      return {frame: {id: 'UNKNOWN_FRAME', loaderId: 'UNKNOWN', url: ''}};
  }
}

/** Fake CDP session which records nothing and answers every command. */
class FakeCdpClient extends EventEmitter<CdpEvents> implements CdpClient {
  readonly sessionId: Protocol.Target.SessionID | undefined;
  frameTree: object | undefined;
  frameTreePromise: Promise<object> | undefined;
  setAutoAttachError?: unknown;

  constructor(sessionId: Protocol.Target.SessionID | undefined) {
    super();
    this.sessionId = sessionId;
  }

  sendCommand<CdpMethod extends keyof ProtocolMapping.Commands>(
    method: CdpMethod,
  ): Promise<ProtocolMapping.Commands[CdpMethod]['returnType']> {
    if (
      method === 'Target.setAutoAttach' &&
      this.setAutoAttachError !== undefined
    ) {
      return Promise.reject(this.setAutoAttachError);
    }
    let result: object = {};
    if (method === 'Page.getFrameTree') {
      if (this.frameTreePromise !== undefined) {
        return this.frameTreePromise.then(
          (frameTree) =>
            ({
              frameTree,
            }) as ProtocolMapping.Commands[CdpMethod]['returnType'],
        );
      }
      result = {
        frameTree: this.frameTree ?? frameTreeForSession(this.sessionId),
      };
    } else if (method === 'Browser.getWindowForTarget') {
      result = {windowId: 1, bounds: {}};
    } else if (method === 'Target.getBrowserContexts') {
      result = {browserContextIds: []};
    }
    return Promise.resolve(
      result as ProtocolMapping.Commands[CdpMethod]['returnType'],
    );
  }

  isCloseError(error: unknown): boolean {
    return error instanceof CloseError;
  }

  /** Emits a CDP event as if it was received on this session. */
  receive(method: string, params: object): void {
    this.emit(method as keyof CdpEvents, params as never);
  }
}

class FakeCdpConnection implements CdpConnection {
  readonly #clients = new Map<Protocol.Target.SessionID, FakeCdpClient>();

  getCdpClient(sessionId: Protocol.Target.SessionID): FakeCdpClient {
    let client = this.#clients.get(sessionId);
    if (client === undefined) {
      client = new FakeCdpClient(sessionId);
      this.#clients.set(sessionId, client);
    }
    return client;
  }
}

function attachedToTarget(
  sessionId: Protocol.Target.SessionID,
  targetId: Protocol.Target.TargetID,
  type: string,
  url = '',
  parentFrameId?: Protocol.Page.FrameId,
): object {
  return {
    sessionId,
    targetInfo: {
      targetId,
      type,
      title: '',
      url,
      attached: true,
      canAccessOpener: false,
      browserContextId: DEFAULT_BROWSER_CONTEXT_ID,
      ...(parentFrameId === undefined ? {} : {parentFrameId}),
    },
    waitingForDebugger: true,
  };
}

describe('CdpTargetManager', () => {
  let connection: FakeCdpConnection;
  let browserClient: FakeCdpClient;
  let pageClient: FakeCdpClient;
  let otherPageClient: FakeCdpClient;
  let browsingContextStorage: BrowsingContextStorage;
  let realmStorage: RealmStorage;
  let networkStorage: NetworkStorage;
  let configStorage: ContextConfigStorage;
  let preloadScriptStorage: PreloadScriptStorage;
  let debugErrorLog: sinon.SinonSpy;
  let eventManager: EventManager;

  beforeEach(() => {
    connection = new FakeCdpConnection();
    browserClient = new FakeCdpClient(undefined);
    browsingContextStorage = new BrowsingContextStorage();
    realmStorage = new RealmStorage();
    debugErrorLog = sinon.spy();
    const logger: LoggerFn = (type) =>
      type === LogType.debugError ? debugErrorLog : undefined;
    const userContextStorage = new UserContextStorage(browserClient);
    eventManager = new EventManager(
      browsingContextStorage,
      userContextStorage,
      new AclManager(browsingContextStorage),
    );
    networkStorage = new NetworkStorage(
      eventManager,
      browsingContextStorage,
      browserClient,
    );
    configStorage = new ContextConfigStorage(
      browsingContextStorage,
      userContextStorage,
    );
    preloadScriptStorage = new PreloadScriptStorage();
    new CdpTargetManager(
      connection,
      browserClient,
      SELF_TARGET_ID,
      eventManager,
      browsingContextStorage,
      realmStorage,
      networkStorage,
      configStorage,
      targetCreatedObserverStub as unknown as BluetoothProcessor,
      targetCreatedObserverStub as unknown as SpeculationProcessor,
      preloadScriptStorage,
      DEFAULT_BROWSER_CONTEXT_ID,
      'Fake User Agent',
      logger,
    );

    // Tab with a same-process iframe and an out-of-process iframe. The OOPIF
    // starts in the parent's process, as Chrome does for a new iframe.
    browserClient.receive(
      'Target.attachedToTarget',
      attachedToTarget(TAB_SESSION_ID, TAB_TARGET_ID, 'tab'),
    );
    connection
      .getCdpClient(TAB_SESSION_ID)
      .receive(
        'Target.attachedToTarget',
        attachedToTarget(PAGE_SESSION_ID, TOP_FRAME_ID, 'page'),
      );
    pageClient = connection.getCdpClient(PAGE_SESSION_ID);
    pageClient.receive('Page.frameAttached', {
      frameId: CHILD_FRAME_ID,
      parentFrameId: TOP_FRAME_ID,
    });
    pageClient.receive('Page.frameAttached', {
      frameId: OOPIF_FRAME_ID,
      parentFrameId: TOP_FRAME_ID,
    });
    pageClient.receive(
      'Target.attachedToTarget',
      attachedToTarget(OOPIF_SESSION_ID, OOPIF_FRAME_ID, 'iframe', OOPIF_URL),
    );

    // Unrelated tab.
    browserClient.receive(
      'Target.attachedToTarget',
      attachedToTarget(OTHER_TAB_SESSION_ID, OTHER_TAB_TARGET_ID, 'tab'),
    );
    connection
      .getCdpClient(OTHER_TAB_SESSION_ID)
      .receive(
        'Target.attachedToTarget',
        attachedToTarget(OTHER_PAGE_SESSION_ID, OTHER_TOP_FRAME_ID, 'page'),
      );
    otherPageClient = connection.getCdpClient(OTHER_PAGE_SESSION_ID);
  });

  function sessionOf(contextId: string): string | undefined {
    return browsingContextStorage.findContext(contextId)?.cdpTarget
      .cdpSessionId;
  }

  it('binds the OOPIF context to the OOPIF session', () => {
    assert.strictEqual(sessionOf(OOPIF_FRAME_ID), OOPIF_SESSION_ID);
    assert.strictEqual(sessionOf(CHILD_FRAME_ID), PAGE_SESSION_ID);
    assert.strictEqual(
      browsingContextStorage.getContext(OOPIF_FRAME_ID).parentId,
      TOP_FRAME_ID,
    );
  });

  describe('Page.frameAttached', () => {
    it('does not rebind a frame hosted by another attached target', () => {
      const oopifContext = browsingContextStorage.getContext(OOPIF_FRAME_ID);

      pageClient.receive('Page.frameAttached', {
        frameId: OOPIF_FRAME_ID,
        parentFrameId: TOP_FRAME_ID,
      });

      assert.strictEqual(
        browsingContextStorage.getContext(OOPIF_FRAME_ID),
        oopifContext,
      );
      assert.strictEqual(sessionOf(OOPIF_FRAME_ID), OOPIF_SESSION_ID);
    });

    it('ignores a parent frame the emitting session does not host', () => {
      otherPageClient.receive('Page.frameAttached', {
        frameId: 'NEW_FRAME',
        parentFrameId: TOP_FRAME_ID,
      });

      assert.isFalse(browsingContextStorage.hasContext('NEW_FRAME'));
    });

    it('refuses to create a parent cycle', () => {
      pageClient.receive('Page.frameAttached', {
        frameId: TOP_FRAME_ID,
        parentFrameId: CHILD_FRAME_ID,
      });

      const topContext = browsingContextStorage.getContext(TOP_FRAME_ID);
      assert.isTrue(topContext.isTopLevelContext());
      assert.strictEqual(
        browsingContextStorage.getContext(CHILD_FRAME_ID).parentId,
        TOP_FRAME_ID,
      );
    });

    it('creates a new frame under a parent hosted by the emitting session', () => {
      pageClient.receive('Page.frameAttached', {
        frameId: 'NEW_FRAME',
        parentFrameId: CHILD_FRAME_ID,
      });

      assert.strictEqual(sessionOf('NEW_FRAME'), PAGE_SESSION_ID);
      assert.strictEqual(
        browsingContextStorage.getContext('NEW_FRAME').parentId,
        CHILD_FRAME_ID,
      );
    });
  });

  describe('Page.frameSubtreeWillBeDetached', () => {
    it('ignores frames the emitting session neither hosts nor parents', () => {
      otherPageClient.receive('Page.frameSubtreeWillBeDetached', {
        frameId: CHILD_FRAME_ID,
      });
      otherPageClient.receive('Page.frameSubtreeWillBeDetached', {
        frameId: OOPIF_FRAME_ID,
      });

      assert.isTrue(browsingContextStorage.hasContext(CHILD_FRAME_ID));
      assert.isTrue(browsingContextStorage.hasContext(OOPIF_FRAME_ID));
    });

    it('disposes a same-process frame of the emitting session', () => {
      pageClient.receive('Page.frameSubtreeWillBeDetached', {
        frameId: CHILD_FRAME_ID,
      });

      assert.isFalse(browsingContextStorage.hasContext(CHILD_FRAME_ID));
    });

    it('keeps an OOPIF context until its target is detached', () => {
      pageClient.receive('Page.frameSubtreeWillBeDetached', {
        frameId: OOPIF_FRAME_ID,
      });

      assert.strictEqual(sessionOf(OOPIF_FRAME_ID), OOPIF_SESSION_ID);

      pageClient.receive('Target.detachedFromTarget', {
        sessionId: OOPIF_SESSION_ID,
        targetId: OOPIF_FRAME_ID,
      });

      assert.isFalse(browsingContextStorage.hasContext(OOPIF_FRAME_ID));
    });

    it('keeps a nested OOPIF when its same-process parent is detached', () => {
      // Attach an out-of-process grandchild beneath same-process `CHILD_FRAME_ID`.
      pageClient.receive('Page.frameAttached', {
        frameId: NESTED_OOPIF_FRAME_ID,
        parentFrameId: CHILD_FRAME_ID,
      });
      pageClient.receive(
        'Target.attachedToTarget',
        attachedToTarget(
          NESTED_OOPIF_SESSION_ID,
          NESTED_OOPIF_FRAME_ID,
          'iframe',
          OOPIF_URL,
        ),
      );
      const registerEventSpy = sinon.spy(eventManager, 'registerEvent');

      // Detaching `CHILD_FRAME_ID` on `pageClient` must not cascade into
      // `NESTED_OOPIF_FRAME_ID`, which is hosted by its own target.
      pageClient.receive('Page.frameSubtreeWillBeDetached', {
        frameId: CHILD_FRAME_ID,
      });

      assert.isFalse(browsingContextStorage.hasContext(CHILD_FRAME_ID));
      assert.strictEqual(
        sessionOf(NESTED_OOPIF_FRAME_ID),
        NESTED_OOPIF_SESSION_ID,
      );

      // Re-attaching `CHILD_FRAME_ID` and `NESTED_OOPIF_FRAME_ID` on `pageClient`
      // must not rebind `NESTED_OOPIF_FRAME_ID`, and must keep it in the tree.
      pageClient.receive('Page.frameAttached', {
        frameId: CHILD_FRAME_ID,
        parentFrameId: TOP_FRAME_ID,
      });
      pageClient.receive('Page.frameAttached', {
        frameId: NESTED_OOPIF_FRAME_ID,
        parentFrameId: CHILD_FRAME_ID,
      });

      assert.strictEqual(
        sessionOf(NESTED_OOPIF_FRAME_ID),
        NESTED_OOPIF_SESSION_ID,
      );
      assert.deepStrictEqual(
        browsingContextStorage
          .getContext(CHILD_FRAME_ID)
          .directChildren.map((child) => child.id),
        [NESTED_OOPIF_FRAME_ID],
      );

      const destroyedIds = registerEventSpy
        .getCalls()
        .filter(
          (call) => call.args[0].method === 'browsingContext.contextDestroyed',
        )
        .map((call) => call.args[1]);
      assert.deepStrictEqual(destroyedIds, [CHILD_FRAME_ID]);
    });

    it('disposes a nested OOPIF cleanly when its target detaches after its parent', () => {
      // Real Chrome removes a same-process parent frame before the browser
      // detaches the nested OOPIF target; only one `contextDestroyed` event
      // (for the parent subtree) should be emitted.
      pageClient.receive('Page.frameAttached', {
        frameId: NESTED_OOPIF_FRAME_ID,
        parentFrameId: CHILD_FRAME_ID,
      });
      pageClient.receive(
        'Target.attachedToTarget',
        attachedToTarget(
          NESTED_OOPIF_SESSION_ID,
          NESTED_OOPIF_FRAME_ID,
          'iframe',
          OOPIF_URL,
        ),
      );
      const registerEventSpy = sinon.spy(eventManager, 'registerEvent');

      pageClient.receive('Page.frameSubtreeWillBeDetached', {
        frameId: CHILD_FRAME_ID,
      });
      pageClient.receive('Target.detachedFromTarget', {
        sessionId: NESTED_OOPIF_SESSION_ID,
        targetId: NESTED_OOPIF_FRAME_ID,
      });

      assert.isFalse(browsingContextStorage.hasContext(NESTED_OOPIF_FRAME_ID));
      const destroyedEvents = registerEventSpy
        .getCalls()
        .filter(
          (call) => call.args[0].method === 'browsingContext.contextDestroyed',
        );
      assert.lengthOf(destroyedEvents, 1);
      assert.strictEqual(destroyedEvents[0]!.args[1], CHILD_FRAME_ID);
    });

    it('emits only one contextDestroyed when two nested OOPIFs detach bottom-up after their same-process ancestor', () => {
      // Build `TOP -> CHILD (same-process) -> OOPIF -> NESTED_OOPIF`.
      const secondOopifFrameId = 'SECOND_OOPIF_FRAME';
      const secondOopifSessionId = 'SECOND_OOPIF_SESSION';
      pageClient.receive('Page.frameAttached', {
        frameId: secondOopifFrameId,
        parentFrameId: CHILD_FRAME_ID,
      });
      pageClient.receive(
        'Target.attachedToTarget',
        attachedToTarget(
          secondOopifSessionId,
          secondOopifFrameId,
          'iframe',
          OOPIF_URL,
        ),
      );
      const secondOopifClient = connection.getCdpClient(secondOopifSessionId);
      secondOopifClient.receive('Page.frameAttached', {
        frameId: NESTED_OOPIF_FRAME_ID,
        parentFrameId: secondOopifFrameId,
      });
      secondOopifClient.receive(
        'Target.attachedToTarget',
        attachedToTarget(
          NESTED_OOPIF_SESSION_ID,
          NESTED_OOPIF_FRAME_ID,
          'iframe',
          OOPIF_URL,
        ),
      );
      const registerEventSpy = sinon.spy(eventManager, 'registerEvent');

      // Detach `CHILD_FRAME_ID` first, then detach `NESTED_OOPIF` before
      // `secondOopifFrameId` (matching `FrameTreeNode::ResetChildren` order).
      pageClient.receive('Page.frameSubtreeWillBeDetached', {
        frameId: CHILD_FRAME_ID,
      });
      secondOopifClient.receive('Target.detachedFromTarget', {
        sessionId: NESTED_OOPIF_SESSION_ID,
        targetId: NESTED_OOPIF_FRAME_ID,
      });
      pageClient.receive('Target.detachedFromTarget', {
        sessionId: secondOopifSessionId,
        targetId: secondOopifFrameId,
      });

      assert.isFalse(browsingContextStorage.hasContext(secondOopifFrameId));
      assert.isFalse(browsingContextStorage.hasContext(NESTED_OOPIF_FRAME_ID));
      const destroyedIds = registerEventSpy
        .getCalls()
        .filter(
          (call) => call.args[0].method === 'browsingContext.contextDestroyed',
        )
        .map((call) => call.args[1]);
      assert.deepStrictEqual(destroyedIds, [CHILD_FRAME_ID]);
    });
  });

  describe('parent document change and target disposal', () => {
    it('keeps an OOPIF context across parent Page.lifecycleEvent init', () => {
      const oopifContext = browsingContextStorage.getContext(OOPIF_FRAME_ID);
      const registerEventSpy = sinon.spy(eventManager, 'registerEvent');

      // A parent renderer reporting a new document (`init`) can only dispose
      // same-process children; the OOPIF remains owned by its own target.
      pageClient.receive('Page.lifecycleEvent', {
        frameId: TOP_FRAME_ID,
        loaderId: 'NEW_TOP_LOADER',
        name: 'init',
        timestamp: 0,
      });
      pageClient.receive('Page.frameAttached', {
        frameId: OOPIF_FRAME_ID,
        parentFrameId: TOP_FRAME_ID,
      });

      assert.isFalse(browsingContextStorage.hasContext(CHILD_FRAME_ID));
      assert.strictEqual(
        browsingContextStorage.getContext(OOPIF_FRAME_ID),
        oopifContext,
      );
      assert.strictEqual(sessionOf(OOPIF_FRAME_ID), OOPIF_SESSION_ID);
      const destroyedIds = registerEventSpy
        .getCalls()
        .filter(
          (call) => call.args[0].method === 'browsingContext.contextDestroyed',
        )
        .map((call) => call.args[1]);
      assert.deepStrictEqual(destroyedIds, [CHILD_FRAME_ID]);
    });

    it('keeps an OOPIF context across parent Page.frameNavigated until its target detaches', () => {
      const oopifContext = browsingContextStorage.getContext(OOPIF_FRAME_ID);

      // `Page.frameNavigated` on the parent session must not silently evict an
      // attached OOPIF or allow `Page.frameAttached` to rebind it.
      pageClient.receive('Page.frameNavigated', {
        frame: {
          id: TOP_FRAME_ID,
          loaderId: 'NEW_TOP_LOADER',
          url: 'http://top-navigated.test/',
        },
        type: 'Navigation',
      });
      pageClient.receive('Page.frameAttached', {
        frameId: OOPIF_FRAME_ID,
        parentFrameId: TOP_FRAME_ID,
      });

      assert.isFalse(browsingContextStorage.hasContext(CHILD_FRAME_ID));
      assert.strictEqual(
        browsingContextStorage.getContext(OOPIF_FRAME_ID),
        oopifContext,
      );
      assert.strictEqual(sessionOf(OOPIF_FRAME_ID), OOPIF_SESSION_ID);

      // Once the browser detaches the OOPIF target, the OOPIF is disposed.
      pageClient.receive('Target.detachedFromTarget', {
        sessionId: OOPIF_SESSION_ID,
        targetId: OOPIF_FRAME_ID,
      });
      assert.isFalse(browsingContextStorage.hasContext(OOPIF_FRAME_ID));
    });

    it('keeps a grandchild OOPIF when its parent OOPIF target detaches first', () => {
      // Attach a grandchild OOPIF through `OOPIF_SESSION_ID`.
      const oopifClient = connection.getCdpClient(OOPIF_SESSION_ID);
      oopifClient.receive('Page.frameAttached', {
        frameId: NESTED_OOPIF_FRAME_ID,
        parentFrameId: OOPIF_FRAME_ID,
      });
      oopifClient.receive(
        'Target.attachedToTarget',
        attachedToTarget(
          NESTED_OOPIF_SESSION_ID,
          NESTED_OOPIF_FRAME_ID,
          'iframe',
          OOPIF_URL,
        ),
      );
      const registerEventSpy = sinon.spy(eventManager, 'registerEvent');

      // Detaching `OOPIF_SESSION_ID` on `pageClient` must not evict
      // `NESTED_OOPIF_FRAME_ID` or let `pageClient` re-attach it.
      pageClient.receive('Target.detachedFromTarget', {
        sessionId: OOPIF_SESSION_ID,
        targetId: OOPIF_FRAME_ID,
      });
      pageClient.receive('Page.frameAttached', {
        frameId: NESTED_OOPIF_FRAME_ID,
        parentFrameId: TOP_FRAME_ID,
      });

      assert.isFalse(browsingContextStorage.hasContext(OOPIF_FRAME_ID));
      assert.strictEqual(
        sessionOf(NESTED_OOPIF_FRAME_ID),
        NESTED_OOPIF_SESSION_ID,
      );

      // When the grandchild target detaches later, no second `contextDestroyed`
      // event is emitted because `OOPIF_FRAME_ID` already covered the subtree.
      oopifClient.receive('Target.detachedFromTarget', {
        sessionId: NESTED_OOPIF_SESSION_ID,
        targetId: NESTED_OOPIF_FRAME_ID,
      });
      assert.isFalse(browsingContextStorage.hasContext(NESTED_OOPIF_FRAME_ID));
      const destroyedIds = registerEventSpy
        .getCalls()
        .filter(
          (call) => call.args[0].method === 'browsingContext.contextDestroyed',
        )
        .map((call) => call.args[1]);
      assert.deepStrictEqual(destroyedIds, [OOPIF_FRAME_ID]);
    });
  });

  describe('remote-to-local frame swap', () => {
    it('re-creates the frame on the parent session after its target is detached', () => {
      // Chrome detaches the OOPIF target before the parent's renderer reports
      // the swapped-in frame and its new document.
      pageClient.receive('Target.detachedFromTarget', {
        sessionId: OOPIF_SESSION_ID,
        targetId: OOPIF_FRAME_ID,
      });
      pageClient.receive('Page.frameAttached', {
        frameId: OOPIF_FRAME_ID,
        parentFrameId: TOP_FRAME_ID,
      });
      pageClient.receive('Runtime.executionContextCreated', {
        context: {
          id: 42,
          uniqueId: 'SWAPPED_IN_REALM',
          origin: 'http://top.test',
          name: '',
          auxData: {frameId: OOPIF_FRAME_ID, isDefault: true, type: 'default'},
        },
      });

      assert.strictEqual(sessionOf(OOPIF_FRAME_ID), PAGE_SESSION_ID);
      assert.strictEqual(
        browsingContextStorage.getContext(OOPIF_FRAME_ID).parentId,
        TOP_FRAME_ID,
      );
      // The default realm belongs to the swapped-in document, on the parent
      // session.
      const realms = realmStorage.findRealms({
        browsingContextId: OOPIF_FRAME_ID,
        sandbox: null,
      });
      assert.lengthOf(realms, 1);
      assert.strictEqual(realms[0]!.executionContextId, 42);
      assert.strictEqual(realms[0]!.cdpClient, pageClient);
    });
  });

  describe('frame events after a context moved to its own target', () => {
    it('ignores events for the frame on the previous session', () => {
      const urlBefore = browsingContextStorage.getContext(OOPIF_FRAME_ID).url;

      pageClient.receive('Page.frameNavigated', {
        frame: {
          id: OOPIF_FRAME_ID,
          parentId: TOP_FRAME_ID,
          loaderId: 'STALE_LOADER',
          url: 'http://stale.test/',
        },
        type: 'Navigation',
      });

      assert.strictEqual(
        browsingContextStorage.getContext(OOPIF_FRAME_ID).url,
        urlBefore,
      );
    });
  });

  describe('Target.targetInfoChanged', () => {
    it('is applied only from the browser session', () => {
      const targetInfo = (url: string) => ({
        targetInfo: {
          targetId: OOPIF_FRAME_ID,
          type: 'iframe',
          title: '',
          url,
          attached: true,
          canAccessOpener: false,
        },
      });

      otherPageClient.receive(
        'Target.targetInfoChanged',
        targetInfo('http://other-session.test/'),
      );
      assert.notStrictEqual(
        browsingContextStorage.getContext(OOPIF_FRAME_ID).url,
        'http://other-session.test/',
      );

      browserClient.receive(
        'Target.targetInfoChanged',
        targetInfo('http://browser-session.test/'),
      );
      assert.strictEqual(
        browsingContextStorage.getContext(OOPIF_FRAME_ID).url,
        'http://browser-session.test/',
      );
    });
  });

  describe('Page.getFrameTree', () => {
    it('does not re-parent or replace a context hosted by another session', async () => {
      const topContext = browsingContextStorage.getContext(TOP_FRAME_ID);
      const popupClient = connection.getCdpClient('POPUP_SESSION');
      popupClient.frameTree = {
        frame: {id: 'POPUP_FRAME', loaderId: 'POPUP_LOADER', url: ''},
        childFrames: [
          {
            frame: {
              id: TOP_FRAME_ID,
              parentId: 'POPUP_FRAME',
              loaderId: 'TOP_LOADER',
              url: 'http://top.test/',
            },
          },
          {
            frame: {
              id: 'FOREIGN_CHILD',
              parentId: TOP_FRAME_ID,
              loaderId: 'FOREIGN_LOADER',
              url: 'http://foreign.test/',
            },
          },
        ],
      };

      browserClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('POPUP_TAB_SESSION', 'POPUP_TAB_TARGET', 'tab'),
      );
      connection
        .getCdpClient('POPUP_TAB_SESSION')
        .receive(
          'Target.attachedToTarget',
          attachedToTarget('POPUP_SESSION', 'POPUP_FRAME', 'page'),
        );
      await browsingContextStorage
        .getContext('POPUP_FRAME')
        .targetUnblockedOrThrow();

      // Navigating the popup and emitting frameAttached for TOP_FRAME_ID on
      // the popup session must leave TOP_FRAME_ID top-level on PAGE_SESSION_ID.
      popupClient.receive('Page.frameNavigated', {
        frame: {
          id: 'POPUP_FRAME',
          loaderId: 'POPUP_NAV_LOADER',
          url: 'http://popup.test/',
        },
        type: 'Navigation',
      });
      popupClient.receive('Page.frameAttached', {
        frameId: TOP_FRAME_ID,
        parentFrameId: 'POPUP_FRAME',
      });

      assert.strictEqual(
        browsingContextStorage.getContext(TOP_FRAME_ID),
        topContext,
      );
      assert.isNull(topContext.parentId);
      assert.strictEqual(sessionOf(TOP_FRAME_ID), PAGE_SESSION_ID);
      assert.isFalse(browsingContextStorage.hasContext('FOREIGN_CHILD'));
    });

    it('does not re-parent a top-level page context', async () => {
      const cycleClient = connection.getCdpClient('CYCLE_PAGE_SESSION');
      cycleClient.frameTree = {
        frame: {
          id: 'CYCLE_TOP_FRAME',
          parentId: 'CYCLE_CHILD_FRAME',
          loaderId: 'CYCLE_LOADER',
          url: '',
        },
        childFrames: [
          {
            frame: {
              id: 'CYCLE_CHILD_FRAME',
              parentId: 'CYCLE_TOP_FRAME',
              loaderId: 'CYCLE_CHILD_LOADER',
              url: 'http://child.test/',
            },
          },
        ],
      };

      browserClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('CYCLE_TAB_SESSION', 'CYCLE_TAB_TARGET', 'tab'),
      );
      connection
        .getCdpClient('CYCLE_TAB_SESSION')
        .receive(
          'Target.attachedToTarget',
          attachedToTarget('CYCLE_PAGE_SESSION', 'CYCLE_TOP_FRAME', 'page'),
        );
      await browsingContextStorage
        .getContext('CYCLE_TOP_FRAME')
        .targetUnblockedOrThrow();

      const cycleTop = browsingContextStorage.getContext('CYCLE_TOP_FRAME');
      const cycleChild = browsingContextStorage.getContext('CYCLE_CHILD_FRAME');
      assert.isNull(cycleTop.parentId);
      assert.strictEqual(cycleChild.parentId, 'CYCLE_TOP_FRAME');
      assert.strictEqual(cycleChild.top, cycleTop);
    });

    it('does not create a parent cycle between OOPIF contexts', async () => {
      const oopifAId = 'OOPIF_A_FRAME';
      const oopifASessionId = 'OOPIF_A_SESSION';
      const oopifBId = 'OOPIF_B_FRAME';
      const oopifBSessionId = 'OOPIF_B_SESSION';
      const oopifAClient = connection.getCdpClient(oopifASessionId);
      const oopifBClient = connection.getCdpClient(oopifBSessionId);
      oopifAClient.frameTree = {
        frame: {
          id: oopifAId,
          parentId: oopifBId,
          loaderId: 'OOPIF_A_LOADER',
          url: '',
        },
      };
      oopifBClient.frameTree = {
        frame: {
          id: oopifBId,
          parentId: oopifAId,
          loaderId: 'OOPIF_B_LOADER',
          url: '',
        },
      };

      pageClient.receive(
        'Target.attachedToTarget',
        attachedToTarget(oopifASessionId, oopifAId, 'iframe'),
      );
      oopifAClient.receive(
        'Target.attachedToTarget',
        attachedToTarget(oopifBSessionId, oopifBId, 'iframe'),
      );
      await browsingContextStorage
        .getContext(oopifAId)
        .targetUnblockedOrThrow();
      await browsingContextStorage
        .getContext(oopifBId)
        .targetUnblockedOrThrow();

      const oopifAContext = browsingContextStorage.getContext(oopifAId);
      // Direct setter guard also refuses cycles if reached directly.
      oopifAContext.parentId = oopifBId;
      assert.isTrue(oopifAContext.isTopLevelContext());
      assert.strictEqual(
        browsingContextStorage.getContext(oopifBId).top,
        oopifAContext,
      );
    });

    it('restores nested same-process frames and OOPIF parent on reconnect', async () => {
      const reconnectPageClient = connection.getCdpClient(
        'RECONNECT_PAGE_SESSION',
      );
      reconnectPageClient.frameTree = {
        frame: {
          id: 'RECONNECT_TOP_FRAME',
          loaderId: 'RECONNECT_TOP_LOADER',
          url: 'http://reconnect.test/',
        },
        childFrames: [
          {
            frame: {
              id: 'RECONNECT_CHILD_FRAME',
              parentId: 'RECONNECT_TOP_FRAME',
              loaderId: 'RECONNECT_CHILD_LOADER',
              url: 'http://reconnect.test/child',
            },
            childFrames: [
              {
                frame: {
                  id: 'RECONNECT_GRANDCHILD_FRAME',
                  parentId: 'RECONNECT_CHILD_FRAME',
                  loaderId: 'RECONNECT_GRANDCHILD_LOADER',
                  url: 'http://reconnect.test/grandchild',
                },
              },
            ],
          },
        ],
      };

      browserClient.receive(
        'Target.attachedToTarget',
        attachedToTarget(
          'RECONNECT_TAB_SESSION',
          'RECONNECT_TAB_TARGET',
          'tab',
        ),
      );
      connection
        .getCdpClient('RECONNECT_TAB_SESSION')
        .receive(
          'Target.attachedToTarget',
          attachedToTarget(
            'RECONNECT_PAGE_SESSION',
            'RECONNECT_TOP_FRAME',
            'page',
            'http://reconnect.test/',
          ),
        );
      await browsingContextStorage
        .getContext('RECONNECT_TOP_FRAME')
        .targetUnblockedOrThrow();

      assert.strictEqual(
        sessionOf('RECONNECT_CHILD_FRAME'),
        'RECONNECT_PAGE_SESSION',
      );
      assert.strictEqual(
        browsingContextStorage.getContext('RECONNECT_CHILD_FRAME').parentId,
        'RECONNECT_TOP_FRAME',
      );
      assert.strictEqual(
        sessionOf('RECONNECT_GRANDCHILD_FRAME'),
        'RECONNECT_PAGE_SESSION',
      );
      assert.strictEqual(
        browsingContextStorage.getContext('RECONNECT_GRANDCHILD_FRAME')
          .parentId,
        'RECONNECT_CHILD_FRAME',
      );

      // Attach an existing OOPIF on the embedding page session when its parent
      // is a nested same-process frame discovered via Page.getFrameTree.
      const reconnectOopifClient = connection.getCdpClient(
        'RECONNECT_OOPIF_SESSION',
      );
      reconnectOopifClient.frameTree = {
        frame: {
          id: 'RECONNECT_OOPIF_FRAME',
          parentId: 'RECONNECT_CHILD_FRAME',
          loaderId: 'RECONNECT_OOPIF_LOADER',
          url: OOPIF_URL,
        },
      };
      reconnectPageClient.receive(
        'Target.attachedToTarget',
        attachedToTarget(
          'RECONNECT_OOPIF_SESSION',
          'RECONNECT_OOPIF_FRAME',
          'iframe',
          OOPIF_URL,
        ),
      );
      await browsingContextStorage
        .getContext('RECONNECT_OOPIF_FRAME')
        .targetUnblockedOrThrow();

      assert.strictEqual(
        sessionOf('RECONNECT_OOPIF_FRAME'),
        'RECONNECT_OOPIF_SESSION',
      );
      assert.strictEqual(
        browsingContextStorage.getContext('RECONNECT_OOPIF_FRAME').parentId,
        'RECONNECT_CHILD_FRAME',
      );

      // Attach an OOPIF carrying `targetInfo.parentFrameId`.
      reconnectPageClient.receive(
        'Target.attachedToTarget',
        attachedToTarget(
          'RECONNECT_OOPIF_2_SESSION',
          'RECONNECT_OOPIF_2_FRAME',
          'iframe',
          OOPIF_URL,
          'RECONNECT_GRANDCHILD_FRAME',
        ),
      );
      assert.strictEqual(
        browsingContextStorage.getContext('RECONNECT_OOPIF_2_FRAME').parentId,
        'RECONNECT_GRANDCHILD_FRAME',
      );
    });

    it('ignores a parent frame not hosted by the parent session of an iframe target', async () => {
      // Remove OTHER_TOP_FRAME_ID so that an iframe target attached on
      // OTHER_PAGE_SESSION_ID starts with parentId === null.
      otherPageClient.receive('Page.frameSubtreeWillBeDetached', {
        frameId: OTHER_TOP_FRAME_ID,
      });

      const nestedOopifClient = connection.getCdpClient('NESTED_OOPIF_SESSION');
      nestedOopifClient.frameTree = {
        frame: {
          id: 'NESTED_OOPIF_FRAME',
          parentId: TOP_FRAME_ID,
          loaderId: 'NESTED_OOPIF_LOADER',
          url: OOPIF_URL,
        },
      };

      otherPageClient.receive(
        'Target.attachedToTarget',
        attachedToTarget(
          'NESTED_OOPIF_SESSION',
          'NESTED_OOPIF_FRAME',
          'iframe',
          OOPIF_URL,
        ),
      );
      await browsingContextStorage
        .getContext('NESTED_OOPIF_FRAME')
        .targetUnblockedOrThrow();

      assert.isNull(
        browsingContextStorage.getContext('NESTED_OOPIF_FRAME').parentId,
      );
      assert.notInclude(
        browsingContextStorage
          .getContext(TOP_FRAME_ID)
          .directChildren.map((c) => c.id),
        'NESTED_OOPIF_FRAME',
      );
    });

    it('waits for the embedding target to restore nested frames before setting OOPIF parent on reconnect', async () => {
      let resolvePageFrameTree!: (frameTree: object) => void;
      const slowPageClient = connection.getCdpClient('SLOW_PAGE_SESSION');
      slowPageClient.frameTreePromise = new Promise((resolve) => {
        resolvePageFrameTree = resolve;
      });

      const slowOopifClient = connection.getCdpClient('SLOW_OOPIF_SESSION');
      slowOopifClient.frameTree = {
        frame: {
          id: 'SLOW_OOPIF_FRAME',
          parentId: 'SLOW_CHILD_FRAME',
          loaderId: 'SLOW_OOPIF_LOADER',
          url: OOPIF_URL,
        },
      };

      browserClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('SLOW_TAB_SESSION', 'SLOW_TAB_TARGET', 'tab'),
      );
      connection
        .getCdpClient('SLOW_TAB_SESSION')
        .receive(
          'Target.attachedToTarget',
          attachedToTarget(
            'SLOW_PAGE_SESSION',
            'SLOW_TOP_FRAME',
            'page',
            'http://slow.test/',
          ),
        );
      slowPageClient.receive(
        'Target.attachedToTarget',
        attachedToTarget(
          'SLOW_OOPIF_SESSION',
          'SLOW_OOPIF_FRAME',
          'iframe',
          OOPIF_URL,
          'SLOW_CHILD_FRAME',
        ),
      );

      // Let the OOPIF's fast `Page.getFrameTree` settle before the page replies.
      await Promise.resolve();
      resolvePageFrameTree({
        frame: {
          id: 'SLOW_TOP_FRAME',
          loaderId: 'SLOW_TOP_LOADER',
          url: 'http://slow.test/',
        },
        childFrames: [
          {
            frame: {
              id: 'SLOW_CHILD_FRAME',
              parentId: 'SLOW_TOP_FRAME',
              loaderId: 'SLOW_CHILD_LOADER',
              url: 'http://slow.test/child',
            },
          },
        ],
      });

      await browsingContextStorage
        .getContext('SLOW_OOPIF_FRAME')
        .targetUnblockedOrThrow();
      await browsingContextStorage
        .getContext('SLOW_TOP_FRAME')
        .targetUnblockedOrThrow();

      assert.strictEqual(
        browsingContextStorage.getContext('SLOW_OOPIF_FRAME').parentId,
        'SLOW_CHILD_FRAME',
      );
      assert.notInclude(
        browsingContextStorage.getTopLevelContexts().map((c) => c.id),
        'SLOW_OOPIF_FRAME',
      );
    });
  });

  describe('Target.attachedToTarget', () => {
    it('does not rebind an existing context from a non-owning session', () => {
      otherPageClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('FOREIGN_SESSION', CHILD_FRAME_ID, 'iframe'),
      );
      otherPageClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('FOREIGN_PAGE_SESSION', CHILD_FRAME_ID, 'page'),
      );

      assert.strictEqual(sessionOf(CHILD_FRAME_ID), PAGE_SESSION_ID);
      assert.strictEqual(
        browsingContextStorage.getContext(CHILD_FRAME_ID).parentId,
        TOP_FRAME_ID,
      );

      // The owning session can still transition the frame to an OOPIF target.
      pageClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('CHILD_OOPIF_SESSION', CHILD_FRAME_ID, 'iframe'),
      );
      assert.strictEqual(sessionOf(CHILD_FRAME_ID), 'CHILD_OOPIF_SESSION');
    });

    it('disposes same-process descendants of a frame that moved to its own target', () => {
      pageClient.receive('Page.frameAttached', {
        frameId: 'NESTED_FRAME',
        parentFrameId: CHILD_FRAME_ID,
      });
      pageClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('CHILD_OOPIF_SESSION', CHILD_FRAME_ID, 'iframe'),
      );
      connection
        .getCdpClient('CHILD_OOPIF_SESSION')
        .receive('Page.frameNavigated', {
          frame: {
            id: CHILD_FRAME_ID,
            parentId: TOP_FRAME_ID,
            loaderId: 'NEW_LOADER',
            url: OOPIF_URL,
          },
          type: 'Navigation',
        });

      assert.isFalse(browsingContextStorage.hasContext('NESTED_FRAME'));
    });

    it('replaces a renderer-claimed context when a browser-only page target attaches', () => {
      pageClient.receive('Page.frameAttached', {
        frameId: 'POPUP_FRAME',
        parentFrameId: TOP_FRAME_ID,
      });
      assert.strictEqual(sessionOf('POPUP_FRAME'), PAGE_SESSION_ID);

      browserClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('POPUP_TAB_SESSION', 'POPUP_TAB_TARGET', 'tab'),
      );
      connection
        .getCdpClient('POPUP_TAB_SESSION')
        .receive(
          'Target.attachedToTarget',
          attachedToTarget('POPUP_PAGE_SESSION', 'POPUP_FRAME', 'page'),
        );

      const popupContext = browsingContextStorage.getContext('POPUP_FRAME');
      assert.strictEqual(sessionOf('POPUP_FRAME'), 'POPUP_PAGE_SESSION');
      assert.isNull(popupContext.parentId);
      assert.notInclude(
        browsingContextStorage
          .getContext(TOP_FRAME_ID)
          .directChildren.map((c) => c.id),
        'POPUP_FRAME',
      );
    });

    it('ignores forged attaches on renderer sessions without blocking browser-only attaches', () => {
      pageClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('FORGED_TAB_SESSION', 'FORGED_TAB', 'tab'),
      );
      pageClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('FORGED_PAGE_SESSION', 'POPUP_FRAME', 'page'),
      );
      pageClient.receive(
        'Target.attachedToTarget',
        attachedToTarget(
          'FORGED_OTHER_SESSION',
          'DEVTOOLS_TARGET',
          'other',
          'devtools://devtools/bundled/inspector.html',
        ),
      );
      pageClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('FORGED_IFRAME_SESSION', 'POPUP_FRAME', 'iframe'),
      );

      assert.isFalse(browsingContextStorage.hasContext('DEVTOOLS_TARGET'));

      browserClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('POPUP_TAB_SESSION', 'POPUP_TAB_TARGET', 'tab'),
      );
      connection
        .getCdpClient('POPUP_TAB_SESSION')
        .receive(
          'Target.attachedToTarget',
          attachedToTarget('POPUP_PAGE_SESSION', 'POPUP_FRAME', 'page'),
        );

      assert.strictEqual(sessionOf('POPUP_FRAME'), 'POPUP_PAGE_SESSION');
      assert.isNull(browsingContextStorage.getContext('POPUP_FRAME').parentId);
    });
  });

  describe('tab Target.setAutoAttach errors', () => {
    it('suppresses CloseError when tab detaches while Target.setAutoAttach is in flight', async () => {
      connection.getCdpClient('CLOSING_TAB_SESSION').setAutoAttachError =
        new CloseError('Tab detached');

      browserClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('CLOSING_TAB_SESSION', 'CLOSING_TAB_TARGET', 'tab'),
      );
      await new Promise((resolve) => setTimeout(resolve, 0));

      sinon.assert.notCalled(debugErrorLog);
    });

    it('logs unexpected Target.setAutoAttach errors via debugError', async () => {
      const unexpectedError = new Error('CDP failure');
      connection.getCdpClient('FAILING_TAB_SESSION').setAutoAttachError =
        unexpectedError;

      browserClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('FAILING_TAB_SESSION', 'FAILING_TAB_TARGET', 'tab'),
      );
      await new Promise((resolve) => setTimeout(resolve, 0));

      sinon.assert.calledOnceWithExactly(debugErrorLog, unexpectedError);
    });
  });

  describe('popup configuration before resume', () => {
    it('applies user-agent override and Fetch.enable before resuming popup and detaching opener session', async () => {
      await configStorage.updateConfig(
        {userAgent: 'custom-popup-ua'},
        undefined,
        ['default'],
        false,
      );
      await eventManager.subscribe(
        [ChromiumBidi.Network.EventNames.BeforeRequestSent],
        [],
        [],
        null,
      );
      networkStorage.addIntercept({
        phases: [Network.InterceptPhase.BeforeRequestSent],
        urlPatterns: [
          {
            protocol: 'http',
            hostname: 'example.com',
            pathname: '/popup.html',
          },
        ],
      });

      const popupTabClient = connection.getCdpClient('POPUP_TAB_SESSION');
      const popupPageClient = connection.getCdpClient('POPUP_PAGE_SESSION');
      const openerPopupClient = connection.getCdpClient('OPENER_POPUP_SESSION');

      const uaDeferred = new Deferred<object>();
      const fetchDeferred = new Deferred<object>();
      const popupCommands: string[] = [];
      const tabCommands: string[] = [];
      const openerCommands: string[] = [];
      const parentPageCommands: string[] = [];

      const origPopupSend = popupPageClient.sendCommand.bind(popupPageClient);
      sinon.stub(popupPageClient, 'sendCommand').callsFake((method) => {
        popupCommands.push(method);
        if (method === 'Emulation.setUserAgentOverride') {
          return uaDeferred as unknown as Promise<never>;
        }
        if (method === 'Fetch.enable') {
          return fetchDeferred as unknown as Promise<never>;
        }
        return origPopupSend(method);
      });

      const origTabSend = popupTabClient.sendCommand.bind(popupTabClient);
      sinon.stub(popupTabClient, 'sendCommand').callsFake((method) => {
        tabCommands.push(method);
        return origTabSend(method);
      });

      const origOpenerSend =
        openerPopupClient.sendCommand.bind(openerPopupClient);
      sinon.stub(openerPopupClient, 'sendCommand').callsFake((method) => {
        openerCommands.push(method);
        return origOpenerSend(method);
      });

      const origPageSend = pageClient.sendCommand.bind(pageClient);
      sinon.stub(pageClient, 'sendCommand').callsFake((method) => {
        parentPageCommands.push(method);
        return origPageSend(method);
      });

      // Opener attaches popup page session first while paused for debugger.
      pageClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('OPENER_POPUP_SESSION', 'POPUP_TARGET', 'page'),
      );
      await new Promise((resolve) => setTimeout(resolve, 0));
      assert.notInclude(openerCommands, 'Runtime.runIfWaitingForDebugger');
      assert.notInclude(parentPageCommands, 'Target.detachFromTarget');

      // Browser attaches tab and page sessions for the popup.
      browserClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('POPUP_TAB_SESSION', 'POPUP_TAB_TARGET', 'tab'),
      );
      popupTabClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('POPUP_PAGE_SESSION', 'POPUP_TARGET', 'page'),
      );
      await new Promise((resolve) => setTimeout(resolve, 0));

      assert.include(popupCommands, 'Emulation.setUserAgentOverride');
      assert.include(popupCommands, 'Fetch.enable');
      assert.notInclude(popupCommands, 'Runtime.runIfWaitingForDebugger');
      assert.notInclude(tabCommands, 'Runtime.runIfWaitingForDebugger');
      assert.notInclude(openerCommands, 'Runtime.runIfWaitingForDebugger');
      assert.notInclude(parentPageCommands, 'Target.detachFromTarget');

      // Resolving Fetch.enable resumes the target and detaches the opener session
      // even while renderer-routed commands remain suspended during navigation.
      fetchDeferred.resolve({});
      await new Promise((resolve) => setTimeout(resolve, 0));

      assert.include(popupCommands, 'Runtime.runIfWaitingForDebugger');
      assert.include(tabCommands, 'Runtime.runIfWaitingForDebugger');
      assert.include(openerCommands, 'Runtime.runIfWaitingForDebugger');
      assert.include(parentPageCommands, 'Target.detachFromTarget');

      const popupTarget =
        browsingContextStorage.getContext('POPUP_TARGET').cdpTarget;
      assert.isFalse(popupTarget.unblocked.isFinished);

      // A non-matching Fetch.requestPaused is auto-continued and does not unblock early.
      popupPageClient.receive('Fetch.requestPaused', {
        requestId: 'FETCH_NON_MATCHING',
        networkId: 'NET_NON_MATCHING',
        request: {
          url: 'http://example.com/other.html',
          method: 'GET',
          headers: {},
          initialPriority: 'VeryHigh',
          referrerPolicy: 'strict-origin-when-cross-origin',
        },
        frameId: 'POPUP_TARGET',
        resourceType: 'Document',
      });
      await new Promise((resolve) => setTimeout(resolve, 0));
      assert.isFalse(popupTarget.unblocked.isFinished);

      // Pausing a matching request in Fetch unblocks the target before renderer responses settle.
      popupPageClient.receive('Fetch.requestPaused', {
        requestId: 'FETCH_1',
        networkId: 'NET_1',
        request: {
          url: 'http://example.com/popup.html',
          method: 'GET',
          headers: {},
          initialPriority: 'VeryHigh',
          referrerPolicy: 'strict-origin-when-cross-origin',
        },
        frameId: 'POPUP_TARGET',
        resourceType: 'Document',
      });
      await browsingContextStorage
        .getContext('POPUP_TARGET')
        .targetUnblockedOrThrow();

      uaDeferred.resolve({});
    });

    it('waits for popup resume before detaching opener session when browser attaches first', async () => {
      networkStorage.addIntercept({
        phases: [Network.InterceptPhase.BeforeRequestSent],
        urlPatterns: [],
      });

      const popupTabClient = connection.getCdpClient('POPUP_TAB_SESSION');
      const popupPageClient = connection.getCdpClient('POPUP_PAGE_SESSION');
      const openerPopupClient = connection.getCdpClient('OPENER_POPUP_SESSION');

      const fetchDeferred = new Deferred<object>();
      const openerCommands: string[] = [];
      const parentPageCommands: string[] = [];

      const origPopupSend = popupPageClient.sendCommand.bind(popupPageClient);
      sinon.stub(popupPageClient, 'sendCommand').callsFake((method) => {
        if (method === 'Fetch.enable') {
          return fetchDeferred as unknown as Promise<never>;
        }
        return origPopupSend(method);
      });
      const origOpenerSend =
        openerPopupClient.sendCommand.bind(openerPopupClient);
      sinon.stub(openerPopupClient, 'sendCommand').callsFake((method) => {
        openerCommands.push(method);
        return origOpenerSend(method);
      });
      const origPageSend = pageClient.sendCommand.bind(pageClient);
      sinon.stub(pageClient, 'sendCommand').callsFake((method) => {
        parentPageCommands.push(method);
        return origPageSend(method);
      });

      // Browser attaches popup first while Fetch.enable is still pending.
      browserClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('POPUP_TAB_SESSION', 'POPUP_TAB_TARGET', 'tab'),
      );
      popupTabClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('POPUP_PAGE_SESSION', 'POPUP_TARGET', 'page'),
      );
      // Opener attaches popup page session while browser target has not resumed yet.
      pageClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('OPENER_POPUP_SESSION', 'POPUP_TARGET', 'page'),
      );
      await new Promise((resolve) => setTimeout(resolve, 0));

      assert.notInclude(openerCommands, 'Runtime.runIfWaitingForDebugger');
      assert.notInclude(parentPageCommands, 'Target.detachFromTarget');

      fetchDeferred.resolve({});
      await new Promise((resolve) => setTimeout(resolve, 0));

      assert.include(openerCommands, 'Runtime.runIfWaitingForDebugger');
      assert.include(parentPageCommands, 'Target.detachFromTarget');
    });

    it('does not let a forged iframe attach drain deferred opener detach', async () => {
      const openerPopupClient = connection.getCdpClient('OPENER_POPUP_SESSION');
      const openerCommands: string[] = [];
      const parentPageCommands: string[] = [];

      const origOpenerSend =
        openerPopupClient.sendCommand.bind(openerPopupClient);
      sinon.stub(openerPopupClient, 'sendCommand').callsFake((method) => {
        openerCommands.push(method);
        return origOpenerSend(method);
      });
      const origPageSend = pageClient.sendCommand.bind(pageClient);
      sinon.stub(pageClient, 'sendCommand').callsFake((method) => {
        parentPageCommands.push(method);
        return origPageSend(method);
      });

      pageClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('OPENER_POPUP_SESSION', 'POPUP_TARGET', 'page'),
      );
      pageClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('FORGED_IFRAME_SESSION', 'POPUP_TARGET', 'iframe'),
      );
      await new Promise((resolve) => setTimeout(resolve, 0));

      assert.notInclude(openerCommands, 'Runtime.runIfWaitingForDebugger');
      assert.notInclude(parentPageCommands, 'Target.detachFromTarget');

      // Browser-only page attach drains the deferred detach once resumed.
      browserClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('POPUP_TAB_SESSION', 'POPUP_TAB_TARGET', 'tab'),
      );
      connection
        .getCdpClient('POPUP_TAB_SESSION')
        .receive(
          'Target.attachedToTarget',
          attachedToTarget('POPUP_PAGE_SESSION', 'POPUP_TARGET', 'page'),
        );
      await new Promise((resolve) => setTimeout(resolve, 0));

      assert.include(openerCommands, 'Runtime.runIfWaitingForDebugger');
      assert.include(parentPageCommands, 'Target.detachFromTarget');
    });

    it('cleans up deferred opener detach if opener session detaches before browser attach', async () => {
      const openerPopupClient = connection.getCdpClient('OPENER_POPUP_SESSION');
      const openerCommands: string[] = [];
      const parentPageCommands: string[] = [];

      const origOpenerSend =
        openerPopupClient.sendCommand.bind(openerPopupClient);
      sinon.stub(openerPopupClient, 'sendCommand').callsFake((method) => {
        openerCommands.push(method);
        return origOpenerSend(method);
      });
      const origPageSend = pageClient.sendCommand.bind(pageClient);
      sinon.stub(pageClient, 'sendCommand').callsFake((method) => {
        parentPageCommands.push(method);
        return origPageSend(method);
      });

      pageClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('OPENER_POPUP_SESSION', 'POPUP_TARGET', 'page'),
      );
      pageClient.receive('Target.detachedFromTarget', {
        sessionId: 'OPENER_POPUP_SESSION',
        targetId: 'POPUP_TARGET',
      });

      browserClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('POPUP_TAB_SESSION', 'POPUP_TAB_TARGET', 'tab'),
      );
      connection
        .getCdpClient('POPUP_TAB_SESSION')
        .receive(
          'Target.attachedToTarget',
          attachedToTarget('POPUP_PAGE_SESSION', 'POPUP_TARGET', 'page'),
        );
      await new Promise((resolve) => setTimeout(resolve, 0));

      assert.notInclude(openerCommands, 'Runtime.runIfWaitingForDebugger');
      assert.notInclude(parentPageCommands, 'Target.detachFromTarget');
    });

    it('does not dispose popup preload scripts when opener popup session detaches', async () => {
      const preloadScript = new PreloadScript({
        functionDeclaration: '() => {}',
      });
      preloadScriptStorage.add(preloadScript);

      pageClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('OPENER_POPUP_SESSION', 'POPUP_TARGET', 'page'),
      );
      browserClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('POPUP_TAB_SESSION', 'POPUP_TAB_TARGET', 'tab'),
      );
      connection
        .getCdpClient('POPUP_TAB_SESSION')
        .receive(
          'Target.attachedToTarget',
          attachedToTarget('POPUP_PAGE_SESSION', 'POPUP_TARGET', 'page'),
        );
      await browsingContextStorage
        .getContext('POPUP_TARGET')
        .targetUnblockedOrThrow();

      assert.isTrue(preloadScript.targetIds.has('POPUP_TARGET'));

      // Chromium emits detachedFromTarget for the detached opener session while
      // POPUP_PAGE_SESSION remains alive.
      pageClient.receive('Target.detachedFromTarget', {
        sessionId: 'OPENER_POPUP_SESSION',
        targetId: 'POPUP_TARGET',
      });

      assert.isTrue(preloadScript.targetIds.has('POPUP_TARGET'));
      assert.isTrue(browsingContextStorage.hasContext('POPUP_TARGET'));
    });

    it('suppresses CloseError and logs unexpected errors during CdpTarget.unblock', async () => {
      const closingPageClient = connection.getCdpClient('CLOSING_PAGE_SESSION');
      sinon
        .stub(closingPageClient, 'sendCommand')
        .rejects(new CloseError('Target closed'));

      browserClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('CLOSING_TAB_SESSION', 'CLOSING_TAB_TARGET', 'tab'),
      );
      connection
        .getCdpClient('CLOSING_TAB_SESSION')
        .receive(
          'Target.attachedToTarget',
          attachedToTarget(
            'CLOSING_PAGE_SESSION',
            'CLOSING_PAGE_TARGET',
            'page',
          ),
        );
      await browsingContextStorage
        .getContext('CLOSING_PAGE_TARGET')
        .targetUnblockedOrThrow();
      sinon.assert.notCalled(debugErrorLog);

      const unexpectedError = new Error('Unexpected CDP failure');
      const failingPageClient = connection.getCdpClient('FAILING_PAGE_SESSION');
      const origFailingSend =
        failingPageClient.sendCommand.bind(failingPageClient);
      sinon.stub(failingPageClient, 'sendCommand').callsFake((method) => {
        if (method === 'Runtime.enable') {
          return Promise.reject(unexpectedError);
        }
        return origFailingSend(method);
      });

      browserClient.receive(
        'Target.attachedToTarget',
        attachedToTarget('FAILING_TAB_SESSION', 'FAILING_TAB_TARGET', 'tab'),
      );
      connection
        .getCdpClient('FAILING_TAB_SESSION')
        .receive(
          'Target.attachedToTarget',
          attachedToTarget(
            'FAILING_PAGE_SESSION',
            'FAILING_PAGE_TARGET',
            'page',
          ),
        );
      await browsingContextStorage
        .getContext('FAILING_PAGE_TARGET')
        .targetUnblockedOrThrow();
      sinon.assert.calledWith(
        debugErrorLog,
        'Error happened when configuring a new target',
        unexpectedError,
      );
    });
  });
});
