// Contains helpers for calculating the dimensions for the various
// scrollbar parts.

// Should be the same value as `kFluentScrollbarThickness` in
// ui\native_theme\native_theme_constants_fluent.h
// Used to provide overlay scrollbars track width, as the current calculation
// method would return 0.
const FLUENT_TRACK_WIDTH = 15;
function fluentOverlayScrollbarsEnabled() {
  return internals.runtimeFlags.fluentOverlayScrollbarsEnabled;
}

// Helper to calculate track-width for non-custom standard
// scrollbars.
function calculateScrollbarThickness() {
    if (fluentOverlayScrollbarsEnabled()) {
      return FLUENT_TRACK_WIDTH;
    }

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

// Returns the width of a acrollbar button. On platforms where there are no
// scrollbar buttons (i.e. there are overlay scrollbars) returns 0.
function calculateScrollbarButtonWidth() {
  if (fluentOverlayScrollbarsEnabled()) {
    // Fluent overlay scrollbars have a little margin over the scrollbar's
    // button that causes the button to be separated from the edges of the
    // screen.
    return calculateScrollbarThickness() + 5;
  }
  if (!hasScrollbarArrows()) {
    return 0;
  }
  return calculateScrollbarThickness();
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
    return internals.runtimeFlags.fractionalScrollOffsetsEnabled ? {
      x: SCROLLBAR_SCROLL_PERCENTAGE * size.x,
      y: SCROLLBAR_SCROLL_PERCENTAGE * size.y
    } : {
      x: Math.round(SCROLLBAR_SCROLL_PERCENTAGE * size.x),
      y: Math.round(SCROLLBAR_SCROLL_PERCENTAGE * size.y)
    }
  };

  clamp = (x, min, max) => Math.min(Math.max(x, min), max)

  scroller_size = {x: scroller.clientWidth, y: scroller.clientHeight};

  // All percent-based scroll clamping is made in physical pixels.
  pixel_delta = percentBasedDelta(scaleCssToPhysicalPixels(scroller_size));

  // Note that, window.inner* matches the size of the innerViewport, and won't
  // match the VisualViewport's dimensions at the C++ code in the presence of
  // UI elements that resize it (e.g. chromeOS OSKs).
  // Note also that window.inner* isn't affected by pinch-zoom, so converting
  // to Blink pixels is enough to get its actual size in Physical pixels.
  max_delta = percentBasedDelta(scaleCssToBlinkPixels({
    x: window.innerWidth, y: window.innerHeight}));

  pixel_delta.x = Math.min(pixel_delta.x, max_delta.x);
  pixel_delta.y = Math.min(pixel_delta.y, max_delta.y);

  return scalePhysicalToCssPixels(pixel_delta);
}

// The percentage scrollbar arrows will scroll, if percent-based scrolling
// is enabled.
const SCROLLBAR_SCROLL_PERCENTAGE = 0.125;

// The number of pixels scrollbar arrows will scroll when percent-based
// scrolling is not enabled.
const SCROLLBAR_SCROLL_PIXELS = 40;

function hasScrollbarArrows() {
  // Mac scrollbars do not have arrow keys.
  if (navigator.platform.toUpperCase().indexOf('MAC') >= 0) {
    return false;
  }

  if (internals.overlayScrollbarsEnabled) {
    return false;
  }

  return true;
}

// Exports 2 functions:
//   placeRTLScrollbarsOnLeftSideInMainFrame
//   isVerticalScrollbarOnLeft
//
// These methods are grouped together since, for a document scrolling element,
// we need to be aware of whether the positioning has been altered.
((exports) => {
  // By default, the main frame scrollbar is on the right regardless of whether
  // the document direction is RTL.
  let main_frame_rtl_scrollbar_on_left = false;

  // Set RTL scrollbar layout via this method rather than directly though
  // internals.settings so that button positioning methods are aware of the
  // configuration.
  exports.placeRTLScrollbarsOnLeftSideInMainFrame = state => {
    main_frame_rtl_scrollbar_on_left = state;
    window.internals.settings.setPlaceRTLScrollbarsOnLeftSideInMainFrame(state);
  };

  // Returns if the vertical scrollbar is on the left. For scrollable containers
  // with direction:rtl, the vertical scrollbar is on the left unless the main
  // frame's scrollbar, in which case it is on the right by default, but
  // the position can be overridden via settings.
  exports.isVerticalScrollbarOnLeft = (scroller) => {
    if (getComputedStyle(scroller).direction != "rtl") {
      return false;
    }
    if (scroller == document.scrollingElement) {
      return main_frame_rtl_scrollbar_on_left;
    }
    return true;
  };
})(window);

function isMainFrameScroller(scroller) {
  return scroller == scroller.ownerDocument.scrollingElement;
}

// TODO(arakeri): Add helpers for arrow widths.
/*
  Getters for the center point in a scroller's scrollbar buttons (CSS visual
  coordinates). An empty argument requests the point for the main frame's
  scrollbars.
  TODO(kevers): These methods assume that both horizontal and vertical
  scrollbars are displayed. Consider updating to correct for the case of a
  single scrollbar being shown, in which case we need to adjust for the scroll
  corner being conditional.
*/
function downArrow(scroller = document.scrollingElement) {
  assert_true(hasScrollbarArrows());
  const TRACK_WIDTH = calculateScrollbarThickness();
  const BUTTON_WIDTH = TRACK_WIDTH;
  const SCROLL_CORNER = TRACK_WIDTH;
  if (isMainFrameScroller(scroller)) {
    // The main frame's scrollbars don't scale with pinch zoom so there's no
    // need to convert from client to visual.
    return {
      x: isVerticalScrollbarOnLeft(document.scrollingElement)
              ? BUTTON_WIDTH / 2
              : window.innerWidth - BUTTON_WIDTH / 2,
      y: window.innerHeight - SCROLL_CORNER - BUTTON_WIDTH / 2
    };
  }

  const scrollerRect = scroller.getBoundingClientRect();
  return cssClientToCssVisual({
    x: isVerticalScrollbarOnLeft(scroller)
           ? scrollerRect.left + BUTTON_WIDTH / 2
           : scrollerRect.right - BUTTON_WIDTH / 2,
    y: scrollerRect.bottom - SCROLL_CORNER - BUTTON_WIDTH / 2
  });
}

function upArrow(scroller = document.scrollingElement) {
  assert_true(hasScrollbarArrows());
  const TRACK_WIDTH = calculateScrollbarThickness();
  const BUTTON_WIDTH = TRACK_WIDTH;
  if (isMainFrameScroller(scroller)) {
    // The main frame's scrollbars don't scale with pinch zoom so there's no
    // need to convert from client to visual.
    return {
      x: isVerticalScrollbarOnLeft(document.scrollingElement)
             ? BUTTON_WIDTH / 2
             : window.innerWidth - BUTTON_WIDTH / 2,
      y: BUTTON_WIDTH / 2
    };
  }

  const scrollerRect = scroller.getBoundingClientRect();
  return cssClientToCssVisual({
    x: isVerticalScrollbarOnLeft(scroller)
           ? scrollerRect.left + BUTTON_WIDTH / 2
           : scrollerRect.right - BUTTON_WIDTH / 2,
    y: scrollerRect.top + BUTTON_WIDTH / 2
  });
}

function leftArrow(scroller = document.scrollingElement) {
  assert_true(hasScrollbarArrows());
  const TRACK_WIDTH = calculateScrollbarThickness();
  const BUTTON_WIDTH = TRACK_WIDTH;
  const SCROLL_CORNER = TRACK_WIDTH;
  if (isMainFrameScroller(scroller)) {
    // The main frame's scrollbars don't scale with pinch zoom so there's no
    // need to convert from client to visual.
    return {
      x: isVerticalScrollbarOnLeft(scroller)
             ? SCROLL_CORNER + BUTTON_WIDTH / 2
             : BUTTON_WIDTH / 2,
      y: window.innerHeight - BUTTON_WIDTH / 2
    };
  }

  const scrollerRect = scroller.getBoundingClientRect();
  const left_arrow = {
    x: isVerticalScrollbarOnLeft(scroller)
           ? scrollerRect.left + SCROLL_CORNER + BUTTON_WIDTH / 2
           : scrollerRect.left + BUTTON_WIDTH / 2,
    y: scrollerRect.bottom  - BUTTON_WIDTH / 2
  };
  return cssClientToCssVisual(left_arrow);
}

function rightArrow(scroller = document.scrollingElement) {
  assert_true(hasScrollbarArrows());
  const TRACK_WIDTH = calculateScrollbarThickness();
  const BUTTON_WIDTH = TRACK_WIDTH;
  const SCROLL_CORNER = TRACK_WIDTH;
  if (isMainFrameScroller(scroller)) {
    // The main frame's scrollbars don't scale with pinch zoom so there's no
    // need to convert from client to visual.
    return {
      x: isVerticalScrollbarOnLeft(document.scrollingElement)
             ? window.innerWidth - BUTTON_WIDTH / 2
             : window.innerWidth - SCROLL_CORNER - BUTTON_WIDTH / 2,
      y: window.innerHeight - BUTTON_WIDTH / 2
    }
  }

  const scrollerRect = scroller.getBoundingClientRect();
  return cssClientToCssVisual({
    x: isVerticalScrollbarOnLeft(scroller)
           ? scrollerRect.right -  BUTTON_WIDTH / 2
           : scrollerRect.right - SCROLL_CORNER - BUTTON_WIDTH / 2,
    y: scrollerRect.bottom  - BUTTON_WIDTH / 2
  });
}

((exports) => {
  let scrollbarThickness_;

  exports.scrollbarThickness = () => {
    if (scrollbarThickness_ === undefined) {
      scrollbarThickness_ = calculateScrollbarThickness();
    }
    return scrollbarThickness_;
  };

  // The following methods assume both the horizontal and vertical scrollbars
  // are visible.
  // TODO: Consider updating the methods to relax these assumptions.

  function verticalScrollbarBounds(scroller) {
    scroller = scroller || document.scrollingElement;
    const width = scrollbarThickness();
    const scrollCorner = width;
    if (isMainFrameScroller(scroller)) {
      return {
        'x': isVerticalScrollbarOnLeft(document.scrollingElement)
               ? 0 : window.innerWidth - width,
        'y': 0,
        'width': width,
        'height': window.innerHeight - scrollCorner
      };
    }

    const scrollerRect = scroller.getBoundingClientRect();
    return {
      'x': isVerticalScrollbarOnLeft(scroller)
             ? scrollerRect.left
             : scrollerRect.right - scrollCorner,
      'y': scrollerRect.top,
      'width': width,
      'height': scrollerRect.height - scrollCorner
    };
  }

  function horizontalScrollbarBounds(scroller) {
    scroller = scroller || document.scrollingElement;
    const height = scrollbarThickness();
    const scrollCorner = height;
    if (isMainFrameScroller(scroller)) {
      return {
        x: isVerticalScrollbarOnLeft(scroller)
               ? scrollCorner : 0,
        y: window.innerHeight - scrollCorner,
        width: window.innerWidth - scrollCorner,
        height: height
      };
    }

    const scrollerRect = scroller.getBoundingClientRect();
    return {
      x: isVerticalScrollbarOnLeft(scroller)
             ? scrollerRect.left + scrollCorner
             : scrollerRect.left,
      y: scrollerRect.bottom - height,
      width: scrollerRect.width - scrollCorner,
      height: height
    };
  }

  function position(x, y, scroller) {
    if (isMainFrameScroller(scroller)) {
      return { x: x, y: y };
    }
    return cssClientToCssVisual({ x: x, y: y });
  }

  exports.trackTop = (scroller) => {
    const bounds = verticalScrollbarBounds(scroller);
    const offset = scrollbarThickness() / 2;
    let x = bounds.x + offset;
    let y = bounds.y + offset;
    if (hasScrollbarArrows()) {
      y += scrollbarThickness();
    }
    return position(x, y, scroller);
  };

  exports.trackBottom = (scroller) => {
    const bounds = verticalScrollbarBounds(scroller);
    const offset = scrollbarThickness() / 2;
    let x = bounds.x + offset;
    let y = bounds.y + bounds.height - offset;
    if (hasScrollbarArrows()) {
      y -= scrollbarThickness();
    }
    return position(x, y, scroller);
  };

 exports.trackLeft = (scroller) => {
    const bounds = horizontalScrollbarBounds(scroller);
    const offset = scrollbarThickness() / 2;
    let x = bounds.x + offset;
    let y = bounds.y + offset;
    if (hasScrollbarArrows()) {
      x += scrollbarThickness();
    }
    return position(x, y, scroller);
  };

  exports.trackRight = (scroller) => {
    const bounds = horizontalScrollbarBounds(scroller);
    const offset = scrollbarThickness() / 2;
    let x = bounds.x + bounds.width - offset;
    let y = bounds.y + offset;
    if (hasScrollbarArrows()) {
      x -= scrollbarThickness();
    }
    return position(x, y, scroller);
  };
})(window);

// Returns a point that falls within the given scroller's vertical thumb part.
function verticalThumb(scroller = document.scrollingElement) {
  assert_equals(scroller.scrollTop, 0,
                "verticalThumb() requires scroller to have scrollTop of 0");
  const TRACK_WIDTH = calculateScrollbarThickness();
  const BUTTON_WIDTH = calculateScrollbarButtonWidth();

  if (isMainFrameScroller(scroller)) {
    // HTML element is special, since scrollbars are not part of its client rect
    // and page scale doesn't affect the scrollbars. Use window properties
    // instead.
    return {
      x: isVerticalScrollbarOnLeft(document.scrollingElement)
             ? TRACK_WIDTH / 2
             : window.innerWidth - TRACK_WIDTH / 2,
      y: BUTTON_WIDTH + 6
    };
  }
  const scrollerRect = scroller.getBoundingClientRect();
  return cssClientToCssVisual({
    x: isVerticalScrollbarOnLeft(scroller)
           ? scrollerRect.left + TRACK_WIDTH / 2
           : scrollerRect.right - TRACK_WIDTH / 2,
    y: scrollerRect.top + BUTTON_WIDTH + 2
  });
}

// Returns a point that falls within the given scroller's horizontal thumb part.
function horizontalThumb(scroller = document.scrollingElement) {
  assert_equals(scroller.scrollLeft, 0,
                "horizontalThumb() requires scroller to have scrollLeft of 0");
  const TRACK_WIDTH = calculateScrollbarThickness();
  const BUTTON_WIDTH = calculateScrollbarButtonWidth();
  if (isMainFrameScroller(scroller)) {
    // HTML element is special, since scrollbars are not part of its client rect
    // and page scale doesn't affect the scrollbars. Use window properties
    // instead.
    return {
      x: isVerticalScrollbarOnLeft(document.scrollingElement)
             ? window.innerWidth - BUTTON_WIDTH - 6
             : BUTTON_WIDTH + 6,
      y: window.innerHeight - TRACK_WIDTH / 2
    };
  }
  const scrollerRect = scroller.getBoundingClientRect();
  return cssClientToCssVisual({
    x: isVerticalScrollbarOnLeft(scroller)
           ? scrollerRect.right - BUTTON_WIDTH - 2
           : scrollerRect.left + BUTTON_WIDTH + 2,
    y: scrollerRect.bottom - TRACK_WIDTH / 2
  });
}

// Determines the scroll amount based on a thumb drag amount.
// The scroll amount is dependent on the thumb length, which in turn has a
// theme dependent minimum size (see ScrollbarTheme::ThumbLength).
function thumbDragScrollAmount(dx, dy, scroller = document.scrollingElement) {
  const TRACK_WIDTH = calculateScrollbarThickness();
  const BUTTON_WIDTH = calculateScrollbarButtonWidth();
  const MINIMUM_LENGTH_THRESHOLD = 2 * BUTTON_WIDTH;
  const thumbDrag = (drag, visibleSize, totalSize, scrollOffset) => {
    // On the Mac BUTTON_WIDTH is zero since scrollbar buttons are not
    // displayed.
    const proportion = visibleSize / totalSize;
    const trackLength = visibleSize - 2 * BUTTON_WIDTH;
    const thumbLength = Math.round(proportion * trackLength);
    if (thumbLength < MINIMUM_LENGTH_THRESHOLD) {
      throw new Error("Thumb length is likely inaccurate due to a theme " +
                      "dependent minimum length. Suggest reducing the " +
                      "scroll range.");
    }
    const residual = trackLength - thumbLength;
    const scale = (totalSize - visibleSize) / residual;
    return Math.round(drag * scale);
  }
  return {
    dx: thumbDrag(dx, scroller.clientWidth, scroller.scrollWidth,
                  scroller.scrollLeft),
    dy: thumbDrag(dy, scroller.clientHeight, scroller.scrollHeight,
                  scroller.scrollTop)
  }
}
