// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ActionsProducerGen, ConcurrentActionInvalidatedError, isActionsProducer} from './actions_producer.js';

/**
 * Actions are handled by the store according to their name and payload,
 * triggering reducers.
 */
export interface Action {
  type: string;
  payload?: any;
}

type ActionFactory<Payload> = (payload: Payload) =>
    (Action&{type: string, payload: Payload});

/** Reducers generate a new state from the current state and a payload. */
export type Reducer<State, Payload> = (state: State, payload: Payload) => State;

type ReducersMap<State> = Map<Action['type'], Array<Reducer<State, any>>>;

/**
 * Slices represent a part of the state that is nested directly under the root
 * state, aggregating its reducers and selectors.
 */
export class Slice<State> {
  handlers: ReducersMap<State> = new Map();

  constructor(public name: string, public debug?: boolean) {}

  /**
   * Add type to slice's registry, ensuring it's unique within the slice, and
   * returning the full action name given by appending the given action name to
   * the slice's.
   *
   * If the given action type is already a full name, nothing is done and the
   * full name is returned.
   */
  private prependSliceName_(type: string) {
    // Full action names are always formatted as "[SLICE_NAME] TYPE".
    if (type[0] === '[') {
      return type;
    }

    if (this.handlers.has(type)) {
      throw new Error('Attempting to register a duplicate action name.');
    }

    this.handlers.set(type, []);

    return `[${this.name}] ${type}`;
  }

  /** Returns an action factory for the added reducer. */
  addReducer<Payload>(type: Action['type'], reducer: Reducer<State, Payload>):
      ActionFactory<Payload> {
    const fullName = this.prependSliceName_(type);
    let reducerList = this.handlers.get(fullName);
    if (!reducerList) {
      reducerList = [];
      this.handlers.set(fullName, reducerList);
    }
    reducerList.push(reducer);

    return (payload) => ({type: fullName, payload});
  }

  /** Creates a selector. */
  addSelector(_selector: (state: State) => any) {
    // TODO(b:297808212) Implement this method.
    return;
  }
}

/**
 * @template State: The shape/interface of the state.
 */
export interface StoreObserver<State> {
  onStateChanged(newState: State): void;
}

/** Merges multiple maps into one. */
function mergeMaps<K, V>(maps: Array<Map<K, V>>): Map<K, V> {
  return new Map(function*() {
    for (const map of maps) {
      yield* map;
    }
  }());
}

/**
 * A generic datastore for the state of a page, where the state is publicly
 * readable but can only be modified by dispatching an Action.
 *
 * The Store should be extended by specifying `StateType`, the app state type
 * associated with the store.
 */
export class BaseStore<State> {
  /**
   * A map of action names to reducers handled by the store.
   */
  private reducers_: ReducersMap<State>;

  /**
   * The current state stored in the Store.
   */
  private state_: State;

  /**
   * Whether the Store has been initialized. See init() method to initialize.
   */
  private initialized_: boolean = false;

  /** Queues actions while the Store un-initialized. */
  private queuedActions_: Action[];

  /**
   * Observers that are notified when the State is updated by Action/Reducer.
   */
  private observers_: Array<StoreObserver<State>>;

  /**
   * Batch mode groups multiple Action mutations and only notify the observes
   * at the end of the batch. See beginBatchUpdate() and endBatchUpdate()
   * methods.
   */
  private batchMode_: boolean = false;

  constructor(state: State, slices: Array<Slice<State>>) {
    this.state_ = state;
    this.queuedActions_ = [];
    this.observers_ = [];
    this.initialized_ = false;
    this.batchMode_ = false;

    const sliceNames = new Set(slices.map(slice => slice.name));
    if (sliceNames.size !== slices.length) {
      throw new Error(
          'One or more given slices have the same name. ' +
          'Please ensure slices are uniquely named: ' +
          [...sliceNames].join(', '));
    }

    this.reducers_ = mergeMaps(slices.map(slice => slice.handlers));
  }

  /**
   * Marks the Store as initialized.
   * While the Store is not initialized, no action is processed and no observes
   * are notified.
   *
   * It should be called by the app's initialization code.
   */
  init(initialState: State) {
    this.state_ = initialState;

    this.queuedActions_.forEach((action) => {
      this.dispatchInternal_(action);
    });

    this.initialized_ = true;
    this.notifyObservers_(this.state_);
  }

  isInitialized(): boolean {
    return this.initialized_;
  }

  /**
   * Subscribe to Store changes/updates.
   * @param observer Callback called whenever the Store is updated.
   * @returns callback to unsubscribe the observer.
   */
  subscribe(observer: StoreObserver<State>): () => void {
    this.observers_.push(observer);
    return this.unsubscribe.bind(this, observer);
  }

  /**
   * Removes the observer which will stop receiving Store updates.
   * @param observer The instance that was observing the store.
   */
  unsubscribe(observer: StoreObserver<State>) {
    // Create new copy of `observers_` to ensure elements are not removed
    // from the array in the middle of the loop in `notifyObservers_()`.
    this.observers_ = this.observers_.filter(o => o !== observer);
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
    this.notifyObservers_(this.state_);
  }

  /** @returns the current state of the store.  */
  getState(): State {
    return this.state_;
  }

  /**
   * Dispatches an Action to the Store.
   *
   * For synchronous actions it sends the action to the reducers, which updates
   * the Store state, then the Store notifies all subscribers.
   * If the Store isn't initialized, the action is queued and dispatched to
   * reducers during the initialization.
   */
  dispatch(action: Action|ActionsProducerGen) {
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
  private dispatchInternal_(action: Action) {
    this.reduce(action);
  }

  /**
   * Consumes the produced actions from the actions producer.
   * It dispatches each generated action.
   */
  private async consumeProducedActions_(actionsProducer: ActionsProducerGen) {
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
        if (isInvalidationError(error)) {
          // This error is expected when the actionsProducer has been
          // invalidated.
          return;
        }
        console.warn('Failure executing actions producer', error);
      }
    }
  }

  /** Apply the `action` to the Store by calling the reducer.  */
  protected reduce(action: Action) {
    if (window.DEBUG_STORE) {
      console.groupCollapsed(`Action: ${action.type}`);
      console.dir(action.payload);
      console.groupEnd();
    }

    const reducers = this.reducers_.get(action.type);
    if (!reducers || reducers.length === 0) {
      console.error(`No registered reducers for action: ${action.type}`);
      return;
    }

    this.state_ = reducers.reduce(
        (state, reducer) => reducer(state, action.payload), this.state_);

    // Batch notifications until after all initialization queuedActions are
    // resolved.
    if (this.initialized_ && !this.batchMode_) {
      this.notifyObservers_(this.state_);
    }
  }

  /** Notify observers with the current state. */
  private notifyObservers_(state: State) {
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

/** Returns true when the error is a ConcurrentActionInvalidatedError. */
function isInvalidationError(error: unknown): boolean {
  if (!error) {
    return false;
  }

  if (error instanceof ConcurrentActionInvalidatedError) {
    return true;
  }

  // Rollup sometimes duplicate the definition of error class so the
  // `instanceof` above fail in this condition.
  if (error.constructor?.name === 'ConcurrentActionInvalidatedError') {
    return true;
  }

  return false;
}
