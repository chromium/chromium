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

import type {CdpClient, CdpEvents} from '../../../cdp/CdpClient.js';
import type {CdpConnection} from '../../../cdp/CdpConnection.js';
import {EventEmitter} from '../../../utils/EventEmitter.js';
import type {BluetoothProcessor} from '../bluetooth/BluetoothProcessor.js';
import {ContextConfigStorage} from '../browser/ContextConfigStorage.js';
import {UserContextStorage} from '../browser/UserContextStorage.js';
import {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';
import {NetworkStorage} from '../network/NetworkStorage.js';
import {PreloadScriptStorage} from '../script/PreloadScriptStorage.js';
import {RealmStorage} from '../script/RealmStorage.js';
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
    default:
      return {frame: {id: 'UNKNOWN_FRAME', loaderId: 'UNKNOWN', url: ''}};
  }
}

/** Fake CDP session which records nothing and answers every command. */
class FakeCdpClient extends EventEmitter<CdpEvents> implements CdpClient {
  readonly sessionId: Protocol.Target.SessionID | undefined;

  constructor(sessionId: Protocol.Target.SessionID | undefined) {
    super();
    this.sessionId = sessionId;
  }

  sendCommand<CdpMethod extends keyof ProtocolMapping.Commands>(
    method: CdpMethod,
  ): Promise<ProtocolMapping.Commands[CdpMethod]['returnType']> {
    let result: object = {};
    if (method === 'Page.getFrameTree') {
      result = {frameTree: frameTreeForSession(this.sessionId)};
    } else if (method === 'Browser.getWindowForTarget') {
      result = {windowId: 1, bounds: {}};
    }
    return Promise.resolve(
      result as ProtocolMapping.Commands[CdpMethod]['returnType'],
    );
  }

  isCloseError(): boolean {
    return false;
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

  beforeEach(() => {
    connection = new FakeCdpConnection();
    browserClient = new FakeCdpClient(undefined);
    browsingContextStorage = new BrowsingContextStorage();
    realmStorage = new RealmStorage();
    const userContextStorage = new UserContextStorage(browserClient);
    const eventManager = new EventManager(
      browsingContextStorage,
      userContextStorage,
    );
    new CdpTargetManager(
      connection,
      browserClient,
      SELF_TARGET_ID,
      eventManager,
      browsingContextStorage,
      realmStorage,
      new NetworkStorage(eventManager, browsingContextStorage, browserClient),
      new ContextConfigStorage(browsingContextStorage, userContextStorage),
      targetCreatedObserverStub as unknown as BluetoothProcessor,
      targetCreatedObserverStub as unknown as SpeculationProcessor,
      new PreloadScriptStorage(),
      DEFAULT_BROWSER_CONTEXT_ID,
      'Fake User Agent',
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
});
