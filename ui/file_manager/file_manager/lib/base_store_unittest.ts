// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {waitUntil} from '../common/js/test_error_reporting.js';

import {ActionsProducerGen} from './actions_producer.js';
import {BaseStore} from './base_store.js';
import {actionsProducerSuccess, TestAction, TestState, TestStore} from './for_tests.js';

/** ActionsProducer that yields an empty/undefined action. */
async function* producesEmpty(payload: string): ActionsProducerGen<TestAction> {
  yield {type: 'step#0', payload};

  yield;

  yield {type: 'final-step', payload};
}

/** ActionsProducer that raises an error */
async function* producesError(): ActionsProducerGen<TestAction> {
  throw new Error('This error is intentional for tests');
}

/** Returns the current numVisitors from the Store. */
function getNumVisitors(store: TestStore): number|undefined {
  return store.getState().numVisitors;
}

/** Counts how many times `onStateChanged()` was called. */
class TestSubscriber {
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
function setupStore() {
  // All actions dispatched via the store and processed by the reducer.
  const dispatchedActions = Array<TestAction>(0);

  /** Test reducer. It's the root and only reducer in these tests. */
  function testReducer(state: TestState, action: TestAction): TestState {
    dispatchedActions.push(action);
    return {
      numVisitors: (state.numVisitors || 0) + 1,
      latestPayload: action.payload,
    };
  }

  const store = new BaseStore({}, testReducer);
  const subscriber = new TestSubscriber();

  return {
    store,
    subscriber,
    dispatchedActions,
  };
}

/**
 * Checks that the subscribers receive the initial state when the store is
 * initialized.
 */
export function testStoreInitEmptyState() {
  const {store, subscriber} = setupStore();
  store.subscribe(subscriber);
  // It starts un-initialized.
  assertEquals(false, store.isInitialized(), `shouldn't be initialized yet`);
  store.init({numVisitors: 2});

  assertEquals(true, store.isInitialized(), 'initialized');
  assertEquals(2, getNumVisitors(store), 'numVisitors should be 2');
  assertEquals(1, subscriber.calledCounter, 'subscriber only called once');
}

/**
 * Checks that the subscribers are only called after the store has been
 * initialized.
 */
export function testStoreInitBatched() {
  const {store, subscriber} = setupStore();
  store.subscribe(subscriber);

  // It starts un-initialized.
  assertEquals(false, store.isInitialized(), `shouldn't be initialized yet`);

  // Nothing happened yet, so counter is still null.
  assertEquals(undefined, getNumVisitors(store), 'should start undefined');
  assertEquals(0, subscriber.calledCounter, 'no subscriber called yet');

  // The action here doesn't matter, since the reducer always increment by 1.
  store.dispatch({type: 'test'});
  store.dispatch({type: 'test'});

  // Since the Store is still un-initialized, the subscriber isn't called.
  assertEquals(undefined, getNumVisitors(store), 'should still be undefined');
  assertEquals(0, subscriber.calledCounter, 'no subscriber called yet');

  store.init({numVisitors: 2});

  assertEquals(true, store.isInitialized(), 'initialized');
  assertEquals(4, getNumVisitors(store), 'numVisitors should be init+2 calls');
  assertEquals(
      1, subscriber.calledCounter,
      'subscriber should be called only once for both actions');
}

/**
 * Checks that the store can be updated in batch mode. As in, multiple calls to
 * dispatch() is only propagated to the subscribers at the end of the batch.
 */
export function testBatchDispatch() {
  const {store, subscriber} = setupStore();
  store.subscribe(subscriber);

  store.init({numVisitors: 2});
  store.beginBatchUpdate();

  // The action here doesn't matter, since the reducer always increment by 1.
  store.dispatch({type: 'test'});
  store.dispatch({type: 'test'});
  store.dispatch({type: 'test'});

  // The subscriber isn't called for the 3 dispatches.
  assertEquals(
      1, subscriber.calledCounter, 'subscriber only called once from init()');

  // However the State in the Store is updated.
  assertEquals(5, getNumVisitors(store), 'should increment to 5');

  store.endBatchUpdate();
  assertEquals(5, getNumVisitors(store), 'numVisitors should be init+3 calls');
  assertEquals(
      2, subscriber.calledCounter,
      'subscriber should be called only once more for all actions');
}

/**
 * Checks that the reducer is called for every action.
 */
export function testDispatch() {
  const {store, subscriber, dispatchedActions} = setupStore();
  assertEquals(0, dispatchedActions.length, 'starts with 0 actions');
  store.subscribe(subscriber);

  // Add some actions before the init().
  store.dispatch({type: 'test', payload: 'msg 1'});
  store.dispatch({type: 'test', payload: 'msg 2'});

  // Dispatches don't happen before the init().
  assertEquals(0, dispatchedActions.length, '0 before the init');

  store.init({numVisitors: 2});

  // Both actions are processed after the init.
  assertEquals(
      2, dispatchedActions.length,
      'all queued actions are processed after the init()');
  // In the same order.
  assertEquals(
      'msg 1, msg 2', dispatchedActions.map(a => a.payload!).join(', '));

  store.dispatch({type: 'test', payload: 'msg 3'});

  // Both actions are processed after the init.
  assertEquals(
      3, dispatchedActions.length,
      'Actions after init() are processed directly');
  assertEquals(
      'msg 1, msg 2, msg 3', dispatchedActions.map(a => a.payload!).join(', '));
}

/** Checks that subscriber can unsubscribe and stops receiving updates. */
export function testObserverUnsubscribe() {
  const {store, subscriber} = setupStore();
  const unsubscribe = store.subscribe(subscriber);
  store.init({numVisitors: 2});

  assertEquals(1, subscriber.calledCounter, 'subscriber called by init()');

  store.dispatch({type: 'test', payload: 'msg 1'});
  assertEquals(2, subscriber.calledCounter, 'subscriber called by dispatch()');

  unsubscribe();
  store.dispatch({type: 'test', payload: 'msg 2'});
  assertEquals(2, subscriber.calledCounter, 'subscriber not called anymore');
}

/**
 * Checks that an exception in one subscriber doesn't stop pushing to other
 * subscribers.
 */
export function testObserverException() {
  const {store, subscriber: successSubscriber} = setupStore();
  store.init({numVisitors: 2});

  // NOTE: Subscribers are called in the order of subscription.
  // So adding the failing subscriber first.
  let errorCounter = 0;
  store.subscribe({
    onStateChanged(_state: TestState) {
      errorCounter += 1;
      throw new Error('always fail subscriber');
    },
  });
  // Add the successful subscriber.
  store.subscribe(successSubscriber);

  assertEquals(
      0, successSubscriber.calledCounter, 'successSubscriber not called yet');
  assertEquals(0, errorCounter, 'errorSubscriber not called yet');

  store.dispatch({type: 'test', payload: 'msg 1'});
  assertEquals(
      1, successSubscriber.calledCounter,
      'successSubscriber called by dispatch()');
  assertEquals(1, errorCounter, 'errorSubscriber called by dispatch()');
}

/**
 * Tests that dispatching an action that is an Actions Producer results in all
 * produced actions being dispatched.
 */
export async function testStoreActionsProducer(done: () => void) {
  const {store, subscriber, dispatchedActions} = setupStore();
  store.init({numVisitors: 2});
  store.subscribe(subscriber);

  store.dispatch(actionsProducerSuccess('attempt 1'));

  await waitUntil(() => dispatchedActions.length == 4);

  done();
}

/**
 * Tests that Actions Producers can generate an empty action `undefined` which
 * is ignored.
 */
export async function testStoreActionsProducerEmpty(done: () => void) {
  const {store, subscriber, dispatchedActions} = setupStore();
  store.init({numVisitors: 2});
  store.subscribe(subscriber);

  store.dispatch(producesEmpty('trying empty #1'));

  // The AP issues 2 non-empty actions and 1 empty between those.
  await waitUntil(() => dispatchedActions.length == 2);

  done();
}

/**
 * Tests that an error during the Actions Producer doesn't stop other
 * actions.
 */
export async function testStoreActionsProducerError(done: () => void) {
  const {store, subscriber, dispatchedActions} = setupStore();
  store.init({numVisitors: 2});
  store.subscribe(subscriber);

  store.dispatch(producesError());
  store.dispatch(actionsProducerSuccess('attempt 1'));
  store.dispatch(producesError());
  store.dispatch(actionsProducerSuccess('attempt 2'));

  // 4 actions from each Success producer.
  await waitUntil(() => dispatchedActions.length == 8);

  done();
}
