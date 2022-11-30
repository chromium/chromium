(async function testRemoteObjects(testRunner) {
  const {dp} = await testRunner.startBlank('Test logging of navigator plugins.');
  dp.Runtime.enable();

  const result = await dp.Runtime.evaluate({ expression:
    `navigator.mimeTypes`
  });

  testRunner.log(result.result.result);
  testRunner.completeTest();
});
