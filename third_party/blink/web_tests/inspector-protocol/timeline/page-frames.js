(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
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

  const frames = new Map();

  const tracingStarted = tracingHelper.findEvent('TracingStartedInBrowser', 'I');
  startFrames = tracingStarted.args['data']['frames'] || [];
  startFrames.forEach(f => frames.set(f.frame, f));

  const commitLoads = tracingHelper.findEvents('CommitLoad', 'X');
  commitLoads.forEach(({args: {data: f}}) => frames.set(f.frame, f));

  const stripUrl = url => url.replace(/.*\/(([^/]*\/){2}[^/]*$)/, '$1');
  testRunner.log('Frames:');
  for (const [_id, frame] of frames) {
    const url = stripUrl(frame.url);
    const parentUrl =
        frame.parent ? stripUrl(frames.get(frame.parent).url) : '-';
    testRunner.log(`  url: ${url} name: ${frame.name} parentUrl: ${parentUrl}`);
  }

  testRunner.completeTest();
})
