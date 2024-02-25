(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
    'Test that heap profiler doesn\'t crash while taking snapshot on a page where iframe was navigated to a new location after ' +
    'storing a hold of a function from the previous page. Bug 103076.');

  await session.evaluateAsync(`
    var frame = document.createElement('iframe');
    frame.src = '${testRunner.url('resources/page-with-function.html')}';
    document.body.appendChild(frame);
    var loadPromise = new Promise(f => frame.onload = f);
    var storeFunctionRefAndNavigateIFramePromise = loadPromise.then(() => {
      window.fooRef = frame.contentWindow.foo;
      frame.src = 'about:blank';
      return new Promise(f => frame.onload = f);
    });
    storeFunctionRefAndNavigateIFramePromise
  `);

  await dp.Profiler.takeHeapSnapshot({reportProgress: false});
  testRunner.log('SUCCESS: didTakeHeapSnapshot');
  testRunner.completeTest();
})
