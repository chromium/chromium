/**
 * Copyright 2021 Google Inc. All rights reserved.
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
import { Connection } from './connection';
import { StubServer } from '../tests/stubServer.spec';

import * as chai from 'chai';
import chaiAsPromised from 'chai-as-promised';
chai.use(chaiAsPromised);

import * as sinon from 'sinon';

import { Protocol } from 'devtools-protocol';

const TEST_SESSION_ID = 'ABCD';

const TEST_TARGET_ID = 'TargetA';
const ANOTHER_TARGET_ID = 'TargetB';

describe('CdpClient tests.', function () {
  it(`given CdpClient, when some command is called, then cdpBindings should be
      called with proper values`, async function () {
    const expectedMessageStr = JSON.stringify({
      id: 0,
      method: 'Target.activateTarget',
      params: {
        targetId: TEST_TARGET_ID,
      },
    });

    const mockCdpServer = new StubServer();
    const conn = new Connection(mockCdpServer);

    const cdpClient = conn.browserClient();
    cdpClient.Target.activateTarget({ targetId: TEST_TARGET_ID });

    sinon.assert.calledOnceWithExactly(
      mockCdpServer.sendMessage,
      expectedMessageStr
    );
  });

  it(`given some command is called, when CDP command is done, then
        'sendMessage' promise is resolved`, async function () {
    const mockCdpServer = new StubServer();
    const conn = new Connection(mockCdpServer);

    const cdpClient = conn.browserClient();

    // Get handler 'onMessage' to notify 'cdpClient' about new CDP messages.
    const onMessage = mockCdpServer.getOnMessage();

    // Send CDP command and store returned promise.
    const commandPromise = cdpClient.Target.activateTarget({
      targetId: TEST_TARGET_ID,
    });

    // Verify CDP command was sent.
    sinon.assert.calledOnce(mockCdpServer.sendMessage);

    // Notify 'cdpClient' the CDP command is finished.
    onMessage(JSON.stringify({ id: 0, result: {} }));

    // Assert 'cdpClient' resolved message promise.
    chai.assert.eventually.deepEqual(commandPromise, {});
  });

  it(`given some command is called 2 times, when CDP commands are done, then
        each command promise is resolved with proper results`, async function () {
    const mockCdpServer = new StubServer();
    const conn = new Connection(mockCdpServer);
    const cdpClient = conn.browserClient();

    const expectedResult1 = {
      someResult: 1,
    } as any as Protocol.Target.AttachToTargetResponse;
    const expectedResult2 = {
      anotherResult: 2,
    } as any as Protocol.Target.AttachToTargetResponse;
    const commandResult1 = { id: 0, result: expectedResult1 };
    const commandResult2 = { id: 1, result: expectedResult2 };

    // Get handler 'onMessage' to notify 'cdpClient' about new CDP messages.
    const onMessage = mockCdpServer.getOnMessage();

    // Send 2 CDP commands and store returned promises.
    const commandPromise1 = cdpClient.Target.attachToTarget({
      targetId: TEST_TARGET_ID,
    });
    const commandPromise2 = cdpClient.Target.attachToTarget({
      targetId: ANOTHER_TARGET_ID,
    });

    // Verify CDP command was sent.
    sinon.assert.calledTwice(mockCdpServer.sendMessage);

    // Notify 'cdpClient' the command2 is finished.
    onMessage(JSON.stringify(commandResult2));
    // Assert second message promise is resolved.
    const actualResult2 = await commandPromise2;

    // Notify 'cdpClient' the command1 is finished.
    onMessage(JSON.stringify(commandResult1));
    // Assert first message promise is resolved.
    const actualResult1 = await commandPromise1;

    chai.assert.deepEqual(actualResult1, expectedResult1);
    chai.assert.deepEqual(actualResult2, expectedResult2);
  });

  it('gets event callbacks when events are received from CDP', async function () {
    const mockCdpServer = new StubServer();
    const conn = new Connection(mockCdpServer);
    const cdpClient = conn.browserClient();

    // Get handler 'onMessage' to notify 'cdpClient' about new CDP messages.
    const onMessage = mockCdpServer.getOnMessage();

    // Register event callbacks.
    const genericCallback = sinon.fake();
    cdpClient.on('event', genericCallback);

    const typedCallback = sinon.fake();
    cdpClient.Target.on('attachedToTarget', typedCallback);

    // Send a CDP event.
    onMessage(
      JSON.stringify({
        method: 'Target.attachedToTarget',
        params: { targetId: TEST_TARGET_ID },
      })
    );

    // Verify that callbacks are called.
    sinon.assert.calledOnceWithExactly(
      genericCallback,
      'Target.attachedToTarget',
      { targetId: TEST_TARGET_ID }
    );
    genericCallback.resetHistory();

    sinon.assert.calledOnceWithExactly(typedCallback, {
      targetId: TEST_TARGET_ID,
    });
    typedCallback.resetHistory();

    // Unregister callbacks.
    cdpClient.off('event', genericCallback);
    cdpClient.Target.off('Target.attachedToTarget', typedCallback);

    // Send another CDP event.
    onMessage(JSON.stringify({ params: { targetId: TEST_TARGET_ID } }));

    sinon.assert.notCalled(genericCallback);
    sinon.assert.notCalled(typedCallback);
  });

  describe('sendCommand()', function () {
    it('sends a raw CDP messages and returns a promise that will be resolved with the result', async function () {
      const mockCdpServer = new StubServer();
      const conn = new Connection(mockCdpServer);
      const cdpClient = conn.browserClient();

      // Get handler 'onMessage' to notify 'cdpClient' about new CDP messages.
      const onMessage = mockCdpServer.getOnMessage();

      // Send CDP command and store returned promise.
      const commandPromise = cdpClient.sendCommand('Target.attachToTarget', {
        targetId: TEST_TARGET_ID,
      });

      // Verify CDP command was sent.
      sinon.assert.calledOnce(mockCdpServer.sendMessage);

      // Notify 'cdpClient' the CDP command is finished.
      onMessage(
        JSON.stringify({ id: 0, result: { targetId: TEST_TARGET_ID } })
      );

      // Assert sendCommand resolved message promise.
      chai.assert.eventually.deepEqual(commandPromise, {
        targetId: TEST_TARGET_ID,
      });
    });

    it('sends a raw CDP messages and returns a promise that will reject on error', async function () {
      const mockCdpServer = new StubServer();
      const conn = new Connection(mockCdpServer);
      const cdpClient = conn.browserClient();

      const expectedError = {
        code: 'some error',
        message: 'something happened',
      };

      // Get handler 'onMessage' to notify 'cdpClient' about new CDP messages.
      const onMessage = mockCdpServer.getOnMessage();

      // Send CDP command and store returned promise.
      const commandPromise = cdpClient.sendCommand('Target.attachToTarget', {
        targetId: TEST_TARGET_ID,
      });

      // Verify CDP command was sent.
      sinon.assert.calledOnce(mockCdpServer.sendMessage);

      // Notify 'cdpClient' the CDP command is finished.
      onMessage(JSON.stringify({ id: 0, error: expectedError }));

      // Assert sendCommand rejects with error.
      chai.assert.isRejected(commandPromise, expectedError);
    });
  });
});
