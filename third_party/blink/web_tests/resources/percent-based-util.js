/*
  Methods for percent-based scrolling webtests
*/

// The scroll percentage per mousewheel tick. Used to determine scroll delta
// if percent based scrolling is enabled. See kScrollPercentPerLineOrChar.
const SCROLL_PERCENT_PER_LINE_OR_CHAR = 0.05;

// Returns true if the feature flag is on and the platform is supported
function isPercentBasedScrollingEnabled() {
  return (
    internals.runtimeFlags.percentBasedScrollingEnabled
    && (
      navigator.userAgent.includes("Linux")
      || navigator.userAgent.includes("Windows")
    )
  );
}

// Calculates expected scrollX/scrollY for a given container or window and
// amount pixels to scroll for x/y axes when the percent-based scrolling
// feature is enabled.
// Returns unchanged pixelsToScrollX/pixelsToScrollY if the feature is disabled.
function calculateExpectedScroll(container, pixelsToScrollX, pixelsToScrollY) {
  if (!isPercentBasedScrollingEnabled()) {
    return {x: pixelsToScrollX, y: pixelsToScrollY};
  }

  let expectedScrollX = (
    pixelsToScrollX * SCROLL_PERCENT_PER_LINE_OR_CHAR / pixelsPerTick()
  );
  let expectedScrollY = (
    pixelsToScrollY * SCROLL_PERCENT_PER_LINE_OR_CHAR / pixelsPerTick()
  );

  if (container === document.scrollingElement) {
    expectedScrollX *= window.visualViewport.width;
    expectedScrollY *= window.visualViewport.height;
  } else {
    expectedScrollX *= Math.min(
      container.clientWidth, window.visualViewport.width
    );
    expectedScrollY *= Math.min(
      container.clientHeight, window.visualViewport.height
    );
  }

  return {x: Math.round(expectedScrollX), y: Math.round(expectedScrollY)};
}

// This is an inverse of the above function.
// Calculates amount of pixels to scroll for x/y axes when the percent-based
// scrolling feature is enabled to match expected scrollX/scrollY for a given
// container or window.
// Returns unchanged expectedScrollX/expectedScrollY if the feature is disabled.
function calculatePixelsToScroll(container, expectedScrollX, expectedScrollY) {
  if (!isPercentBasedScrollingEnabled()) {
    return {x: expectedScrollX, y: expectedScrollY};
  }

  let pixelsToScrollX = (
    expectedScrollX * pixelsPerTick() / SCROLL_PERCENT_PER_LINE_OR_CHAR
  );
  let pixelsToScrollY = (
    expectedScrollY * pixelsPerTick() / SCROLL_PERCENT_PER_LINE_OR_CHAR
  );

  if (container === document.scrollingElement) {
    pixelsToScrollX /= window.visualViewport.width;
    pixelsToScrollY /= window.visualViewport.height;
  } else {
    pixelsToScrollX /= Math.min(
      container.clientWidth, window.visualViewport.width
    );
    pixelsToScrollY /= Math.min(
      container.clientHeight, window.visualViewport.height
    );
  }

  return {x: Math.floor(pixelsToScrollX), y: Math.floor(pixelsToScrollY)};
}

// Scrollbar Arrows Tests

// Tests if scrolling the |scroller| element through its scrollbar arrows
// cause |expected_x| and |expected_y| scroll deltas (in CSS pixels) both in
// positive and negative directions.
async function runScrollbarArrowsTest(scroller, expected_x, expected_y) {
  // use tolerance for comparisons due to floating point approximations.
  const tolerance = 1;
  await waitForCompositorReady();

  /* Step 1 - Click down button */
  scroller.scrollTop = 0;
  await clickAndWaitForAnimationEnd(downArrow(scroller),
    () => {return scroller.scrollTop}, expected_y, tolerance,
    "Click scroll down");

  /* Step 2 - Click right button */
  scroller.scrollLeft = 0;
  await clickAndWaitForAnimationEnd(rightArrow(scroller),
    () => {return scroller.scrollLeft}, expected_x, tolerance,
    "Click scroll right");

  // Creates an offset > EXPECTED_SCROLL so a negative delta bigger than
  // EXPECTED_SCROLL can be detected
  scroller.scrollTo(2*expected_x, 2*expected_y);

  /* Step 3 - Click up button */
  await clickAndWaitForAnimationEnd(upArrow(scroller),
    () => {return scroller.scrollTop}, expected_y, tolerance,
    "Click scroll up");;

  /* Step 4 - Click left button */
  await clickAndWaitForAnimationEnd(leftArrow(scroller),
    () => {return scroller.scrollLeft}, expected_x, tolerance,
    "Click scroll left");
}

// Mousewheel Tests

// Tests if scrolling the |scroller| element performing a |percentage| units
// mousewheel scroll reaches |expected_x| and |expected_y| scroll deltas (in CSS
// pixels) both in positive and negative directions.
async function runMousewheelTest(scroller, expected_x, expected_y, percentage) {
  // Return page to initial conditions.
  await reset(scroller);

  // use tolerance for comparisons due to floating point approximations.
  const tolerance = 1;

  /* Step 1 - Runs vertical and horizontal scrolls at the same time in
  positive directions */
  await runPercentScrollAndWaitForAnimationEnd(scroller, percentage,
    percentage, expected_x, expected_y, tolerance);

  // Creates an offset > EXPECTED_SCROLL so a negative delta bigger than
  // EXPECTED_SCROLL can be detected
  scroller.scrollTo(2 * expected_x, 2 * expected_y);

  /* Step 2 - Runs vertical and horizontal scrolls at the same time in
  negative directions */
  await runPercentScrollAndWaitForAnimationEnd(scroller, -percentage,
    -percentage, expected_x, expected_y, tolerance);
}


// Helpers for Scrollbar Arrows tests
/*
  Methods for user interactions in test.
*/
// Method to click in a |click_pos| position (CSS visual coordinates) and wait
// until a |waitForEnd| function stops changing its value and is equals to
// |expected_at_end|. This method is used for clicking in the scrollbar and
// waiting for the scroll animation to finish.
async function clickAndWaitForAnimationEnd(click_pos, waitForEnd,
  expected_at_end, tolerance, on_failure) {
  let pos = await moveViewportToVisible(click_pos);

  pos = scaleCssToDIPixels(pos)
  await mouseClickOn(pos.x, pos.y);
  await waitForAnimationEndTimeBased(waitForEnd);
  assert_approx_equals(waitForEnd(), expected_at_end, tolerance, on_failure);
}

// Moves the visual viewport so |point| is visible. Distributes the scroll
// through the visual and layout viewports.
function moveViewportToVisible(point) {
  let shrink = (rect, pixels) => {
    return {top: rect.top + pixels, bottom: rect.bottom - pixels,
      left: rect.left + pixels, right: rect.right - pixels};
  }

  point = cssVisualToCssPage(point);

  // Shrinks the visible viewport to ensure |point| will be visible after moving.
  let visual_rect = shrink(getVisualViewportRect(), 10);
  let visual_delta = offsetFromBounds(point, visual_rect);
  let visual_target = cssVisualToCssClient(visual_delta)
  internals.setVisualViewportOffset(visual_target.x, visual_target.y);

  // Note that layout viewport = client
  let layout_rect = shrink(getLayoutViewportRect(), 10);
  let layout_delta = offsetFromBounds(point, layout_rect);
  let layout_target = cssClientToCssPage(layout_delta);
  window.scroll(layout_target.x, layout_target.y);

  return cssPageToCssVisual(point);
}

// Helpers for Mousewheel tests
/*
  Methods for user interactions in test.
*/
// Resets the document and viewport to the origin.
async function reset(scroller) {
  window.scroll(0, 0);
  internals.setVisualViewportOffset(0, 0);
  await waitForScrollReset(scroller);
}

// Performs a mousewheel percent-scroll of |scroll_x| and |scroll_y| units (in
// percentage) of the |scroller| element and waits until the scrolling animation
// ends. Checks if the scroll deltas reach |expected_x| and |expected_y|.
async function runPercentScrollAndWaitForAnimationEnd(scroller, scroll_x, scroll_y,
  expected_x, expected_y, tolerance) {
  function isNear(actual, expected, tolerance) {
    return Math.abs(actual - expected) <= tolerance;
  }

  isCorrectOffset = () => {
    return (
      isNear(scroller.scrollLeft, expected_x, tolerance) &&
      isNear(scroller.scrollTop, expected_y, tolerance));
  }

  await waitForCompositorReady();
  const rect = scroller.getBoundingClientRect();
  assert_greater_than(rect.width, 20, "Test requires a scroller bigger than 20x20");
  assert_greater_than(rect.height, 20, "Test requires a scroller bigger than 20x20");
  const mouse_pos = clientToViewport({x: rect.left + 20, y: rect.top + 20});
  await percentScroll(scroll_x, scroll_y, mouse_pos.x, mouse_pos.y,
    GestureSourceType.MOUSE_INPUT);

  await waitFor(isCorrectOffset);
  await conditionHolds(isCorrectOffset);

  assert_approx_equals(scroller.scrollLeft, expected_x, tolerance, "Horizontal scroll");
  assert_approx_equals(scroller.scrollTop, expected_y, tolerance, "Vetical scroll");
}
