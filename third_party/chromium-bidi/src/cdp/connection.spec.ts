import { StubServer } from '../tests/stubServer.spec';

import * as chai from 'chai';
import chaiAsPromised from 'chai-as-promised';
chai.use(chaiAsPromised);

import * as sinon from 'sinon';

import { Connection } from './connection';

const SOME_SESSION_ID = 'ABCD';
const ANOTHER_SESSION_ID = 'EFGH';

describe('Connection', function () {
  it('can send a command message for a CdpClient', async function () {
    const mockCdpServer = new StubServer();
    const conn = new Connection(mockCdpServer);

    const browserMessage = JSON.stringify({
      id: 0,
      method: 'Browser.getVersion',
    });
    conn.browserClient().Browser.getVersion();

    sinon.assert.calledOnceWithExactly(
      mockCdpServer.sendMessage,
      browserMessage
    );
  });

  it('creates a CdpClient for a session when the Target.attachedToTarget event is received', async function () {
    const mockCdpServer = new StubServer();
    const conn = new Connection(mockCdpServer);

    let client = conn.sessionClient(SOME_SESSION_ID);
    chai.assert.isNull(client);

    const onMessage = mockCdpServer.getOnMessage();
    onMessage(
      JSON.stringify({
        method: 'Target.attachedToTarget',
        params: { sessionId: SOME_SESSION_ID },
      })
    );

    client = conn.sessionClient(SOME_SESSION_ID);
    chai.assert.isNotNull(client);
  });

  it('removes the CdpClient for a session when the Target.detachedFromTarget event is received', async function () {
    const mockCdpServer = new StubServer();
    const conn = new Connection(mockCdpServer);

    const onMessage = mockCdpServer.getOnMessage();
    onMessage(
      JSON.stringify({
        method: 'Target.attachedToTarget',
        params: { sessionId: SOME_SESSION_ID },
      })
    );

    let client = conn.sessionClient(SOME_SESSION_ID);
    chai.assert.isNotNull(client);

    onMessage(
      JSON.stringify({
        method: 'Target.detachedFromTarget',
        params: { sessionId: SOME_SESSION_ID },
      })
    );

    client = conn.sessionClient(SOME_SESSION_ID);
    chai.assert.isNull(client);
  });

  it('routes event messages to the correct handler based on sessionId', async function () {
    const mockCdpServer = new StubServer();
    const conn = new Connection(mockCdpServer);

    const browserMessage = { method: 'Browser.downloadWillBegin' };
    const sessionMessage = {
      sessionId: SOME_SESSION_ID,
      method: 'Page.frameNavigated',
    };
    const othersessionMessage = {
      sessionId: ANOTHER_SESSION_ID,
      method: 'Page.loadEventFired',
    };
    const onMessage = mockCdpServer.getOnMessage();

    const browserCallback = sinon.fake();
    const sessionCallback = sinon.fake();
    const otherSessionCallback = sinon.fake();

    // Register for browser message callbacks.
    const browserClient = conn.browserClient();
    browserClient.Browser.on('downloadWillBegin', browserCallback);

    // Verify that the browser callback receives the message.
    onMessage(JSON.stringify(browserMessage));
    sinon.assert.calledOnceWithExactly(browserCallback, {});
    browserCallback.resetHistory();

    // Attach session A.
    onMessage(
      JSON.stringify({
        method: 'Target.attachedToTarget',
        params: { sessionId: SOME_SESSION_ID },
      })
    );

    const sessionClient = conn.sessionClient(SOME_SESSION_ID);
    chai.assert.isNotNull(sessionClient);
    sessionClient.Page.on('frameNavigated', sessionCallback);

    // Send another message for the browser and verify that only the browser callback receives it.
    // Verifies that adding another client doesn't affect routing for existing clients.
    onMessage(JSON.stringify(browserMessage));
    sinon.assert.notCalled(sessionCallback);
    sinon.assert.calledOnceWithExactly(browserCallback, {});
    browserCallback.resetHistory();

    // Send a message for session A and verify that it is received.
    onMessage(JSON.stringify(sessionMessage));
    sinon.assert.notCalled(browserCallback);
    sinon.assert.calledOnceWithExactly(sessionCallback, {});
    sessionCallback.resetHistory();

    // Attach session B.
    onMessage(
      JSON.stringify({
        method: 'Target.attachedToTarget',
        params: { sessionId: ANOTHER_SESSION_ID },
      })
    );

    const otherSessionClient = conn.sessionClient(ANOTHER_SESSION_ID);
    chai.assert.isNotNull(otherSessionClient);
    otherSessionClient.Page.on('loadEventFired', otherSessionCallback);

    // Send a message for session B and verify that only the session B callback receives it.
    // Verifies that a message is sent only to the session client it is intended for.
    onMessage(JSON.stringify(othersessionMessage));
    sinon.assert.notCalled(browserCallback);
    sinon.assert.notCalled(sessionCallback);
    sinon.assert.calledOnceWithExactly(otherSessionCallback, {});
    otherSessionCallback.resetHistory();
  });

  it('closes the transport connection when closed', async function () {
    const mockCdpServer = new StubServer();
    const conn = new Connection(mockCdpServer);
    conn.close();
    sinon.assert.calledOnce(mockCdpServer.close);
  });
});
