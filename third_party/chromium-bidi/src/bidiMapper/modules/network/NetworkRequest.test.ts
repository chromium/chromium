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
