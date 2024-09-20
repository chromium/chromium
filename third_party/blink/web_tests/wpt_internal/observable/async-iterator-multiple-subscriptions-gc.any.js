// Almost identical to the test JUST LIKE THIS in
// `external/wpt/dom/observable/tentative/observable-from.any.js`. The only
// difference is that we force a garbage collection event right after both slow
// and fast promises are created, to ensure that no state internal to the in-flight
// subscriptions is messed with, when there are multiple in-flight subscriptions.
promise_test(async t => {
  const async_iterable = {
    slow: true,
    [Symbol.asyncIterator]() {
      const shouldBeSlow = this.slow;
      this.slow = false;

      return {
        val: 0,
        next() {
          // Returns a Promise that resolves in a random amount of time less
          // than a second.
          return new Promise(resolve => {
            t.step_timeout(() => resolve({
              value: `${this.val}-${shouldBeSlow ? 'slow' : 'fast'}`,
              done: this.val++ === 4 ? true : false,
            }), shouldBeSlow ? 200 : 0);
          });
        },
      };
    },
  };

  const results = [];
  const source = Observable.from(async_iterable);

  const subscribeFunction = function(resolve, reject) {
    source.subscribe({
      next: v => results.push(v),
      complete: () => resolve(),
    });

    // A broken implementation will rely on this timeout.
    t.step_timeout(() => reject('TIMEOUT'), 3000);
  }

  const slow_promise = new Promise(subscribeFunction);
  const fast_promise = new Promise(subscribeFunction);
  await gc({type: 'major', execution: 'async'});

  await Promise.all([slow_promise, fast_promise]);
  assert_array_equals(results, [
    '0-fast',
    '1-fast',
    '2-fast',
    '3-fast',
    '0-slow',
    '1-slow',
    '2-slow',
    '3-slow',
  ]);
}, "from(): Asynchronous iterable multiple in-flight subscriptions competing");
