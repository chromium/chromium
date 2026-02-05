// Regression test for crbug.com/479812827. Clicking on the overlay while paused
// in a web scheduling task should not crash the renderer.
(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(`
    <!DOCTYPE html>
    <script>
      function pauseInPrioritizedTask() {
        scheduler.postTask(() => {
          debugger;
        }, {priority: 'user-blocking'});
      }
    </script>
  `, 'Verifies that clicking on the overlay doesn\'t crash the renderer.');

  await dp.DOM.enable();
  await dp.Overlay.enable();
  await dp.Debugger.enable();

  // 1. Pause the page and draw the paused message overlay.
  dp.Runtime.evaluate({ expression: 'pauseInPrioritizedTask()' });
  await dp.Debugger.oncePaused();
  await dp.Overlay.setPausedInDebuggerMessage({ message: 'Paused on debugger statement' });
  testRunner.log('Paused on "debugger" statement.');

  // 2. Simulate "click" by dispatching Mouse down + Mouse up via input domain.
  await dp.Input.dispatchMouseEvent({
    type: 'mousePressed',
    button: 'left',
    clickCount: 1,
    x: 100,
    y: 100,
  });
  await dp.Input.dispatchMouseEvent({
    type: 'mouseReleased',
    button: 'left',
    clickCount: 1,
    x: 100,
    y: 100,
  });
  testRunner.log('Clicked on the overlay.');

  testRunner.completeTest();
});
