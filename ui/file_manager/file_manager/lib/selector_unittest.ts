// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {waitForElementUpdate} from '../common/js/unittest_util.js';
import {customElement, html, XfBase} from '../widgets/xf_base.js';

import {BaseStore, Slice} from './base_store.js';
import type {TestState} from './for_tests.js';
import {combine1Selector, combine2Selectors, SelectorEmitter, SelectorNode, shallowEqual, strictlyEqual} from './selector.js';

// Test that DAG nodes only emit if at least one of their parents also emits a
// new value and if their value has changed.
export function testSelectorEmitter() {
  let state = {slice1: 123, slice2: 'asd'};

  const selectorEmitter = new SelectorEmitter();

  const rootNode = SelectorNode.createSourceNode(() => state);
  selectorEmitter.addSource(rootNode);

  const slice1Node = new SelectorNode([rootNode], (state) => state.slice1);
  const slice2Node = new SelectorNode([rootNode], (state) => state.slice2);
  const nestedSlice2Node =
      new SelectorNode([slice2Node], (slice2) => slice2 + 'qwerty');

  // Verify initial states.
  assertEquals(rootNode.get(), state);
  assertEquals(slice1Node.get(), state.slice1);
  assertEquals(slice2Node.get(), state.slice2);
  assertEquals(nestedSlice2Node.get(), 'asdqwerty');

  let rootCount = 0;
  let child1Count = 0;
  let child2Count = 0;
  let nestedChildCount = 0;
  rootNode.subscribe(() => rootCount++);
  slice1Node.subscribe(() => child1Count++);
  slice2Node.subscribe(() => child2Count++);
  nestedSlice2Node.subscribe(() => nestedChildCount++);

  // Only the root selector and the slice1 selector should emit.
  state = {...state, slice1: 9999};
  selectorEmitter.processChange();

  assertEquals(rootCount, 1);
  assertEquals(child1Count, 1);
  assertEquals(child2Count, 0);
  assertEquals(nestedChildCount, 0);
  assertEquals(slice1Node.get(), 9999);
  assertEquals(slice2Node.get(), 'asd');
  assertEquals(nestedSlice2Node.get(), 'asdqwerty');

  // Only root, slice2, and slice2's child should update.
  state = {...state, slice2: 'qwerty'};
  selectorEmitter.processChange();

  assertEquals(rootCount, 2);
  assertEquals(child1Count, 1);
  assertEquals(child2Count, 1);
  assertEquals(nestedChildCount, 1);
  assertEquals(slice1Node.get(), 9999);
  assertEquals(slice2Node.get(), 'qwerty');
  assertEquals(nestedSlice2Node.get(), 'qwertyqwerty');
}

/**
 * Tests selectors' `get()` and `subscribe()` functions.
 */
export function testSelectorGetAndSubscribe() {
  const numVisitorsSlice =
      new Slice<TestState, TestState['numVisitors']>('numVisitors');

  const increaseBy = numVisitorsSlice.addReducer(
      'increase-by',
      (state: TestState, payload: number) =>
          ({...state, numVisitors: state.numVisitors! + payload}));

  let counter = 0;
  const notifiedValues: Array<TestState['numVisitors']> = [];
  numVisitorsSlice.selector.subscribe((numVisitors) => {
    counter++;
    notifiedValues.push(numVisitors);
  });

  // The store hasn't been initialized yet, so the slice's state should be
  // undefined.
  assertEquals(numVisitorsSlice.selector.get(), undefined);

  const store = new BaseStore<TestState>({}, [numVisitorsSlice]);
  store.init({numVisitors: 0});

  // Now that the store has been initialized with `numVisitors: 0`, the slice's
  // state should be 0.
  assertEquals(numVisitorsSlice.selector.get(), 0);

  store.dispatch(increaseBy(2));

  // Similarly, it should reflect the numVisitors being increased to 2 by the
  // reducer handling the action `increaseBy(2)`.
  assertEquals(numVisitorsSlice.selector.get(), 2);
  assertEquals(counter, 2);
  assertEquals(notifiedValues[0], 0);
  assertEquals(notifiedValues[1], 2);

  // Introduce a combined selector and verify it works as expected.
  const combined = combine2Selectors(
      (storeState, sliceState) => JSON.stringify(storeState) + sliceState,
      store.selector, numVisitorsSlice.selector);
  assertEquals(combined.get(), '{"numVisitors":2}2');
  store.dispatch(increaseBy(1));
  assertEquals(combined.get(), '{"numVisitors":3}3');
}

// Test that Lit components work with selector's ReactiveControllers.
export async function testSelectorController(done: () => void) {
  const slice = new Slice<TestState, TestState['numVisitors']>('numVisitors');

  const increaseBy = slice.addReducer(
      'increase-by',
      (state: TestState, payload: number) =>
          ({...state, numVisitors: state.numVisitors! + payload}));

  const store = new BaseStore<TestState>({}, [slice]);
  store.init({numVisitors: 0});

  // Create a test lit component using our selector's controller.
  @customElement('xf-test')
  class XfTest extends XfBase {
    testCtrl = slice.selector.createController(this);

    override render() {
      return html`
      <div id="test">${this.testCtrl.value}</div>
    `;
    }
  }

  // Silence unused var TS error.
  console.info(!!XfTest);

  // Create test element and add it to the DOM, then wait for it to be ready on
  // the next event loop cycle.
  const testEl = document.createElement('xf-test');
  document.body.appendChild(testEl);
  await waitForElementUpdate(testEl);

  const testDiv = testEl.shadowRoot!.querySelector<HTMLDivElement>('#test')!;
  assertEquals(testDiv.textContent, '0');

  // Update the store and verify that the component automatically re-renders.
  store.dispatch(increaseBy(2));
  await waitForElementUpdate(testEl);


  assertEquals(testDiv.textContent, '2');

  done();
}

// Test that if one selector's subscriber errors out, the remaining subscribers
// are still notified.
export function testSubscriberErrorIsSelfContained() {
  let state = 0;

  const selectorEmitter = new SelectorEmitter();

  const rootNode = SelectorNode.createSourceNode(() => state);
  selectorEmitter.addSource(rootNode);

  let count = 0;
  rootNode.subscribe(() => count++);
  rootNode.subscribe(() => {
    throw new Error('Boom!');
  });
  rootNode.subscribe(() => count++);

  state = 1;
  selectorEmitter.processChange();

  assertEquals(count, 2);
}

// Test that updates are no longer received after the selector subscription is
// cancelled.
export function testUnsubscribing() {
  let state = 0;

  const selectorEmitter = new SelectorEmitter();

  const rootNode = SelectorNode.createSourceNode(() => state);
  selectorEmitter.addSource(rootNode);

  let count = 0;
  const unsubscribe = rootNode.subscribe(() => count++);

  state = 1;
  selectorEmitter.processChange();
  assertEquals(count, 1);

  // Count should no longer increase after calling unsubscribe.
  unsubscribe();

  state = 2;
  selectorEmitter.processChange();
  assertEquals(count, 1);  // Count should still be 1, not 2.
}

// Test that deleted selectors no longer provide updates.
export function testDeletingSelector() {
  let state = 0;

  const selectorEmitter = new SelectorEmitter();

  const rootNode = SelectorNode.createSourceNode(() => state);
  selectorEmitter.addSource(rootNode);

  let count = 0;
  rootNode.subscribe(() => count++);

  state = 1;
  selectorEmitter.processChange();
  assertEquals(count, 1);

  // Count should no longer increase after deleting the selector.
  rootNode.delete();

  state = 2;
  selectorEmitter.processChange();
  assertEquals(count, 1);  // Count should still be 1, not 2.
}

// Test that Selector Emitter nodes are explored in the ascending order of
// depth.
export function testSelectorEmitterTraversal() {
  let state = 0;

  const selectorEmitter = new SelectorEmitter();

  const rootNode = SelectorNode.createSourceNode(() => state);
  selectorEmitter.addSource(rootNode);

  const child1 = new SelectorNode([rootNode], (r) => r + 1);
  const child2 = new SelectorNode([rootNode, child1], (r, c1) => r + c1 + 1);
  const child3 = new SelectorNode([rootNode, child2], (r, c2) => r + c2 + 1);

  let order = '';
  rootNode.subscribe(() => order += 0);
  child1.subscribe(() => order += 1);
  child2.subscribe(() => order += 2);
  child3.subscribe(() => order += 3);

  state = 1;
  selectorEmitter.processChange();

  // Verify nodes emit in the expected order (lowest depth first).
  // Explanation: after the root is explored, all 3 children should be queued
  // for later exploration. If they are explored in the wrong order, we'd get
  // '0321', instead of '0123'. Note: Although all child nodes are children of
  // root, child1 has a depth of 1, child2 of 2, and child3 of 3 - because the
  // root node isn't their only parent.
  assertEquals(order, '0123');
}

// Test that strictlyEqual and shallowEqual works in different ways.
export function testCustomCompare() {
  const obj1 = {a: 'aaa', b: 123};
  const obj2 = {a: 'aaa', b: 123};

  assertTrue(shallowEqual(obj1, obj2));
  assertFalse(strictlyEqual(obj1, obj2));

  // shallowEqual only compares the first level, it doesn't go deeper.
  const b = {b: 'bbb'};
  const obj3 = {a: b};
  const obj4 = {a: {b: 'bbb'}};
  const obj5 = {a: b};

  assertTrue(shallowEqual(obj3, obj5));
  assertFalse(shallowEqual(obj3, obj4));
  assertFalse(strictlyEqual(obj3, obj4));
  assertFalse(strictlyEqual(obj3, obj5));

  // shallowEqual can't be called with non-object.
  try {
    shallowEqual('aaa' as any, 'bbb' as any);
    assertNotReached();
  } catch (e: unknown) {
    assertTrue(e instanceof Error);
  }
}

// Test that selector can accepts a custom isEqual function which will be used
// when checking changes.
export function testSelectorCustomEqual() {
  const slice1 = new Slice<TestState, TestState['numVisitors']>('numVisitors');
  const slice2 =
      new Slice<TestState, TestState['latestPayload']>('latestPayload');

  const increaseBy = slice1.addReducer(
      'increase-by',
      (state: TestState, payload: number) =>
          ({...state, numVisitors: state.numVisitors! + payload}));

  const store = new BaseStore<TestState>({}, [slice1, slice2]);
  store.init({numVisitors: 0, latestPayload: 'aaa'});

  // Construct 2 selectors which returns a plain object (whose reference will
  // change every time the selector runs) contains "latestPayload" only.
  const selectorWithStrictEqual = combine1Selector(
      (state: TestState) => ({a: state.latestPayload}), store.selector,
      'strictly-equal');
  const selectorWithShallowEqual = combine1Selector(
      (state: TestState) => ({a: state.latestPayload}), store.selector,
      'shallow-equal', shallowEqual);

  let counter1 = 0;
  let counter2 = 0;
  selectorWithStrictEqual.subscribe(() => {
    counter1++;
  });
  selectorWithShallowEqual.subscribe(() => {
    counter2++;
  });

  // Change "numVisitors" twice.
  store.dispatch(increaseBy(1));
  store.dispatch(increaseBy(2));

  // Expect the selector calls its callback twice because its returned object
  // changes its reference every time (strict equal).
  assertEquals(2, counter1);
  // Expect the selector never calls its callback because its returned object
  // never changes its content (shallow equal).
  assertEquals(0, counter2);
}
