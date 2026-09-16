(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(
      `
    <style>
      body { margin: 0; }
      #main_btn {
        position: absolute;
        left: 10px;
        top: 10px;
        width: 80px;
        height: 40px;
      }
      #subframe {
        position: absolute;
        left: 10px;
        top: 100px;
        width: 200px;
        height: 100px;
        border: none;
      }
    </style>
    <button id="main_btn">Main</button>
    <iframe id="subframe" srcdoc="
      <style>body { margin: 0; } #child_btn { width: 80px; height: 40px; }</style>
      <button id='child_btn'>Child</button>
      <script>
        document.getElementById('child_btn').addEventListener('pointerdown', () => {
          const start = performance.now();
          while (performance.now() - start < 50) {}
        });
      </script>
    "></iframe>
    <script>
      document.getElementById('main_btn').addEventListener('pointerdown', () => {
        const start = performance.now();
        while (performance.now() - start < 50) {}
      });
    </script>
  `,
      'Tests EventTiming trace events and enqueuedToMainThreadTime for main frame and subframe.');

  await session.evaluateAsync(`new Promise(resolve => {
    const iframe = document.getElementById('subframe');
    if (iframe.contentDocument && iframe.contentDocument.readyState === 'complete') {
      resolve();
    } else {
      iframe.onload = resolve;
    }
  })`);

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing('devtools.timeline');

  async function click(x, y) {
    await dp.Input.dispatchMouseEvent(
        {type: 'mouseMoved', button: 'left', buttons: 0, clickCount: 1, x, y});
    await dp.Input.dispatchMouseEvent({
      type: 'mousePressed',
      button: 'left',
      buttons: 0,
      clickCount: 1,
      x,
      y
    });
    await dp.Input.dispatchMouseEvent({
      type: 'mouseReleased',
      button: 'left',
      buttons: 1,
      clickCount: 1,
      x,
      y
    });
    await session.evaluateAsync(
        `new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)))`);
  }

  // Click main frame button
  await click(30, 30);
  // Click subframe button
  await click(30, 120);

  const events = await tracingHelper.stopTracing(/devtools\.timeline/);
  const pointerdownEvents =
      events.filter(e => e.name === 'EventTiming' && e.args.data &&
                        e.args.data.type === 'pointerdown');

  testRunner.log(`Found ${
      pointerdownEvents.length} pointerdown EventTiming trace events.`);

  const frames = new Set();
  for (const e of pointerdownEvents) {
    const data = e.args.data;
    frames.add(data.frame);
    const hasValidEnqueuedTime =
        typeof data.enqueuedToMainThreadTime === 'number' &&
        data.enqueuedToMainThreadTime >= data.timeStamp &&
        data.enqueuedToMainThreadTime <= data.processingStart;
    testRunner.log(
        `Event valid enqueuedToMainThreadTime: ${hasValidEnqueuedTime}`);
  }
  testRunner.log(`Distinct frames recorded: ${frames.size}`);

  testRunner.completeTest();
})
