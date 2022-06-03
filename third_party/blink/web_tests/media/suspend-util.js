let UPON_VISIBILITY = 'uponVisibility';

// Requests that |video| suspends upon reaching or exceeding |expectedState|;
// |callback| will be called once the suspend is detected.
function suspendMediaElement(video, expectedState, callback) {
  var pollSuspendState = function() {
    if (!internals.isMediaElementSuspended(video)) {
      window.requestAnimationFrame(pollSuspendState);
      return;
    }

    callback();
  };

  window.requestAnimationFrame(pollSuspendState);
  internals.forceStaleStateForMediaElement(video, expectedState);
}

// Calls play() on |video| and executes t.done() when currentTime > 0.
function completeTestUponPlayback(t, video) {
  var timeWatcher = t.step_func(function() {
    if (video.currentTime > 0) {
      assert_false(internals.isMediaElementSuspended(video),
                   'Element should not be suspended.');
      t.done();
    } else {
      window.requestAnimationFrame(timeWatcher);
    }
  });

  window.requestAnimationFrame(timeWatcher);
  video.play();
}

function preloadMetadataSuspendTest(t, video, src, expectSuspend) {
  assert_true(!!window.internals, 'This test requires windows.internals.');
  video.onerror = t.unreached_func();

  var retried = false;
  var eventListener = t.step_func(function() {
    var hasExpectedSuspendState =
        !!expectSuspend == internals.isMediaElementSuspended(video);

    // The delivery of 'loadedmetadata' may occur immediately before the element
    // reaches the suspended state; so allow one retry in that case.
    if (!hasExpectedSuspendState && !retried) {
      retried = true;
      setTimeout(eventListener, 0);
      return;
    }

    assert_true(hasExpectedSuspendState,
                'Element has incorrect suspend state.');
    if (!expectSuspend) {
      t.done();
      return;
    }

    if (expectSuspend == UPON_VISIBILITY)
      return;

    completeTestUponPlayback(t, video);
  });

  if (expectSuspend == UPON_VISIBILITY) {
    video.requestVideoFrameCallback(t.step_func(function() {
      assert_false(
          internals.isMediaElementSuspended(video),
          'Element should not have been suspended by the first frame.');
      t.done();
    }));
  }

  video.addEventListener('loadedmetadata', eventListener, false);
  video.src = src;
}

function suspendTest(t, video, src, expectedState) {
  assert_true(!!window.internals, 'This test requires windows.internals.');
  video.onerror = t.unreached_func();

  // We can't force a suspend state until loading has started.
  video.addEventListener('loadstart', t.step_func(function() {
    suspendMediaElement(video, expectedState, t.step_func(function() {
      assert_true(internals.isMediaElementSuspended(video),
                  'Element did not suspend as expected.');
      completeTestUponPlayback(t, video);
    }));
  }), false);

  video.src = src;
}
