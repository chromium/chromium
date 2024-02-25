// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ActionsProducerGen} from './actions_producer.js';
import {type Action, BaseStore, Slice} from './base_store.js';

export type TestStore = BaseStore<TestState>;

/** Async function to emulate async API calls. */
export async function sleep(time: number) {
  let resolve: (v: any) => void;
  const p = new Promise((r => resolve = r));
  setTimeout((v: any) => resolve(v), time);
  return p;
}

/** Store state used across all unittests in this file. */
export interface TestState {
  // Just an incremental counter.
  numVisitors?: number;

  // The latest string sent in the Action.
  latestPayload?: string;
}

/** Counts how many times `onStateChanged()` was called. */
export class TestSubscriber {
  calledCounter: number = 0;

  onStateChanged(_state: TestState) {
    this.calledCounter += 1;
  }
}

/**
 * Creates new Store, subscriber and dispatchedActions.
 * These can be called multiple times for each test, which create new
 * independent instances of those.
 */
export function setupTestStore() {
  // All actions dispatched via the store and processed by the reducer.
  const dispatchedActions = Array<Action>(0);
  const testSlice = new Slice<TestState, any>('test');
  const createTestAction = testSlice.addReducer(
      'test', (state: TestState, payload: string|void): TestState => {
        dispatchedActions.push({type: 'test', payload});

        return {
          numVisitors: (state.numVisitors || 0) + 1,
          latestPayload: payload || undefined,
        };
      });
  const createStepAction = testSlice.addReducer(
      'step', (state: TestState, payload: string): TestState => {
        dispatchedActions.push({type: 'step', payload});
        return state;
      });

  /**
   * This is an Actions Producer implemented.
   *
   * This produces 4 actions.
   *
   * Imagine that each sleep() is an API call.
   * @param payload: A string to identify the call to foo.
   */
  async function* actionsProducerSuccess(payload: string): ActionsProducerGen {
    // Produce an action before any async code.
    yield createStepAction(`0 ${payload}`);

    let counter = 0;
    for (const waitingTime of [2, 2]) {
      counter++;
      // Emulate an async API call.
      await sleep(waitingTime);

      yield createStepAction(`${counter} ${payload}`);
    }

    // Yield the final action to the store.
    yield createStepAction(`final ${payload}`);
  }

  /** ActionsProducer that yields an empty/undefined action. */
  async function* producesEmpty(payload: string): ActionsProducerGen {
    yield createStepAction(`0 ${payload}`);

    yield;

    yield createStepAction(`final ${payload}`);
  }

  const store = new BaseStore({}, [testSlice]);
  const subscriber = new TestSubscriber();

  return {
    store,
    subscriber,
    dispatchedActions,
    createTestAction,
    producesEmpty,
    actionsProducerSuccess,
  };
}
