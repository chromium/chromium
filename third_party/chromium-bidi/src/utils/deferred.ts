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

export class Deferred<T> implements Promise<T> {
  #resolve: (value: T) => void = () => {};
  #reject: (value: T) => void = () => {};
  #promise: Promise<T>;
  #isFinished = false;

  get isFinished(): boolean {
    return this.#isFinished;
  }

  constructor() {
    this.#promise = new Promise((resolve, reject) => {
      this.#resolve = resolve;
      this.#reject = reject;
    });
  }

  then<TResult1 = T, TResult2 = never>(
    onFulfilled?: ((value: T) => TResult1 | PromiseLike<TResult1>) | null,
    onRejected?: ((reason: any) => TResult2 | PromiseLike<TResult2>) | null
  ): Promise<TResult1 | TResult2> {
    return this.#promise.then(onFulfilled, onRejected);
  }

  catch<TResult = never>(
    onRejected?: ((reason: any) => TResult | PromiseLike<TResult>) | null
  ): Promise<T | TResult> {
    return this.#promise.catch(onRejected);
  }

  public resolve(value: T) {
    this.#isFinished = true;
    this.#resolve(value);
  }

  public reject(reason: any) {
    this.#isFinished = true;
    this.#reject(reason);
  }

  finally(onFinally?: (() => void) | undefined | null): Promise<T> {
    return this.#promise.finally(onFinally);
  }

  [Symbol.toStringTag] = 'Promise';
}
