// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests exception message contains stack with correct function name.');

  await dp.Runtime.enable();
  await dp.Runtime.setAsyncCallStackDepth({maxDepth: 32});

  await session.evaluate(`
    var foo = function ()
    {
      throw new Error();
    }

    foo.displayName = 'foo.displayName';
    Object.defineProperty(foo, 'name', { value: 'foo.function.name' } );

    var bar = function()
    {
      foo();
    }

    bar.displayName = 'bar.displayName';

    var baz = function()
    {
      bar();
    }

    Object.defineProperty(baz, 'name', { value: 'baz.function.name' } );
//# sourceURL=function-name-in-exception-stack.js`);

  const exceptionPromise = dp.Runtime.onceExceptionThrown();
  await session.evaluate('setTimeout(baz, 0);\n//# sourceURL=schedule.js');

  const {params: {exceptionDetails}} = await exceptionPromise;
  if (!exceptionDetails.stackTrace) {
    testRunner.fail('Missing exceptionDetails.stackTrace');
  }
  const descriptionLine =
      (exceptionDetails.exception?.description || '').split('\n')[0];
  testRunner.log(`${exceptionDetails.url}:${exceptionDetails.lineNumber + 1} ${
      exceptionDetails.text} ${descriptionLine}`);

  for (let stack = exceptionDetails.stackTrace; stack; stack = stack.parent) {
    if (stack.description) {
      testRunner.log(stack.description);
    }
    for (const frame of stack.callFrames) {
      testRunner.log(`${frame.functionName || '(anonymous)'} @ ${frame.url}:${
          frame.lineNumber + 1}`);
    }
  }

  await dp.Runtime.setAsyncCallStackDepth({maxDepth: 0});
  testRunner.completeTest();
});
