(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL('../resources/frame-attached-stacktrace-page.html', 'Tests stackTrace reported on frame attach.');

  const expectedNumberOfFrames = 3;
  var currentFrameCount = 0;

  dp.Page.onFrameAttached(data => {
    currentFrameCount++;
    testRunner.log("Frame Attached");
    var stack = data.params.stack;
    if (stack) {
      logCallframes(stack.callFrames);
    } else {
      testRunner.log("Stack is empty");
      testRunner.log("");
    }
    if (currentFrameCount >= expectedNumberOfFrames) {
      testRunner.completeTest();
    }
  });

  dp.Page.enable();
  dp.Page.reload({ "ignoreCache": false });
  await dp.Page.onceLoadEventFired();
  session.evaluate('createFrame()');

  // showUrl left in for debugging reasons.
  function logCallframes(frames) {
    testRunner.log("Call Frames :");
    if (!frames) {
      testRunner.log("No callframes");
      testRunner.log("");
      return;
    }
    testRunner.log("[");
    for (var i = 0; i < frames.length; i++) {
      var frame = frames[i];
      testRunner.log("  [" + i + "] : {");
      if (!frame) {
        testRunner.log("  No Frame");
        continue;
      }
      var url = frame.url || '';
      if (url.indexOf('data:') !== 0 && url.indexOf('/') !== -1) {
        var urlParts = url.split('/');
        url = "<only showing file name>/" + urlParts[urlParts.length - 1];
      }
      testRunner.log("    columnNumber : " + frame.columnNumber);
      testRunner.log("    functionName : " + frame.functionName);
      testRunner.log("    lineNumber : " + frame.lineNumber);
      testRunner.log("    scriptId : " + (frame.scriptId ? "<scriptId>" : null));
      testRunner.log("    lineNumber : " + frame.lineNumber);
      testRunner.log("    url : " + url);
      testRunner.log("  }");
    }
    testRunner.log("]");
    testRunner.log("");
  }
})
