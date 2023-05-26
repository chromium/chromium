/**
 * Copyright 2023 Google LLC.
 * Copyright (c) Microsoft Corporation.
 * Copyright 2022 The Chromium Authors.
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

import {expect} from 'chai';

import {Mutex} from './Mutex.js';

describe('Mutex', () => {
  async function triggerMicroTaskQueue(): Promise<void> {
    await new Promise((resolve) => setTimeout(resolve, 0));
  }

  async function notAcquired(): Promise<'not acquired'> {
    await triggerMicroTaskQueue();
    return 'not acquired';
  }

  it('should acquire the lock immediately if no one is holding it', async () => {
    const mutex = new Mutex();
    const release = await mutex.acquire();
    release();
  });

  it('should acquire the lock once another holder releases it', async () => {
    const mutex = new Mutex();
    const lock1 = mutex.acquire();
    const lock2 = mutex.acquire();
    const release = await lock1;
    // lock2 should not be resolved set.
    expect(await Promise.race([lock2, notAcquired()])).equals('not acquired');
    release();
    await triggerMicroTaskQueue();
    expect(await lock2).instanceOf(Function);
  });

  it('should work for two async functions accessing shared state', async () => {
    const mutex = new Mutex();
    const shared: string[] = [];
    // eslint-disable-next-line @typescript-eslint/no-empty-function
    let action1Resolver = () => {};
    const action1ReadyPromise = new Promise<void>((resolve) => {
      action1Resolver = resolve;
    });
    // eslint-disable-next-line @typescript-eslint/no-empty-function
    let action2Resolver = () => {};
    const action2ReadyPromise = new Promise<void>((resolve) => {
      action2Resolver = resolve;
    });

    async function action1() {
      const release = await mutex.acquire();
      try {
        await action1ReadyPromise;
        shared.push('action1');
      } finally {
        release();
      }
    }

    async function action2() {
      const release = await mutex.acquire();
      try {
        await action2ReadyPromise;
        shared.push('action2');
      } finally {
        release();
      }
    }
    const promises = Promise.all([action1(), action2()]);
    action2Resolver();
    action1Resolver();
    await promises;
    expect(shared[0]).to.eq('action1');
    expect(shared[1]).to.eq('action2');
  });
});
