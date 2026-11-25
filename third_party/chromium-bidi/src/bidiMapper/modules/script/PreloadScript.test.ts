/**
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
import {afterEach, describe, it} from 'node:test';
import {assert} from 'chai';
import sinon from 'sinon';

import {CloseError, type CdpClient} from '../../../cdp/CdpClient.js';
import {CdpErrorConstants} from '../../../utils/cdpErrorConstants.js';
import {CdpTarget} from '../cdp/CdpTarget.js';

import {PreloadScript} from './PreloadScript.js';

const CDP_TARGET_ID = 'TARGET_ID';
const OTHER_CDP_TARGET_ID = 'OTHER_TARGET_ID';
const USER_CONTEXT = '11111111-0000-0000-0000-000000000000';

describe('PreloadScript', () => {
  afterEach(() => {
    sinon.restore();
  });

  it('ignores CloseError and session-not-found errors when removing scripts', async () => {
    const sendCommand = sinon.stub();
    const cdpClient = {
      sendCommand,
      isCloseError: (error: unknown) => error instanceof CloseError,
    } as unknown as CdpClient;
    const target1 = sinon.createStubInstance(CdpTarget);
    sinon.stub(target1, 'id').get(() => CDP_TARGET_ID);
    sinon.stub(target1, 'cdpClient').get(() => cdpClient);
    const target2 = sinon.createStubInstance(CdpTarget);
    sinon.stub(target2, 'id').get(() => OTHER_CDP_TARGET_ID);
    sinon.stub(target2, 'cdpClient').get(() => cdpClient);

    sendCommand
      .withArgs('Page.addScriptToEvaluateOnNewDocument')
      .onFirstCall()
      .resolves({identifier: '1'})
      .onSecondCall()
      .resolves({identifier: '2'});

    const preloadScript = new PreloadScript({
      functionDeclaration: '() => {}',
      userContexts: [USER_CONTEXT],
    });
    await preloadScript.initInTargets([target1, target2], false);

    sendCommand
      .withArgs('Page.removeScriptToEvaluateOnNewDocument', {identifier: '1'})
      .rejects(new CloseError('Target closed'));
    sendCommand
      .withArgs('Page.removeScriptToEvaluateOnNewDocument', {identifier: '2'})
      .rejects({
        code: CdpErrorConstants.CONNECTION_CLOSED,
        message: 'Session with given id not found.',
      });

    await preloadScript.remove();

    sinon.assert.calledWithExactly(
      sendCommand,
      'Page.removeScriptToEvaluateOnNewDocument',
      {identifier: '1'},
    );
    sinon.assert.calledWithExactly(
      sendCommand,
      'Page.removeScriptToEvaluateOnNewDocument',
      {identifier: '2'},
    );
    assert.isEmpty(preloadScript.targetIds);
  });

  it('rethrows unexpected errors when removing scripts', async () => {
    const sendCommand = sinon.stub();
    const cdpClient = {
      sendCommand,
      isCloseError: (error: unknown) => error instanceof CloseError,
    } as unknown as CdpClient;
    const target = sinon.createStubInstance(CdpTarget);
    sinon.stub(target, 'id').get(() => CDP_TARGET_ID);
    sinon.stub(target, 'cdpClient').get(() => cdpClient);

    sendCommand
      .withArgs('Page.addScriptToEvaluateOnNewDocument')
      .resolves({identifier: '1'});

    const preloadScript = new PreloadScript({
      functionDeclaration: '() => {}',
    });
    await preloadScript.initInTarget(target, false);

    const unexpectedError = new Error('Unexpected failure');
    sendCommand
      .withArgs('Page.removeScriptToEvaluateOnNewDocument', {identifier: '1'})
      .rejects(unexpectedError);

    let caughtError: unknown;
    try {
      await preloadScript.remove();
    } catch (error) {
      caughtError = error;
    }
    assert.strictEqual(caughtError, unexpectedError);
  });
});
