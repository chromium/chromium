(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Tests async stacks for message channel API.');
  let debuggerId = (await dp.Debugger.enable()).result.debuggerId;
  let debuggers = new Map([[debuggerId, dp.Debugger]]);
  dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});

  await dp.Runtime.evaluate({
    expression: `
    let frame = document.createElement('iframe');
    frame.src = 'data:text/html,<script>onmessage = (e) => e.ports[0].postMessage(\\'pong\\');//%23 sourceURL=iframe.js</script>';
    let p = new Promise(resolve => frame.onload = resolve);
    document.body.appendChild(frame);
    p
  `,
    awaitPromise: true
  });
  testRunner.log('iframe created');
  dp.Debugger.setBreakpointByUrl({url: 'test.js', lineNumber: 3});
  testRunner.log('breakpoint set before postMessage');
  dp.Runtime.evaluate({
    expression: `
    let channel = new MessageChannel();
    let otherWindow = frame.contentWindow;
    otherWindow.postMessage('ping', '*', [channel.port2]);
    channel.port1.onmessage = (e) => 42;
    //# sourceURL=test.js
  `
  });
  await dp.Debugger.oncePaused();
  testRunner.log('paused at breakpoint');
  dp.Debugger.stepInto({breakOnAsyncCall: true});
  testRunner.log('requested stepInto with breakOnAsyncCall flag');
  let {params: {callFrames, asyncStackTrace, asyncStackTraceId}} =
      await dp.Debugger.oncePaused();
  await testRunner.logStackTrace(
      debuggers,
      {callFrames, parent: asyncStackTrace, parentId: asyncStackTraceId},
      debuggerId);

  testRunner.log('\nrequested stepInto with breakOnAsyncCall flag');
  dp.Debugger.stepInto({breakOnAsyncCall: true});
  ({params: {callFrames, asyncStackTrace, asyncStackTraceId}} =
       await dp.Debugger.oncePaused());
  await testRunner.logStackTrace(
      debuggers,
      {callFrames, parent: asyncStackTrace, parentId: asyncStackTraceId},
      debuggerId);

  testRunner.completeTest();
})
