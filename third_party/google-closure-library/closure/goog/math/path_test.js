/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.math.PathTest');
goog.setTestOnly();

const AffineTransform = goog.require('goog.math.AffineTransform');
const Path = goog.require('goog.math.Path');
const testSuite = goog.require('goog.testing.testSuite');

/**
 * Array mapping numeric segment constant to a descriptive character.
 * @type {!Array<string>}
 */
const SEGMENT_NAMES = function() {
  const arr = [];
  arr[Path.Segment.MOVETO] = 'M';
  arr[Path.Segment.LINETO] = 'L';
  arr[Path.Segment.CURVETO] = 'C';
  arr[Path.Segment.ARCTO] = 'A';
  arr[Path.Segment.CLOSE] = 'X';
  return arr;
}();

/**
 * Test if the given path matches the expected array of commands and parameters.
 * @param {Array<string|number>} expected The expected array of commands and
 *     parameters.
 * @param {Path} path The path to test against.
 */
const assertPathEquals = (expected, path) => {
  const actual = [];
  path.forEachSegment((seg, args) => {
    actual.push(SEGMENT_NAMES[seg]);
    Array.prototype.push.apply(actual, args);
  });
  assertEquals(expected.length, actual.length);
  for (let i = 0; i < expected.length; i++) {
    if (typeof expected[i] === 'number') {
      assertTrue(typeof actual[i] === 'number');
      assertRoughlyEquals(expected[i], actual[i], 0.01);
    } else {
      assertEquals(expected[i], actual[i]);
    }
  }
};

testSuite({
  testConstructor() {
    const path = new Path();
    assertTrue(path.isSimple());
    assertNull(path.getCurrentPoint());
    assertPathEquals([], path);
  },

  testGetSegmentCount() {
    assertArrayEquals([2, 2, 6, 6, 0], [
      Path.Segment.MOVETO, Path.Segment.LINETO, Path.Segment.CURVETO,
      Path.Segment.ARCTO, Path.Segment.CLOSE
    ].map(Path.getSegmentCount));
  },

  testSimpleMoveTo() {
    const path = new Path();
    path.moveTo(30, 50);
    assertTrue(path.isSimple());
    assertObjectEquals([30, 50], path.getCurrentPoint());
    assertPathEquals(['M', 30, 50], path);
  },

  testRepeatedMoveTo() {
    const path = new Path();
    path.moveTo(30, 50);
    path.moveTo(40, 60);
    assertTrue(path.isSimple());
    assertObjectEquals([40, 60], path.getCurrentPoint());
    assertPathEquals(['M', 40, 60], path);
  },

  testSimpleLineTo_fromArgs() {
    const path = new Path();
    const e = assertThrows(() => {
      path.lineTo(30, 50);
    });
    assertEquals('Path cannot start with lineTo', e.message);
    path.moveTo(0, 0);
    path.lineTo(30, 50);
    assertTrue(path.isSimple());
    assertObjectEquals([30, 50], path.getCurrentPoint());
    assertPathEquals(['M', 0, 0, 'L', 30, 50], path);
  },

  testSimpleLineTo_fromArray() {
    const path = new Path();
    const e = assertThrows(() => {
      path.lineToFromArray([30, 50]);
    });
    assertEquals('Path cannot start with lineTo', e.message);
    path.moveTo(0, 0);
    path.lineToFromArray([30, 50]);
    assertTrue(path.isSimple());
    assertObjectEquals([30, 50], path.getCurrentPoint());
    assertPathEquals(['M', 0, 0, 'L', 30, 50], path);
  },

  testMultiArgLineTo_fromArgs() {
    const path = new Path();
    path.moveTo(0, 0);
    path.lineTo(30, 50, 40, 60);
    assertTrue(path.isSimple());
    assertObjectEquals([40, 60], path.getCurrentPoint());
    assertPathEquals(['M', 0, 0, 'L', 30, 50, 40, 60], path);
  },

  testMultiArgLineTo_fromArray() {
    const path = new Path();
    path.moveTo(0, 0);
    path.lineToFromArray([30, 50, 40, 60]);
    assertTrue(path.isSimple());
    assertObjectEquals([40, 60], path.getCurrentPoint());
    assertPathEquals(['M', 0, 0, 'L', 30, 50, 40, 60], path);
  },

  testRepeatedLineTo_fromArgs() {
    const path = new Path();
    path.moveTo(0, 0);
    path.lineTo(30, 50);
    path.lineTo(40, 60);
    assertTrue(path.isSimple());
    assertObjectEquals([40, 60], path.getCurrentPoint());
    assertPathEquals(['M', 0, 0, 'L', 30, 50, 40, 60], path);
  },

  testRepeatedLineTo_fromArray() {
    const path = new Path();
    path.moveTo(0, 0);
    path.lineToFromArray([30, 50]);
    path.lineToFromArray([40, 60]);
    assertTrue(path.isSimple());
    assertObjectEquals([40, 60], path.getCurrentPoint());
    assertPathEquals(['M', 0, 0, 'L', 30, 50, 40, 60], path);
  },

  testSimpleCurveTo_fromArgs() {
    const path = new Path();
    const e = assertThrows(() => {
      path.curveTo(10, 20, 30, 40, 50, 60);
    });
    assertEquals('Path cannot start with curve', e.message);
    path.moveTo(0, 0);
    path.curveTo(10, 20, 30, 40, 50, 60);
    assertTrue(path.isSimple());
    assertObjectEquals([50, 60], path.getCurrentPoint());
    assertPathEquals(['M', 0, 0, 'C', 10, 20, 30, 40, 50, 60], path);
  },

  testSimpleCurveTo_fromArray() {
    const path = new Path();
    const e = assertThrows(() => {
      path.curveToFromArray([10, 20, 30, 40, 50, 60]);
    });
    assertEquals('Path cannot start with curve', e.message);
    path.moveTo(0, 0);
    path.curveToFromArray([10, 20, 30, 40, 50, 60]);
    assertTrue(path.isSimple());
    assertObjectEquals([50, 60], path.getCurrentPoint());
    assertPathEquals(['M', 0, 0, 'C', 10, 20, 30, 40, 50, 60], path);
  },

  testMultiCurveTo_fromArgs() {
    const path = new Path();
    path.moveTo(0, 0);
    path.curveTo(10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120);
    assertTrue(path.isSimple());
    assertObjectEquals([110, 120], path.getCurrentPoint());
    assertPathEquals(
        ['M', 0, 0, 'C', 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120],
        path);
  },

  testMultiCurveTo_fromArray() {
    const path = new Path();
    path.moveTo(0, 0);
    path.curveToFromArray([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120]);
    assertTrue(path.isSimple());
    assertObjectEquals([110, 120], path.getCurrentPoint());
    assertPathEquals(
        ['M', 0, 0, 'C', 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120],
        path);
  },

  testRepeatedCurveTo_fromArgs() {
    const path = new Path();
    path.moveTo(0, 0);
    path.curveTo(10, 20, 30, 40, 50, 60);
    path.curveTo(70, 80, 90, 100, 110, 120);
    assertTrue(path.isSimple());
    assertObjectEquals([110, 120], path.getCurrentPoint());
    assertPathEquals(
        ['M', 0, 0, 'C', 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120],
        path);
  },

  testRepeatedCurveTo_fromArray() {
    const path = new Path();
    path.moveTo(0, 0);
    path.curveToFromArray([10, 20, 30, 40, 50, 60]);
    path.curveToFromArray([70, 80, 90, 100, 110, 120]);
    assertTrue(path.isSimple());
    assertObjectEquals([110, 120], path.getCurrentPoint());
    assertPathEquals(
        ['M', 0, 0, 'C', 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120],
        path);
  },

  testSimpleArc() {
    const path = new Path();
    path.arc(50, 60, 10, 20, 30, 30, false);
    assertFalse(path.isSimple());
    const p = path.getCurrentPoint();
    assertEquals(55, p[0]);
    assertRoughlyEquals(77.32, p[1], 0.01);
    assertPathEquals(['M', 58.66, 70, 'A', 10, 20, 30, 30, 55, 77.32], path);
  },

  testArcNonConnectClose() {
    const path = new Path();
    path.moveTo(0, 0);
    path.arc(10, 10, 10, 10, -90, 180, false);
    assertObjectEquals([10, 20], path.getCurrentPoint());
    path.close();
    assertObjectEquals([10, 0], path.getCurrentPoint());
  },

  testRepeatedArc() {
    const path = new Path();
    path.arc(50, 60, 10, 20, 30, 30, false);
    path.arc(50, 60, 10, 20, 60, 30, false);
    assertFalse(path.isSimple());
    assertObjectEquals([50, 80], path.getCurrentPoint());
    assertPathEquals(
        [
          'M', 58.66, 70,    'A', 10, 20, 30, 30, 55, 77.32,
          'M', 55,    77.32, 'A', 10, 20, 60, 30, 50, 80,
        ],
        path);
  },

  testRepeatedArc2() {
    const path = new Path();
    path.arc(50, 60, 10, 20, 30, 30, false);
    path.arc(50, 60, 10, 20, 60, 30, true);
    assertPathEquals(
        [
          'M',
          58.66,
          70,
          'A',
          10,
          20,
          30,
          30,
          55,
          77.32,
          'A',
          10,
          20,
          60,
          30,
          50,
          80,
        ],
        path);
  },

  testCompleteCircle() {
    const path = new Path();
    path.arc(0, 0, 10, 10, 0, 360, false);
    assertFalse(path.isSimple());
    const p = path.getCurrentPoint();
    assertRoughlyEquals(10, p[0], 0.01);
    assertRoughlyEquals(0, p[1], 0.01);
    assertPathEquals(['M', 10, 0, 'A', 10, 10, 0, 360, 10, 0], path);
  },

  testClose() {
    const path = new Path();
    assertThrows('Path cannot start with close', () => {
      path.close();
    });

    path.moveTo(0, 0);
    path.lineTo(10, 20, 30, 40, 50, 60);
    path.close();
    assertTrue(path.isSimple());
    assertObjectEquals([0, 0], path.getCurrentPoint());
    assertPathEquals(['M', 0, 0, 'L', 10, 20, 30, 40, 50, 60, 'X'], path);
  },

  testClear() {
    const path = new Path();
    path.moveTo(0, 0);
    path.arc(50, 60, 10, 20, 30, 30, false);
    path.clear();
    assertTrue(path.isSimple());
    assertNull(path.getCurrentPoint());
    assertPathEquals([], path);
  },

  testCreateSimplifiedPath() {
    let path = new Path();
    path.moveTo(0, 0);
    path.arc(50, 60, 10, 20, 30, 30, false);
    assertFalse(path.isSimple());
    path = Path.createSimplifiedPath(path);
    assertTrue(path.isSimple());
    const p = path.getCurrentPoint();
    assertEquals(55, p[0]);
    assertRoughlyEquals(77.32, p[1], 0.01);
    assertPathEquals(
        ['M', 58.66, 70, 'C', 57.78, 73.04, 56.52, 75.57, 55, 77.32], path);
  },

  testCreateSimplifiedPath2() {
    let path = new Path();
    path.arc(50, 60, 10, 20, 30, 30, false);
    path.arc(50, 60, 10, 20, 60, 30, false);
    assertFalse(path.isSimple());
    path = Path.createSimplifiedPath(path);
    assertTrue(path.isSimple());
    assertPathEquals(
        [
          'M', 58.66, 70,    'C', 57.78, 73.04, 56.52, 75.57, 55, 77.32,
          'M', 55,    77.32, 'C', 53.48, 79.08, 51.76, 80,    50, 80,
        ],
        path);
  },

  testCreateSimplifiedPath3() {
    let path = new Path();
    path.arc(50, 60, 10, 20, 30, 30, false);
    path.arc(50, 60, 10, 20, 60, 30, true);
    path.close();
    path = Path.createSimplifiedPath(path);
    assertPathEquals(
        [
          'M',
          58.66,
          70,
          'C',
          57.78,
          73.04,
          56.52,
          75.57,
          55,
          77.32,
          53.48,
          79.08,
          51.76,
          80,
          50,
          80,
          'X',
        ],
        path);
    const p = path.getCurrentPoint();
    assertRoughlyEquals(58.66, p[0], 0.01);
    assertRoughlyEquals(70, p[1], 0.01);
  },

  testArcToAsCurves() {
    const path = new Path();
    path.moveTo(58.66, 70);
    path.arcToAsCurves(10, 20, 30, 30);
    assertPathEquals(
        ['M', 58.66, 70, 'C', 57.78, 73.04, 56.52, 75.57, 55, 77.32], path);
  },

  testCreateTransformedPath() {
    const path = new Path();
    path.moveTo(0, 0);
    path.lineTo(0, 10, 10, 10, 10, 0);
    path.close();
    const tx = new AffineTransform(2, 0, 0, 3, 10, 20);
    const path2 = path.createTransformedPath(tx);
    assertPathEquals(['M', 0, 0, 'L', 0, 10, 10, 10, 10, 0, 'X'], path);
    assertPathEquals(['M', 10, 20, 'L', 10, 50, 30, 50, 30, 20, 'X'], path2);
  },

  testTransform() {
    const path = new Path();
    path.moveTo(0, 0);
    path.lineTo(0, 10, 10, 10, 10, 0);
    path.close();
    const tx = new AffineTransform(2, 0, 0, 3, 10, 20);
    const path2 = path.transform(tx);
    assertTrue(path === path2);
    assertPathEquals(['M', 10, 20, 'L', 10, 50, 30, 50, 30, 20, 'X'], path2);
  },

  testTransformCurrentAndClosePoints() {
    const path = new Path();
    path.moveTo(0, 0);
    assertObjectEquals([0, 0], path.getCurrentPoint());
    path.transform(new AffineTransform(1, 0, 0, 1, 10, 20));
    assertObjectEquals([10, 20], path.getCurrentPoint());
    path.lineTo(50, 50);
    path.close();
    assertObjectEquals([10, 20], path.getCurrentPoint());
  },

  testTransformNonSimple() {
    const path = new Path();
    path.arc(50, 60, 10, 20, 30, 30, false);
    assertThrows(() => {
      path.transform(new AffineTransform(1, 0, 0, 1, 10, 20));
    });
  },

  testAppendPath() {
    const path1 = new Path();
    path1.moveTo(0, 0);
    path1.lineTo(0, 10, 10, 10, 10, 0);
    path1.close();

    const path2 = new Path();
    path2.arc(50, 60, 10, 20, 30, 30, false);

    assertTrue(path1.isSimple());
    path1.appendPath(path2);
    assertFalse(path1.isSimple());
    assertPathEquals(
        [
          'M', 0,     0,  'L', 0,  10, 10, 10, 10, 0,     'X',
          'M', 58.66, 70, 'A', 10, 20, 30, 30, 55, 77.32,
        ],
        path1);
  },

  testIsEmpty() {
    const path = new Path();
    assertTrue('Initially path is empty', path.isEmpty());

    path.moveTo(0, 0);
    assertFalse('After command addition, path is not empty', path.isEmpty());

    path.clear();
    assertTrue('After clear, path is empty again', path.isEmpty());
  },

  testGetSegmentTypes() {
    const path = new Path();
    path.moveTo(0, 0);
    path.lineTo(10, 20, 30, 40);
    path.close();

    const Segment = Path.Segment;
    const segmentTypes = path.getSegmentTypes();
    assertArrayEquals(
        'The returned segment types do not match the expected values',
        [Segment.MOVETO, Segment.LINETO, Segment.CLOSE], segmentTypes);

    segmentTypes[2] = Segment.LINETO;
    assertArrayEquals(
        'Modifying the returned segment types changed the path',
        [Segment.MOVETO, Segment.LINETO, Segment.CLOSE],
        path.getSegmentTypes());
  },

  testGetSegmentCounts() {
    const path = new Path();
    path.moveTo(0, 0);
    path.lineTo(10, 20, 30, 40);
    path.close();

    const segmentTypes = path.getSegmentCounts();
    assertArrayEquals(
        'The returned segment counts do not match the expected values',
        [1, 2, 1], segmentTypes);

    segmentTypes[1] = 3;
    assertArrayEquals(
        'Modifying the returned segment counts changed the path', [1, 2, 1],
        path.getSegmentCounts());
  },

  testGetSegmentArgs() {
    const path = new Path();
    path.moveTo(0, 0);
    path.lineTo(10, 20, 30, 40);
    path.close();

    const segmentTypes = path.getSegmentArgs();
    assertArrayEquals(
        'The returned segment args do not match the expected values',
        [0, 0, 10, 20, 30, 40], segmentTypes);

    segmentTypes[1] = -10;
    assertArrayEquals(
        'Modifying the returned segment args changed the path',
        [0, 0, 10, 20, 30, 40], path.getSegmentArgs());
  },
});
