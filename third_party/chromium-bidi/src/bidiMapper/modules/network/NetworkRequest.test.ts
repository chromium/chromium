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

describe('NetworkRequest duplicate response headers and ExtraInfo', () => {
  function setupRequest(interceptPhase?: Network.InterceptPhase): {
    request: NetworkRequest;
    eventManager: sinon.SinonStubbedInstance<EventManager>;
  } {
    const eventManager = sinon.createStubInstance(EventManager);
    const networkStorage = sinon.createStubInstance(NetworkStorage);
    networkStorage.getInterceptsForPhase.callsFake((_req, phase) =>
      interceptPhase !== undefined && phase === interceptPhase
        ? new Set(['intercept-1'])
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

  async function resolveResponseStarted(
    eventManager: sinon.SinonStubbedInstance<EventManager>,
  ): Promise<Network.ResponseData> {
    const startedCall = eventManager.registerPromiseEvent
      .getCalls()
      .find((call) => call.args[2] === 'network.responseStarted');
    assert.isDefined(startedCall);
    const eventEntry = startedCall!.args[0];
    const resolvedStarted = await (typeof eventEntry === 'function'
      ? eventEntry()
      : eventEntry);
    if (resolvedStarted.kind !== 'success') {
      assert.fail('Expected success responseStarted event');
    }
    return (resolvedStarted.value.params as {response: Network.ResponseData})
      .response;
  }

  it('waits for responseReceivedExtraInfo and splits duplicate response headers', async () => {
    const {request, eventManager} = setupRequest();
    const responseInfoHeaders = {'content-type': 'text/plain'};
    request.onResponseReceivedEvent({
      requestId: REQUEST_ID,
      loaderId: 'loader-1',
      timestamp: 2,
      type: 'Document',
      hasExtraInfo: true,
      frameId: 'frame',
      response: {
        url: REQUEST_URL,
        status: 200,
        statusText: 'OK',
        headers: responseInfoHeaders,
        mimeType: 'text/plain',
        connectionReused: false,
        connectionId: 1,
        encodedDataLength: 10,
        securityState: 'insecure',
        charset: '',
      },
    });

    // `responseStarted` waits for `responseReceivedExtraInfo`.
    assert.isFalse(
      eventManager.registerPromiseEvent
        .getCalls()
        .some((call) => call.args[2] === 'network.responseStarted'),
    );

    request.onResponseReceivedExtraInfoEvent({
      requestId: REQUEST_ID,
      blockedCookies: [],
      headers: {
        'Content-Type': 'text/plain',
        'Set-Cookie': 'a=b\nc=d',
        'X-Custom': 'v1\nv2',
      },
      resourceIPAddressSpace: 'Loopback',
      statusCode: 200,
    });

    const response = await resolveResponseStarted(eventManager);
    assert.deepEqual(response.headers, [
      {name: 'Content-Type', value: {type: 'string', value: 'text/plain'}},
      {name: 'Set-Cookie', value: {type: 'string', value: 'a=b'}},
      {name: 'Set-Cookie', value: {type: 'string', value: 'c=d'}},
      {name: 'X-Custom', value: {type: 'string', value: 'v1'}},
      {name: 'X-Custom', value: {type: 'string', value: 'v2'}},
    ]);
    assert.deepEqual(responseInfoHeaders, {'content-type': 'text/plain'});
  });

  it('uses Fetch.requestPaused responseHeaders when response info and extraInfo are absent', async () => {
    const {request, eventManager} = setupRequest(
      Network.InterceptPhase.ResponseStarted,
    );
    request.onRequestPaused({
      ...pausedEvent,
      responseStatusCode: 200,
      responseStatusText: 'OK',
      responseHeaders: [
        {name: 'Set-Cookie', value: 'a=b'},
        {name: 'Set-Cookie', value: 'c=d'},
        {name: 'Content-Type', value: 'text/plain'},
      ],
    });

    const response = await resolveResponseStarted(eventManager);
    assert.deepEqual(response.headers, [
      {name: 'Set-Cookie', value: {type: 'string', value: 'a=b'}},
      {name: 'Set-Cookie', value: {type: 'string', value: 'c=d'}},
      {name: 'Content-Type', value: {type: 'string', value: 'text/plain'}},
    ]);
  });

  it('emits responseStarted immediately for disk-cached responses and ignores stale extraInfo headers', async () => {
    const {request, eventManager} = setupRequest();
    request.onResponseReceivedEvent({
      requestId: REQUEST_ID,
      loaderId: 'loader-1',
      timestamp: 2,
      type: 'Document',
      hasExtraInfo: true,
      frameId: 'frame',
      response: {
        url: REQUEST_URL,
        status: 200,
        statusText: 'OK',
        headers: {'Content-Type': 'text/plain'},
        mimeType: 'text/plain',
        fromDiskCache: true,
        connectionReused: false,
        connectionId: 1,
        encodedDataLength: 0,
        securityState: 'insecure',
        charset: '',
      },
    });
    // `responseStarted` is queued immediately without waiting for `extraInfo`.
    const immediateCall = eventManager.registerPromiseEvent
      .getCalls()
      .find((call) => call.args[2] === 'network.responseStarted');
    assert.isDefined(immediateCall);

    // A stale `extraInfo` event arriving before serialization is ignored.
    request.onResponseReceivedExtraInfoEvent({
      requestId: REQUEST_ID,
      blockedCookies: [],
      headers: {'X-Stale': 'stale'},
      resourceIPAddressSpace: 'Loopback',
      statusCode: 304,
    });

    const response = await resolveResponseStarted(eventManager);
    assert.strictEqual(response.status, 200);
    assert.deepEqual(response.headers, [
      {name: 'Content-Type', value: {type: 'string', value: 'text/plain'}},
    ]);
  });
});

describe('NetworkRequest beforeRequestSent wire headers', () => {
  it('waits for requestWillBeSentExtraInfo even with an authRequired intercept and merges headers case-insensitively', async () => {
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
        headers: {'user-agent': 'UA'},
        initialPriority: 'High',
        referrerPolicy: 'no-referrer',
      },
      timestamp: 1,
      wallTime: 1,
      initiator: {type: 'other'},
      redirectHasExtraInfo: false,
      frameId: 'frame',
    });

    assert.isFalse(
      eventManager.registerPromiseEvent
        .getCalls()
        .some((call) => call.args[2] === 'network.beforeRequestSent'),
    );

    request.onRequestWillBeSentExtraInfoEvent({
      requestId: REQUEST_ID,
      associatedCookies: [],
      headers: {
        Host: '127.0.0.1:8080',
        Accept: 'text/html',
        'User-Agent': 'UA',
      },
      connectTiming: {requestTime: 1},
    });

    const beforeCall = eventManager.registerPromiseEvent
      .getCalls()
      .find((call) => call.args[2] === 'network.beforeRequestSent');
    assert.isDefined(beforeCall);
    const entry = beforeCall!.args[0];
    const resolved = await (typeof entry === 'function' ? entry() : entry);
    if (resolved.kind !== 'success') {
      assert.fail('Expected success event result');
    }
    const headers = (
      resolved.value.params as {request: {headers: Network.Header[]}}
    ).request.headers;
    assert.deepEqual(headers, [
      {name: 'Host', value: {type: 'string', value: '127.0.0.1:8080'}},
      {name: 'Accept', value: {type: 'string', value: 'text/html'}},
      {name: 'User-Agent', value: {type: 'string', value: 'UA'}},
    ]);
  });
});

describe('NetworkRequest response and request cookie overrides', () => {
  function createBlockedRequest(
    sendCommand: sinon.SinonStub,
    phase: Network.InterceptPhase,
    headers: Protocol.Network.Headers = {},
  ) {
    const eventManager = sinon.createStubInstance(EventManager);
    const networkStorage = sinon.createStubInstance(NetworkStorage);
    networkStorage.getInterceptsForPhase.returns(new Set(['intercept-1']));
    const cdpTarget = sinon.createStubInstance(CdpTarget);
    Object.defineProperty(cdpTarget, 'cdpClient', {
      value: {sendCommand},
    });
    cdpTarget.isSubscribedTo.returns(true);

    const request = new NetworkRequest(
      REQUEST_ID,
      eventManager,
      networkStorage,
      cdpTarget,
      0,
    );
    request.onRequestPaused({
      ...pausedEvent,
      request: {...pausedEvent.request, headers},
      ...(phase === Network.InterceptPhase.ResponseStarted
        ? {
            responseStatusCode: 200,
            responseHeaders: [
              {name: 'X-Orig', value: '1'},
              {name: 'Set-Cookie', value: 'orig=1'},
            ],
          }
        : {}),
    });
    return request;
  }

  it('converts provideResponse cookies to Set-Cookie headers', async () => {
    const sendCommand = sinon.stub().resolves({});
    const request = createBlockedRequest(
      sendCommand,
      Network.InterceptPhase.BeforeRequestSent,
    );

    await request.provideResponse({
      statusCode: 200,
      reasonPhrase: 'OK',
      headers: [
        {name: 'content-type', value: {type: 'string', value: 'text/plain'}},
      ],
      cookies: [
        {name: 'a', value: {type: 'string', value: '1'}},
        {name: 'b', value: {type: 'string', value: '2'}, httpOnly: true},
      ],
    });

    sinon.assert.calledWithMatch(sendCommand, 'Fetch.fulfillRequest', {
      requestId: FETCH_ID,
      responseCode: 200,
      responseHeaders: [
        {name: 'content-type', value: 'text/plain'},
        {name: 'Set-Cookie', value: 'a=1'},
        {name: 'Set-Cookie', value: 'b=2;HttpOnly'},
      ],
    });
  });

  it('does not add a cookie header when provideResponse receives empty cookies', async () => {
    const sendCommand = sinon.stub().resolves({});
    const request = createBlockedRequest(
      sendCommand,
      Network.InterceptPhase.BeforeRequestSent,
    );

    await request.provideResponse({
      statusCode: 200,
      headers: [
        {name: 'content-type', value: {type: 'string', value: 'text/plain'}},
      ],
      cookies: [],
    });

    sinon.assert.calledWithMatch(sendCommand, 'Fetch.fulfillRequest', {
      requestId: FETCH_ID,
      responseHeaders: [{name: 'content-type', value: 'text/plain'}],
    });
  });

  it('replaces existing Set-Cookie headers when continueResponse passes cookies', async () => {
    const sendCommand = sinon.stub().resolves({});
    const request = createBlockedRequest(
      sendCommand,
      Network.InterceptPhase.ResponseStarted,
    );

    await request.continueResponse({
      cookies: [
        {name: 'a', value: {type: 'string', value: '1'}},
        {name: 'b', value: {type: 'string', value: '2'}, httpOnly: true},
      ],
    });

    sinon.assert.calledWithMatch(sendCommand, 'Fetch.continueResponse', {
      requestId: FETCH_ID,
      responseHeaders: [
        {name: 'X-Orig', value: '1'},
        {name: 'Set-Cookie', value: 'a=1'},
        {name: 'Set-Cookie', value: 'b=2;HttpOnly'},
      ],
    });
  });

  it('drops existing Set-Cookie headers when continueResponse passes empty cookies', async () => {
    const sendCommand = sinon.stub().resolves({});
    const request = createBlockedRequest(
      sendCommand,
      Network.InterceptPhase.ResponseStarted,
    );

    await request.continueResponse({cookies: []});

    sinon.assert.calledWithMatch(sendCommand, 'Fetch.continueResponse', {
      requestId: FETCH_ID,
      responseHeaders: [{name: 'X-Orig', value: '1'}],
    });
  });

  it('replaces existing Cookie header with empty Cookie when continueRequest passes empty cookies', async () => {
    const sendCommand = sinon.stub().resolves({});
    const request = createBlockedRequest(
      sendCommand,
      Network.InterceptPhase.BeforeRequestSent,
      {Cookie: 'old=1', 'X-Keep': 'yes'},
    );

    await request.continueRequest({cookies: []});

    sinon.assert.calledWithMatch(sendCommand, 'Fetch.continueRequest', {
      requestId: FETCH_ID,
      headers: [
        {name: 'X-Keep', value: 'yes'},
        {name: 'Cookie', value: ''},
      ],
    });
  });
});
