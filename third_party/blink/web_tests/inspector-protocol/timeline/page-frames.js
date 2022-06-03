(async function(testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
    <iframe src='data:text/html,<script>window.foo = 42</script>' name='frame0'></iframe>
  `, 'Tests certain trace events in iframes.');

  function performActions() {
    const frame1 = document.createElement('iframe');
    frame1.name = 'Frame No. 1';
    document.body.appendChild(frame1);
    frame1.contentWindow.document.write('console.log("frame2")');

    const frame2 = document.createElement('iframe');
    frame2.src = 'blank.html';
    document.body.appendChild(frame2);

    return new Promise(fulfill => { frame2.addEventListener('load', fulfill, false) });
  }

  const TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.invokeAsyncWithTracing(performActions);

  testRunner.log('Frames in TracingStartedInBrowser');
  const tracingStarted = tracingHelper.findEvent('TracingStartedInBrowser', 'I');
  for (const frame of tracingStarted.args['data']['frames'] || []) {
    dumpFrame(frame);
  }

  testRunner.log('Frames in CommitLoad events');
  const commitLoads = tracingHelper.findEvents('CommitLoad', 'X');
  for (const event of commitLoads) {
    dumpFrame(event.args['data']);
  }
  testRunner.completeTest();

  function dumpFrame(frame) {
    const url = frame.url.replace(/.*\/(([^/]*\/){2}[^/]*$)/, '$1');
    testRunner.log(`url: ${url} name: ${frame.name} parent: ${typeof frame.parent} nodeId: ${typeof frame.nodeId}`);
  }
})
