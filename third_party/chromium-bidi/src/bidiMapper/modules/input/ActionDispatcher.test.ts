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

import {afterEach, beforeEach, describe, it} from 'node:test';
import vm from 'node:vm';

import {assert} from 'chai';
import sinon from 'sinon';

import {
  Input,
  MoveTargetOutOfBoundsException,
} from '../../../protocol/protocol.js';
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

  afterEach(() => {
    sinon.restore();
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
    sinon.stub(performance, 'now').callsFake(() => (fakeNow += 400));

    await dispatcher.dispatchActions([
      [{id: 'mouse', action: {type: 'pointerDown', button: 0}}],
      [{id: 'mouse', action: {type: 'pointerUp', button: 0}}],
      [{id: 'mouse', action: {type: 'pointerDown', button: 0}}],
      [{id: 'mouse', action: {type: 'pointerUp', button: 0}}],
    ]);

    const pressedClickCounts = sendCommand.args
      .filter(
        ([method, params]) =>
          method === 'Input.dispatchMouseEvent' &&
          (params as {type: string}).type === 'mousePressed',
      )
      .map(([, params]) => (params as {clickCount: number}).clickCount);

    assert.deepEqual(pressedClickCounts, [1, 2]);
  });

  it('waits once for an animation frame at the end of wheel scroll actions', async () => {
    // Model Chromium acknowledging passive-listener `mouseWheel` events before
    // the main thread dispatches them at the next animation frame.
    inputState.getOrCreate('wheel', SourceType.Wheel);
    const animationFrame = new Deferred<unknown>();
    const callFunction = sinon.stub().returns(animationFrame);
    getOrCreateHiddenSandbox.resolves({callFunction});

    let dispatchCompleted = false;
    const dispatchPromise = (async () => {
      await dispatcher.dispatchActions([
        [
          {
            id: 'wheel',
            action: {type: 'scroll', x: 0, y: 0, deltaX: 5, deltaY: 10},
          },
        ],
        [
          {
            id: 'wheel',
            action: {type: 'scroll', x: 0, y: 0, deltaX: 10, deltaY: 20},
          },
        ],
      ]);
      dispatchCompleted = true;
    })();

    // Let both scroll ticks and the final frame wait start, then verify that
    // both ticks ran before dispatch waits once at the end of `dispatchActions`.
    await new Promise((resolve) => setTimeout(resolve, 0));
    assert.isFalse(dispatchCompleted);
    assert.isTrue(sendCommand.calledTwice);
    assert.isTrue(callFunction.calledOnce);
    assert.isTrue(callFunction.firstCall.args[1], 'awaitPromise');

    animationFrame.resolve({type: 'success'});
    await dispatchPromise;
    assert.isTrue(dispatchCompleted);
    assert.isTrue(callFunction.calledOnce);
    // Warmed up once on Tick 1 (skipped on Tick 2 because rAF input is already
    // pending), then queried once in `#waitForRafAlignedInput()`.
    assert.strictEqual(getOrCreateHiddenSandbox.callCount, 2);
  });

  it('waits for an animation frame only when a touchMove or mouseWheel CDP command is sent', async () => {
    inputState.getOrCreate(
      'touch',
      SourceType.Pointer,
      Input.PointerType.Touch,
    );
    inputState.getOrCreate('wheel', SourceType.Wheel);
    const callFunction = sinon.stub().resolves({type: 'success'});
    getOrCreateHiddenSandbox.resolves({callFunction});

    // Unpressed touch move dispatches no `touchMove` CDP command.
    await dispatcher.dispatchActions([
      [{id: 'touch', action: {type: 'pointerMove', x: 30, y: 40}}],
    ]);
    assert.isFalse(getOrCreateHiddenSandbox.called);
    assert.isFalse(callFunction.called);

    // Zero-delta scroll and pressed touch move to the current position send no
    // `mouseWheel` / `touchMove` CDP command.
    await dispatcher.dispatchActions([
      [
        {
          id: 'wheel',
          action: {type: 'scroll', x: 0, y: 0, deltaX: 0, deltaY: 0},
        },
      ],
      [{id: 'touch', action: {type: 'pointerDown', button: 0}}],
      [{id: 'touch', action: {type: 'pointerMove', x: 30, y: 40}}],
      [{id: 'touch', action: {type: 'pointerUp', button: 0}}],
    ]);
    assert.isFalse(getOrCreateHiddenSandbox.called);
    assert.isFalse(callFunction.called);

    // Pressed touch move to a new position warms up the hidden sandbox while
    // `touchMove` is in flight and waits once for rAF at the end.
    const touchMoveDeferred = new Deferred<Record<string, never>>();
    sendCommand.callsFake(
      (method: string, params: {type?: string}): Promise<unknown> => {
        if (
          method === 'Input.dispatchTouchEvent' &&
          params.type === 'touchMove'
        ) {
          return touchMoveDeferred;
        }
        return Promise.resolve({});
      },
    );
    const dispatchPromise = dispatcher.dispatchActions([
      [{id: 'touch', action: {type: 'pointerDown', button: 0}}],
      [{id: 'touch', action: {type: 'pointerMove', x: 10, y: 20}}],
      [{id: 'touch', action: {type: 'pointerUp', button: 0}}],
    ]);

    await new Promise((resolve) => setTimeout(resolve, 0));
    assert.isTrue(getOrCreateHiddenSandbox.calledOnce);
    assert.isFalse(callFunction.called);

    touchMoveDeferred.resolve({});
    await dispatchPromise;
    assert.isTrue(callFunction.calledOnce);
  });

  it('resolves when waiting for an animation frame fails and avoids unhandled warmup rejections', async () => {
    inputState.getOrCreate('wheel', SourceType.Wheel);
    getOrCreateHiddenSandbox.rejects(new Error('context destroyed'));

    await dispatcher.dispatchActions([
      [
        {
          id: 'wheel',
          action: {type: 'scroll', x: 0, y: 0, deltaX: 10, deltaY: 20},
        },
      ],
    ]);
    assert.isTrue(getOrCreateHiddenSandbox.called);

    // A rejected warmup must not emit an unhandled rejection when `sendCommand`
    // also fails and skips `#waitForRafAlignedInput()`.
    const unhandled: unknown[] = [];
    const onUnhandledRejection = (reason: unknown) => {
      unhandled.push(reason);
    };
    process.on('unhandledRejection', onUnhandledRejection);
    try {
      const wheelDeferred = new Deferred<Record<string, never>>();
      sendCommand.returns(wheelDeferred);
      const sendError = new Error('target closed');
      const dispatchPromise = dispatcher.dispatchActions([
        [
          {
            id: 'wheel',
            action: {type: 'scroll', x: 0, y: 0, deltaX: 10, deltaY: 20},
          },
        ],
      ]);

      // Let the warmup rejection settle across a macrotask before rejecting
      // `sendCommand`.
      await new Promise((resolve) => setTimeout(resolve, 0));
      wheelDeferred.reject(sendError);

      let thrown: unknown;
      try {
        await dispatchPromise;
      } catch (err) {
        thrown = err;
      }
      await new Promise((resolve) => setTimeout(resolve, 0));
      assert.strictEqual(thrown, sendError);
      assert.deepEqual(unhandled, []);
    } finally {
      process.off('unhandledRejection', onUnhandledRejection);
    }

    // A transient warmup rejection must not be sticky on `#waitForRafAlignedInput()`.
    const callFunction = sinon.stub().resolves({type: 'success'});
    getOrCreateHiddenSandbox.reset();
    getOrCreateHiddenSandbox.onFirstCall().rejects(new Error('transient'));
    getOrCreateHiddenSandbox.onSecondCall().resolves({callFunction});
    sendCommand.resolves({});

    await dispatcher.dispatchActions([
      [
        {
          id: 'wheel',
          action: {type: 'scroll', x: 0, y: 0, deltaX: 10, deltaY: 20},
        },
      ],
    ]);
    assert.isTrue(getOrCreateHiddenSandbox.calledTwice);
    assert.isTrue(callFunction.calledOnce);
  });

  it('flushes pending rAF-aligned input before querying element-origin coordinates on a later tick', async () => {
    inputState.getOrCreate('wheel', SourceType.Wheel);
    const events: string[] = [];
    const callFunction = sinon.stub().callsFake((_decl, awaitPromise) => {
      if (awaitPromise) {
        events.push('rAF');
        return Promise.resolve({type: 'success'});
      }
      events.push('elementCenter');
      return Promise.resolve({
        type: 'success',
        result: {
          type: 'array',
          value: [
            {type: 'number', value: 50},
            {type: 'number', value: 60},
          ],
        },
      });
    });
    getOrCreateHiddenSandbox.resolves({callFunction});

    await dispatcher.dispatchActions([
      [
        {
          id: 'wheel',
          action: {type: 'scroll', x: 0, y: 0, deltaX: 0, deltaY: 20},
        },
      ],
      [
        {
          id: 'mouse',
          action: {
            type: 'pointerMove',
            x: 0,
            y: 0,
            origin: {type: 'element', element: {sharedId: 'el_1'}},
          },
        },
      ],
    ]);

    // The scroll tick's rAF-aligned input is flushed before querying element
    // bounds, and is not flushed a second time at the end of `dispatchActions`.
    assert.deepEqual(events, ['rAF', 'elementCenter']);
  });

  it('flushes pending rAF-aligned input on the local-root context even when a later tick throws', async () => {
    const childContextId = 'child_context_id';
    const childGetOrCreateHiddenSandbox = sinon.stub();
    const childContext = {
      id: childContextId,
      cdpTarget: {
        id: CONTEXT_ID,
        cdpClient: {sendCommand},
      } as unknown as CdpTarget,
      getOrCreateHiddenSandbox: childGetOrCreateHiddenSandbox,
    } as unknown as BrowsingContextImpl;
    browsingContextStorage.getContext
      .withArgs(childContextId)
      .returns(childContext);

    const events: string[] = [];
    sendCommand.callsFake((method: string) => {
      events.push(method);
      if (method === 'DOM.getFrameOwner') {
        return Promise.resolve({backendNodeId: 42});
      }
      if (method === 'DOM.getBoxModel') {
        return Promise.resolve({model: {content: [10, 20]}});
      }
      return Promise.resolve({});
    });

    const callFunction = sinon.stub().callsFake(() => {
      events.push('rAF');
      return Promise.resolve({type: 'success'});
    });
    getOrCreateHiddenSandbox.resolves({callFunction});

    inputState.getOrCreate('wheel', SourceType.Wheel);
    const childDispatcher = new ActionDispatcher(
      inputState,
      browsingContextStorage,
      childContextId,
    );

    // Tick 1 scrolls in the child frame; Tick 2 queries child-frame layout in
    // `#getFrameOffset()` (which flushes Tick 1's scroll on the local-root
    // context before `DOM.getFrameOwner`) and then throws
    // `MoveTargetOutOfBoundsException`.
    let thrown: unknown;
    try {
      await childDispatcher.dispatchActions([
        [
          {
            id: 'wheel',
            action: {type: 'scroll', x: 0, y: 0, deltaX: 0, deltaY: 15},
          },
        ],
        [
          {
            id: 'mouse',
            action: {type: 'pointerMove', x: -100, y: -100},
          },
        ],
      ]);
    } catch (err) {
      thrown = err;
    }

    assert.instanceOf(thrown, MoveTargetOutOfBoundsException);
    assert.isTrue(getOrCreateHiddenSandbox.called);
    assert.isFalse(childGetOrCreateHiddenSandbox.called);
    assert.deepEqual(events, [
      'DOM.getFrameOwner',
      'DOM.getBoxModel',
      'Input.dispatchMouseEvent',
      'rAF',
      'DOM.getFrameOwner',
      'DOM.getBoxModel',
    ]);

    // Also verify the `finally` block flushes when a top-level tick throws
    // without querying layout first.
    events.length = 0;
    thrown = undefined;
    try {
      await dispatcher.dispatchActions([
        [
          {
            id: 'wheel',
            action: {type: 'scroll', x: 0, y: 0, deltaX: 0, deltaY: 15},
          },
        ],
        [
          {
            id: 'mouse',
            action: {type: 'pointerMove', x: -100, y: -100},
          },
        ],
      ]);
    } catch (err) {
      thrown = err;
    }

    assert.instanceOf(thrown, MoveTargetOutOfBoundsException);
    assert.deepEqual(events, ['Input.dispatchMouseEvent', 'rAF']);
  });

  it('flushes trailing deferred hover pointerMove and rAF-aligned input in parallel', async () => {
    inputState.getOrCreate('wheel', SourceType.Wheel);
    const moveDeferred = new Deferred<Record<string, never>>();
    const animationFrame = new Deferred<unknown>();
    const callFunction = sinon.stub().returns(animationFrame);
    getOrCreateHiddenSandbox.resolves({callFunction});

    sendCommand.callsFake(
      (method: string, params: {type?: string}): Promise<unknown> => {
        if (
          method === 'Input.dispatchMouseEvent' &&
          params.type === 'mouseMoved'
        ) {
          return moveDeferred;
        }
        return Promise.resolve({});
      },
    );

    let dispatchCompleted = false;
    const dispatchPromise = (async () => {
      await dispatcher.dispatchActions([
        [
          {
            id: 'wheel',
            action: {type: 'scroll', x: 0, y: 0, deltaX: 0, deltaY: 20},
          },
        ],
        [
          {
            id: 'mouse',
            action: {type: 'pointerMove', x: 10, y: 20},
          },
        ],
      ]);
      dispatchCompleted = true;
    })();

    // Let Tick 1 (`mouseWheel`), Tick 2 (deferred `mouseMoved`), and `finally`
    // start: `rAF` must start while `mouseMoved` is still unresolved.
    await new Promise((resolve) => setTimeout(resolve, 0));
    assert.isFalse(dispatchCompleted);
    assert.isTrue(callFunction.calledOnce);

    // Resolving only `rAF` must still wait for the deferred `mouseMoved`.
    animationFrame.resolve({type: 'success'});
    await new Promise((resolve) => setTimeout(resolve, 0));
    assert.isFalse(dispatchCompleted);

    moveDeferred.resolve({});
    await dispatchPromise;
    assert.isTrue(dispatchCompleted);
  });

  it('warms up the local-root hidden sandbox while dispatching rAF-aligned input without duplicating in-flight creation', async () => {
    inputState.getOrCreate('wheel', SourceType.Wheel);
    inputState.getOrCreate('wheel2', SourceType.Wheel);
    const wheelDeferred = new Deferred<Record<string, never>>();
    const warmupDeferred = new Deferred<{callFunction: sinon.SinonStub}>();
    const staleCallFunction = sinon.stub().rejects(new Error('stale context'));
    const liveCallFunction = sinon.stub().resolves({type: 'success'});
    getOrCreateHiddenSandbox.onFirstCall().returns(warmupDeferred);
    getOrCreateHiddenSandbox
      .onSecondCall()
      .resolves({callFunction: liveCallFunction});

    let wheelCount = 0;
    sendCommand.callsFake(
      (method: string, params: {type?: string}): Promise<unknown> => {
        if (
          method === 'Input.dispatchMouseEvent' &&
          params.type === 'mouseWheel' &&
          ++wheelCount <= 2
        ) {
          return wheelDeferred;
        }
        return Promise.resolve({});
      },
    );

    const dispatchPromise = dispatcher.dispatchActions([
      [
        {
          id: 'wheel',
          action: {type: 'scroll', x: 0, y: 0, deltaX: 0, deltaY: 10},
        },
        // Second concurrent scroll in the same tick while `#hasPendingRafAlignedInput`
        // is still `false` exercises `#inFlightHiddenSandboxWarmup` deduplication.
        {
          id: 'wheel2',
          action: {type: 'scroll', x: 0, y: 0, deltaX: 5, deltaY: 0},
        },
      ],
      [
        {
          id: 'wheel',
          action: {type: 'scroll', x: 0, y: 0, deltaX: 0, deltaY: 20},
        },
      ],
    ]);

    // Hidden sandbox creation starts once while both Tick 1 `mouseWheel`
    // commands are in flight, and `rAF` does not run until input completes.
    await new Promise((resolve) => setTimeout(resolve, 0));
    assert.isTrue(getOrCreateHiddenSandbox.calledOnce);
    assert.isFalse(liveCallFunction.called);

    // Completing both ticks while the warmup is still in flight must not
    // trigger a second `getOrCreateHiddenSandbox()` call until the warmup
    // settles.
    wheelDeferred.resolve({});
    await new Promise((resolve) => setTimeout(resolve, 0));
    assert.isTrue(getOrCreateHiddenSandbox.calledOnce);
    assert.isFalse(staleCallFunction.called);
    assert.isFalse(liveCallFunction.called);

    // Once the warmup settles, `#waitForRafAlignedInput()` queries the live
    // sandbox from `BrowsingContextImpl` (avoiding any stale pre-navigation
    // realm) and runs `rAF` there.
    warmupDeferred.resolve({callFunction: staleCallFunction});
    await dispatchPromise;
    assert.isTrue(getOrCreateHiddenSandbox.calledTwice);
    assert.isFalse(staleCallFunction.called);
    assert.isTrue(liveCallFunction.calledOnce);
  });

  it('flushes pending rAF-aligned input before starting a timed tick', async () => {
    inputState.getOrCreate('wheel', SourceType.Wheel);
    const animationFrame = new Deferred<unknown>();
    const events: string[] = [];
    const callFunction = sinon.stub().callsFake(() => {
      events.push('rAF');
      return animationFrame;
    });
    getOrCreateHiddenSandbox.resolves({callFunction});
    sendCommand.callsFake((_method: string, params: {type?: string}) => {
      events.push(params.type ?? 'unknown');
      return Promise.resolve({});
    });

    const dispatchPromise = dispatcher.dispatchActions([
      [
        {
          id: 'wheel',
          action: {type: 'scroll', x: 0, y: 0, deltaX: 0, deltaY: 20},
        },
      ],
      [
        {
          id: 'mouse',
          action: {type: 'pointerMove', x: 10, y: 20, duration: 10},
        },
      ],
    ]);

    // Tick 2 (`duration > 0`) must flush Tick 1's rAF-aligned scroll before
    // recording `#tickStart` and dispatching `mouseMoved`.
    await new Promise((resolve) => setTimeout(resolve, 0));
    assert.deepEqual(events, ['mouseWheel', 'rAF']);

    animationFrame.resolve({type: 'success'});
    await dispatchPromise;
    assert.isTrue(callFunction.calledOnce);
    assert.strictEqual(events[0], 'mouseWheel');
    assert.strictEqual(events[1], 'rAF');
    assert.isTrue(events.slice(2).includes('mouseMoved'));
  });

  it('resolves the rAF helper on animation frame, hidden document, or visibility loss', async () => {
    inputState.getOrCreate('wheel', SourceType.Wheel);
    const callFunction = sinon.stub().resolves({type: 'success'});
    getOrCreateHiddenSandbox.resolves({callFunction});

    await dispatcher.dispatchActions([
      [
        {
          id: 'wheel',
          action: {type: 'scroll', x: 0, y: 0, deltaX: 0, deltaY: 10},
        },
      ],
    ]);
    const decl = callFunction.firstCall.args[0] as string;

    const runHelper = (initialVisibility: string) => {
      const listenersByType = new Map<string, Set<() => void>>();
      const getListeners = (type: string) => {
        let set = listenersByType.get(type);
        if (!set) {
          set = new Set();
          listenersByType.set(type, set);
        }
        return set;
      };
      const rafCallbacks = new Map<number, () => void>();
      let nextRafId = 1;
      const canceledRafIds: number[] = [];
      const doc = {
        visibilityState: initialVisibility,
        addEventListener: (type: string, cb: () => void) => {
          getListeners(type).add(cb);
        },
        removeEventListener: (type: string, cb: () => void) => {
          listenersByType.get(type)?.delete(cb);
        },
      };
      const promise = vm.runInNewContext(`(${decl})()`, {
        document: doc,
        requestAnimationFrame: (cb: () => void) => {
          const id = nextRafId++;
          rafCallbacks.set(id, cb);
          return id;
        },
        cancelAnimationFrame: (id: number) => {
          canceledRafIds.push(id);
          rafCallbacks.delete(id);
        },
      }) as Promise<void>;
      return {doc, listenersByType, rafCallbacks, canceledRafIds, promise};
    };

    // 1. Hidden document resolves immediately without scheduling rAF.
    {
      const {listenersByType, rafCallbacks, promise} = runHelper('hidden');
      await promise;
      assert.strictEqual(rafCallbacks.size, 0);
      assert.strictEqual(listenersByType.size, 0);
    }

    // 2. Visible document waits for rAF and removes its visibilitychange listener.
    {
      const {listenersByType, rafCallbacks, promise} = runHelper('visible');
      const visibilityListeners = listenersByType.get('visibilitychange');
      let resolved = false;
      const done = (async () => {
        await promise;
        resolved = true;
      })();
      await new Promise((resolve) => setTimeout(resolve, 0));
      assert.isFalse(resolved);
      assert.strictEqual(rafCallbacks.size, 1);
      assert.strictEqual(visibilityListeners?.size, 1);

      const [cb] = rafCallbacks.values();
      cb!();
      await done;
      assert.isTrue(resolved);
      assert.strictEqual(visibilityListeners?.size, 0);
    }

    // 3. Becoming hidden before rAF cancels rAF, cleans up, and resolves;
    // a visibilitychange event while still visible does not resolve early.
    {
      const {doc, listenersByType, rafCallbacks, canceledRafIds, promise} =
        runHelper('visible');
      const visibilityListeners = listenersByType.get('visibilitychange');
      let resolved = false;
      const done = (async () => {
        await promise;
        resolved = true;
      })();
      await new Promise((resolve) => setTimeout(resolve, 0));
      assert.isFalse(resolved);
      assert.strictEqual(rafCallbacks.size, 1);
      assert.strictEqual(visibilityListeners?.size, 1);

      for (const listener of [...visibilityListeners!]) {
        listener();
      }
      await new Promise((resolve) => setTimeout(resolve, 0));
      assert.isFalse(resolved);
      assert.deepEqual(canceledRafIds, []);
      assert.strictEqual(visibilityListeners?.size, 1);

      doc.visibilityState = 'hidden';
      for (const listener of [...visibilityListeners!]) {
        listener();
      }
      await done;
      assert.isTrue(resolved);
      assert.deepEqual(canceledRafIds, [1]);
      assert.strictEqual(visibilityListeners?.size, 0);
    }
  });

  it('holds concurrent element-origin queries in the same tick behind one rAF flush', async () => {
    inputState.getOrCreate('wheel', SourceType.Wheel);
    inputState.getOrCreate(
      'mouse2',
      SourceType.Pointer,
      Input.PointerType.Mouse,
    );
    const animationFrame = new Deferred<unknown>();
    const events: string[] = [];
    const callFunction = sinon.stub().callsFake((_decl, awaitPromise) => {
      if (awaitPromise) {
        events.push('rAF');
        return animationFrame;
      }
      events.push('elementCenter');
      return Promise.resolve({
        type: 'success',
        result: {
          type: 'array',
          value: [
            {type: 'number', value: 50},
            {type: 'number', value: 60},
          ],
        },
      });
    });
    getOrCreateHiddenSandbox.resolves({callFunction});

    const dispatchPromise = dispatcher.dispatchActions([
      [
        {
          id: 'wheel',
          action: {type: 'scroll', x: 0, y: 0, deltaX: 0, deltaY: 20},
        },
      ],
      [
        {
          id: 'mouse',
          action: {
            type: 'pointerMove',
            x: 0,
            y: 0,
            origin: {type: 'element', element: {sharedId: 'el_1'}},
          },
        },
        {
          id: 'mouse2',
          action: {
            type: 'pointerMove',
            x: 0,
            y: 0,
            origin: {type: 'element', element: {sharedId: 'el_2'}},
          },
        },
      ],
    ]);

    await new Promise((resolve) => setTimeout(resolve, 0));
    assert.deepEqual(events, ['rAF']);

    animationFrame.resolve({type: 'success'});
    await dispatchPromise;
    assert.deepEqual(events, ['rAF', 'elementCenter', 'elementCenter']);
  });
});
