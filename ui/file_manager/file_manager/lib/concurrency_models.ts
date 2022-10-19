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
        generator.throw(new ConcurrentActionInvalidatedError(
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
