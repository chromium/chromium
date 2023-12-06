// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {type ActionsProducer, type ActionsProducerGen, ConcurrentActionInvalidatedError} from './actions_producer.js';

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
export function keepLatest<Args extends any[]>(
    actionsProducer: ActionsProducer<Args>): ActionsProducer<Args> {
  // Scope #1: Initial setup.
  let counter = 0;

  async function* wrap(...args: Args): ActionsProducerGen {
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
export function keyedKeepFirst<Args extends any[]>(
    actionsProducer: ActionsProducer<Args>,
    generateKey: (...args: Args) => string): ActionsProducer<Args> {
  // Scope #1: Initial setup.
  // Key for the current AP.
  let inFlightKey: string|null = null;

  async function* wrap(...args: Args): ActionsProducerGen {
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

/**
 * While the key is the same it cancels the previous pending Actions
 * Producer (AP).
 * Note: APs with different keys can happen simultaneously, e.g. `key-2` won't
 * cancel a pending `key-1`.
 */
export function keyedKeepLatest<Args extends any[]>(
    actionsProducer: ActionsProducer<Args>,
    generateKey: (...args: Args) => string): ActionsProducer<Args> {
  // Scope #1: Initial setup.
  let counter = 0;
  // Key->index map for all in-flight AP.
  const inFlightKeyToActionId = new Map<string, number>();

  async function* wrap(...args: Args): ActionsProducerGen {
    // Scope #2: Per-call to the ActionsProducer.
    const key = generateKey(...args);
    const actionId = ++counter;
    inFlightKeyToActionId.set(key, actionId);

    const generator = actionsProducer(...args);
    for await (const producedAction of generator) {
      // Scope #3: The generated action.
      const latestActionId = inFlightKeyToActionId.get(key);
      if (latestActionId === undefined || actionId < latestActionId) {
        const error = new ConcurrentActionInvalidatedError(
            `A new ActionProducer with the same key ${
                key} is started, invalidate this one.`);
        await generator.throw(error);
        // We rely on the above throw to break the loop.
      }
      yield producedAction;
    }

    // If the action producer finishes without being cancelled, remove the key.
    inFlightKeyToActionId.delete(key);
  }
  return wrap;
}
