(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that embedder constructors throw on side effect. Should not crash.`);

  var badConstructorNames = await session.evaluate(`
    var global_badConstructors = [
      // Worker, SharedWorker constructors are known to be able to run in
      // parallel, fetch URLs, and modify the global worker set.
      // See spec section 10.2.6.3, step 9.
      // https://html.spec.whatwg.org/#dedicated-workers-and-the-worker-interface
      'Worker',
      // See spec section 10.2.6.4, step 11.
      // https://html.spec.whatwg.org/#shared-workers-and-the-sharedworker-interface
      'SharedWorker',

      // Check named constructors.
      'Audio',
      'Image',
      'Option',

      // Check constructors with JS source.
      'ReadableStream',
      'WritableStream',
      'TransformStream'
    ];
    global_badConstructors;
  `);

  for (var i = 0; i < badConstructorNames.length; i++) {
    var name = badConstructorNames[i];
    var response = await dp.Runtime.evaluate({expression: `new window[global_badConstructors[${i}]]`, throwOnSideEffect: true});
    var exception = response.result.exceptionDetails;
    var hasSideEffect = false;
    var exceptionDetails = response.result.exceptionDetails;
    if (exceptionDetails &&
        exceptionDetails.exception.description.startsWith('EvalError: Possible side-effect in debug-evaluate'))
      hasSideEffect = true;
    testRunner.log(`${hasSideEffect ? 'PASS: ' : 'FAIL: '}Constructor "${name}"\nhas side effect: ${hasSideEffect}`);
  }

  testRunner.completeTest();
})
