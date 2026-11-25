/*
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

import {setImmediate} from 'node:timers/promises';
import {describe, it} from 'node:test';
import {assert} from 'chai';
import sinon from 'sinon';

import type {Protocol} from 'devtools-protocol';

import {Network} from '../../../protocol/protocol.js';
import {LogType, type LoggerFn} from '../../../utils/log.js';
import {CdpTarget} from '../cdp/CdpTarget.js';
import {EventManager} from '../session/EventManager.js';

import {NetworkRequest} from './NetworkRequest.js';
import {NetworkStorage} from './NetworkStorage.js';

const REQUEST_ID = 'network-request';
const FETCH_ID = 'fetch-request';
const REQUEST_URL = 'http://local-proof.invalid/';

const pausedEvent: Protocol.Fetch.RequestPausedEvent = {
  requestId: FETCH_ID,
  networkId: REQUEST_ID,
  frameId: 'frame',
  resourceType: 'Document',
  request: {
    url: REQUEST_URL,
    method: 'GET',
    headers: {},
    initialPriority: 'High',
    referrerPolicy: 'no-referrer',
  },
};

function createRequest(sendCommand: sinon.SinonStub, logger?: LoggerFn) {
  const eventManager = sinon.createStubInstance(EventManager);
  const networkStorage = sinon.createStubInstance(NetworkStorage);
  const cdpTarget = sinon.createStubInstance(CdpTarget);
  Object.defineProperty(cdpTarget, 'cdpClient', {
    value: {sendCommand},
  });
  cdpTarget.isSubscribedTo.returns(false);

  return new NetworkRequest(
    REQUEST_ID,
    eventManager,
    networkStorage,
    cdpTarget,
    0,
    logger,
  );
}

const continuations = [
  {
    method: 'Fetch.continueRequest',
    trigger: (request: NetworkRequest) => request.onRequestPaused(pausedEvent),
  },
  {
    method: 'Fetch.continueResponse',
    trigger: (request: NetworkRequest) =>
      request.onRequestPaused({...pausedEvent, responseStatusCode: 200}),
  },
  {
    method: 'Fetch.continueWithAuth',
    trigger: (request: NetworkRequest) =>
      request.onAuthRequired({
        ...pausedEvent,
        authChallenge: {
          source: 'Server',
          origin: REQUEST_URL,
          scheme: 'basic',
          realm: 'synthetic',
        },
      }),
  },
];

describe('NetworkRequest automatic continuation', () => {
  for (const {method, trigger} of continuations) {
    it(`logs a delayed ${method} rejection`, async () => {
      const error = new Error('Session closed');
      const logged = sinon.stub();
      const logger = sinon.stub().returns(logged);
      let rejectCommand!: (reason: unknown) => void;
      const pending = new Promise<never>((_resolve, reject) => {
        rejectCommand = reject;
      });
      const sendCommand = sinon.stub().returns(pending);
      const request = createRequest(sendCommand, logger);
      trigger(request);
      sinon.assert.calledWith(sendCommand, method);
      rejectCommand(error);
      await setImmediate();
      sinon.assert.calledWith(logger, LogType.debugError);
      sinon.assert.calledWith(logged, error);
    });

    it(`handles ${method} rejection without a logger`, async () => {
      const request = createRequest(
        sinon.stub().rejects(new Error('Session closed')),
      );
      trigger(request);
      // node:test fails the test if the rejection is unhandled.
      await setImmediate();
    });
  }

  it('still rejects an explicit continueRequest command', async () => {
    const error = new Error('Invalid request');
    const sendCommand = sinon
      .stub()
      .onFirstCall()
      .resolves({})
      .onSecondCall()
      .rejects(error);
    const request = createRequest(sendCommand);
    request.onRequestPaused(pausedEvent);
    await setImmediate();
    await request.continueRequest({}).then(
      () => assert.fail('Expected continueRequest to reject'),
      (reason: unknown) => assert.strictEqual(reason, error),
    );
  });
});

describe('NetworkRequest authRequired challenges and headers', () => {
  function createInterceptedRequest() {
    const eventManager = sinon.createStubInstance(EventManager);
    const networkStorage = sinon.createStubInstance(NetworkStorage);
    networkStorage.getInterceptsForPhase.callsFake((_req, phase) =>
      phase === Network.InterceptPhase.AuthRequired
        ? new Set(['auth-intercept'])
        : new Set(),
    );
    const cdpTarget = sinon.createStubInstance(CdpTarget);
    Object.defineProperty(cdpTarget, 'cdpClient', {
      value: {sendCommand: sinon.stub().resolves({})},
    });
    cdpTarget.isSubscribedTo.returns(true);

    const request = new NetworkRequest(
      REQUEST_ID,
      eventManager,
      networkStorage,
      cdpTarget,
      0,
    );

    request.onRequestWillBeSentEvent({
      requestId: REQUEST_ID,
      loaderId: 'loader-1',
      documentURL: REQUEST_URL,
      request: {
        url: REQUEST_URL,
        method: 'GET',
        headers: {},
        initialPriority: 'High',
        referrerPolicy: 'no-referrer',
      },
      timestamp: 1,
      wallTime: 1,
      initiator: {type: 'other'},
      redirectHasExtraInfo: false,
      frameId: 'frame',
    });
    request.onRequestWillBeSentExtraInfoEvent({
      requestId: REQUEST_ID,
      associatedCookies: [],
      headers: {},
      connectTiming: {requestTime: 1},
    });
    return {request, eventManager};
  }

  function authRequired(
    request: NetworkRequest,
    source: 'Server' | 'Proxy',
    realm: string,
  ) {
    request.onAuthRequired({
      ...pausedEvent,
      authChallenge: {
        source,
        origin: REQUEST_URL,
        scheme: 'basic',
        realm,
      },
    });
  }

  function responseReceived(
    request: NetworkRequest,
    status: number,
    statusText: string,
    hasExtraInfo: boolean,
  ) {
    request.onResponseReceivedEvent({
      requestId: REQUEST_ID,
      loaderId: 'loader-1',
      timestamp: 2,
      type: 'Document',
      hasExtraInfo,
      frameId: 'frame',
      response: {
        url: REQUEST_URL,
        status,
        statusText,
        headers: {'Content-Type': 'text/plain'},
        mimeType: 'text/plain',
        connectionReused: false,
        connectionId: 1,
        encodedDataLength: 10,
        securityState: 'insecure',
        charset: '',
      },
    });
  }

  function responseReceivedExtraInfo(
    request: NetworkRequest,
    statusCode: number,
    headers: Protocol.Network.Headers,
  ) {
    request.onResponseReceivedExtraInfoEvent({
      requestId: REQUEST_ID,
      blockedCookies: [],
      headers: {'Content-Type': 'text/plain', ...headers},
      resourceIPAddressSpace: 'Loopback',
      statusCode,
    });
  }

  async function getResponseData(
    eventManager: sinon.SinonStubbedInstance<EventManager>,
    eventName: string,
  ): Promise<Network.ResponseData> {
    const call = eventManager.registerPromiseEvent
      .getCalls()
      .find((call) => call.args[2] === eventName);
    assert.isDefined(call);
    const entry = call!.args[0];
    const resolved = await (typeof entry === 'function' ? entry() : entry);
    if (resolved.kind !== 'success') {
      assert.fail(`Expected success ${eventName} event`);
    }
    return (resolved.value.params as {response: Network.ResponseData}).response;
  }

  it('populates status, statusText and authChallenges on network.authRequired', async () => {
    const {request, eventManager} = createInterceptedRequest();
    authRequired(request, 'Server', 'Access');

    const authResponse = await getResponseData(
      eventManager,
      'network.authRequired',
    );
    assert.strictEqual(authResponse.status, 401);
    assert.strictEqual(authResponse.statusText, 'Unauthorized');
    assert.deepEqual(authResponse.authChallenges, [
      {scheme: 'Basic', realm: 'Access'},
    ]);
  });

  it('keeps www-authenticate and parses authChallenges from the header after cancel', async () => {
    const {request, eventManager} = createInterceptedRequest();
    authRequired(request, 'Server', 'Access');

    await request.continueWithAuth({action: 'cancel'});
    responseReceived(request, 401, 'Unauthorized', true);
    responseReceivedExtraInfo(request, 401, {
      'WWW-Authenticate': 'Basic realm="Header realm"',
    });

    const started = await getResponseData(
      eventManager,
      'network.responseStarted',
    );
    assert.strictEqual(started.status, 401);
    assert.strictEqual(started.statusText, 'Unauthorized');
    assert.deepInclude(started.headers, {
      name: 'WWW-Authenticate',
      value: {type: 'string', value: 'Basic realm="Header realm"'},
    });
    // The challenge comes from the header, not from `Fetch.authRequired`.
    assert.deepEqual(started.authChallenges, [
      {scheme: 'Basic', realm: 'Header realm'},
    ]);
  });

  it('reports the proxy status and challenges', async () => {
    const {request, eventManager} = createInterceptedRequest();
    authRequired(request, 'Proxy', 'Proxy');

    const authResponse = await getResponseData(
      eventManager,
      'network.authRequired',
    );
    assert.strictEqual(authResponse.status, 407);
    assert.strictEqual(
      authResponse.statusText,
      'Proxy Authentication Required',
    );
    assert.deepEqual(authResponse.authChallenges, [
      {scheme: 'Basic', realm: 'Proxy'},
    ]);

    await request.continueWithAuth({action: 'cancel'});
    responseReceived(request, 407, 'Proxy Authentication Required', true);
    responseReceivedExtraInfo(request, 407, {
      'Proxy-Authenticate': 'Digest realm="Proxy realm"',
      // A 407 response does not use this header.
      'WWW-Authenticate': 'Basic realm="Server realm"',
    });

    const started = await getResponseData(
      eventManager,
      'network.responseStarted',
    );
    assert.strictEqual(started.status, 407);
    assert.deepEqual(started.authChallenges, [
      {scheme: 'Digest', realm: 'Proxy realm'},
    ]);
  });

  const realmCases: [string, string, Network.AuthChallenge[]][] = [
    [
      'an upper-case parameter name',
      'Basic REALM="Upper"',
      [{scheme: 'Basic', realm: 'Upper'}],
    ],
    [
      'quoted pairs',
      'Digest xrealm="wrong", realm="a \\"b\\" \\\\c"',
      [{scheme: 'Digest', realm: 'a "b" \\c'}],
    ],
    [
      'a parameter name that only ends with realm',
      'Basic xrealm="wrong"',
      [{scheme: 'Basic', realm: ''}],
    ],
    [
      'two header values',
      'Basic realm="one"\nDigest realm="two"',
      [
        {scheme: 'Basic', realm: 'one'},
        {scheme: 'Digest', realm: 'two'},
      ],
    ],
  ];
  for (const [name, headerValue, expected] of realmCases) {
    it(`parses authChallenges with ${name}`, async () => {
      const {request, eventManager} = createInterceptedRequest();
      responseReceived(request, 401, 'Unauthorized', true);
      responseReceivedExtraInfo(request, 401, {
        'WWW-Authenticate': headerValue,
      });

      const started = await getResponseData(
        eventManager,
        'network.responseStarted',
      );
      assert.deepEqual(started.authChallenges, expected);
    });
  }

  it('reports no authChallenges when the received headers have no challenge', async () => {
    const {request, eventManager} = createInterceptedRequest();
    authRequired(request, 'Server', 'Access');

    await request.continueWithAuth({action: 'cancel'});
    responseReceived(request, 401, 'Unauthorized', true);
    responseReceivedExtraInfo(request, 401, {});

    const started = await getResponseData(
      eventManager,
      'network.responseStarted',
    );
    assert.strictEqual(started.status, 401);
    assert.deepEqual(started.authChallenges, []);
  });

  it('keeps an empty status text of the response after credentials', async () => {
    const {request, eventManager} = createInterceptedRequest();
    authRequired(request, 'Server', 'Access');

    const withCredentials: Omit<
      Network.ContinueWithAuthCredentials,
      'request'
    > = {
      action: 'provideCredentials',
      credentials: {type: 'password', username: 'user', password: 'pass'},
    };
    await request.continueWithAuth(withCredentials);
    // HTTP/2 and HTTP/3 responses have no reason phrase.
    responseReceived(request, 200, '', false);

    const started = await getResponseData(
      eventManager,
      'network.responseStarted',
    );
    assert.strictEqual(started.status, 200);
    assert.strictEqual(started.statusText, '');
    assert.isUndefined(started.authChallenges);
  });
});
