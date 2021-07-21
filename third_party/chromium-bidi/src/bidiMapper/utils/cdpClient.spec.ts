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
import { connectCdp } from './cdpClient';
import { StubServer } from './tests/stubServer.spec';
import { assert as chai_assert } from 'chai';
import { assert as sinon_assert } from 'sinon';
import { Protocol } from 'devtools-protocol';

describe('CdpClient tests.', () => {
  it(`given CdpClient, when some command is called, then cdpBindings should be
      called with proper values`, async () => {
    const expectedMessageStr = JSON.stringify({
      id: 0,
      method: 'Target.activateTarget',
      params: {
        targetId: 'TargetID',
      },
    });

    const mockCdpServer = new StubServer();

    const cdpClient = connectCdp(mockCdpServer);
    cdpClient.Target.activateTarget({ targetId: 'TargetID' });

    sinon_assert.calledOnceWithExactly(
      mockCdpServer.sendMessage,
      expectedMessageStr
    );
  });

  it(`given some command is called, when CDP command is done, then
        'sendMessage' promise is resolved`, async () => {
    const mockCdpServer = new StubServer();
    const cdpClient = connectCdp(mockCdpServer);

    // Get handler 'onMessage' to notify 'cdpClient' about new CDP messages.
    const onMessage = mockCdpServer.getOnMessage();

    // Send CDP command and store returned promise.
    const commandPromise = cdpClient.Target.activateTarget({
      targetId: 'TargetID',
    });

    // Verify CDP command was sent.
    sinon_assert.calledOnce(mockCdpServer.sendMessage);

    // Notify 'cdpClient' the CDP command is finished.
    onMessage(JSON.stringify({ id: 0, result: {} }));

    // Assert 'cdpClient' resolved message promise.
    await commandPromise;
  });

  it(`given some command is called 2 times, when CDP commands are done, then
        each command promise is resolved with proper results`, async () => {
    const mockCdpServer = new StubServer();
    const cdpClient = connectCdp(mockCdpServer);
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
      targetId: 'Target_1',
    });
    const commandPromise2 = cdpClient.Target.attachToTarget({
      targetId: 'Target_2',
    });

    // Verify CDP command was sent.
    sinon_assert.calledTwice(mockCdpServer.sendMessage);

    // Notify 'cdpClient' the command2 is finished.
    onMessage(JSON.stringify(commandResult2));
    // Assert second message promise is resolved.
    const actualResult2 = await commandPromise2;

    // Notify 'cdpClient' the command1 is finished.
    onMessage(JSON.stringify(commandResult1));
    // Assert first message promise is resolved.
    const actualResult1 = await commandPromise1;

    chai_assert.deepEqual(actualResult1, expectedResult1);
    chai_assert.deepEqual(actualResult2, expectedResult2);
  });
});
