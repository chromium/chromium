/*
  Methods for working with the blink coordinate spaces.

  For the blink definitions of coordinate spaces and pixel scales:
  https://www.chromium.org/developers/design-documents/blink-coordinate-spaces

  For the CSS definitions:
  https://rbyers.github.io/inputCoords.html
*/

/*
  Conversion methods - CSS (WEB APIs) coordinates
*/
function cssPageToCssVisual(point) {
  const origin = {x: visualViewport.pageLeft, y: visualViewport.pageTop};
  return {x: point.x - origin.x, y: point.y - origin.y};
}

function cssClientToCssPage(point) {
  const origin = {x: window.pageXOffset, y: window.pageYOffset};
  return {x: point.x + origin.x, y: point.y + origin.y};
}

function cssClientToCssVisual(point) {
  const origin = {x: visualViewport.offsetLeft, y: visualViewport.offsetTop};
  return {x: point.x - origin.x, y: point.y - origin.y};
}

function cssVisualToCssPage(point) {
  const origin = {x: visualViewport.pageLeft, y: visualViewport.pageTop};
  return {x: point.x + origin.x, y: point.y + origin.y};
}

function cssVisualToCssClient(point) {
  const origin = {x: visualViewport.offsetLeft, y: visualViewport.offsetTop};
  return {x: point.x + origin.x, y: point.y + origin.y};
}


/*
  Blink - pixel scale factors getters
*/
function pageScaleFactor() {
  return visualViewport.scale;
}

function layoutZoomFactor() {
  const scale = internals.layoutZoomFactor();
  assert_greater_than(scale, 0, "internals.layoutZoomFactor() error");
  return scale;
}

/*
  Conversion methods - Pixels scaling
*/
function scaleCssToBlinkPixels(point) {
  // Note that:
  // window.devicePixelRatio = "deviceScaleFactor" * layoutZoomFactor()
  const scale = window.devicePixelRatio;
  return {x: point.x * scale, y: point.y * scale}
}

function scaleCssToDIPixels(point) {
  const scale = pageScaleFactor() * layoutZoomFactor();
  return {x: point.x * scale, y: point.y * scale};
}

function scaleCssToPhysicalPixels(point) {
  const scale = window.devicePixelRatio * pageScaleFactor();
  return {x: point.x * scale, y: point.y * scale}
}

function scalePhysicalToCssPixels(point) {
  const scale = window.devicePixelRatio * pageScaleFactor();
  return {x: point.x / scale, y: point.y / scale}
}

/*
  Visual Viewport helper methods.
*/
// Returns the viewport's bounds in CSS coordinates
function getVisualViewportRect() {
  return {
    left: visualViewport.pageLeft,
    right: visualViewport.pageLeft + visualViewport.width,
    top: visualViewport.pageTop,
    bottom: visualViewport.pageTop + visualViewport.height
  };
}

function getLayoutViewportRect() {
  return {
    left: window.pageXOffset,
    top: window.pageYOffset,
    right: window.pageXOffset + document.documentElement.clientWidth,
    bottom: window.pageYOffset + document.documentElement.clientHeight
  };
}

function offsetFromBounds(point, bounds) {
  let delta = {x: 0, y: 0};
  delta.x = offset(point.x, bounds.left, bounds.right);
  delta.y = offset(point.y, bounds.top, bounds.bottom);

  return delta;
}

// Returns the distance from a point |x| to a (|min|, |max|) interval.
function offset (x, min, max) {
  if (x < min)
    return x - min;
  else if (x > max)
    return x - max;
  else
    return 0;
}
