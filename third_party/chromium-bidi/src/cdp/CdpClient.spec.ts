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

import * as chai from 'chai';
import {expect} from 'chai';
import chaiAsPromised from 'chai-as-promised';
import sinon from 'sinon';

import {StubTransport} from '../utils/transportStub.spec.js';

import type {CdpClient} from './CdpClient';
import {MapperCdpConnection} from './CdpConnection.js';

chai.use(chaiAsPromised);

const TEST_TARGET_ID = 'TargetA';
const ANOTHER_TARGET_ID = 'TargetB';
const CDP_SESSION_ID = 'SessionA';

async function createCdpClient(
  stubTransport: StubTransport,
  sessionId: string,
): Promise<CdpClient> {
  const cdpConnection = new MapperCdpConnection(stubTransport);

  await stubTransport.emulateIncomingMessage({
    method: 'Target.attachedToTarget',
    params: {sessionId},
  });

  return cdpConnection.getCdpClient(CDP_SESSION_ID);
}

describe('CdpClient', () => {
  it(`when command is called cdpBindings should have proper values`, async () => {
    const expectedMessageStr = JSON.stringify({
      id: 0,
      method: 'Target.activateTarget',
      params: {
        targetId: TEST_TARGET_ID,
      },
      sessionId: CDP_SESSION_ID,
    });

    const mockCdpServer = new StubTransport();
    const cdpClient = await createCdpClient(mockCdpServer, CDP_SESSION_ID);

    void cdpClient.sendCommand('Target.activateTarget', {
      targetId: TEST_TARGET_ID,
    });

    sinon.assert.calledOnceWithExactly(
      mockCdpServer.sendMessage,
      expectedMessageStr,
    );
  });

  it(`when command is called and it's done, sendMessage promise is resolved`, async () => {
    const mockCdpServer = new StubTransport();
    const cdpClient = await createCdpClient(mockCdpServer, CDP_SESSION_ID);

    // Send CDP command and store returned promise.
    const commandPromise = cdpClient.sendCommand('Target.activateTarget', {
      targetId: TEST_TARGET_ID,
    });

    // Verify CDP command was sent.
    sinon.assert.calledOnce(mockCdpServer.sendMessage);

    // Notify 'cdpClient' the CDP command is finished.
    await mockCdpServer.emulateIncomingMessage({id: 0, result: {}});

    // Assert 'cdpClient' resolved message promise.
    await expect(commandPromise).to.eventually.deep.equal({});
  });

  it(`when some command is called 2 times and it's done each command promise is resolved with proper results`, async () => {
    const mockCdpServer = new StubTransport();
    const cdpClient = await createCdpClient(mockCdpServer, CDP_SESSION_ID);

    const expectedResult1 = {
      someResult: 1,
    };
    const expectedResult2 = {
      anotherResult: 2,
    };
    const commandResult1 = {id: 0, result: expectedResult1};
    const commandResult2 = {id: 1, result: expectedResult2};

    // Send 2 CDP commands and store returned promises.
    const commandPromise1 = cdpClient.sendCommand('Target.attachToTarget', {
      targetId: TEST_TARGET_ID,
    });
    const commandPromise2 = cdpClient.sendCommand('Target.attachToTarget', {
      targetId: ANOTHER_TARGET_ID,
    });

    // Verify CDP command was sent.
    sinon.assert.calledTwice(mockCdpServer.sendMessage);

    // Notify 'cdpClient' the command2 is finished.
    await mockCdpServer.emulateIncomingMessage(commandResult2);
    // Assert second message promise is resolved.
    const actualResult2 = await commandPromise2;

    // Notify 'cdpClient' the command1 is finished.
    await mockCdpServer.emulateIncomingMessage(commandResult1);
    // Assert first message promise is resolved.
    const actualResult1 = await commandPromise1;

    expect(actualResult1).to.deep.equal(expectedResult1);
    expect(actualResult2).to.deep.equal(expectedResult2);
  });

  it('gets event callbacks when events are received from CDP', async () => {
    const mockCdpServer = new StubTransport();
    const cdpClient = await createCdpClient(mockCdpServer, CDP_SESSION_ID);

    // Register event callbacks.
    const genericCallback = sinon.fake();
    cdpClient.on('*', genericCallback);

    const typedCallback = sinon.fake();
    cdpClient.on('Target.attachedToTarget', typedCallback);

    // Send a CDP event.
    await mockCdpServer.emulateIncomingMessage({
      method: 'Target.attachedToTarget',
      params: {targetId: TEST_TARGET_ID},
      sessionId: CDP_SESSION_ID,
    });

    // Verify that callbacks are called.
    sinon.assert.calledOnceWithExactly(
      genericCallback,
      'Target.attachedToTarget',
      {targetId: TEST_TARGET_ID},
    );
    genericCallback.resetHistory();

    sinon.assert.calledOnceWithExactly(typedCallback, {
      targetId: TEST_TARGET_ID,
    });
    typedCallback.resetHistory();

    // Unregister callbacks.
    cdpClient.off('*', genericCallback);
    cdpClient.off('Target.attachedToTarget', typedCallback);

    // Send another CDP event.
    await mockCdpServer.emulateIncomingMessage({
      params: {targetId: TEST_TARGET_ID},
    });

    sinon.assert.notCalled(genericCallback);
    sinon.assert.notCalled(typedCallback);
  });

  describe('sendCommand()', () => {
    it('sends raw CDP messages and returns a promise that will be resolved with the result', async () => {
      const mockCdpServer = new StubTransport();
      const cdpClient = await createCdpClient(mockCdpServer, CDP_SESSION_ID);

      // Send CDP command and store returned promise.
      const commandPromise = cdpClient.sendCommand('Target.attachToTarget', {
        targetId: TEST_TARGET_ID,
      });

      // Verify CDP command was sent.
      sinon.assert.calledOnce(mockCdpServer.sendMessage);

      // Notify 'cdpClient' the CDP command is finished.
      await mockCdpServer.emulateIncomingMessage({
        id: 0,
        result: {targetId: TEST_TARGET_ID},
      });

      // Assert sendCommand resolved message promise.
      await expect(commandPromise).to.eventually.deep.equal({
        targetId: TEST_TARGET_ID,
      });
    });

    it('sends raw CDP messages and returns a promise that will reject on error', async () => {
      const mockCdpServer = new StubTransport();
      const cdpClient = await createCdpClient(mockCdpServer, CDP_SESSION_ID);

      const expectedError = {
        name: 'Error',
        code: 'some error',
        message: 'something happened',
      };

      // Send CDP command and store returned promise.
      const resultOrError = cdpClient
        .sendCommand('Target.attachToTarget', {
          targetId: TEST_TARGET_ID,
        })
        .catch((error) => error);

      // Verify CDP command was sent.
      sinon.assert.calledOnce(mockCdpServer.sendMessage);

      // Notify 'cdpClient' the CDP command is finished.
      await mockCdpServer.emulateIncomingMessage({
        id: 0,
        error: expectedError,
      });

      // Assert sendCommand rejects with error.
      await expect(resultOrError).to.eventually.deep.equal(expectedError);
    });
  });
});
