// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests asynchronous call stacks for async functions.');
  const runAsyncCallStacksTest =
      await testRunner.loadScript('resources/async-callstack.js');

  await runAsyncCallStacksTest(testRunner, session, dp, {
    totalDebuggerStatements: 4,
    async setup() {
      await session.evaluate(`${'\n'.repeat(27)}
        function timeoutPromise(value, ms)
        {
          return new Promise(function promiseCallback(resolve, reject) {
            function resolvePromise()
            {
              resolve(value);
            }
            function rejectPromise()
            {
              reject(value);
            }
            if (value instanceof Error)
              setTimeout(rejectPromise, ms || 0);
            else
              setTimeout(resolvePromise, ms || 0);
          });
        }

        function settledPromise(value)
        {
          function resolveCallback(resolve, reject)
          {
            resolve(value);
          }
          function rejectCallback(resolve, reject)
          {
            reject(value);
          }
          if (value instanceof Error)
            return new Promise(rejectCallback);
          else
            return new Promise(resolveCallback);
        }

        function testFunction()
        {
          setTimeout(testFunctionTimeout, 0);
        }

        function testFunctionTimeout()
        {
          var functions = [
            doTestPromiseConstructor,
            doTestSettledPromisesResolved,
            doTestSettledPromisesRejected,
          ];
          for (var i = 0; i < functions.length; ++i)
            functions[i]();
        }

        function thenCallback(value)
        {
          debugger;
        }

        function errorCallback(error)
        {
          debugger;
        }

        async function doTestPromiseConstructor()
        {
          try {
            let result = await new Promise(function promiseCallback(resolve, reject) {
              resolve(1);
              debugger;
            });
            thenCallback(result);
          } catch (e) {
            errorCallback(e);
          }
        }

        async function doTestSettledPromisesResolved()
        {
          try {
            let value = await settledPromise('resolved');
            thenCallback(value);
          } catch (e) {
            errorCallback(e);
          }
        }

        async function doTestSettledPromisesRejected()
        {
          try {
            let value = await settledPromise(new Error('rejected'));
            thenCallback(value);
          } catch (e) {
            errorCallback(e);
          }
        }
        //# sourceURL=async-callstack-async-await1.js
      `);
    },
  });
});
