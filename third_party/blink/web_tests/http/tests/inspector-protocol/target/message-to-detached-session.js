(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank('Tests that sending message to detached session will return an error.');

  const createChildFrame = function(url) {
    return new Promise(resolve => {
      const frame = document.createElement(`iframe`);
      frame.src = url;
      frame.addEventListener('load', resolve);
      document.body.appendChild(frame);
    });
  }
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  const [outerAttached] = await Promise.all([
    dp.Target.onceAttachedToTarget(),
    session.evaluateAsync(`(${createChildFrame.toString()})('http://devtools.oopif-a.test:8080/inspector-protocol/resources/iframe.html')`)
  ]);
  const outerSession = session.createChild(outerAttached.params.sessionId);
  const outerDp = outerSession.protocol;
  await outerDp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  const [innerAttached] = await Promise.all([
    outerDp.Target.onceAttachedToTarget(),
    outerSession.evaluateAsync(`(${createChildFrame.toString()})('http://devtools.oopif-b.test:8080/inspector-protocol/resources/iframe.html')`)
  ]);
  const innerSession = session.createChild(innerAttached.params.sessionId);
  const innerDp = innerSession.protocol;

  // Wait for both sessions to detach.
  await Promise.all([
    outerDp.Target.onceDetachedFromTarget(),
    dp.Target.onceDetachedFromTarget(),
    session.evaluateAsync(() => document.querySelector(`iframe`).remove())
  ]);
  testRunner.log('removed frames from dom');
  for (const dp of [outerDp, innerDp]) {
    const result = await dp.Runtime.evaluate({expression: '1 + 1'});
    if (result.error)
      testRunner.log(result.error, 'PASS: got protocolerror: ');
    else
      testRunner.log(result, 'FAIL: eval succeeded ');
  }

  testRunner.completeTest();
})
