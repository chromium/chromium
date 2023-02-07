// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ActionsProducer, ActionsProducerGen, ConcurrentActionInvalidatedError} from './actions_producer.js';
import {BaseAction} from './base_store.js';

/**
 * Wraps the Actions Producer and enforces the Keep Last concurrency model.
 *
 * Assigns an `actionId` for each action call.
 * This consumes the generator from the Actions Producer.
 * In between each yield it might throw an exception if the actionId
 * isn't the latest action anymore. This effectively cancels any pending
 * generator()/action.
 *
 * @template T Type of the action yielded by the Actions Producer.
 * @template Args the inferred type for all the args for foo().
 *
 * @param actionsProducer This will be the `foo` above.
 */
export function keepLatest<T extends BaseAction, Args extends any[]>(
    actionsProducer: ActionsProducer<T, Args>): ActionsProducer<T, Args> {
  // Scope #1: Initial setup.
  let counter = 0;

  async function* wrap(...args: Args): ActionsProducerGen<T> {
    // Scope #2: Per-call to the ActionsProducer.
    const actionId = ++counter;

    const generator = actionsProducer(...args);

    for await (const producedAction of generator) {
      // Scope #3: The generated action.

      if (actionId !== counter) {
        await generator.throw(new ConcurrentActionInvalidatedError(
            `ActionsProducer invalidated running id: ${actionId} current: ${
                counter}:`));
        break;
      }

      // The generator is still valid, send the action to the store.
      yield producedAction;
    }
  }
  return wrap;
}

/**
 * While the key is the same it doesn't start a new Actions Producer (AP).
 *
 * If the key changes, then it cancels the previous one and starts a new one.
 *
 * If there is no other running AP, then it just starts a new one.
 */
export function keyedKeepFirst<T extends BaseAction, Args extends any[]>(
    actionsProducer: ActionsProducer<T, Args>,
    generateKey: (...args: Args) => string): ActionsProducer<T, Args> {
  // Scope #1: Initial setup.
  // Key for the current AP.
  let inFlightKey: string|null = null;

  async function* wrap(...args: Args): ActionsProducerGen<T> {
    // Scope #2: Per-call to the ActionsProducer.
    const key = generateKey(...args);
    // One already exists, just leave that finish.
    if (inFlightKey && inFlightKey === key) {
      return;
    }

    // This will force the previously running AP to cancel when yielding.
    inFlightKey = key;

    const generator = actionsProducer(...args);
    try {
      for await (const producedAction of generator) {
        // Scope #3: The generated action.
        if (inFlightKey && inFlightKey !== key) {
          const error = new ConcurrentActionInvalidatedError(
              `ActionsProducer invalidated running key: ${key} current: ${
                  inFlightKey}:`);
          await generator.throw(error);
          throw error;
        }
        yield producedAction;
      }
    } catch (error) {
      if (!(error instanceof ConcurrentActionInvalidatedError)) {
        // This error we don't want to clear the `inFlightKey`, because it's
        // pointing to the actually valid AP instance.
        inFlightKey = null;
      }
      throw error;
    }

    // Clear the key if it wasn't invalidated.
    inFlightKey = null;
  }
  return wrap;
}
