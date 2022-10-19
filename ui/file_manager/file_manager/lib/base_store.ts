// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ActionsProducerGen, ConcurrentActionInvalidatedError, isActionsProducer} from './actions_producer.js';

/**
 * The base interface for actions.
 * The application should extend this to enforce its own Actions.
 */
export interface BaseAction<T = any> {
  // Unique type for the Action.
  type: string;

  // Any additional data used by the Action.
  payload?: T;
}

/**
 * @template StateType: The shape/interface of the state.
 */
export interface StoreObserver<StateType> {
  onStateChanged(newState: StateType): void;
}

/**
 * A generic datastore for the state of a page, where the state is publicly
 * readable but can only be modified by dispatching an Action.
 *
 * The Store should be extended by specifying `StateType`, the app state type
 * associated with the store; and `BaseAction` the app's type for Actions.
 */
export class BaseStore<StateType, ActionType extends BaseAction> {
  /** The current state stored in the Store. */
  private data_: StateType;

  /** The root reducer used to process every Action. */
  private reducer_: (initialState: StateType, action: ActionType) => StateType;

  /**
   * Whether the Store has been initialized. See init() method to initialize.
   */
  private initialized_: boolean = false;

  /** Queues actions while the Store un-initialized. */
  private queuedActions_: ActionType[];

  /**
   * Observers that are notified when the State is updated by Action/Reducer.
   */
  private observers_: Array<StoreObserver<StateType>>;

  /**
   * Batch mode groups multiple Action mutations and only notify the observes
   * at the end of the batch. See beginBatchUpdate() and endBatchUpdate()
   * methods.
   */
  private batchMode_: boolean = false;

  constructor(
      emptyState: StateType,
      reducer: (initialState: StateType, action: ActionType) => StateType) {
    this.data_ = emptyState;
    this.reducer_ = reducer;
    this.queuedActions_ = [];
    this.observers_ = [];
    this.initialized_ = false;
    this.batchMode_ = false;
  }

  /**
   * Marks the Store as initialized.
   * While the Store is not initialized, no action is processed and no observes
   * are notified.
   *
   * It should be called by the app's initialization code.
   */
  init(initialState: StateType) {
    this.data_ = initialState;

    this.queuedActions_.forEach((action) => {
      this.dispatchInternal_(action);
    });

    this.initialized_ = true;
    this.notifyObservers_(this.data_);
  }

  isInitialized(): boolean {
    return this.initialized_;
  }

  /**
   * Subscribe to Store changes/updates.
   * @param observer Callback called whenever the Store is updated.
   * @returns callback to unsubscribe the observer.
   */
  subscribe(observer: StoreObserver<StateType>): () => void {
    this.observers_.push(observer);
    return this.unsubscribe.bind(this, observer);
  }

  /**
   * Removes the observer which will stop receiving Store updates.
   * @param observer The instance that was observing the store.
   */
  unsubscribe(observer: StoreObserver<StateType>) {
    const index = this.observers_.indexOf(observer);
    this.observers_.splice(index, 1);
  }

  /**
   * Begin a batch update to store data, which will disable updates to the
   * observers until `endBatchUpdate()` is called. This is useful when a single
   * UI operation is likely to cause many sequential model updates.
   */
  beginBatchUpdate() {
    this.batchMode_ = true;
  }

  /**
   * End a batch update to the store data, notifying the observers of any
   * changes which occurred while batch mode was enabled.
   */
  endBatchUpdate() {
    this.batchMode_ = false;
    this.notifyObservers_(this.data_);
  }

  /** @returns the current state of the store.  */
  getState(): StateType {
    return this.data_;
  }

  /**
   * Dispatches an Action to the Store.
   *
   * For synchronous actions it sends the action to the reducers, which updates
   * the Store state, then the Store notifies all subscribers.
   * If the Store isn't initialized, the action is queued and dispatched to
   * reducers during the initialization.
   */
  dispatch(action: ActionType|ActionsProducerGen<ActionType>) {
    if (isActionsProducer(action)) {
      this.consumeProducedActions_(action);
      return;
    }
    if (!this.initialized_) {
      this.queuedActions_.push(action);
      return;
    }
    this.dispatchInternal_(action);
  }

  /** Synchronously call apply the `action` by calling the reducer.  */
  private dispatchInternal_(action: ActionType) {
    this.reduce(action);
  }

  /**
   * Consumes the produced actions from the actions producer.
   * It dispatches each generated action.
   */
  private async consumeProducedActions_(actionsProducer:
                                            ActionsProducerGen<ActionType>) {
    while (true) {
      try {
        const {done, value} = await actionsProducer.next();

        // Accept undefined to accept empty `yield;` or `return;`.
        // The empty `yield` is useful to allow the generator to be stopped at
        // any arbitrary point.
        if (value !== undefined) {
          this.dispatch(value);
        }
        if (done) {
          return;
        }
      } catch (error) {
        if (error instanceof ConcurrentActionInvalidatedError) {
          // This error is expected when the actionsProducer has been
          // invalidated.
          return;
        }
        console.warn('Failure executing actions producer', error);
      }
    }
  }

  /** Apply the `action` to the Store by calling the reducer.  */
  protected reduce(action: ActionType) {
    this.data_ = this.reducer_(this.data_, action);

    // Batch notifications until after all initialization queuedActions are
    // resolved.
    if (this.initialized_ && !this.batchMode_) {
      this.notifyObservers_(this.data_);
    }
  }

  /** Notify observers with the current state. */
  private notifyObservers_(state: StateType) {
    this.observers_.forEach(o => {
      try {
        o.onStateChanged(state);
      } catch (error) {
        // Subscribers shouldn't fail, here we only log and continue to all
        // other subscribers.
        console.error(error);
      }
    });
  }
}
