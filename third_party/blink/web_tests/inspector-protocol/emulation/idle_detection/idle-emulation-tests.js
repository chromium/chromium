(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  let {page, session, dp} = await testRunner.startHTML(`
    <script>
      var userState = null;
      var screenState = null;

      function getState() {
        return "userState: " + userState + ", screenState: " + screenState;
      }

      async function setIdleDetector() {
        try {
          const idleDetector = new IdleDetector({threshold: 1});
          idleDetector.addEventListener('change', (e) => {
            userState = idleDetector.userState;
            screenState = idleDetector.screenState;
          });

          await idleDetector.start();

          return "idleDetector started";
        } catch(err) {
          return "Error: " + err.name + ", " + err.message;
        }
      }
    </script>
  `, 'Verifies that setIdleOverride overrides Idle state');

  async function evaluateAndWrite(cmd) {
    let v = await session.evaluateAsync(cmd);
    testRunner.log(v);
  }

  await dp.Browser.grantPermissions({
    origin: location.origin,
    permissions: ['idleDetection'],
  });

  // Prepare and run IdleDetector.
  await evaluateAndWrite("setIdleDetector()");

  // log initial state. It can be different based on the system.
  testRunner.log("remember initial state");
  let initialState = await session.evaluateAsync("getState()");

  // Set overrides and verify state.
  testRunner.log("set isUserActive: false, isScreenUnlocked: false");
  await dp.Emulation.setIdleOverride({isUserActive: false, isScreenUnlocked: false});
  await evaluateAndWrite("getState()");

  testRunner.log("set isUserActive: true, isScreenUnlocked: true");
  await dp.Emulation.setIdleOverride({isUserActive: true, isScreenUnlocked: true});
  await evaluateAndWrite("getState()");

  testRunner.log("set isUserActive: true, isScreenUnlocked: false");
  await dp.Emulation.setIdleOverride({isUserActive: true, isScreenUnlocked: false});
  await evaluateAndWrite("getState()");

  testRunner.log("set isUserActive: false, isScreenUnlocked: true");
  await dp.Emulation.setIdleOverride({isUserActive: false, isScreenUnlocked: true});
  await evaluateAndWrite("getState()");

  // Clear overrides and verify state.
  testRunner.log("call clearIdleOverride");
  await dp.Emulation.clearIdleOverride();
  let stateAfterClearingOverrides = await session.evaluateAsync("getState()");

  if(stateAfterClearingOverrides == initialState) {
    testRunner.log("State after clearIdleOverride equals initial state");
  } else {
    testRunner.log('[FAIL]: ' + stateAfterClearingOverrides + ' instead of ' + initialState);
  }

  testRunner.completeTest();
})
