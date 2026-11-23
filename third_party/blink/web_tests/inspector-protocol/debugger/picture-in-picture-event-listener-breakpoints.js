// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `<!DOCTYPE html>
      <video id="video" src="../../media/content/test.ogv"></video>`,
      'Tests Picture-in-Picture event listener breakpoints.\n');

  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });

  await dp.DOM.enable();
  await dp.Runtime.enable();
  await dp.Debugger.enable();
  await dp.DOMDebugger.enable();

  await dp.Runtime.evaluate({
    expression: `${'\n'.repeat(14)}function playVideo()
      {
          var video = document.getElementById("video");
          video.addEventListener("play", onVideoPlay, false);
          video.play();
      }

      function onVideoPlay()
      {
          return 0;
      }

      function requestPictureInPicture()
      {
          var video = document.getElementById("video");
          video.addEventListener("enterpictureinpicture", onEnterPictureInPicture, false);
          video.requestPictureInPicture();
      }

      function onEnterPictureInPicture()
      {
          return 0;
      }

      function exitPictureInPicture()
      {
          var video = document.getElementById("video");
          video.addEventListener("leavepictureinpicture", onLeavePictureInPicture, false);
          document.exitPictureInPicture();
      }

      function onLeavePictureInPicture()
      {
          return 0;
      }

      new Promise(resolve => {
          var video = document.getElementById("video");
          if (video.readyState >= 1)
              resolve();
          else
              video.addEventListener("loadedmetadata", () => resolve(), {once: true});
      })
  //# sourceURL=test://evaluations/0/picture-in-picture-event-listener-breakpoints.js`,
    objectGroup: 'console',
    awaitPromise: true,
  });

  function logCallFrames(callFrames) {
    testRunner.log('Call stack:');
    for (let i = 0; i < callFrames.length; ++i) {
      const frame = callFrames[i];
      const url = (scripts.get(frame.location.scriptId) || frame.url || '')
                      .split('/')
                      .pop();
      testRunner.log(`    ${i}) ${frame.functionName} (${url}:${
          frame.location.lineNumber + 1})`);
    }
  }

  function printEventTargetName(auxData) {
    const targetName = auxData && auxData.targetName;
    if (targetName) {
      testRunner.log('Event target: ' + targetName);
    } else {
      testRunner.log('FAIL: No event target name received!');
    }
  }

  async function runStep(name, eventName, targetNames, expression, userGesture,
                         fetchSource) {
    testRunner.log('\nRunning: ' + name);
    for (const targetName of targetNames) {
      await dp.DOMDebugger.setEventListenerBreakpoint({eventName, targetName});
    }

    const pausedPromise = dp.Debugger.oncePaused();
    await dp.Runtime.evaluate({
      expression:
          `setTimeout(unescape('${encodeURIComponent(expression)}'), 1)`,
      objectGroup: 'console',
      ...(userGesture ? {userGesture: true} : {}),
    });

    const {params: {callFrames, data}} = await pausedPromise;
    if (fetchSource) {
      await dp.Debugger.getScriptSource({
        scriptId: callFrames[0].location.scriptId,
      });
    }
    testRunner.log('Script execution paused.');
    logCallFrames(callFrames);
    printEventTargetName(data);

    for (const targetName of targetNames) {
      await dp.DOMDebugger.removeEventListenerBreakpoint(
          {eventName, targetName});
    }
    const resumedPromise = dp.Debugger.onceResumed();
    await dp.Debugger.resume({terminateOnResume: false});
    await resumedPromise;
    testRunner.log('Script execution resumed.');
  }

  await runStep('testPlayVideoEventBreakpoint', 'play', ['audio', 'video'],
                'playVideo()', false, true);
  await runStep('testEnterPictureInPictureEventBreakpoint',
                'enterpictureinpicture', ['video'], 'requestPictureInPicture()',
                true, false);
  await runStep('testLeavePictureInPictureEventBreakpoint',
                'leavepictureinpicture', ['video'], 'exitPictureInPicture()',
                false, false);

  testRunner.completeTest();
});
