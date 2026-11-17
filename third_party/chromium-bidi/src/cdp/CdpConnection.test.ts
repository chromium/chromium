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

import {describe, it} from 'node:test';
import {assert} from 'chai';
import sinon from 'sinon';

import {StubTransport} from '../utils/transportStub.test.js';

import {CloseError} from './CdpClient.js';
import {MapperCdpConnection} from './CdpConnection.js';

const SOME_SESSION_ID = 'ABCD';
const ANOTHER_SESSION_ID = 'EFGH';
const THIRD_SESSION_ID = 'IJKL';

describe('CdpConnection', () => {
  it('can send a command message for a CdpClient', async () => {
    const mockCdpServer = new StubTransport();
    const cdpConnection = new MapperCdpConnection(mockCdpServer);

    const browserMessage = JSON.stringify({
      id: 0,
      method: 'Browser.getVersion',
      sessionId: SOME_SESSION_ID,
    });

    // Create a client for the session.
    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.attachedToTarget',
      params: {sessionId: SOME_SESSION_ID},
    });

    void cdpConnection
      .getCdpClient(SOME_SESSION_ID)
      .sendCommand('Browser.getVersion');

    sinon.assert.calledOnceWithExactly(
      mockCdpServer.sendMessage,
      browserMessage,
    );
  });

  it('creates a CdpClient for a session when the Target.attachedToTarget event is received', async () => {
    const mockCdpServer = new StubTransport();
    const cdpConnection = new MapperCdpConnection(mockCdpServer);

    assert.throws(
      () => cdpConnection.getCdpClient(SOME_SESSION_ID),
      'Unknown CDP session ID',
    );

    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.attachedToTarget',
      params: {sessionId: SOME_SESSION_ID},
    });

    const cdpClient = cdpConnection.getCdpClient(SOME_SESSION_ID);
    assert.isNotNull(cdpClient);
  });

  it('removes the CdpClient for a session when the Target.detachedFromTarget event is received', async () => {
    const mockCdpServer = new StubTransport();
    const cdpConnection = new MapperCdpConnection(mockCdpServer);

    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.attachedToTarget',
      params: {sessionId: SOME_SESSION_ID},
    });

    const cdpClient = cdpConnection.getCdpClient(SOME_SESSION_ID);
    assert.isNotNull(cdpClient);

    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.detachedFromTarget',
      params: {sessionId: SOME_SESSION_ID},
    });

    assert.throws(
      () => cdpConnection.getCdpClient(SOME_SESSION_ID),
      'Unknown CDP session ID',
    );
  });

  it('routes event messages to the correct handler based on sessionId', async () => {
    const mockCdpServer = new StubTransport();
    const cdpConnection = new MapperCdpConnection(mockCdpServer);

    const sessionMessage = {
      sessionId: SOME_SESSION_ID,
      method: 'Page.frameNavigated',
    };
    const otherSessionMessage = {
      sessionId: ANOTHER_SESSION_ID,
      method: 'Page.loadEventFired',
    };

    const sessionCallback = sinon.fake();
    const otherSessionCallback = sinon.fake();

    // Attach session A.
    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.attachedToTarget',
      params: {sessionId: SOME_SESSION_ID},
    });

    const sessionClient = cdpConnection.getCdpClient(SOME_SESSION_ID);
    assert.isNotNull(sessionClient);
    sessionClient.on('Page.frameNavigated', sessionCallback);

    // Send a message for session A and verify that it is received.
    await mockCdpServer.emulateIncomingMessage(sessionMessage);
    sinon.assert.calledOnceWithExactly(sessionCallback, {});
    sessionCallback.resetHistory();

    // Attach session B.
    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.attachedToTarget',
      params: {sessionId: ANOTHER_SESSION_ID},
    });

    const otherSessionClient = cdpConnection.getCdpClient(ANOTHER_SESSION_ID);
    assert.isNotNull(otherSessionClient);
    otherSessionClient.on('Page.loadEventFired', otherSessionCallback);

    // Send a message for session B and verify that only the session B callback receives it.
    // Verifies that a message is sent only to the session client it is intended for.
    await mockCdpServer.emulateIncomingMessage(otherSessionMessage);
    sinon.assert.notCalled(sessionCallback);
    sinon.assert.calledOnceWithExactly(otherSessionCallback, {});
    otherSessionCallback.resetHistory();
  });

  it('closes the transport idempotently, clears session listeners, and ignores late messages', async () => {
    const mockCdpServer = new StubTransport();
    const cdpConnection = new MapperCdpConnection(mockCdpServer);

    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.attachedToTarget',
      params: {sessionId: SOME_SESSION_ID},
    });
    const sessionClient = cdpConnection.getCdpClient(SOME_SESSION_ID);
    const sessionCallback = sinon.fake();
    sessionClient.on('Page.frameNavigated', sessionCallback);

    // Resolve `Target.attachToBrowserTarget` synchronously, then close the
    // connection before `createBrowserSession`'s microtask continuation runs.
    const createSessionError = cdpConnection.createBrowserSession().then(
      () => null,
      (error: unknown) => error,
    );
    void mockCdpServer.emulateIncomingMessage({
      id: 0,
      result: {sessionId: ANOTHER_SESSION_ID},
    });
    cdpConnection.close();
    cdpConnection.close();

    sinon.assert.calledOnce(mockCdpServer.close);
    assert.instanceOf(await createSessionError, CloseError);
    assert.throws(
      () => cdpConnection.getCdpClient(ANOTHER_SESSION_ID),
      'Unknown CDP session ID',
    );

    sessionClient.emit('Page.frameNavigated', {} as never);
    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.attachedToTarget',
      params: {sessionId: THIRD_SESSION_ID},
    });
    sinon.assert.notCalled(sessionCallback);
    assert.throws(
      () => cdpConnection.getCdpClient(THIRD_SESSION_ID),
      'Unknown CDP session ID',
    );
  });

  it('ignores Target.attachedToTarget reusing a known session ID', async () => {
    const mockCdpServer = new StubTransport();
    const cdpConnection = new MapperCdpConnection(mockCdpServer);

    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.attachedToTarget',
      params: {sessionId: SOME_SESSION_ID},
    });
    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.attachedToTarget',
      params: {sessionId: ANOTHER_SESSION_ID},
    });
    const someSessionClient = cdpConnection.getCdpClient(SOME_SESSION_ID);
    const anotherSessionCallback = sinon.fake();
    cdpConnection
      .getCdpClient(ANOTHER_SESSION_ID)
      .on('Target.attachedToTarget', anotherSessionCallback);

    // A session announces a child session reusing an existing session ID.
    await mockCdpServer.emulateIncomingMessage({
      sessionId: ANOTHER_SESSION_ID,
      method: 'Target.attachedToTarget',
      params: {sessionId: SOME_SESSION_ID},
    });

    assert.strictEqual(
      cdpConnection.getCdpClient(SOME_SESSION_ID),
      someSessionClient,
    );
    sinon.assert.notCalled(anotherSessionCallback);
  });

  it('ignores Target.detachedFromTarget from a session that does not own the detached session', async () => {
    const mockCdpServer = new StubTransport();
    const cdpConnection = new MapperCdpConnection(mockCdpServer);

    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.attachedToTarget',
      params: {sessionId: SOME_SESSION_ID},
    });
    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.attachedToTarget',
      params: {sessionId: ANOTHER_SESSION_ID},
    });
    const someSessionClient = cdpConnection.getCdpClient(SOME_SESSION_ID);
    const someSessionCallback = sinon.fake();
    someSessionClient.on('Page.frameNavigated', someSessionCallback);
    const anotherSessionCallback = sinon.fake();
    cdpConnection
      .getCdpClient(ANOTHER_SESSION_ID)
      .on('Target.detachedFromTarget', anotherSessionCallback);

    // `SOME_SESSION_ID` was attached through the browser session, not through
    // `ANOTHER_SESSION_ID`.
    await mockCdpServer.emulateIncomingMessage({
      sessionId: ANOTHER_SESSION_ID,
      method: 'Target.detachedFromTarget',
      params: {sessionId: SOME_SESSION_ID},
    });

    sinon.assert.notCalled(anotherSessionCallback);
    assert.strictEqual(
      cdpConnection.getCdpClient(SOME_SESSION_ID),
      someSessionClient,
    );
    await mockCdpServer.emulateIncomingMessage({
      sessionId: SOME_SESSION_ID,
      method: 'Page.frameNavigated',
    });
    sinon.assert.calledOnceWithExactly(someSessionCallback, {});
  });

  it('removes a child session when its owner reports it detached', async () => {
    const mockCdpServer = new StubTransport();
    const cdpConnection = new MapperCdpConnection(mockCdpServer);

    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.attachedToTarget',
      params: {sessionId: SOME_SESSION_ID},
    });
    // `ANOTHER_SESSION_ID` is a child of `SOME_SESSION_ID`.
    await mockCdpServer.emulateIncomingMessage({
      sessionId: SOME_SESSION_ID,
      method: 'Target.attachedToTarget',
      params: {sessionId: ANOTHER_SESSION_ID},
    });

    await mockCdpServer.emulateIncomingMessage({
      sessionId: SOME_SESSION_ID,
      method: 'Target.detachedFromTarget',
      params: {sessionId: ANOTHER_SESSION_ID},
    });

    assert.throws(
      () => cdpConnection.getCdpClient(ANOTHER_SESSION_ID),
      'Unknown CDP session ID',
    );
  });

  it('removes descendant sessions and rejects their pending commands when a parent session detaches', async () => {
    const mockCdpServer = new StubTransport();
    const cdpConnection = new MapperCdpConnection(mockCdpServer);

    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.attachedToTarget',
      params: {sessionId: SOME_SESSION_ID},
    });
    await mockCdpServer.emulateIncomingMessage({
      sessionId: SOME_SESSION_ID,
      method: 'Target.attachedToTarget',
      params: {sessionId: ANOTHER_SESSION_ID},
    });
    await mockCdpServer.emulateIncomingMessage({
      sessionId: ANOTHER_SESSION_ID,
      method: 'Target.attachedToTarget',
      params: {sessionId: THIRD_SESSION_ID},
    });

    const childClient = cdpConnection.getCdpClient(ANOTHER_SESSION_ID);
    const grandchildClient = cdpConnection.getCdpClient(THIRD_SESSION_ID);
    const pendingChildError = childClient
      .sendCommand('Runtime.evaluate', {
        expression: '1 + 1',
      })
      .then(
        () => null,
        (error: unknown) => error,
      );
    const pendingGrandchildError = grandchildClient
      .sendCommand('Runtime.evaluate', {
        expression: '2 + 2',
      })
      .then(
        () => null,
        (error: unknown) => error,
      );

    // Detach the root session; CDP tears down multi-hop child/grandchild
    // sessions without emitting separate `Target.detachedFromTarget` events.
    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.detachedFromTarget',
      params: {sessionId: SOME_SESSION_ID},
    });

    assert.instanceOf(await pendingChildError, CloseError);
    assert.instanceOf(await pendingGrandchildError, CloseError);
    assert.throws(
      () => cdpConnection.getCdpClient(SOME_SESSION_ID),
      'Unknown CDP session ID',
    );
    assert.throws(
      () => cdpConnection.getCdpClient(ANOTHER_SESSION_ID),
      'Unknown CDP session ID',
    );
    assert.throws(
      () => cdpConnection.getCdpClient(THIRD_SESSION_ID),
      'Unknown CDP session ID',
    );

    mockCdpServer.sendMessage.resetHistory();
    const postDetachError = await grandchildClient
      .sendCommand('Runtime.evaluate', {expression: '3 + 3'})
      .then(
        () => null,
        (error: unknown) => error,
      );
    assert.instanceOf(postDetachError, CloseError);
    sinon.assert.notCalled(mockCdpServer.sendMessage);
  });

  it('closeBrowserSession synchronously detaches the browser session without closing the shared transport', async () => {
    const mockCdpServer = new StubTransport();
    const logger = sinon.fake();
    const cdpConnection = new MapperCdpConnection(mockCdpServer, logger);

    const createSessionPromise = cdpConnection.createBrowserSession();
    sinon.assert.calledOnceWithExactly(
      mockCdpServer.sendMessage,
      JSON.stringify({id: 0, method: 'Target.attachToBrowserTarget'}),
    );
    // Chromium emits `Target.attachedToTarget` before returning the result.
    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.attachedToTarget',
      params: {sessionId: SOME_SESSION_ID},
    });
    const attachedClient = cdpConnection.getCdpClient(SOME_SESSION_ID);
    await mockCdpServer.emulateIncomingMessage({
      id: 0,
      result: {sessionId: SOME_SESSION_ID},
    });
    const browserClient = await createSessionPromise;
    assert.strictEqual(browserClient, attachedClient);
    assert.strictEqual(browserClient.sessionId, SOME_SESSION_ID);

    const eventHandler = sinon.fake();
    browserClient.on('Page.frameNavigated', eventHandler);

    const closePromise = cdpConnection.closeBrowserSession(SOME_SESSION_ID);
    // Session and its listeners are detached synchronously before the
    // `Target.detachFromTarget` IPC round-trip finishes.
    assert.throws(
      () => cdpConnection.getCdpClient(SOME_SESSION_ID),
      'Unknown CDP session ID',
    );
    await mockCdpServer.emulateIncomingMessage({
      sessionId: SOME_SESSION_ID,
      method: 'Page.frameNavigated',
    });
    sinon.assert.notCalled(eventHandler);
    // Late child attachment from the already-detached session is ignored.
    await mockCdpServer.emulateIncomingMessage({
      sessionId: SOME_SESSION_ID,
      method: 'Target.attachedToTarget',
      params: {sessionId: ANOTHER_SESSION_ID},
    });
    assert.throws(
      () => cdpConnection.getCdpClient(ANOTHER_SESSION_ID),
      'Unknown CDP session ID',
    );

    sinon.assert.calledWithExactly(
      mockCdpServer.sendMessage,
      JSON.stringify({
        id: 1,
        method: 'Target.detachFromTarget',
        params: {sessionId: SOME_SESSION_ID},
      }),
    );
    logger.resetHistory();
    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.detachedFromTarget',
      params: {sessionId: SOME_SESSION_ID},
    });
    // Expected `Target.detachedFromTarget` for the detaching browser session
    // must not log a `debug:error`.
    assert.isFalse(
      logger.getCalls().some((call) => call.args[0] === 'debug:error'),
    );
    await mockCdpServer.emulateIncomingMessage({
      id: 1,
      result: {},
    });
    await closePromise;

    sinon.assert.notCalled(mockCdpServer.close);
  });

  it('closeBrowserSession cleans up the session even when Target.detachFromTarget fails without a detach event', async () => {
    const mockCdpServer = new StubTransport();
    const cdpConnection = new MapperCdpConnection(mockCdpServer);

    const createSessionPromise = cdpConnection.createBrowserSession();
    await mockCdpServer.emulateIncomingMessage({
      id: 0,
      result: {sessionId: SOME_SESSION_ID},
    });
    const browserClient = await createSessionPromise;

    const closeError = cdpConnection.closeBrowserSession(SOME_SESSION_ID).then(
      () => null,
      (error: unknown) => error,
    );
    await mockCdpServer.emulateIncomingMessage({
      id: 1,
      error: {code: -32602, message: 'No session with given id'},
    });
    assert.deepEqual(await closeError, {
      code: -32602,
      message: 'No session with given id',
    });

    assert.throws(
      () => cdpConnection.getCdpClient(SOME_SESSION_ID),
      'Unknown CDP session ID',
    );
    mockCdpServer.sendMessage.resetHistory();
    const postCloseError = await browserClient
      .sendCommand('Browser.getVersion')
      .then(
        () => null,
        (error: unknown) => error,
      );
    assert.instanceOf(postCloseError, CloseError);
    sinon.assert.notCalled(mockCdpServer.sendMessage);
  });
});
