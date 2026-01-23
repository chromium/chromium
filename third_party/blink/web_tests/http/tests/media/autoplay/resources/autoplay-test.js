/**
 * Tests autoplay on a page and its subframes.
 */

// Tracks the results and how many active tests we have running.
let testExpectations = {};
let doneCallback;
let resultCallback = (data) => { top.postMessage(data, '*') };
let completedTestResults = [];
let results = {};

function tearDown() {
  // Reset the flag state.
  internals.settings.setAutoplayPolicy('no-user-gesture-required');

  // Ensure that play failed because autoplay was blocked. If playback failed
  // for another reason then we don't care because autoplay is always checked
  // first.
  const canAutoplayMedia = !results.media ||
                           results.media.name != 'NotAllowedError';
  const canAutoplayWebAudio = results.webAudio == 'running';

  // `canAutoplay` must match autoplay for both media element and Web Audio.
  // Special value `undefined` will be propagated otherwise.
  const canAutoplay = canAutoplayMedia == canAutoplayWebAudio ? canAutoplayMedia
                                                              : undefined;

  receivedResult({
    url: window.location.href,
    message: canAutoplay
  });
}

function receivedResult(data) {
  // Forward the result to the top frame.
  if (!doneCallback) {
    top.postMessage(data, '*');
    return;
  }

  completedTestResults.push(data);
  processTestResults();
}

function processTestResults() {
  // Check if we have completed all the tests.
  if (Object.keys(testExpectations).length != completedTestResults.length)
    return;

  completedTestResults.forEach((data) => {
    assert_equals(testExpectations[data.url], data.message);
  });

  doneCallback();
}

function runMediaTest() {
  return new Promise(resolve => {
    const video = document.createElement('video');
    video.src = '/media-resources/content/test.ogv';
    video.play().then(result => results.media = result,
                      result => results.media = result)
                .then(resolve);
  });
}

async function runWebAudioTest() {
  const audioContext = new AudioContext();

  if (audioContext.state === 'running') {
    results.webAudio = audioContext.state;
    return;
  }

  // When autoplay is allowed, we wait for the state transition to "running".
  // When autoplay is blocked, no "statechange" event will be fired.
  if (results.media?.name !== 'NotAllowedError') {
    await new Promise(resolve => {
      audioContext.onstatechange = resolve;
    });
  }
  results.webAudio = audioContext.state;
}

async function runAutoplayTest() {
  await runMediaTest();
  await runWebAudioTest();
  tearDown();
}

function simulateViewportClick(callback) {
  chrome.gpuBenchmarking.pointerActionSequence([
      {"source": "mouse",
       "actions": [
       { "name": "pointerDown", "x": 0, "y": 0 },
       { "name": "pointerUp" } ]}], callback);
}

function simulateFrameClick(callback) {
  const frame = document.getElementsByTagName('iframe')[0];
  const rect = frame.getBoundingClientRect();

  chrome.gpuBenchmarking.pointerActionSequence([
      {"source": "mouse",
       "actions": [
       {
         "name": "pointerDown",
         "x": rect.left + (rect.width / 2),
         "y": rect.top + (rect.height / 2)
       },
       { "name": "pointerUp" } ]}], callback);
}

function simulateNoGesture(callback) {
  callback();
}

function runTest(pointerSequence, expectations) {
  // Setup the global variables.
  expectations.forEach((expectation) => {
    testExpectations[expectation[0]] = expectation[1];
  });

  // Run the test.
  async_test((t) => {
    doneCallback = t.step_func_done();

    // Fire the pointer sequence and then run the video test.
    pointerSequence(t.step_func(() => {
      runAutoplayTest();

      // Navigate the iframe now we have the gesture.
      document.getElementsByTagName('iframe')[0].src =
          expectations[1][0];
    }));

    resultCallback = t.step_func(receivedResult);
  });
}

// Setup the flags before the test is run.
internals.settings.setAutoplayPolicy('document-user-activation-required');
internals.runtimeFlags.autoplayIgnoresWebAudioEnabled = false;

// Setup the event listener to forward messages.
window.addEventListener('message', (e) => { resultCallback(e.data); });

// If we are on an iframe then run the video test automatically.
if (window.self !== window.top)
  runAutoplayTest();
