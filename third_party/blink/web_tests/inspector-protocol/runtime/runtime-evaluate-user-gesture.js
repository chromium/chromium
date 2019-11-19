(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests that Runtime.evaluate works with userGesture flag.`);

  function dumpResult(result) {
    if (result.exceptionDetails) {
      result.exceptionDetails.scriptId = '';
      result.exceptionDetails.exceptionId = 0;
      result.exceptionDetails.exception.objectId = 0;
    }
    testRunner.log(result);
  }

  await testRunner.runTestSuite([
    async function testInitialUserActivation() {
      var result = await dp.Runtime.evaluate({ expression: 'navigator.userActivation.isActive' });
      dumpResult(result.result);
    },

    async function testActiveWithoutUserGesture() {
      var result = await dp.Runtime.evaluate({ expression: 'navigator.userActivation.isActive' });
      dumpResult(result.result);
    },

    async function testActiveWithUserGesture() {
      var result = await dp.Runtime.evaluate({ expression: 'navigator.userActivation.isActive', userGesture: true });
      dumpResult(result.result);
    }
  ]);
})
