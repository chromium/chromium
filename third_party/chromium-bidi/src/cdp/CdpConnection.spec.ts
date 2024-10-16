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

import {expect} from 'chai';
import sinon from 'sinon';

import {StubTransport} from '../utils/transportStub.spec.js';

import {MapperCdpConnection} from './CdpConnection.js';

const SOME_SESSION_ID = 'ABCD';
const ANOTHER_SESSION_ID = 'EFGH';

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

    expect(() => cdpConnection.getCdpClient(SOME_SESSION_ID)).to.throw(
      'Unknown CDP session ID',
    );

    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.attachedToTarget',
      params: {sessionId: SOME_SESSION_ID},
    });

    const cdpClient = cdpConnection.getCdpClient(SOME_SESSION_ID);
    expect(cdpClient).to.not.be.null;
  });

  it('removes the CdpClient for a session when the Target.detachedFromTarget event is received', async () => {
    const mockCdpServer = new StubTransport();
    const cdpConnection = new MapperCdpConnection(mockCdpServer);

    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.attachedToTarget',
      params: {sessionId: SOME_SESSION_ID},
    });

    const cdpClient = cdpConnection.getCdpClient(SOME_SESSION_ID);
    expect(cdpClient).to.not.be.null;

    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.detachedFromTarget',
      params: {sessionId: SOME_SESSION_ID},
    });

    expect(() => cdpConnection.getCdpClient(SOME_SESSION_ID)).to.throw(
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
    expect(sessionClient).to.not.be.null;
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
    expect(otherSessionClient).to.not.be.null;
    otherSessionClient.on('Page.loadEventFired', otherSessionCallback);

    // Send a message for session B and verify that only the session B callback receives it.
    // Verifies that a message is sent only to the session client it is intended for.
    await mockCdpServer.emulateIncomingMessage(otherSessionMessage);
    sinon.assert.notCalled(sessionCallback);
    sinon.assert.calledOnceWithExactly(otherSessionCallback, {});
    otherSessionCallback.resetHistory();
  });

  it('closes the transport connection when closed', () => {
    const mockCdpServer = new StubTransport();
    const cdpConnection = new MapperCdpConnection(mockCdpServer);
    cdpConnection.close();
    sinon.assert.calledOnce(mockCdpServer.close);
  });
});
