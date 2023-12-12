// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview a base class and utility types for managing javascript WebUI
 * state in a redux-like fashion.
 */

export interface Action {
  name: string;
}

export type DeferredAction<A extends Action = Action> =
    (callback: (p: A|null) => void) => void;

export type Reducer<S, A extends Action = Action> = (state: S, action: A) => S;

export interface StoreObserver<S> {
  onStateChanged(newState: S): void;
}

/**
 * A generic datastore for the state of a page, where the state is publicly
 * readable but can only be modified by dispatching an Action.
 * The Store should be extended by specifying S, the page state type
 * associated with the store.
 */
export class Store<S, A extends Action = Action> {
  data: S;
  private reducer_: Reducer<S, A>;
  protected initialized_: boolean = false;
  private queuedActions_: Array<DeferredAction<A>> = [];
  private observers_: Set<StoreObserver<S>> = new Set();
  private batchMode_: boolean = false;

  constructor(emptyState: S, reducer: Reducer<S, A>) {
    this.data = emptyState;
    this.reducer_ = reducer;
  }

  init(initialState: S) {
    this.data = initialState;

    this.queuedActions_.forEach((action) => {
      this.dispatchInternal_(action);
    });
    this.queuedActions_ = [];

    this.initialized_ = true;
    this.notifyObservers_(this.data);
  }

  isInitialized(): boolean {
    return this.initialized_;
  }

  addObserver(observer: StoreObserver<S>) {
    this.observers_.add(observer);
  }

  removeObserver(observer: StoreObserver<S>) {
    this.observers_.delete(observer);
  }

  hasObserver(observer: StoreObserver<S>): boolean {
    return this.observers_.has(observer);
  }

  /**
   * Begin a batch update to store data, which will disable updates to the
   * UI until `endBatchUpdate` is called. This is useful when a single UI
   * operation is likely to cause many sequential model updates (eg, deleting
   * 100 bookmarks).
   */
  beginBatchUpdate() {
    this.batchMode_ = true;
  }

  /**
   * End a batch update to the store data, notifying the UI of any changes
   * which occurred while batch mode was enabled.
   */
  endBatchUpdate() {
    this.batchMode_ = false;
    this.notifyObservers_(this.data);
  }

  /**
   * Handles a 'deferred' action, which can asynchronously dispatch actions
   * to the Store in order to reach a new UI state. DeferredActions have the
   * form `dispatchAsync(function(dispatch) { ... })`). Inside that function,
   * the |dispatch| callback can be called asynchronously to dispatch Actions
   * directly to the Store.
   */
  dispatchAsync(action: DeferredAction<A>) {
    if (!this.initialized_) {
      this.queuedActions_.push(action);
      return;
    }
    this.dispatchInternal_(action);
  }

  /**
   * Transition to a new UI state based on the supplied |action|, and notify
   * observers of the change. If the Store has not yet been initialized, the
   * action will be queued and performed upon initialization.
   */
  dispatch(action: A|null) {
    this.dispatchAsync(function(dispatch) {
      dispatch(action);
    });
  }

  private dispatchInternal_(action: DeferredAction<A>) {
    action(this.reduce.bind(this));
  }

  protected reduce(action: A|null) {
    if (!action) {
      return;
    }

    this.data = this.reducer_(this.data, action);

    // Batch notifications until after all initialization queuedActions are
    // resolved.
    if (this.isInitialized() && !this.batchMode_) {
      this.notifyObservers_(this.data);
    }
  }

  protected notifyObservers_(state: S) {
    this.observers_.forEach(function(o) {
      o.onStateChanged(state);
    });
  }
}
