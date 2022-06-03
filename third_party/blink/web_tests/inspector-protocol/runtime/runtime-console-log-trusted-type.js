(async function testRemoteObjects(testRunner) {
  const {dp} = await testRunner.startBlank('Test description generation for Trusted Types.');
  dp.Runtime.enable();

  await dp.Runtime.evaluate({ expression:
    `policy = trustedTypes.createPolicy("generalPolicy", {
      createHTML: string => string
    });`
  });
  // The description should not use an overridden version of toString()
  const result = await dp.Runtime.evaluate({ expression:
    `x = policy.createHTML("<foo>"); x.toString = () => ""; x`
  });

  testRunner.log(result.result.result);
  testRunner.completeTest();
});