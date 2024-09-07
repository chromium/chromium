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

async function waitForCompositorReady() {
  const animation =
      document.body.animate({ opacity: [ 0, 1 ] }, {duration: 1 });
  return animation.finished;
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

// Waits until scrolling has stopped for a period of time.
// @deprecated:
//    If a scroll is expected then use waitForScrollendEvent.
//    TODO(kevers): Not possible in all cases, e.g. an input field does not
//    fire a scrollend event when scrolled. This is the exception rather than
//    the rule. Once each remaining call has been reviewed, we can see if
//    there are enough special cases to warrant a waitForScrollend with
//    "polyfilled"  behavior in cases where a scrollend event is not expected.
//    If asserting that no scroll takes place then:
//        Add a scroll listener with assert_unreached(message)
//        If possible, wait on a sentinel event that indicates when handling of
//        the gesture is complete. Otherwise, wait a few animation frames.
//        Pointerup is an example of a suitable sentinel event if the test
//        is driven by pointer events since pointerup is replace by
//        pointercancel if scrolling occurs.
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

function scrollendEventTarget(scroller) {
  const isRootScroller =
    scroller == scroller.ownerDocument.scrollingElement;
  return isRootScroller ? scroller.ownerDocument : scroller;
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

// The number of pixels keyboard arrows will scroll when the device scale factor
// equals to 1. Defined in cc/input/scroll_utils.h.
// Matches SCROLLBAR_SCROLL_PIXELS from scrollbar-util.js.
const KEYBOARD_SCROLL_PIXELS = 40;

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

// Improves test readability by accepting a struct.
function mouseClickHelper(point) {
  return mouseClickOn(point.x, point.y, point.left_click, point.input_modifier);
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

// @deprecated: Use touchDrag, which uses test-driver.
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



// Convenience enums for elementPosition function.
ElementAlignment = {
  LEFT: 0,
  TOP: 0,
  CENTER: 0.5,
  BOTTOM: 1,
  RIGHT: 1
};

// Returns a point within an element's rect in visual viewport
// coordinates. The relative offsets are between 0 (left or top) and 1
// (right or bottom). Returned object is a point with |x| and |y| properties.
function elementPosition(element,
                         relativeHorizontalOffset,
                         relativeVerticalOffset,
                         insets = { x: 0, y: 0 } ) {
  const rect = element.getBoundingClientRect();
  rect.x += insets.x;
  rect.y += insets.y;
  rect.width -= 2 * insets.x;
  rect.height -= 2 * insets.y;
  const center_point = {
    x: rect.x + relativeHorizontalOffset * rect.width,
    y: rect.y + relativeVerticalOffset * rect.height
  };
  return clientToViewport(center_point);
}

// Returns the center point of the given element's rect in visual viewport
// coordinates.  Returned object is a point with |x| and |y| properties.
function elementCenter(element) {
  return elementPosition(element,
                         ElementAlignment.CENTER,
                         ElementAlignment.CENTER);
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

// Resets the scroll position to (x,y). If a scroll is required, then the
// promise is not resolved until the scrollend event is received.
async function waitForScrollReset(scroller = document.scrollingElement,
                                  x = 0, y = 0) {
  return new Promise(resolve => {
    if (scroller.scrollTop == y &&
        scroller.scrollLeft == x) {
      resolve();
    } else {
      scroller.scrollTop = y;
      scroller.scrollLeft = x;
      // Though setting the scroll position is synchronous, it still triggers a
      // scrollend event, and we need to wait for this event before continuing
      // so that it is not mistakenly attributed to a later scroll trigger.
      waitForScrollendEvent(scrollendEventTarget(scroller)).then(resolve);
    }
  });
}

function waitForWindowScrollBy(options) {
  const scrollPromise = waitForScrollendEvent(document);
  window.scrollBy(options);
  return scrollPromise;
}

function waitForWindowScrollTo(options) {
  const scrollPromise = waitForScrollendEvent(document);
  window.scrollTo(options);
  return scrollPromise;
}

// Verifies that triggered scroll animations smoothly. Requires at least 2
// scroll updates to be considered smooth.
function animatedScrollPromise(scrollTarget) {
  return new Promise((resolve, reject) => {
    // Set to roughly a third to a quarter of the expected animation duration.
    const maxAllowableFrameIntervalInMs = 80;
    let scrollCount = 0;
    let ticking = true;
    let lastFrameTime = performance.now();
    let largestFrameInterval = undefined;

    // Pump rAFs until the scrollend event is received inspecting the timing
    // between frames. If the frame interval becomes too large, detection of a
    // smooth scroll becomes unreliable. Record this outcome as a pass. A fail
    // is recorded when we don't see a smooth scroll, but had the opportunity
    // to observe one.
    const tick = () => {
      requestAnimationFrame((frameTime) => {
        const frameInterval = frameTime - lastFrameTime;
        if (!largestFrameInterval || frameInterval > largestFrameInterval) {
          largestFrameInterval = frameInterval;
        }
        lastFrameTime = frameTime;
        if (ticking) {
          requestAnimationFrame(tick);
        } else {
          cleanup();
          if (scrollCount > 1) {
            resolve();
          } else if (largestFrameInterval > maxAllowableFrameIntervalInMs) {
            // Though we didn't see a smooth scroll, we didn't have the
            // opportunity because of the coarse granularity of main frame
            // updates. What this means is that test could trigger a false pass
            // should animated scrolls be turned off; however, safer to
            // relax expectations than flake.
            resolve();
          } else {
             reject('expected smooth scroll');
           }
        }
      });
    };
    tick();
    const scrollListener = () => {
      scrollCount++;
    }
    const scrollendListener = (event) => {
      ticking = false;
    }
    const scrollendTarget =
        scrollTarget == document.scrollingElement ? document : scrollTarget;

    scrollTarget.addEventListener('scroll', scrollListener);
    scrollendTarget.addEventListener('scrollend', scrollendListener);
    const cleanup = () => {
      scrollTarget.removeEventListener('scroll', scrollListener);
      scrollendTarget.removeEventListener('scrollend', scrollendListener);
    };
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

function verifyTestDriverLoaded() {
  if (!window.test_driver) {
    throw new Error('Test requires import of testdriver. Please add ' +
                    'testdriver.js, testdriver-actions.js and ' +
                    'testdriver-vendor.js to your test file');
  }
}

// Generates a synthetic click and returns a promise that is resolved once
// |scrollendEventReceiver| gets the scrollend event.
async function clickAndWaitForScroll(x, y, scrollendEventReceiver = document) {
  verifyTestDriverLoaded();
  return triggerScrollAndWaitForScrollEnd(async () => {
    return new test_driver.Actions()
        .pointerMove(x, y)
        .pointerDown()
        .addTick()
        .pointerUp()
        .send();
  }, scrollendEventReceiver);
}

// Verify that a point is onscreen.  Origin may be "viewport" or an element.
// In the case of an element, (x,y) is relative to the center of the element.
function assert_point_within_viewport(x, y, origin = "viewport") {
  if (origin !== "viewport") {
    const bounds = elementCenter(origin);
    x += bounds.x;
    y += bounds.y;
  }
  assert_true(x >= 0 && x <= window.innerWidth,
              'x coordinate outside viewport');
  assert_true(y >= 0 && y <= window.innerHeight,
              'y coordinate outside viewport');
}

// Performs a drag operation from a starting point (x,y) which may be relative
// to the viewport or the center of an element as determined by the origin
// option, or "viewport" if missing. Verifies that both ends of the drag are
//  inside the viewport. The returned promise is resolved when a pointerup or
// pointercancel event is received. Pointercancel replaces pointerup when
// scrolling takes place. Thus, any scrolling decisions has been made prior to
// dispatching either of these events.
// Supported options:
//    origin: May be the string "viewport" (default) or an element.
//    eventTarget: Indicates the target for the pointerup or pointercancel
//                 event. Defaults to  document.
//    pointerType: 'mouse' (default), 'pen' or 'touch'
//    prevent_fling_pause_ms: How long to wait after the move to avoid a
//                            momentum fling. Default to 0ms.
//    adjust_for_touch_slop: Indicates if we should adjust to drag range to
//                           compensate for touch slop. At the start of a touch
//                           drag, we do not know if we are scrolling or not.
//                           Once scrolled past the slop region, a touch
//                           scroll will stick to the finger position.
//                           Defaults to false.
//    button: String with the button type, 'Left' (default), 'Middle' or 'Right'
function pointerDrag(x, y, deltaX, deltaY, options = {}) {
  const origin = options.origin || "viewport";
  const eventTarget = options.eventTarget || document;
  const pointerType = options.pointerType || 'mouse';
  const buttonType = options.button || Buttons.LEFT;
  const prevent_fling_pause_ms = options.prevent_fling_pause_ms || 0;
  if (options.adjust_for_touch_slop) {
    // TODO(kevers): This value may become platform specific, in which case
    // we may need to perform a test to measure the slop and then apply in
    // subsequent tests.
    const TOUCH_SLOP_AMOUNT = 15;
    if (deltaX) {
      deltaX += TOUCH_SLOP_AMOUNT * Math.sign(deltaX);
    }
    if (deltaY) {
      deltaY += TOUCH_SLOP_AMOUNT * Math.sign(deltaY);
    }
  }
  assert_point_within_viewport(x, y, origin);
  assert_point_within_viewport(x + deltaX, y + deltaY, origin);
  verifyTestDriverLoaded();
  // Expect a pointerup or pointercancel event depending on whether scrolling
  // actually took place.
  return new Promise(resolve => {
    const pointerPromise = new Promise(resolve => {
      const pointerListener = (event) => {
        eventTarget.removeEventListener('pointerup', pointerListener);
        eventTarget.removeEventListener('pointercancel', pointerListener);
        resolve(event.type);
      };
      eventTarget.addEventListener('pointerup', pointerListener);
      eventTarget.addEventListener('pointercancel', pointerListener);
    });
    const actionPromise = new test_driver.Actions()
      .addPointer("pointer1", pointerType)
      .pointerMove(x, y, { origin: origin })
      .pointerDown({button: pointerActionButtonId(buttonType)})
      .pointerMove(x + deltaX, y + deltaY, { origin: origin })
      .pause(prevent_fling_pause_ms)
      .pointerUp({button: pointerActionButtonId(buttonType)})
      .send();
    Promise.all([actionPromise, pointerPromise]).then(responses => {
      resolve(responses[1]);
    });
  });
}


// Performs a touch drag gesture. The prevent_fling_pause_ms options is used
// to prevent the drag from having fling momentum.
function touchDrag(x, y, deltaX, deltaY, options = {}) {
  options.pointerType = 'touch';
  if (options.prevent_fling_pause_ms === undefined) {
    options.prevent_fling_pause_ms = 100;
  }
  return pointerDrag(x, y, deltaX, deltaY, options);
}

// Performs a touch scroll operations.  The promise is resolved when the
// pointer cancel and scrollend events are received.
// The supported options are documented in pointerDrag.
function touchScroll(x, y, deltaX, deltaY, scroller, options = {}) {
  if (!options.eventTarget) {
    options.eventTarget = scroller.ownerDocument;
  }
  const scrollPromise =
      waitForScrollendEvent(scrollendEventTarget(scroller));
  const dragGesturePromise =
      touchDrag(x, y, deltaX, deltaY, options);
  return Promise.all([dragGesturePromise, scrollPromise]);
}

function mouseDrag(x, y, deltaX, deltaY, scroller, options = {}) {
  return pointerDrag(x, y, deltaX, deltaY, scroller, options);
}

function mouseDragScroll(x, y, deltaX, deltaY, scroller, options = {}) {
  if (!options.eventTarget) {
    options.eventTarget = scroller.ownerDocument;
  }
  const scrollPromise = waitForScrollendEvent(scroller);
  const dragPromise = mouseDrag(x, y, deltaX, deltaY, options);
  return Promise.all([scrollPromise, dragPromise]);
}

function wheelScroll(x, y, deltaX, deltaY, scrollEventListener = document,
                     origin = "viewport", duration_ms = 250) {
  verifyTestDriverLoaded();
  const promises = [];
  if (scrollEventListener) {
    promises.push(waitForScrollendEvent(scrollEventListener));
  }
  const gesturePromise = new test_driver.Actions()
        .scroll(x, y, deltaX, deltaY, origin, duration_ms)
        .send();
  promises.push(gesturePromise);
  return Promise.all(promises);
}

// Simulates a pointer tap gesutre.
// options:
//    origin: defaults to "viewport" coordinates.  If an element is specified,
//            then relative to the center of the element.
//    pointerType: Default to mouse, but may be mouse, touch, or pen.
//    pointerDownUpOptions: Optional callback function to set parameters for
//                          pointerDown and pointerUp.
function pointerTap(x, y, options = {}) {
  const origin = options.origin || "viewport";
  const pointerType = options.pointerType || 'mouse';
  const emptyCallback = () => {};
  const pointerDownUpOptions = options.pointerDownUpOptions || emptyCallback;
  verifyTestDriverLoaded();
  assert_point_within_viewport(x, y, origin);
  const promises = [];
  if (!options.skipWaitOnPointer) {
    const pointerPromise = new Promise((resolve) => {
      const listener = () => {
        document.removeEventListener('pointerup', listener);
        document.removeEventListener('pointercancel', listener);
        resolve();
      }
      document.addEventListener('pointerup', listener);
      document.addEventListener('pointercancel', listener);
    });
    promises.push(pointerPromise);
  }
  const actions = new test_driver.Actions();
  actions.addPointer("pointer1", pointerType)
         .pointerMove(x, y, { origin: origin })
         .pointerDown(pointerDownUpOptions(actions))
         .pointerUp(pointerDownUpOptions(actions));
  const gesturePromise = actions.send();
  promises.push(gesturePromise);
  return Promise.all(promises);
}

// Performs a mouse click using the left-mouse button by default. The selected
// button may be set via options.buttons.
function mouseClick(x, y, options = {}) {
  options.pointerType = 'mouse';
  if (!options.pointerDownUpOptions) {
    options.pointerDownUpOptions = (actions) => {
      return { button: actions.ButtonType.LEFT };
    };
  }
  return pointerTap(x, y, options);
}

// Performs a touch tap actions.
function touchTap(x, y, options = {}) {
  options.pointerType = 'touch';
  return pointerTap(x, y, options);
}

// Long press on the target element. The options are of the form:
// {
//    x: horizontal offset from midpoint of the element (default 0)
//    y: vertical offset from the midpoint of the element (default 0)
//    duration: duration of the press in milliseconds (default 400)
// }
//
// Be sure to call preventContextMenu during test setup to avoid a memory leak
// before calling this method. If event handling is permitted to transfer to the
// browser process, we are unable to fully tear down the test resulting in a
// leak.
function touchLongPressElement(target, options) {
  // Conservative long-press duration based on timing for a context menu popup.
  // Some long-press operations require longer.
  const LONG_PRESS_DURATION = 400;
  const x = (options && options.x)? options.x : 0;
  const y = (options && options.y)? options.y : 0;
  const duration = (options && options.duration !== undefined)
                       ? options.duration
                       : LONG_PRESS_DURATION;
  verifyTestDriverLoaded();
  const pointerPromise = new Promise((resolve) => {
    const listener = () => {
      document.removeEventListener('pointerup', listener);
      document.removeEventListener('pointercancel', listener);
      resolve();
    }
    document.addEventListener('pointerup', listener);
    document.addEventListener('pointercancel', listener);
  });
  const actionPromise = new test_driver.Actions()
      .addPointer('pointer1', 'touch')
      .pointerMove(x, y, {origin: target})
      .pointerDown()
      .pause(duration)
      .pointerUp()
      .send();
  return actionPromise.then(pointerPromise);
}

function preventContextMenu(test) {
  const listener = (event) => {
    event.preventDefault();
  }
  document.addEventListener('contextmenu', listener);
  test.add_cleanup(() => {
    document.removeEventListener('contextmenu', listener);
  });
}

// Perform a click action where a scroll is expected such as on a scrollbar
// arrow or on a scrollbar track. The promise will timeout if no scrolling is
// triggered.
function clickScroll(x, y, scroller, options = {}) {
  const scrollPromise = waitForScrollendEvent(scroller);
  const clickPromise = mouseClick(x, y, options);
  return Promise.all([scrollPromise, clickPromise]);
}

// Perform a tap action where a scroll is expected such as on a scrollbar
// arrow or on a scrollbar track. The promise will timeout if no scrolling is
// triggered.
function touchTapScroll(x, y, scroller, options = {}) {
  verifyTestDriverLoaded();
  const scrollPromise = waitForScrollendEvent(scroller);
  // Not seeing pointerup or pointercancel when synthetically tapping on a
  // scrollbar button. A pointerup event is observed when testing manually
  // suggesting this might be an issue in test driver. We can safely skip the
  // check for touch tap scrolls since it is safe to continue the test even if
  // we have not had a chance to process a pointerup event.
  options.skipWaitOnPointer = true;
  const tapPromise = touchTap(x, y, options);
  return Promise.all([scrollPromise, tapPromise]);
}

function waitForStableScrollOffset(scroller, timeout) {
  timeout = timeout || 5000;
  return new Promise((resolve, reject) => {
    let last_x = scroller.scrollLeft;
    let last_y = scroller.scrollTop;
    let start_timestamp = performance.now();
    let last_change_timestamp = start_timestamp;
    let last_change_frame_number = 0;
    function tick(frame_number, timestamp) {
      // We run a rAF loop until 100 milliseconds and at least five animation
      // frames have elapsed since the last scroll offset change, with a timeout
      // after `timeout` milliseconds.
      if (scroller.scrollLeft != last_x || scroller.scrollTop != last_y) {
        last_change_timestamp = timestamp;
        last_x = scroller.scrollLeft;
        last_y = scroller.scrollTop;
      }
      if (timestamp - last_change_timestamp > 100 &&
          frame_number - last_change_frame_number > 4) {
        resolve();
      } else if (timestamp - start_timestamp > timeout) {
        reject();
      } else {
        requestAnimationFrame(tick.bind(null, frame_number + 1));
      }
    }
    tick(0, start_timestamp);
  });
}

function keyPress(key) {
  return new Promise((resolve, reject) => {
    if (window.eventSender) {
      eventSender.keyDown(key);
      resolve();
    }
    else {
      reject('This test requires window.eventSender');
    }
  })
}

function keyboardScroll(key, scroller) {
  const scrollPromise = waitForScrollendEvent(scroller);
  return Promise.all([ keyPress(key), scrollPromise ]);
}

/**
 * Trigger a gesture that results in a scroll and wait for scroll
 * completion. Where possible, use a specialized test-driver compatible
 * method. This is a catch all for cases not explicitly addressed by
 * a specialized method.
 */
function gestureScroll(gesturePromiseCallback, scroller) {
  const scrollPromise =
      waitForScrollendEvent(scrollendEventTarget(scroller));
  return Promise.all([ gesturePromiseCallback(), scrollPromise ]);
}
