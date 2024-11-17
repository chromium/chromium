(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(`
    <!DOCTYPE html>
    <script>
      var pointerup_event_count = 0;
      document.addEventListener('pointerup', (event) => {
         ++pointerup_event_count;
      });
    </script>
  `, 'Verifies that clicking the "Resume" button in the overlay doesn\'t leak pointerup events into the page.');

  await dp.DOM.enable();
  await dp.Overlay.enable();
  await dp.Debugger.enable();

  // 1. Pause the page and draw the paused message overlay.
  dp.Runtime.evaluate({ expression: 'debugger' });
  await dp.Debugger.oncePaused();
  await dp.Overlay.setPausedInDebuggerMessage({ message: 'Paused on debugger statement' });
  testRunner.log('Paused on "debugger" statement.');

  // 2. Find the coordinates of the resume button in the overlay.
  async function getResumeButtonCoordinates() {
    return session.evaluate(() => {
      return internals.evaluateInInspectorOverlay(`(function () {
        const resumeButton = document.getElementById('resume-button');
        return JSON.stringify(resumeButton.getBoundingClientRect());
      })()`);
    });
  }
  const clientRect = JSON.parse(await getResumeButtonCoordinates());
  testRunner.log((typeof clientRect === 'object' && clientRect.x && clientRect.y) ? "Found 'Resume' button" : "Unable to find 'Resume' button");

  // 3. Simulate "click" by dispatching Mouse down + Mouse up via input domain.
  await dp.Input.dispatchMouseEvent({
    type: 'mousePressed',
    button: 'left',
    clickCount: 1,
    x: clientRect.x + (clientRect.width / 2),
    y: clientRect.y + (clientRect.height / 2),
  });
  dp.Input.dispatchMouseEvent({
    type: 'mouseReleased',
    button: 'left',
    clickCount: 1,
    x: clientRect.x + (clientRect.width / 2),
    y: clientRect.y + (clientRect.height / 2),
  });
  await dp.Debugger.onceResumed();
  testRunner.log("Clicked 'Resume' button");

  // 4. Check that the page did not receive any events.
  testRunner.log(await dp.Runtime.evaluate({ expression: 'pointerup_event_count' }), 'pointerup event count: ');

  testRunner.completeTest();
});
