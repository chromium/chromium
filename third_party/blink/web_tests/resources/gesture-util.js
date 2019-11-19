function waitForCompositorCommit() {
  return new Promise((resolve) => {
    if (window.testRunner) {
      testRunner.capturePixelsAsyncThen(resolve);
    } else {
      // Fall back to just rAF twice.
      window.requestAnimationFrame(() => {
        window.requestAnimationFrame(resolve);
      });
    }
  });
}

// Returns a promise that resolves when the given condition is met or rejects
// after 200 animation frames.
function waitFor(condition, error_message = 'Reaches the maximum frames.') {
  const MAX_FRAME = 200;
  return new Promise((resolve, reject) => {
    function tick(frames) {
      // We requestAnimationFrame either for 200 frames or until condition is
      // met.
      if (frames >= MAX_FRAME)
        reject(error_message);
      else if (condition())
        resolve();
      else
        requestAnimationFrame(tick.bind(this, frames + 1));
    }
    tick(0);
  });
}

// Returns a promise that only gets resolved when the condition is met.
function waitUntil(condition) {
  return new Promise((resolve, reject) => {
    function tick() {
      if (condition())
        resolve();
      else
        requestAnimationFrame(tick.bind(this));
    }
    tick();
  });
}

// Returns a promise that resolves when the given condition holds for 10
// animation frames or rejects if the condition changes to false within 10
// animation frames.
function conditionHolds(condition, error_message = 'Condition is not true anymore.') {
  const MAX_FRAME = 10;
  return new Promise((resolve, reject) => {
    function tick(frames) {
      // We requestAnimationFrame either for 10 frames or until condition is
      // violated.
      if (frames >= MAX_FRAME)
        resolve();
      else if (!condition())
        reject(error_message);
      else
        requestAnimationFrame(tick.bind(this, frames + 1));
    }
    tick(0);
  });
}

// TODO: Frames are animated every 1ms for testing. It may be better to have the
// timeout based on time rather than frame count.
function waitForAnimationEnd(getValue, max_frame, max_unchanged_frame) {
  const MAX_FRAME = max_frame;
  const MAX_UNCHANGED_FRAME = max_unchanged_frame;
  var last_changed_frame = 0;
  var last_position = getValue();
  return new Promise((resolve, reject) => {
    function tick(frames) {
    // We requestAnimationFrame either for MAX_FRAME or until
    // MAX_UNCHANGED_FRAME with no change have been observed.
      if (frames >= MAX_FRAME || frames - last_changed_frame > MAX_UNCHANGED_FRAME) {
        resolve();
      } else {
        current_value = getValue();
        if (last_position != current_value) {
          last_changed_frame = frames;
          last_position = current_value;
        }
        requestAnimationFrame(tick.bind(this, frames + 1));
      }
    }
    tick(0);
  })
}

// Enums for gesture_source_type parameters in gpuBenchmarking synthetic
// gesture methods. Must match C++ side enums in synthetic_gesture_params.h
const GestureSourceType = (function() {
  var isDefined = (window.chrome && chrome.gpuBenchmarking);
  return {
    DEFAULT_INPUT: isDefined && chrome.gpuBenchmarking.DEFAULT_INPUT,
    TOUCH_INPUT: isDefined && chrome.gpuBenchmarking.TOUCH_INPUT,
    MOUSE_INPUT: isDefined && chrome.gpuBenchmarking.MOUSE_INPUT,
    TOUCHPAD_INPUT: isDefined && chrome.gpuBenchmarking.TOUCHPAD_INPUT,
    PEN_INPUT: isDefined && chrome.gpuBenchmarking.PEN_INPUT,
    ToString: function(value) {
      if (!isDefined)
        return 'Synthetic gestures unavailable';
      switch (value) {
        case chrome.gpuBenchmarking.DEFAULT_INPUT:
          return 'DefaultInput';
        case chrome.gpuBenchmarking.TOUCH_INPUT:
          return 'Touchscreen';
        case chrome.gpuBenchmarking.MOUSE_INPUT:
          return 'MouseWheel/Touchpad';
        case chrome.gpuBenchmarking.PEN_INPUT:
          return 'Pen';
        default:
          return 'Invalid';
      }
    }
  }
})();

// Use this for speed to make gestures (effectively) instant. That is, finish
// entirely within one Begin|Update|End triplet. This is in physical
// pixels/second.
// TODO(bokan): This isn't really instant but high enough that it works for
// current purposes. This should be replaced with the Infinity value and
// the synthetic gesture code modified to guarantee the single update behavior.
// https://crbug.com/893608
const SPEED_INSTANT = 400000;

function smoothScroll(pixels_to_scroll, start_x, start_y, gesture_source_type, direction, speed_in_pixels_s, precise_scrolling_deltas, scroll_by_page, cursor_visible) {
  return new Promise((resolve, reject) => {
    if (window.chrome && chrome.gpuBenchmarking) {
      chrome.gpuBenchmarking.smoothScrollBy(pixels_to_scroll,
                                            resolve,
                                            start_x,
                                            start_y,
                                            gesture_source_type,
                                            direction,
                                            speed_in_pixels_s,
                                            precise_scrolling_deltas,
                                            scroll_by_page,
                                            cursor_visible);
    } else {
      reject('This test requires chrome.gpuBenchmarking');
    }
  });
}

// Returns the number of pixels per wheel tick which is a platform specific value.
function pixelsPerTick() {
  // Comes from ui/events/event.cc
  if (navigator.platform.indexOf("Win") != -1)
    return 120;

  if (navigator.platform.indexOf("Mac") != -1 || navigator.platform.indexOf("iPhone") != -1 ||
      navigator.platform.indexOf("iPod") != -1 || navigator.platform.indexOf("iPad") != -1) {
    return 40;
  }

  // Some android devices return android while others return Android.
  if (navigator.platform.toLowerCase().indexOf("android") != -1)
    return 64;

  // Comes from ui/events/event.cc
  return 53;
}

function swipe(pixels_to_scroll, start_x, start_y, direction, speed_in_pixels_s, fling_velocity, gesture_source_type) {
  return new Promise((resolve, reject) => {
    if (window.chrome && chrome.gpuBenchmarking) {
      chrome.gpuBenchmarking.swipe(direction,
                                   pixels_to_scroll,
                                   resolve,
                                   start_x,
                                   start_y,
                                   speed_in_pixels_s,
                                   fling_velocity,
                                   gesture_source_type);
    } else {
      reject('This test requires chrome.gpuBenchmarking');
    }
  });
}

function pinchBy(scale, centerX, centerY, speed_in_pixels_s, gesture_source_type) {
  return new Promise((resolve, reject) => {
    if (window.chrome && chrome.gpuBenchmarking) {
      chrome.gpuBenchmarking.pinchBy(scale,
                                     centerX,
                                     centerY,
                                     resolve,
                                     speed_in_pixels_s,
                                     gesture_source_type);
    } else {
      reject('This test requires chrome.gpuBenchmarking');
    }
  });
}


function mouseMoveTo(xPosition, yPosition) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      chrome.gpuBenchmarking.pointerActionSequence([
        {source: 'mouse',
         actions: [
            { name: 'pointerMove', x: xPosition, y: yPosition },
      ]}], resolve);
    } else {
      reject('This test requires chrome.gpuBenchmarking');
    }
  });
}

function mouseDownAt(xPosition, yPosition) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      chrome.gpuBenchmarking.pointerActionSequence([
        {source: 'mouse',
         actions: [
            { name: 'pointerDown', x: xPosition, y: yPosition },
      ]}], resolve);
    } else {
      reject('This test requires chrome.gpuBenchmarking');
    }
  });
}

function mouseUpAt(xPosition, yPosition) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      chrome.gpuBenchmarking.pointerActionSequence([
        {source: 'mouse',
         actions: [
            { name: 'pointerUp', x: xPosition, y: yPosition },
      ]}], resolve);
    } else {
      reject('This test requires chrome.gpuBenchmarking');
    }
  });
}

// Simulate a mouse click on point.
function mouseClickOn(x, y, button = 0 /* left */, keys = '') {
  return new Promise((resolve, reject) => {
    if (window.chrome && chrome.gpuBenchmarking) {
      let pointerActions = [{
        source: 'mouse',
        actions: [
          { 'name': 'pointerMove', 'x': x, 'y': y },
          { 'name': 'pointerDown', 'x': x, 'y': y, 'button': button, 'keys': keys  },
          { 'name': 'pointerUp', 'button': button },
        ]
      }];
      chrome.gpuBenchmarking.pointerActionSequence(pointerActions, resolve);
    } else {
      reject('This test requires chrome.gpuBenchmarking');
    }
  });
}

// Simulate a mouse press on point for a certain time.
function mousePressOn(x, y, t) {
  return new Promise((resolve, reject) => {
    if (window.chrome && chrome.gpuBenchmarking) {
      let pointerActions = [{
        source: 'mouse',
        actions: [
          { 'name': 'pointerMove', 'x': x, 'y': y },
          { 'name': 'pointerDown', 'x': x, 'y': y },
          { 'name': 'pause', duration: t},
          { 'name': 'pointerUp' },
        ]
      }];
      chrome.gpuBenchmarking.pointerActionSequence(pointerActions, resolve);
    } else {
      reject('This test requires chrome.gpuBenchmarking');
    }
  });
}

// Simulate a mouse drag and drop. mouse down at {start_x, start_y}, move to
// {end_x, end_y} and release.
function mouseDragAndDrop(start_x, start_y, end_x, end_y, button = 0 /* left */, t = 0) {
  return new Promise((resolve, reject) => {
    if (window.chrome && chrome.gpuBenchmarking) {
      let pointerActions = [{
        source: 'mouse',
        actions: [
          { 'name': 'pointerMove', 'x': start_x, 'y': start_y },
          { 'name': 'pointerDown', 'x': start_x, 'y': start_y, 'button': button },
          { 'name': 'pause', 'duration': t},
          { 'name': 'pointerMove', 'x': end_x, 'y': end_y },
          { 'name': 'pause', 'duration': t},
          { 'name': 'pointerUp', 'button': button },
        ]
      }];
      chrome.gpuBenchmarking.pointerActionSequence(pointerActions, resolve);
    } else {
      reject('This test requires chrome.gpuBenchmarking');
    }
  });
}

// Helper functions used in some of the gesture scroll layouttests.
function recordScroll() {
  scrollEventsOccurred++;
}
function notScrolled() {
  return scrolledElement.scrollTop == 0 && scrolledElement.scrollLeft == 0;
}
function checkScrollOffset() {
  // To avoid flakiness up to two pixels off per gesture is allowed.
  var pixels = 2 * (gesturesOccurred + 1);
  var result = approx_equals(scrolledElement.scrollTop, scrollAmountY[gesturesOccurred], pixels) &&
      approx_equals(scrolledElement.scrollLeft, scrollAmountX[gesturesOccurred], pixels);
  if (result)
    gesturesOccurred++;

  return result;
}

// This promise gets resolved in iframe onload.
var iframeLoadResolve;
iframeOnLoadPromise = new Promise(function(resolve) {
  iframeLoadResolve = resolve;
});

// Include run-after-layout-and-paint.js to use this promise.
function WaitForlayoutAndPaint() {
  return new Promise((resolve, reject) => {
    if (typeof runAfterLayoutAndPaint !== 'undefined')
      runAfterLayoutAndPaint(resolve);
    else
      reject('This test requires run-after-layout-and-paint.js');
  });
}

function touchTapOn(xPosition, yPosition) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      chrome.gpuBenchmarking.pointerActionSequence( [
        {source: 'touch',
         actions: [
            { name: 'pointerDown', x: xPosition, y: yPosition },
            { name: 'pointerUp' }
        ]}], resolve);
    } else {
      reject();
    }
  });
}

function doubleTapAt(xPosition, yPosition) {
  // This comes from config constants in gesture_detector.cc.
  const DOUBLE_TAP_MINIMUM_DURATION_MS = 40;

  return new Promise(function(resolve, reject) {
    if (!window.chrome || !chrome.gpuBenchmarking) {
      reject("chrome.gpuBenchmarking not found.");
      return;
    }

    chrome.gpuBenchmarking.pointerActionSequence( [{
      source: 'touch',
      actions: [
        { name: 'pointerDown', x: xPosition, y: yPosition },
        { name: 'pointerUp' },
        { name: 'pause', duration: DOUBLE_TAP_MINIMUM_DURATION_MS },
        { name: 'pointerDown', x: xPosition, y: yPosition },
        { name: 'pointerUp' }
      ]
    }], resolve);
  });
}

function approx_equals(actual, expected, epsilon) {
  return actual >= expected - epsilon && actual <= expected + epsilon;
}

// Returns the given element's client rect center in an object with |x| and |y|
// properties. Client rect being relative to the layout viewport. i.e. this will
// not do what you thing if the page is pinch-zoomed.
function elementCenter(element) {
  const rect = element.getBoundingClientRect();
  return {
    x: rect.x + rect.width / 2,
    y: rect.y + rect.height / 2
  };
}

// Waits for 'time' ms before resolving the promise.
function waitForMs(time) {
  return new Promise((resolve) => {
    window.setTimeout(function() { resolve(); }, time);
  });
}

// Requests an animation frame.
function raf() {
  return new Promise((resolve) => {
    requestAnimationFrame(() => {
      resolve();
    });
  });
}
