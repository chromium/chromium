// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A Selector implementation for redux, bundled with a
 * SelectorEmitter helper class that allows selectors to be efficiently updated.
 */

import type {ReactiveController, ReactiveControllerHost} from 'chrome://resources/mwc/lit/index.js';

import {isDebugStoreEnabled} from './base_store.js';

type Callback<T> = (value: T) => void;
type Unsubscribe = () => void;

/**
 * Interface implemented by SelectorNode. Used to expose SelectorNodes outside
 * the store without giving access to SelectorNode's implementation details.
 *
 * SelectorNode's public methods that don't implement Selector's are meant to be
 * used by the BaseStore but not by the rest of the app.
 *
 * Selectors let callers get either a selected part of the state immediately
 * through `get()`, or subscribe to updates whenever the selected state changes
 * through `subscribe()`. The `controller` class is a `ReactiveController` and
 * is meant for lit elements to conveniently re-render based on changes to the
 * selected state.
 */
export interface Selector<T> {
  /** Immediately return the selector's current value. */
  get: () => T;
  /**
   * Subscribe to changes to the selector's current value.
   *
   * Note: Updates are only received when the selected state has actually
   * changed, regardless of changes to other parts of the root state. Therefore,
   * it's unnecessary to check if received values have in fact changed - the
   * selector guarantees that all new values differ from the previous.
   */
  subscribe: (cb: Callback<T>) => Unsubscribe;
  /**
   * Creates a ReactiveController that lets Lit components automatically
   * re-render when the selected state changes.
   *
   * It automatically unsubscribes from the selector when the component is
   * destroyed.
   *
   * For usage examples, see selector_unittest.ts.
   *
   * For more information on ReactiveControllers, refer to the Lit
   * documentation.
   */
  createController: (host: ReactiveControllerHost) => SelectorController<T>;
  /** Deletes this Selector if it no longer has any children. */
  delete: () => void;
}

/**
 * A class implementing ReactiveController in order to provide an ergonomic
 * way to update Lit elements based on selected data.
 */
class SelectorController<T> implements ReactiveController {
  unsubscribe?: Unsubscribe;

  constructor(
      public host: ReactiveControllerHost,
      public value: T,
      public subscribe: Selector<T>['subscribe'],
  ) {
    this.host.addController(this);
  }

  hostConnected() {
    this.unsubscribe = this.subscribe((value: T) => {
      this.value = value;
      this.host.requestUpdate();
    });
  }

  hostDisconnected() {
    this.unsubscribe!();
  }
}

/**
 * A node in the selector DAG (Directed Acyclic Graph). Used to efficiently
 * process selectors and eliminate redundant calculations and state updates. A
 * selector node essentially connects parent selectors through a `select()`
 * function that combines all of their parents' emitted values to form a new
 * value.
 *
 * Note: `SelectorNode` implements the `Selector` interface, allowing the store
 * to expose nodes as `Selector`s, hiding complexities related to
 * `SelectorNode`'s implementation.
 */
export class SelectorNode<T> implements Selector<T> {
  /** Last value emitted by the selector. */
  private value_: T = undefined as T;

  /** List of selector's current subscribers. */
  private subscribers_: Array<Callback<T>> = [];

  /** List of selector's current parents. */
  private parents_: Array<SelectorNode<any>|Selector<any>> = [];

  /**
   * The depth of this node in the SelectorEmitter DAG. Used to ensure Selector
   * nodes are emitted in the correct order.
   *
   * Nodes of depth D+1 are only processed after all nodes of depth D have been
   * processed, starting from D=0.
   *
   * Only source nodes (nodes without parents) have depth=0;
   */
  depth = 0;

  /** List of selector's current children. */
  children: Array<SelectorNode<any>> = [];

  /**
   * @param parents Either an array of Selectors or SelectorNodes whose values
   *     should be fed into the `select` function to calculate the selector's
   *     new value.
   * @param select The function that calculates the selector's new value once at
   *     least one its parents emits a new value or, initially, after the
   *     selector node is constructed. The arguments of select() must match the
   *     order and type of what is emitted by the parents. This typing match is
   *     not enforced here because SelectorNodes are only meant to be created by
   *     the Store. Users of the Store should use `combineXSelectors()` to
   * combine selectors.
   * @param name An optional human-readable name used for debugging purposes.
   *     Named selectors will log to the console when DEBUG_STORE is set,
   *     whenever they emit a new value.
   * @param isEqual_ An optional comparison function which will be used
   *     when compare the old value and the new value form the selector. By
   *     default it will use triple equal.
   */
  constructor(
      parents: Array<SelectorNode<any>|Selector<any>>,
      public select: (...values: any[]) => T, public name?: string,
      private isEqual_: (oldValue: T, newValue: T) => boolean = strictlyEqual) {
    this.parents = parents as Array<SelectorNode<any>>;
  }

  /**
   * Creates a new source node (a node with no parents).
   *
   * The store's default selector should be a source node, but other data
   * sources can be registered as source nodes as well.
   *
   * Slice's default selectors are then connected to the store's source node,
   * and additional selector nodes can then be created from store and slices'
   * default selectors using `combineXSelectors()` (and resulting selectors can
   * be further combined using `combineXSelectors()`).
   */
  static createSourceNode<T>(select: () => T, name?: string) {
    return new SelectorNode<T>([], select, name);
  }

  /**
   * Creates a selector node that doesn't have parents or select function. Used
   * by slices to create selectors that are not yet connected to the store but
   * that can be subscribed to before the store is constructed.
   *
   * In other words, disconnected nodes should eventually be connected to the
   * SelectorEmitter DAG and should retain their list of subscribers after doing
   * so.
   *
   * Disconnected nodes are exclusively used internally by slices and are not
   * meant to be used outside of it.
   */
  static createDisconnectedNode<T>(name?: string) {
    return new SelectorNode<T>([], () => undefined as T, name);
  }

  /**
   * We use a getter for parents to make sure they are always retrieved as
   * SelectorNodes, even though they might be passed in as Selectors in the
   * `combineXSelectors()` functions.
   */
  get parents(): Array<SelectorNode<any>> {
    return this.parents_ as Array<SelectorNode<any>>;
  }

  set parents(parents: Array<SelectorNode<any>>) {
    // Disconnect current parents, if any, before replacing them.
    this.disconnect_();

    this.parents_ = parents;

    // Connects this node to its new parents.
    for (const parent of parents) {
      parent.children.push(this);
      this.depth = Math.max(this.depth, parent.depth + 1);
    }

    // Calculate the node's initial value.
    this.emit();
  }

  /**
   * Disconnects itself from the DAG by deleting its connections with its
   * parents.
   */
  private disconnect_() {
    // Disconnect node from its parents.
    this.parents.forEach(p => p.disconnectChild_(this));
    this.parents_ = [];
  }

  /** Disconnects the node from one of its children. */
  private disconnectChild_(node: SelectorNode<any>) {
    this.children.splice(this.children.indexOf(node), 1);
  }

  /**
   * Sets a new value, if such new value is different from the current. If
   * it's different, returns true and notify subscribers. Else, returns false.
   */
  emit(): boolean {
    const parentValues = this.parents.map(p => p.get());
    const newValue = this.select(...parentValues);

    if (this.isEqual_(this.value_, newValue)) {
      return false;
    }

    if (isDebugStoreEnabled() && this.name) {
      console.info(`Selector '${this.name}' emitted a new value:`);
      console.info(newValue);
    }

    this.value_ = newValue;

    for (const subscriber of this.subscribers_) {
      try {
        subscriber(newValue!);
      } catch (e) {
        console.error(e);
      }
    }

    return true;
  }

  get() {
    return this.value_;
  }

  subscribe(cb: Callback<T>) {
    this.subscribers_.push(cb);
    return () => this.subscribers_.splice(this.subscribers_.indexOf(cb), 1);
  }

  createController(host: ReactiveControllerHost) {
    return new SelectorController(host, this.get(), this.subscribe.bind(this));
  }

  delete() {
    if (this.children.length > 0) {
      throw new Error('Attempting to delete node that still has children.');
    }
    this.disconnect_();
    this.subscribers_ = [];
  }
}

/** Create a selector whose value derives from a single Selector. */
export function combine1Selector<O, I1>(
    combineFunction: (i1: I1) => O,
    s1: Selector<I1>,
    name?: string,
    isEqual: (oldValue: O, newValue: O) => boolean = strictlyEqual,
    ): Selector<O> {
  return new SelectorNode<O>([s1], combineFunction as any, name, isEqual);
}
/** Create a selector whose value derives from 2 Selectors. */
export function combine2Selectors<O, I1, I2>(
    combineFunction: (i1: I1, i2: I2) => O,
    s1: Selector<I1>,
    s2: Selector<I2>,
    name?: string,
    isEqual: (oldValue: O, newValue: O) => boolean = strictlyEqual,
    ): Selector<O> {
  return new SelectorNode<O>([s1, s2], combineFunction as any, name, isEqual);
}
/** Create a selector whose value derives from 3 Selectors. */
export function combine3Selectors<O, I1, I2, I3>(
    combineFunction: (i1: I1, i2: I2, i3: I3) => O,
    s1: Selector<I1>,
    s2: Selector<I2>,
    s3: Selector<I3>,
    name?: string,
    isEqual: (oldValue: O, newValue: O) => boolean = strictlyEqual,
    ): Selector<O> {
  return new SelectorNode<O>(
      [s1, s2, s3], combineFunction as any, name, isEqual);
}
/** Create a selector whose value derives from 4 Selectors. */
export function combine4Selectors<O, I1, I2, I3, I4>(
    combineFunction: (i1: I1, i2: I2, i3: I3, i4: I4) => O,
    s1: Selector<I1>,
    s2: Selector<I2>,
    s3: Selector<I3>,
    s4: Selector<I4>,
    name?: string,
    isEqual: (oldValue: O, newValue: O) => boolean = strictlyEqual,
    ): Selector<O> {
  return new SelectorNode<O>(
      [s1, s2, s3, s4], combineFunction as any, name, isEqual);
}

/**
 * A DAG (Directed Acyclic Graph) representation of chains of selectors where
 * one selector only emits if at least one of their parents has emitted, while
 * also guaranteeing that, when multiple parents of a given node emit, their
 * child only emits a single time.
 */
export class SelectorEmitter {
  /** Source nodes. I.e., nodes with no parents. */
  private sourceNodes_: Array<SelectorNode<any>> = [];

  /** Connect source node to the DAG. */
  addSource(node: SelectorNode<any>) {
    this.sourceNodes_.push(node);
  }

  /**
   * Propagates changes from sourceNodes to the rest of the DAG.
   *
   * Nodes of depth D+1 are only processed after all nodes of depth D have been
   * processed, starting from D=0.
   *
   * This method ensures selectors are evaluated efficiently by:
   * - Only evaluating nodes if at least one of their parents has emitted a new
   * value;
   * - Ensuring each node only emits once per call to `processChange()` unlike a
   * naive implementation that would emit every time a parent emitted a new
   * value (meaning the node would emit multiple times per iteration if it had
   * multiple emitting parents).
   */
  processChange() {
    const toExplore = [...this.sourceNodes_];
    while (toExplore.length > 0) {
      const node = toExplore.pop()!;

      // Only traverse children if a new value is emitted. Children with
      // multiple parents might still be enqueued by the remaining parents.
      if (node.emit()) {
        toExplore.push(...node.children);
        // TODO(300209290): use heap instead.
        // Ensure nodes are explored in ascending order of depth.
        toExplore.sort((a, b) => b.depth - a.depth);
      }
    }
  }
}

// Comparison functions can be passed to selectors when initialized.

/** strictlyEqual use triple equal to compare values. */
export function strictlyEqual(oldValue: unknown, newValue: unknown): boolean {
  return oldValue === newValue;
}

/** shallowEqual compares the immediate property of the passed objects. */
export function shallowEqual(
    oldValue: Record<string, unknown>,
    newValue: Record<string, unknown>): boolean {
  // Only throw error when `newValue` is not an object because `oldValue` could
  // be `undefined` initially.
  if (!(newValue && typeof newValue === 'object')) {
    throw new Error('Can not use shallowEqual for non object comparison');
  }
  if (typeof oldValue !== 'object') {
    return false;
  }

  const keys = Object.keys(newValue);
  for (const key of keys) {
    if (oldValue[key] !== newValue[key]) {
      return false;
    }
  }

  return true;
}
