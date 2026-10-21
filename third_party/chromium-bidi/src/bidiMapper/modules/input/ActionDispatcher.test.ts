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

import {beforeEach, describe, it} from 'node:test';

import {assert} from 'chai';
import sinon from 'sinon';

import {Input} from '../../../protocol/protocol.js';
import {Deferred} from '../../../utils/Deferred.js';
import type {CdpTarget} from '../cdp/CdpTarget.js';
import type {BrowsingContextImpl} from '../context/BrowsingContextImpl.js';
import {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';

import {ActionDispatcher} from './ActionDispatcher.js';
import {SourceType} from './InputSource.js';
import {InputState} from './InputState.js';

describe('ActionDispatcher', () => {
  const CONTEXT_ID = 'context_id';

  let browsingContextStorage: sinon.SinonStubbedInstance<BrowsingContextStorage>;
  let sendCommand: sinon.SinonStub;
  let getOrCreateHiddenSandbox: sinon.SinonStub;
  let inputState: InputState;
  let dispatcher: ActionDispatcher;

  beforeEach(() => {
    browsingContextStorage = sinon.createStubInstance(BrowsingContextStorage);
    sendCommand = sinon.stub().resolves({});
    getOrCreateHiddenSandbox = sinon.stub();

    const mockTarget = {
      id: CONTEXT_ID,
      cdpClient: {sendCommand},
    } as unknown as CdpTarget;
    const mockContext = {
      id: CONTEXT_ID,
      cdpTarget: mockTarget,
      getOrCreateHiddenSandbox,
    } as unknown as BrowsingContextImpl;

    browsingContextStorage.getContext.withArgs(CONTEXT_ID).returns(mockContext);

    inputState = new InputState();
    inputState.getOrCreate(
      'mouse',
      SourceType.Pointer,
      Input.PointerType.Mouse,
    );
    dispatcher = new ActionDispatcher(
      inputState,
      browsingContextStorage,
      CONTEXT_ID,
    );
  });

  it('pipelines pointerDown behind zero-duration hover pointerMove without waiting for mouseMoved', async () => {
    // Hold `mouseMoved` unresolved until `mousePressed` is sent, modeling
    // Chromium's rAF-aligned `MainThreadEventQueue` flushed by `kMouseDown`.
    const moveDeferred = new Deferred<Record<string, never>>();
    let moveStillPendingOnPress = false;

    sendCommand.callsFake(
      (method: string, params: {type?: string}): Promise<unknown> => {
        if (method === 'Input.dispatchMouseEvent') {
          if (params.type === 'mouseMoved') {
            return moveDeferred;
          }
          if (params.type === 'mousePressed') {
            moveStillPendingOnPress = !moveDeferred.isFinished;
            moveDeferred.resolve({});
          }
        }
        return Promise.resolve({});
      },
    );

    await dispatcher.dispatchActions([
      [
        {
          id: 'mouse',
          action: {type: 'pointerMove', x: 10, y: 20},
        },
      ],
      [
        {
          id: 'mouse',
          action: {type: 'pointerDown', button: 0},
        },
      ],
      [
        {
          id: 'mouse',
          action: {type: 'pointerUp', button: 0},
        },
      ],
    ]);

    // Verify `mousePressed` was dispatched before `mouseMoved` resolved and without platform evaluation.
    assert.isTrue(moveStillPendingOnPress);
    assert.isFalse(getOrCreateHiddenSandbox.called);
    assert.deepEqual(
      sendCommand.args.map(
        ([method, params]) => `${method}:${(params as {type: string}).type}`,
      ),
      [
        'Input.dispatchMouseEvent:mouseMoved',
        'Input.dispatchMouseEvent:mousePressed',
        'Input.dispatchMouseEvent:mouseReleased',
      ],
    );
  });

  it('flushes trailing deferred hover pointerMove before dispatchActions resolves', async () => {
    // Keep a standalone zero-duration hover `mouseMoved` pending to verify final flush.
    const moveDeferred = new Deferred<Record<string, never>>();
    sendCommand.returns(moveDeferred);

    let dispatchCompleted = false;
    const dispatchPromise = (async () => {
      await dispatcher.dispatchActions([
        [
          {
            id: 'mouse',
            action: {type: 'pointerMove', x: 15, y: 25},
          },
        ],
      ]);
      dispatchCompleted = true;
    })();

    // Yield microtasks and verify `dispatchActions` waits for the trailing `mouseMoved`.
    await Promise.resolve();
    assert.isFalse(dispatchCompleted);

    moveDeferred.resolve({});
    await dispatchPromise;
    assert.isTrue(dispatchCompleted);
  });

  it('computes multi-click clickCount from simulated tick durations excluding CDP IPC latency', async () => {
    // Advance wall-clock `performance.now()` by 400ms per CDP command so two
    // `mousePressed`/`mouseReleased` round-trips exceed the 500ms double-click threshold.
    let fakeNow = 1000;
    const nowStub = sinon
      .stub(performance, 'now')
      .callsFake(() => (fakeNow += 400));

    try {
      await dispatcher.dispatchActions([
        [{id: 'mouse', action: {type: 'pointerDown', button: 0}}],
        [{id: 'mouse', action: {type: 'pointerUp', button: 0}}],
        [{id: 'mouse', action: {type: 'pointerDown', button: 0}}],
        [{id: 'mouse', action: {type: 'pointerUp', button: 0}}],
      ]);
    } finally {
      nowStub.restore();
    }

    const pressedClickCounts = sendCommand.args
      .filter(
        ([method, params]) =>
          method === 'Input.dispatchMouseEvent' &&
          (params as {type: string}).type === 'mousePressed',
      )
      .map(([, params]) => (params as {clickCount: number}).clickCount);

    assert.deepEqual(pressedClickCounts, [1, 2]);
  });
});
