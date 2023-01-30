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

import {StubTransport} from './stubTransport.spec.js';

import * as chai from 'chai';
import chaiAsPromised from 'chai-as-promised';
chai.use(chaiAsPromised);

import * as sinon from 'sinon';

import {CdpConnection} from './cdpConnection.js';

const SOME_SESSION_ID = 'ABCD';
const ANOTHER_SESSION_ID = 'EFGH';

describe('CdpConnection', () => {
  it('can send a command message for a CdpClient', () => {
    const mockCdpServer = new StubTransport();
    const cdpConnection = new CdpConnection(mockCdpServer);

    const browserMessage = JSON.stringify({
      id: 0,
      method: 'Browser.getVersion',
    });
    cdpConnection.browserClient().sendCommand('Browser.getVersion');

    sinon.assert.calledOnceWithExactly(
      mockCdpServer.sendMessage,
      browserMessage
    );
  });

  it('creates a CdpClient for a session when the Target.attachedToTarget event is received', async () => {
    const mockCdpServer = new StubTransport();
    const cdpConnection = new CdpConnection(mockCdpServer);

    chai.assert.throws(
      () => cdpConnection.getCdpClient(SOME_SESSION_ID),
      'Unknown CDP session ID'
    );

    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.attachedToTarget',
      params: {sessionId: SOME_SESSION_ID},
    });

    const cdpClient = cdpConnection.getCdpClient(SOME_SESSION_ID);
    chai.assert.isNotNull(cdpClient);
  });

  it('removes the CdpClient for a session when the Target.detachedFromTarget event is received', async () => {
    const mockCdpServer = new StubTransport();
    const cdpConnection = new CdpConnection(mockCdpServer);

    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.attachedToTarget',
      params: {sessionId: SOME_SESSION_ID},
    });

    const cdpClient = cdpConnection.getCdpClient(SOME_SESSION_ID);
    chai.assert.isNotNull(cdpClient);

    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.detachedFromTarget',
      params: {sessionId: SOME_SESSION_ID},
    });

    chai.assert.throws(
      () => cdpConnection.getCdpClient(SOME_SESSION_ID),
      'Unknown CDP session ID'
    );
  });

  it('routes event messages to the correct handler based on sessionId', async () => {
    const mockCdpServer = new StubTransport();
    const cdpConnection = new CdpConnection(mockCdpServer);

    const browserMessage = {method: 'Browser.downloadWillBegin'};
    const sessionMessage = {
      sessionId: SOME_SESSION_ID,
      method: 'Page.frameNavigated',
    };
    const othersessionMessage = {
      sessionId: ANOTHER_SESSION_ID,
      method: 'Page.loadEventFired',
    };

    const browserCallback = sinon.fake();
    const sessionCallback = sinon.fake();
    const otherSessionCallback = sinon.fake();

    // Register for browser message callbacks.
    const browserClient = cdpConnection.browserClient();
    browserClient.on('Browser.downloadWillBegin', browserCallback);

    // Verify that the browser callback receives the message.
    await mockCdpServer.emulateIncomingMessage(browserMessage);
    sinon.assert.calledOnceWithExactly(browserCallback, {});
    browserCallback.resetHistory();

    // Attach session A.
    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.attachedToTarget',
      params: {sessionId: SOME_SESSION_ID},
    });

    const sessionClient = cdpConnection.getCdpClient(SOME_SESSION_ID)!;
    chai.assert.isNotNull(sessionClient);
    sessionClient.on('Page.frameNavigated', sessionCallback);

    // Send another message for the browser and verify that only the browser callback receives it.
    // Verifies that adding another client doesn't affect routing for existing clients.
    await mockCdpServer.emulateIncomingMessage(browserMessage);
    sinon.assert.notCalled(sessionCallback);
    sinon.assert.calledOnceWithExactly(browserCallback, {});
    browserCallback.resetHistory();

    // Send a message for session A and verify that it is received.
    await mockCdpServer.emulateIncomingMessage(sessionMessage);
    sinon.assert.notCalled(browserCallback);
    sinon.assert.calledOnceWithExactly(sessionCallback, {});
    sessionCallback.resetHistory();

    // Attach session B.
    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.attachedToTarget',
      params: {sessionId: ANOTHER_SESSION_ID},
    });

    const otherSessionClient = cdpConnection.getCdpClient(ANOTHER_SESSION_ID)!;
    chai.assert.isNotNull(otherSessionClient);
    otherSessionClient.on('Page.loadEventFired', otherSessionCallback);

    // Send a message for session B and verify that only the session B callback receives it.
    // Verifies that a message is sent only to the session client it is intended for.
    await mockCdpServer.emulateIncomingMessage(othersessionMessage);
    sinon.assert.notCalled(browserCallback);
    sinon.assert.notCalled(sessionCallback);
    sinon.assert.calledOnceWithExactly(otherSessionCallback, {});
    otherSessionCallback.resetHistory();
  });

  it('closes the transport connection when closed', () => {
    const mockCdpServer = new StubTransport();
    const cdpConnection = new CdpConnection(mockCdpServer);
    cdpConnection.close();
    sinon.assert.calledOnce(mockCdpServer.close);
  });
});
