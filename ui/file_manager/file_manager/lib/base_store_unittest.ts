// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {waitUntil} from '../common/js/test_error_reporting.js';
import type {GetActionFactoryPayload} from '../common/js/util.js';

import type {ActionsProducerGen} from './actions_producer.js';
import {BaseStore, Slice} from './base_store.js';
import {setupTestStore, type TestState, type TestStore} from './for_tests.js';

/** ActionsProducer that raises an error */
async function* producesError(): ActionsProducerGen {
  throw new Error('This error is intentional for tests');
}

/** Returns the current numVisitors from the Store. */
function getNumVisitors(store: TestStore): number|undefined {
  return store.getState().numVisitors;
}

/**
 * Checks that the subscribers receive the initial state when the store is
 * initialized.
 */
export function testStoreInitEmptyState() {
  const {store, subscriber} = setupTestStore();
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
  const {store, subscriber, createTestAction} = setupTestStore();
  store.subscribe(subscriber);

  // It starts un-initialized.
  assertEquals(false, store.isInitialized(), `shouldn't be initialized yet`);

  // Nothing happened yet, so counter is still null.
  assertEquals(undefined, getNumVisitors(store), 'should start undefined');
  assertEquals(0, subscriber.calledCounter, 'no subscriber called yet');

  // The action here doesn't matter, since the reducer always increment by 1.
  store.dispatch(createTestAction());
  store.dispatch(createTestAction());

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
  const {store, subscriber, createTestAction} = setupTestStore();
  store.subscribe(subscriber);

  store.init({numVisitors: 2});
  store.beginBatchUpdate();

  // The action here doesn't matter, since the reducer always increment by 1.
  store.dispatch(createTestAction());
  store.dispatch(createTestAction());
  store.dispatch(createTestAction());

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
  const {store, subscriber, dispatchedActions, createTestAction} =
      setupTestStore();
  assertEquals(0, dispatchedActions.length, 'starts with 0 actions');
  store.subscribe(subscriber);

  // Add some actions before the init().
  store.dispatch(createTestAction('msg 1'));
  store.dispatch(createTestAction('msg 2'));

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

  store.dispatch(createTestAction('msg 3'));

  // Both actions are processed after the init.
  assertEquals(
      3, dispatchedActions.length,
      'Actions after init() are processed directly');
  assertEquals(
      'msg 1, msg 2, msg 3', dispatchedActions.map(a => a.payload!).join(', '));
}

/** Checks that subscriber can unsubscribe and stops receiving updates. */
export function testObserverUnsubscribe() {
  const {store, subscriber, createTestAction} = setupTestStore();
  const unsubscribe = store.subscribe(subscriber);
  store.init({numVisitors: 2});

  assertEquals(1, subscriber.calledCounter, 'subscriber called by init()');

  store.dispatch(createTestAction('msg 1'));
  assertEquals(2, subscriber.calledCounter, 'subscriber called by dispatch()');

  unsubscribe();
  store.dispatch(createTestAction('msg 2'));
  assertEquals(2, subscriber.calledCounter, 'subscriber not called anymore');
}

/**
 * Checks that an exception in one subscriber doesn't stop pushing to other
 * subscribers.
 */
export function testObserverException() {
  const {store, subscriber: successSubscriber, createTestAction} =
      setupTestStore();
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

  store.dispatch(createTestAction('msg 1'));
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
  const {store, subscriber, dispatchedActions, actionsProducerSuccess} =
      setupTestStore();
  store.init({numVisitors: 2});
  store.subscribe(subscriber);

  store.dispatch(actionsProducerSuccess('attempt 1'));

  await waitUntil(() => dispatchedActions.length === 4);

  done();
}

/**
 * Tests that Actions Producers can generate an empty action `undefined` which
 * is ignored.
 */
export async function testStoreActionsProducerEmpty(done: () => void) {
  const {store, subscriber, dispatchedActions, producesEmpty} =
      setupTestStore();
  store.init({numVisitors: 2});
  store.subscribe(subscriber);

  store.dispatch(producesEmpty('trying empty #1'));

  // The AP issues 2 non-empty actions and 1 empty between those.
  await waitUntil(() => dispatchedActions.length === 2);

  done();
}

/**
 * Tests that an error during the Actions Producer doesn't stop other
 * actions.
 */
export async function testStoreActionsProducerError(done: () => void) {
  const {store, subscriber, dispatchedActions, actionsProducerSuccess} =
      setupTestStore();
  store.init({numVisitors: 2});
  store.subscribe(subscriber);

  store.dispatch(producesError());
  store.dispatch(actionsProducerSuccess('attempt 1'));
  store.dispatch(producesError());
  store.dispatch(actionsProducerSuccess('attempt 2'));

  // 4 actions from each Success producer.
  await waitUntil(() => dispatchedActions.length === 8);

  done();
}

/**
 * Tests that the Store throws when passed slices with colliding names.
 */
export function testStoreErrorsOnSliceNameCollision() {
  const slice1 = new Slice<TestState, any>('name');
  const slice2 = new Slice<TestState, any>('name');

  let didError = false;

  try {
    new BaseStore<TestState>({}, [slice1, slice2]);
  } catch (error) {
    didError = true;
  }

  assertEquals(didError, true);
}

/**
 * Tests that Slice::addReducer throws when trying to register actions with
 * the same name.
 */
export function testSliceErrorsOnActionNameCollision() {
  const slice = new Slice<TestState, any>('name');

  let didError = false;

  try {
    slice.addReducer('name', () => ({}));
    slice.addReducer('name', () => ({}));
  } catch (error) {
    didError = true;
  }

  assertEquals(didError, true);
}

/**
 * Tests that Slice::addReducer produces action factories that can be shared
 * across slices to register other reducers.
 */
export function testSliceReducerSplitting() {
  const slice1 = new Slice<TestState, any>('name1');
  const slice2 = new Slice<TestState, any>('name2');

  const doThing = slice1.addReducer(
      'do-thing',
      (state: TestState, payload: number) =>
          ({...state, numVisitors: state.numVisitors! + payload}));

  slice2.addReducer(
      doThing.type,
      (state: TestState, payload: GetActionFactoryPayload<typeof doThing>) =>
          ({...state, numVisitors: state.numVisitors! + payload * 2}));

  const store = new BaseStore<TestState>({}, [slice1, slice2]);
  store.init({numVisitors: 0});
  store.dispatch(doThing(2));

  assertEquals(store.getState().numVisitors, 6 /* =2+2*2 */);
  assertEquals(doThing.type, '[name1] do-thing');
}
