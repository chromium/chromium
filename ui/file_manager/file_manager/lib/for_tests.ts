// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ActionsProducerGen} from './actions_producer.js';
import {BaseAction, BaseStore} from './base_store.js';

export type TestStore = BaseStore<TestState, any>;

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

/** Test action used across all unittests in this file. */
export interface TestAction extends BaseAction {
  payload?: string;
}

/**
 * This is an Actions Producer implemented.
 *
 * This produces 4 actions.
 *
 * The return type says this is an Async Generator for TestAction, so each yield
 * returns a TestAction.
 *
 * Imagine that each sleep() is an API call.
 * @param payload: A string to identify the call to foo.
 */
export async function*
    actionsProducerSuccess(payload: string): ActionsProducerGen<TestAction> {
  // Produce an action before any async code.
  yield {type: 'step#0', payload};

  let counter = 0;
  for (const waitingTime of [2, 2]) {
    counter++;
    // Emulate an async API call.
    await sleep(waitingTime);

    yield {type: `step#${counter}`, payload};
  }

  // Yield the final action to the store.
  yield {type: 'final-step', payload};
}
