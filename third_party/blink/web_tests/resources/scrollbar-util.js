// Contains helpers for calculating the dimensions for the various
// scrollbar parts.

// Helper to calculate track-width for non-custom standard
// scrollbars.
function calculateScrollbarThickness() {
    var container = document.createElement("div");
    container.style.width = "100px";
    container.style.height = "100px";
    container.style.position = "absolute";
    container.style.visibility = "hidden";
    container.style.overflow = "auto";

    document.body.appendChild(container);

    var widthBefore = container.clientWidth;
    var longContent = document.createElement("div");
    longContent.style.height = "1000px";
    container.appendChild(longContent);

    var widthAfter = container.clientWidth;

    container.remove();

    return widthBefore - widthAfter;
}

// Resets scroll offsets (only supports LTR for now).
function resetScrollOffset(scrollElement) {
  if(scrollElement !== undefined) {
    if(scrollElement.scrollLeft !== undefined) {
      scrollElement.scrollLeft = 0;
    }
    if(scrollElement.scrollTop !== undefined) {
      scrollElement.scrollTop = 0;
    }
  }
}

// Returns the expected CSS pixels delta of a percent-based scroll of a
// |scroller| element.
function getScrollbarButtonScrollDelta(scroller) {
  if (!internals.runtimeFlags.percentBasedScrollingEnabled) {
    return { x: SCROLLBAR_SCROLL_PIXELS, y: SCROLLBAR_SCROLL_PIXELS };
  }

  percentBasedDelta = (size) => {
    return {
      x: Math.floor(SCROLLBAR_SCROLL_PERCENTAGE * size.x),
      y: Math.floor(SCROLLBAR_SCROLL_PERCENTAGE * size.y)
    }
  };

  clamp = (x, min, max) => Math.min(Math.max(x, min), max)

  scroller_size = {x: scroller.clientWidth, y: scroller.clientHeight};

  // All percent-based scroll clamping is made in physical pixels.
  pixel_delta = percentBasedDelta(scaleCssToPhysicalPixels(scroller_size));
  min_delta = MIN_SCROLL_DELTA_PCT_BASED;
  // Note that, window.inner* matches the size of the innerViewport, and won't
  // match the VisualViewport's dimensions at the C++ code in the presence of
  // UI elements that resize it (e.g. chromeOS OSKs).
  // Note also that window.inner* isn't affected by pinch-zoom, so converting
  // to Blink pixels is enough to get its actual size in Physical pixels.
  max_delta = percentBasedDelta(scaleCssToBlinkPixels({
    x: window.innerWidth, y: window.innerHeight}));

  pixel_delta.x = clamp(pixel_delta.x, min_delta, max_delta.x);
  pixel_delta.y = clamp(pixel_delta.y, min_delta, max_delta.y);

  return scalePhysicalToCssPixels(pixel_delta);
}

// The minimum amount of pixels scrolled if percent-based scrolling is enabled
const MIN_SCROLL_DELTA_PCT_BASED = 16;

// The percentage scrollbar arrows will scroll, if percent-based scrolling
// is enabled.
const SCROLLBAR_SCROLL_PERCENTAGE = 0.125;

// The number of pixels scrollbar arrows will scroll when percent-based
// scrolling is not enabled.
const SCROLLBAR_SCROLL_PIXELS = 40;

// TODO(arakeri): Add helpers for arrow widths.

/*
  Getters for the center point in a scroller's scrollbar buttons (CSS visual
  coordinates). An empty argument requests the point for the main frame's
  scrollbars.
*/
function downArrow(scroller) {
  const TRACK_WIDTH = calculateScrollbarThickness();
  const BUTTON_WIDTH = TRACK_WIDTH;
  const SCROLL_CORNER = TRACK_WIDTH;
  if (typeof(scroller) == 'undefined') {
    // The main frame's scrollbars don't scale with pinch zoom so there's no
    // need to convert from client to visual.
    return { x: window.innerWidth - BUTTON_WIDTH / 2,
             y: window.innerHeight - SCROLL_CORNER - BUTTON_WIDTH / 2 }
  }

  const scrollerRect = scroller.getBoundingClientRect();
  const down_arrow = {
    x: scrollerRect.right - BUTTON_WIDTH / 2,
    y: scrollerRect.bottom - SCROLL_CORNER - BUTTON_WIDTH / 2
  };
  return cssClientToCssVisual(down_arrow);
}

function upArrow(scroller) {
  const TRACK_WIDTH = calculateScrollbarThickness();
  const BUTTON_WIDTH = TRACK_WIDTH;
  if (typeof(scroller) == 'undefined') {
    // The main frame's scrollbars don't scale with pinch zoom so there's no
    // need to convert from client to visual.
    return { x: window.innerWidth - BUTTON_WIDTH / 2,
             y: BUTTON_WIDTH / 2 }
  }

  const scrollerRect = scroller.getBoundingClientRect();
  const up_arrow = {
    x: scrollerRect.right - BUTTON_WIDTH / 2,
    y: scrollerRect.top + BUTTON_WIDTH / 2
  };
  return cssClientToCssVisual(up_arrow);
}

function leftArrow(scroller) {
  const TRACK_WIDTH = calculateScrollbarThickness();
  const BUTTON_WIDTH = TRACK_WIDTH;
  if (typeof(scroller) == 'undefined') {
    // The main frame's scrollbars don't scale with pinch zoom so there's no
    // need to convert from client to visual.
    return { x: BUTTON_WIDTH / 2,
             y: window.innerHeight - BUTTON_WIDTH / 2 }
  }

  const scrollerRect = scroller.getBoundingClientRect();
  const left_arrow = {
    x: scrollerRect.left + BUTTON_WIDTH / 2,
    y: scrollerRect.bottom  - BUTTON_WIDTH / 2
  };
  return cssClientToCssVisual(left_arrow);
}

function rightArrow(scroller) {
  const TRACK_WIDTH = calculateScrollbarThickness();
  const BUTTON_WIDTH = TRACK_WIDTH;
  const SCROLL_CORNER = TRACK_WIDTH;
  if (typeof(scroller) == 'undefined') {
    // The main frame's scrollbars don't scale with pinch zoom so there's no
    // need to convert from client to visual.
    return { x: window.innerWidth - SCROLL_CORNER - BUTTON_WIDTH / 2,
             y: window.innerHeight - BUTTON_WIDTH / 2 }
  }

  const scrollerRect = scroller.getBoundingClientRect();
  const right_arrow = {
    x: scrollerRect.right - SCROLL_CORNER - BUTTON_WIDTH / 2,
    y: scrollerRect.bottom  - BUTTON_WIDTH / 2
  };
  return cssClientToCssVisual(right_arrow);
}
