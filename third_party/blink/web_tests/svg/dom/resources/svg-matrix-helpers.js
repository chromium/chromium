// Shared assertion helpers for SVG cross-API invariant tests.
//
// Matrix arithmetic uses native SVGMatrix methods (inverse, multiply)
// and SVGPoint.matrixTransform; this file only provides assertion-shaped
// utilities that build on top of those.

// Asserts component-wise approximate equality of two 2D affine matrices
// (any object exposing a/b/c/d/e/f). Pure assertion: caller is responsible
// for wrapping the call in its own test(...) block so any throw at the
// call site (e.g., a null getCTM()) is caught by the testharness.
function assertMatrixApproxEquals(actual, expected, epsilon) {
  for (const prop of ['a', 'b', 'c', 'd', 'e', 'f']) {
    assert_approx_equals(actual[prop], expected[prop], epsilon,
                         `matrix.${prop}`);
  }
}

// Computes the getBoundingClientRect()-equivalent for an SVG element by
// mapping the four corners of its getBBox() through getScreenCTM() and
// taking the axis-aligned bounding box of the result. This matches what
// getBoundingClientRect() returns for both axis-aligned and rotated
// transforms (in the rotated case, the AABB envelope of the rotated
// rectangle is exactly what gBCR reports).
function bboxToClientRect(el) {
  const ctm = el.getScreenCTM();
  const bbox = el.getBBox();
  const svg = el.ownerSVGElement || el;
  const corners = [
    [bbox.x,              bbox.y],
    [bbox.x + bbox.width, bbox.y],
    [bbox.x,              bbox.y + bbox.height],
    [bbox.x + bbox.width, bbox.y + bbox.height],
  ].map(([x, y]) => {
    const pt = svg.createSVGPoint();
    pt.x = x;
    pt.y = y;
    return pt.matrixTransform(ctm);
  });
  const xs = corners.map(p => p.x);
  const ys = corners.map(p => p.y);
  const left = Math.min(...xs);
  const top = Math.min(...ys);
  const right = Math.max(...xs);
  const bottom = Math.max(...ys);
  return { left, top, right, bottom,
           width: right - left, height: bottom - top };
}
