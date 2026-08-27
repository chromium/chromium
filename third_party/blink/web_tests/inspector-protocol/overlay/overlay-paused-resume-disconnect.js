(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(
      `
    <!DOCTYPE html>
    <script>
      function pause() {
        debugger;
      }
    </script>
  `,
      'Verifies that clearing inspect tool and disconnecting session after sending resume/stepOver in overlay does not cause use-after-free.');

  testRunner.log('Testing resume:');
  await dp.DOM.enable();
  await dp.Overlay.enable();
  await dp.Debugger.enable();

  dp.Runtime.evaluate({expression: 'pause()'});
  await dp.Debugger.oncePaused();
  await dp.Overlay.setPausedInDebuggerMessage(
      {message: 'Paused on debugger statement'});
  testRunner.log('Paused on debugger statement.');

  // Trigger 'resume' action via overlay host (posts ExecuteOnV8Session task)
  // and immediately clear inspect tool and disconnect before the posted task
  // executes.
  session.evaluate(() => {
    internals.evaluateInInspectorOverlay(`(function () {
      window.InspectorOverlayHost.send('resume');
    })()`);
  });
  dp.Overlay.setPausedInDebuggerMessage({});
  await session.disconnect();
  testRunner.log('Cleared inspect tool and disconnected session after resume.');

  testRunner.log('Testing stepOver:');
  const session2 = await page.createSession();
  const dp2 = session2.protocol;

  await dp2.DOM.enable();
  await dp2.Overlay.enable();
  await dp2.Debugger.enable();

  dp2.Runtime.evaluate({expression: 'pause()'});
  await dp2.Debugger.oncePaused();
  await dp2.Overlay.setPausedInDebuggerMessage(
      {message: 'Paused on debugger statement'});
  testRunner.log('Paused on debugger statement.');

  // Trigger 'stepOver' action via overlay host (posts ExecuteOnV8Session task)
  // and immediately clear inspect tool and disconnect before the posted task
  // executes.
  session2.evaluate(() => {
    internals.evaluateInInspectorOverlay(`(function () {
      window.InspectorOverlayHost.send('stepOver');
    })()`);
  });
  dp2.Overlay.setPausedInDebuggerMessage({});
  await session2.disconnect();
  testRunner.log(
      'Cleared inspect tool and disconnected session after stepOver.');

  testRunner.completeTest();
});
