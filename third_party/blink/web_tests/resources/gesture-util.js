/*
  gpuBenchmarking finishes a gesture by sending a completion callback to the
  renderer after the final input event. When the renderer receives the callback
  it requests a new frame. This should flush all input through the system - and
  DOM events should synchronously run here - and produce a compositor frame.
  The callback is resolved when the frame is presented to the screen.
  For methods in this file, the callback is the resolve method of the Promise
  returned.

  Example:
  await mouseMoveTo(10,10);
  The await returns after the mousemove event fired.

  Note:
  Given the event handler runs synchronous code, the await returns after
  the event handler finished running.
*/

function waitForCompositorCommit() {
  return new Promise((resolve) => {
    if (window.testRunner) {
      // After doing the composite, we also allow any async tasks to run before
      // resolving, via setTimeout().
      testRunner.updateAllLifecyclePhasesAndCompositeThen(() => {
        window.setTimeout(resolve, 0);
      });
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

// TODO(bokan): Replace uses of the above with this one.
function waitForAnimationEndTimeBased(getValue) {
  // Give up if the animation still isn't done after this many milliseconds.
  const TIMEOUT_MS = 1000;

  // If the value is unchanged for this many milliseconds, we consider the
  // animation ended and return.
  const END_THRESHOLD_MS = 200;

  const START_TIME = performance.now();

  let last_changed_time = START_TIME;
  let last_value = getValue();
  return new Promise((resolve, reject) => {
    function tick() {
      let cur_time = performance.now();

      if (cur_time - last_changed_time > END_THRESHOLD_MS) {
        resolve();
        return;
      }

      if (cur_time - START_TIME > TIMEOUT_MS) {
        reject(new Error("Timeout waiting for animation to end"));
        return;
      }

      let current_value = getValue();
      if (last_value != current_value) {
        last_changed_time = cur_time;
        last_value = current_value;
      }

      requestAnimationFrame(tick);
    }
    tick();
  })
}

function waitForEvent(eventTarget, eventName, timeoutMs = 2000) {
  return new Promise((resolve, reject) => {
    const eventListener = (evt) => {
      clearTimeout(timeout);
      eventTarget.removeEventListener(eventName, eventListener);
      resolve(evt);
    };
    let timeout = setTimeout(() => {
      eventTarget.removeEventListener(eventName, eventListener);
      reject(`Timeout waiting for ${eventName} event`);
    }, timeoutMs);
    eventTarget.addEventListener(eventName, eventListener);
  });
}

function waitForScrollEvent(eventTarget, timeoutMs = 2000) {
  return waitForEvent(eventTarget, 'scroll', timeoutMs);
}

function waitForScrollendEvent(eventTarget, timeoutMs = 2000) {
  return waitForEvent(eventTarget, 'scrollend', timeoutMs);
}

// Event driven scroll promise. This method has the advantage over timing
// methods, as it is more forgiving to delays in event dispatch or between
// chained smooth scrolls. It has an additional advantage of completing sooner
// once the end condition is reached.
// The promise is resolved when the result of calling getValue matches the
// target value. The timeout timer starts once the first event has been
// received.
function waitForScrollEnd(eventTarget, getValue, targetValue, errorMessage) {
  // Give up if the animation still isn't done after this many milliseconds from
  // the time of the first scroll event.
  const TIMEOUT_MS = 1000;

  return new Promise((resolve, reject) => {
    let timeout = undefined;
    const scrollListener = () => {
      if (!timeout)
        timeout = setTimeout(() => {
          reject(errorMessage || 'Timeout waiting for scroll end');
        }, TIMEOUT_MS);

      if (getValue() == targetValue) {
        clearTimeout(timeout);
        eventTarget.removeEventListener('scroll', scrollListener);
        // Wait for a commit to allow the scroll to propagate through the
        // compositor before resolving.
        return waitForCompositorCommit().then(() => { resolve(); });
      }
    };
    if (getValue() == targetValue)
      resolve();
    else
      eventTarget.addEventListener('scroll', scrollListener);
  });
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

// Enums used as input to the |modifier_keys| parameters of methods in this
// file like smoothScrollWithXY and wheelTick.
const Modifiers = (function() {
  return {
    ALT: "Alt",
    CONTROL: "Control",
    META: "Meta",
    SHIFT: "Shift",
    CAPSLOCK: "CapsLock",
    NUMLOCK: "NumLock",
    ALTGRAPH: "AltGraph",
  }
})();

// Enums used as input to the |modifier_buttons| parameters of methods in this
// file like smoothScrollWithXY and wheelTick.
const Buttons = (function() {
  return {
    LEFT: "Left",
    MIDDLE: "Middle",
    RIGHT: "Right",
    BACK: "Back",
    FORWARD: "Forward",
  }
})();

// Takes a value from the Buttons enum (above) and returns an integer suitable
// for the "button" property in the gpuBenchmarking.pointerActionSequence API.
// Keep in sync with ToSyntheticMouseButton in actions_parser.cc.
function pointerActionButtonId(button_str) {
  if (button_str === undefined)
    return undefined;

  switch (button_str) {
    case Buttons.LEFT:
      return 0;
    case Buttons.MIDDLE:
      return 1;
    case Buttons.RIGHT:
      return 2;
    case Buttons.BACK:
      return 3;
    case Buttons.FORWARD:
      return 4;
  }
  throw new Error("invalid button");
}

// Use this for speed to make gestures (effectively) instant. That is, finish
// entirely within one Begin|Update|End triplet. This is in physical
// pixels/second.
// TODO(bokan): This isn't really instant but high enough that it works for
// current purposes. This should be replaced with the Infinity value and
// the synthetic gesture code modified to guarantee the single update behavior.
// https://crbug.com/893608
const SPEED_INSTANT = 400000;

// Constant wheel delta value when percent based scrolling is enabled
const WHEEL_DELTA = 100;

// kMinFractionToStepWhenPaging constant from cc/input/scroll_utils.h
const MIN_FRACTION_TO_STEP_WHEN_PAGING = 0.875;

// This will be replaced by smoothScrollWithXY.
function smoothScroll(pixels_to_scroll, start_x, start_y, gesture_source_type,
                      direction, speed_in_pixels_s, precise_scrolling_deltas,
                      scroll_by_page, cursor_visible, scroll_by_percentage,
                      modifier_keys) {
  let pixels_to_scroll_x = 0;
  let pixels_to_scroll_y = 0;
  if (direction == "down") {
    pixels_to_scroll_y = pixels_to_scroll;
  } else if (direction == "up") {
    pixels_to_scroll_y = -pixels_to_scroll;
  } else if (direction == "right") {
    pixels_to_scroll_x = pixels_to_scroll;
  } else if (direction == "left") {
    pixels_to_scroll_x = -pixels_to_scroll;
  } else if (direction == "upleft") {
    pixels_to_scroll_x = -pixels_to_scroll;
    pixels_to_scroll_y = -pixels_to_scroll;
  } else if (direction == "upright") {
    pixels_to_scroll_x = pixels_to_scroll;
    pixels_to_scroll_y = -pixels_to_scroll;
  } else if (direction == "downleft") {
    pixels_to_scroll_x = -pixels_to_scroll;
    pixels_to_scroll_y = pixels_to_scroll;
  } else if (direction == "downright") {
    pixels_to_scroll_x = pixels_to_scroll;
    pixels_to_scroll_y = pixels_to_scroll;
  }
  return smoothScrollWithXY(pixels_to_scroll_x, pixels_to_scroll_y, start_x,
                            start_y, gesture_source_type, speed_in_pixels_s,
                            precise_scrolling_deltas, scroll_by_page,
                            cursor_visible, scroll_by_percentage, modifier_keys);
}

// Perform a percent based scroll using smoothScrollWithXY
function percentScroll(percent_to_scroll_x, percent_to_scroll_y, start_x, start_y, gesture_source_type) {
  return smoothScrollWithXY(percent_to_scroll_x, percent_to_scroll_y, start_x, start_y,
    gesture_source_type,
    undefined /* speed_in_pixels_s - not defined for percent based scrolls */,
    false /* precise_scrolling_deltas */,
    false /* scroll_by_page */,
    true /* cursor_visible */,
    true /* scroll_by_percentage */);
}

// modifier_keys means the keys pressed while doing the mouse wheel scroll, it
// should be one of the values in the |Modifiers| or a comma separated string
// to specify multiple values.
// modifier_buttons means the mouse buttons pressed while doing the mouse wheel
// scroll, it should be one of the values in the |Buttons| or a comma separated
// string to specify multiple values.
function smoothScrollWithXY(pixels_to_scroll_x, pixels_to_scroll_y, start_x,
                            start_y, gesture_source_type, speed_in_pixels_s,
                            precise_scrolling_deltas, scroll_by_page,
                            cursor_visible, scroll_by_percentage, modifier_keys,
                            modifier_buttons) {
  return new Promise((resolve, reject) => {
    if (window.chrome && chrome.gpuBenchmarking) {
      chrome.gpuBenchmarking.smoothScrollByXY(pixels_to_scroll_x,
                                              pixels_to_scroll_y,
                                              resolve,
                                              start_x,
                                              start_y,
                                              gesture_source_type,
                                              speed_in_pixels_s,
                                              precise_scrolling_deltas,
                                              scroll_by_page,
                                              cursor_visible,
                                              scroll_by_percentage,
                                              modifier_keys,
                                              modifier_buttons);
    } else {
      reject('This test requires chrome.gpuBenchmarking');
    }
  });
}

// modifier_keys means the keys pressed while doing the mouse wheel scroll, it
// should be one of the values in the |Modifiers| or a comma separated string
// to specify multiple values.
// modifier_buttons means the mouse buttons pressed while doing the mouse wheel
// scroll, it should be one of the values in the |Buttons| or a comma separated
// string to specify multiple values.
function wheelTick(scroll_tick_x, scroll_tick_y, center, speed_in_pixels_s,
                   modifier_keys, modifier_buttons) {
  if (typeof(speed_in_pixels_s) == "undefined")
    speed_in_pixels_s = SPEED_INSTANT;
  // Do not allow precise scrolling deltas for tick wheel scroll.
  return smoothScrollWithXY(scroll_tick_x * pixelsPerTick(),
                            scroll_tick_y * pixelsPerTick(),
                            center.x, center.y, GestureSourceType.MOUSE_INPUT,
                            speed_in_pixels_s, false /* precise_scrolling_deltas */,
                            false /* scroll_by_page */, true /* cursor_visible */,
                            false /* scroll_by_percentage */, modifier_keys,
                            modifier_buttons);
}

const LEGACY_MOUSE_WHEEL_TICK_MULTIPLIER = 120;

// Returns the number of pixels per wheel tick which is a platform specific value.
function pixelsPerTick() {
  // Comes from ui/events/event.cc
  if (navigator.platform.indexOf("Win") != -1 || navigator.platform.indexOf("Linux") != -1)
    return 120;

  if (navigator.platform.indexOf("Mac") != -1 || navigator.platform.indexOf("iPhone") != -1 ||
      navigator.platform.indexOf("iPod") != -1 || navigator.platform.indexOf("iPad") != -1) {
    return 40;
  }

  // Some android devices return android while others return Android.
  if (navigator.platform.toLowerCase().indexOf("android") != -1)
    return 64;

  // Legacy, comes from ui/events/event.cc
  return 53;
}

// Note: unlike other functions in this file, the |direction| parameter here is
// the "finger direction". This means |y| pixels "up" causes the finger to move
// up so the page scrolls down (i.e. scrollTop increases).
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


function mouseMoveTo(xPosition, yPosition, withButtonPressed) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      chrome.gpuBenchmarking.pointerActionSequence([
        {source: 'mouse',
         actions: [
            { name: 'pointerMove', x: xPosition, y: yPosition,
              button: pointerActionButtonId(withButtonPressed) },
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
            { name: 'pointerMove', x: xPosition, y: yPosition },
            { name: 'pointerUp' },
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

// Simulate a mouse double click on point.
function mouseDoubleClickOn(x, y, button = 0 /* left */, keys = '') {
  return new Promise((resolve, reject) => {
    if (window.chrome && chrome.gpuBenchmarking) {
      let pointerActions = [{
        source: 'mouse',
        actions: [
          { 'name': 'pointerMove', 'x': x, 'y': y },
          { 'name': 'pointerDown', 'x': x, 'y': y, 'button': button, 'keys': keys  },
          { 'name': 'pointerUp', 'button': button },
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

function touchPull(pull) {
  const PREVENT_FLING_PAUSE = 40;
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      chrome.gpuBenchmarking.pointerActionSequence( [
        {source: 'touch',
         actions: [
            { name: 'pointerDown', x: pull.start_x, y: pull.start_y },
            { name: 'pause', duration: PREVENT_FLING_PAUSE },
            { name: 'pointerMove', x: pull.end_x, y: pull.end_y},
            { name: 'pause', duration: PREVENT_FLING_PAUSE },
        ]}], resolve);
    } else {
      reject('This test requires chrome.gpuBenchmarking');
    }
  });
}

function touchDragTo(drag) {
  const PREVENT_FLING_PAUSE = 40;
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      chrome.gpuBenchmarking.pointerActionSequence( [
        {source: 'touch',
         actions: [
            { name: 'pointerDown', x: drag.start_x, y: drag.start_y },
            { name: 'pause', duration: PREVENT_FLING_PAUSE },
            { name: 'pointerMove', x: drag.end_x, y: drag.end_y},
            { name: 'pause', duration: PREVENT_FLING_PAUSE },
            { name: 'pointerUp', x: drag.end_x, y: drag.end_y }
        ]}], resolve);
    } else {
      reject('This test requires chrome.gpuBenchmarking');
    }
  });
}

// Trigger fling by doing pointerUp right after pointerMoves.
function touchFling(drag) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      chrome.gpuBenchmarking.pointerActionSequence( [
        {source: 'touch',
         actions: [
            { name: 'pointerDown', x: drag.start_x, y: drag.start_y },
            { name: 'pointerMove', x: drag.end_x, y: drag.end_y},
            { name: 'pointerUp', x: drag.end_x, y: drag.end_y }
        ]}], resolve);
    } else {
      reject('This test requires chrome.gpuBenchmarking');
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

function clientToViewport(client_point) {
  const viewport_point = {
    x: (client_point.x - visualViewport.offsetLeft) * visualViewport.scale,
    y: (client_point.y - visualViewport.offsetTop) * visualViewport.scale
  };
  return viewport_point;
}

// Returns the center point of the given element's rect in visual viewport
// coordinates.  Returned object is a point with |x| and |y| properties.
function elementCenter(element) {
  const rect = element.getBoundingClientRect();
  const center_point = {
    x: rect.x + rect.width / 2,
    y: rect.y + rect.height / 2
  };
  return clientToViewport(center_point);
}

// Returns a position in the given element with an offset of |x| and |y| from
// the element's top-left position. Returned object is a point with |x| and |y|
// properties. The returned coordinate is in visual viewport coordinates.
function pointInElement(element, x, y) {
  const rect = element.getBoundingClientRect();
  const point = {
    x: rect.x + x,
    y: rect.y + y
  };
  return clientToViewport(point);
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

// Resets the scroll position to (0,0).  If a scroll is required, then the
// promise is not resolved until the scrollend event is received.
async function waitForScrollReset(scroller) {
  return new Promise(resolve => {
    if (scroller.scrollTop == 0 &&
        scroller.scrollLeft == 0) {
      resolve();
    } else {
      const eventTarget =
        scroller == document.scrollingElement ? document : scroller;
      scroller.scrollTop = 0;
      scroller.scrollLeft = 0;
      waitForScrollendEvent(eventTarget).then(resolve);
    }
  });
}

// Call with an asynchronous function that triggers a scroll. The promise is
// resolved once |scrollendEventReceiver| gets the scrollend event.
async function triggerScrollAndWaitForScrollEnd(
    scrollTriggerFn, scrollendEventReceiver = document) {
  const scrollPromise = waitForScrollendEvent(scrollendEventReceiver);
  await scrollTriggerFn();
  return scrollPromise;
}

// Generates a synthetic click and returns a promise that is resolved once
// |scrollendEventReceiver| gets the scrollend event.
async function clickAndWaitForScroll(x, y, scrollendEventReceiver = document) {
  return triggerScrollAndWaitForScrollEnd(async () => {
    if (!window.test_driver) {
      throw new Error('Test requires import of testdriver. Please add ' +
                      'testdriver.js, testdriver-actions.js and ' +
                      'testdriver-vendor.js to your test file');
    }

    return new test_driver.Actions()
        .pointerMove(x, y)
        .pointerDown()
        .addTick()
        .pointerUp()
        .send();
  }, scrollendEventReceiver);
}
