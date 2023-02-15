/**
 * Copyright 2022 Google LLC.
 * Copyright (c) Microsoft Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import {LogType, LoggerFn} from './log.js';

export class ProcessingQueue<T> {
  readonly #catch: (error: unknown) => Promise<void>;
  readonly #logger?: LoggerFn;
  readonly #processor: (arg: T) => Promise<void>;
  readonly #queue: Promise<T>[] = [];

  // Flag to keep only 1 active processor.
  #isProcessing = false;

  constructor(
    processor: (arg: T) => Promise<void>,
    _catch: (error: unknown) => Promise<void> = () => Promise.resolve(),
    logger?: LoggerFn
  ) {
    this.#catch = _catch;
    this.#processor = processor;
    this.#logger = logger;
  }

  add(entry: Promise<T>) {
    this.#queue.push(entry);
    // No need in waiting. Just initialise processor if needed.
    // noinspection JSIgnoredPromiseFromCall
    this.#processIfNeeded();
  }

  async #processIfNeeded() {
    if (this.#isProcessing) {
      return;
    }
    this.#isProcessing = true;
    while (this.#queue.length > 0) {
      const entryPromise = this.#queue.shift();
      if (entryPromise !== undefined) {
        await entryPromise
          .then((entry) => this.#processor(entry))
          .catch((e) => {
            this.#logger?.(LogType.system, 'Event was not processed:', e);
            this.#catch(e);
          })
          .finally();
      }
    }

    this.#isProcessing = false;
  }
}
