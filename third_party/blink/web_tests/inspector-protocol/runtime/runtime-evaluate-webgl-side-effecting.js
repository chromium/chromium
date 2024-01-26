(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { dp } = await testRunner.startBlank(`Tests that Runtime.evaluate for WebGL2RenderingContext preview is side-effect free`);

  // Retrieving constructors from |window| should be side-effect free.
  const result = await dp.Runtime.evaluate({
    expression: 'WebGL2RenderingContext',
    replMode: true,
    throwOnSideEffect: true,
    generatePreview: true
  });

  testRunner.log(result);
  testRunner.completeTest();
})
