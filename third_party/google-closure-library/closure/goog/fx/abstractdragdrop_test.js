/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.fx.AbstractDragDropTest');
goog.setTestOnly('goog.fx.AbstractDragDropTest');

const AbstractDragDrop = goog.require('goog.fx.AbstractDragDrop');
const Box = goog.require('goog.math.Box');
const Coordinate = goog.require('goog.math.Coordinate');
const DragDropItem = goog.require('goog.fx.DragDropItem');
const EventType = goog.require('goog.events.EventType');
const TagName = goog.require('goog.dom.TagName');
const array = goog.require('goog.array');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const functions = goog.require('goog.functions');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');

const {ActiveDropTarget} = AbstractDragDrop.TEST_ONLY;

const targets = [
  {box_: new Box(0, 3, 1, 1)}, {box_: new Box(0, 7, 2, 6)},
  {box_: new Box(2, 2, 3, 1)}, {box_: new Box(4, 1, 6, 1)},
  {box_: new Box(4, 9, 7, 6)}, {box_: new Box(9, 9, 10, 1)}
];

const targets2 = [
  {box_: new Box(10, 50, 20, 10)}, {box_: new Box(20, 50, 30, 10)},
  {box_: new Box(60, 50, 70, 10)}, {box_: new Box(70, 50, 80, 10)}
];

const targets3 = [
  {box_: new Box(0, 4, 1, 1)}, {box_: new Box(1, 6, 4, 5)},
  {box_: new Box(5, 5, 6, 2)}, {box_: new Box(2, 1, 5, 0)}
];


/**
 * An enum describing how two ranges overlap (non-symmetrical relation).
 * @enum {number}
 */
const RangeOverlap = {
  LEFT: 1,      // First range is placed to the left of the second.
  LEFT_IN: 2,   // First range overlaps on the left side of the second.
  IN: 3,        // First range is completely contained in the second.
  RIGHT_IN: 4,  // First range overlaps on the right side of the second.
  RIGHT: 5,     // First range is placed to the right side of the second.
  CONTAINS: 6   // First range contains the second.
};


/**
 * Computes how two one dimensional ranges overlap.
 *
 * @param {number} left1 Left inclusive bound of the first range.
 * @param {number} right1 Right exclusive bound of the first range.
 * @param {number} left2 Left inclusive bound of the second range.
 * @param {number} right2 Right exclusive bound of the second range.
 * @return {RangeOverlap} The enum value describing the type of the overlap.
 */
function rangeOverlap(left1, right1, left2, right2) {
  if (right1 <= left2) return RangeOverlap.LEFT;
  if (left1 >= right2) return RangeOverlap.RIGHT;
  const leftIn = left1 >= left2;
  const rightIn = right1 <= right2;
  if (leftIn && rightIn) return RangeOverlap.IN;
  if (leftIn) return RangeOverlap.RIGHT_IN;
  if (rightIn) return RangeOverlap.LEFT_IN;
  return RangeOverlap.CONTAINS;
}


/**
 * Tells whether two boxes overlap.
 *
 * @param {!Box} box1 First box in question.
 * @param {!Box} box2 Second box in question.
 * @return {boolean} Whether boxes overlap in any way.
 */
function boxOverlaps(box1, box2) {
  const horizontalOverlap =
      rangeOverlap(box1.left, box1.right, box2.left, box2.right);
  const verticalOverlap =
      rangeOverlap(box1.top, box1.bottom, box2.top, box2.bottom);
  return horizontalOverlap != RangeOverlap.LEFT &&
      horizontalOverlap != RangeOverlap.RIGHT &&
      verticalOverlap != RangeOverlap.LEFT &&
      verticalOverlap != RangeOverlap.RIGHT;
}



/**
 * Checks whether a given box overlaps any of given DnD target boxes.
 *
 * @param {!Box} box The box to check.
 * @param {!Array<!Object>} targets The array of targets with boxes to check
 *     if they overlap with the given box.
 * @return {boolean} Whether the box overlaps any of the target boxes.
 */
function boxOverlapsTargets(box, targets) {
  return array.some(
      targets, /**
                  @suppress {strictMissingProperties} suppression added to
                  enable type checking
                */
      function(target) {
        return boxOverlaps(box, target.box_);
      });
}



/**
 * Helper function for manual debugging.
 * @param {!Array<*>} targets
 * @param {number} multiplier
 */
function drawTargets(targets, multiplier) {
  const colors = ['green', 'blue', 'red', 'lime', 'pink', 'silver', 'orange'];
  const cont = document.getElementById('cont');
  cont.innerHTML = '';
  for (let i = 0; i < targets.length; i++) {
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    const box = targets[i].box_;
    const el = dom.createElement(TagName.DIV);
    el.style.top = (box.top * multiplier) + 'px';
    el.style.left = (box.left * multiplier) + 'px';
    el.style.width = ((box.right - box.left) * multiplier) + 'px';
    el.style.height = ((box.bottom - box.top) * multiplier) + 'px';
    el.style.backgroundColor = colors[i];
    cont.appendChild(el);
  }
}


testSuite({
  /**
   * Test the utility function which tells how two one dimensional ranges
   * overlap.
   */
  testRangeOverlap() {
    assertEquals(RangeOverlap.LEFT, rangeOverlap(1, 2, 3, 4));
    assertEquals(RangeOverlap.LEFT, rangeOverlap(2, 3, 3, 4));
    assertEquals(RangeOverlap.LEFT_IN, rangeOverlap(1, 3, 2, 4));
    assertEquals(RangeOverlap.IN, rangeOverlap(1, 3, 1, 4));
    assertEquals(RangeOverlap.IN, rangeOverlap(2, 3, 1, 4));
    assertEquals(RangeOverlap.IN, rangeOverlap(3, 4, 1, 4));
    assertEquals(RangeOverlap.RIGHT_IN, rangeOverlap(2, 4, 1, 3));
    assertEquals(RangeOverlap.RIGHT, rangeOverlap(2, 3, 1, 2));
    assertEquals(RangeOverlap.RIGHT, rangeOverlap(3, 4, 1, 2));
    assertEquals(RangeOverlap.CONTAINS, rangeOverlap(1, 4, 2, 3));
  },


  /**
   * Tests if the utility function to compute box overlapping functions
   * properly.
   */
  testBoxOverlaps() {
    // Overlapping tests.
    let box2 = new Box(1, 4, 4, 1);

    // Corner overlaps.
    assertTrue('NW overlap', boxOverlaps(new Box(0, 2, 2, 0), box2));
    assertTrue('NE overlap', boxOverlaps(new Box(0, 5, 2, 3), box2));
    assertTrue('SE overlap', boxOverlaps(new Box(3, 5, 5, 3), box2));
    assertTrue('SW overlap', boxOverlaps(new Box(3, 2, 5, 0), box2));

    // Inside.
    assertTrue('Inside overlap', boxOverlaps(new Box(2, 3, 3, 2), box2));

    // Around.
    assertTrue('Outside overlap', boxOverlaps(new Box(0, 5, 5, 0), box2));

    // Edge overlaps.
    assertTrue('N overlap', boxOverlaps(new Box(0, 3, 2, 2), box2));
    assertTrue('E overlap', boxOverlaps(new Box(2, 5, 3, 3), box2));
    assertTrue('S overlap', boxOverlaps(new Box(3, 3, 5, 2), box2));
    assertTrue('W overlap', boxOverlaps(new Box(2, 2, 3, 0), box2));

    assertTrue('N-in overlap', boxOverlaps(new Box(0, 5, 2, 0), box2));
    assertTrue('E-in overlap', boxOverlaps(new Box(0, 5, 5, 3), box2));
    assertTrue('S-in overlap', boxOverlaps(new Box(3, 5, 5, 0), box2));
    assertTrue('W-in overlap', boxOverlaps(new Box(0, 2, 5, 0), box2));

    // Does not overlap.
    box2 = new Box(3, 6, 6, 3);

    // Along the edge - shorter.
    assertFalse('N-in no overlap', boxOverlaps(new Box(1, 5, 2, 4), box2));
    assertFalse('E-in no overlap', boxOverlaps(new Box(4, 8, 5, 7), box2));
    assertFalse('S-in no overlap', boxOverlaps(new Box(7, 5, 8, 4), box2));
    assertFalse('N-in no overlap', boxOverlaps(new Box(4, 2, 5, 1), box2));

    // By the corner.
    assertFalse('NE no overlap', boxOverlaps(new Box(1, 8, 2, 7), box2));
    assertFalse('SE no overlap', boxOverlaps(new Box(7, 8, 8, 7), box2));
    assertFalse('SW no overlap', boxOverlaps(new Box(7, 2, 8, 1), box2));
    assertFalse('NW no overlap', boxOverlaps(new Box(1, 2, 2, 1), box2));

    // Perpendicular to an edge.
    assertFalse('NNE no overlap', boxOverlaps(new Box(1, 7, 2, 5), box2));
    assertFalse('NEE no overlap', boxOverlaps(new Box(2, 8, 4, 7), box2));
    assertFalse('SEE no overlap', boxOverlaps(new Box(5, 8, 7, 7), box2));
    assertFalse('SSE no overlap', boxOverlaps(new Box(7, 7, 8, 5), box2));
    assertFalse('SSW no overlap', boxOverlaps(new Box(7, 4, 8, 2), box2));
    assertFalse('SWW no overlap', boxOverlaps(new Box(5, 2, 7, 1), box2));
    assertFalse('NWW no overlap', boxOverlaps(new Box(2, 2, 4, 1), box2));
    assertFalse('NNW no overlap', boxOverlaps(new Box(1, 4, 2, 2), box2));

    // Along the edge - longer.
    assertFalse('N no overlap', boxOverlaps(new Box(0, 7, 1, 2), box2));
    assertFalse('E no overlap', boxOverlaps(new Box(2, 9, 7, 8), box2));
    assertFalse('S no overlap', boxOverlaps(new Box(8, 7, 9, 2), box2));
    assertFalse('W no overlap', boxOverlaps(new Box(2, 1, 7, 0), box2));
  },


  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testMaybeCreateDummyTargetForPosition() {
    const testGroup = new AbstractDragDrop();
    /** @suppress {visibility} suppression added to enable type checking */
    testGroup.targetList_ = targets;
    /** @suppress {visibility} suppression added to enable type checking */
    testGroup.targetBox_ = new Box(0, 9, 10, 1);

    /** @suppress {visibility} suppression added to enable type checking */
    let target = testGroup.maybeCreateDummyTargetForPosition_(3, 3);
    assertFalse(boxOverlapsTargets(target.box_, testGroup.targetList_));
    assertTrue(testGroup.isInside(3, 3, target.box_));

    /** @suppress {visibility} suppression added to enable type checking */
    target = testGroup.maybeCreateDummyTargetForPosition_(2, 4);
    assertFalse(boxOverlapsTargets(target.box_, testGroup.targetList_));
    assertTrue(testGroup.isInside(2, 4, target.box_));

    /** @suppress {visibility} suppression added to enable type checking */
    target = testGroup.maybeCreateDummyTargetForPosition_(2, 7);
    assertFalse(boxOverlapsTargets(target.box_, testGroup.targetList_));
    assertTrue(testGroup.isInside(2, 7, target.box_));

    testGroup.targetList_.push({box_: new Box(5, 6, 6, 0)});

    /** @suppress {visibility} suppression added to enable type checking */
    target = testGroup.maybeCreateDummyTargetForPosition_(3, 3);
    assertFalse(boxOverlapsTargets(target.box_, testGroup.targetList_));
    assertTrue(testGroup.isInside(3, 3, target.box_));

    /** @suppress {visibility} suppression added to enable type checking */
    target = testGroup.maybeCreateDummyTargetForPosition_(2, 7);
    assertFalse(boxOverlapsTargets(target.box_, testGroup.targetList_));
    assertTrue(testGroup.isInside(2, 7, target.box_));

    /** @suppress {visibility} suppression added to enable type checking */
    target = testGroup.maybeCreateDummyTargetForPosition_(6, 3);
    assertFalse(boxOverlapsTargets(target.box_, testGroup.targetList_));
    assertTrue(testGroup.isInside(6, 3, target.box_));

    /** @suppress {visibility} suppression added to enable type checking */
    target = testGroup.maybeCreateDummyTargetForPosition_(0, 3);
    assertNull(target);
    /** @suppress {visibility} suppression added to enable type checking */
    target = testGroup.maybeCreateDummyTargetForPosition_(9, 0);
    assertNull(target);
  },


  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testMaybeCreateDummyTargetForPosition2() {
    const testGroup = new AbstractDragDrop();
    /** @suppress {visibility} suppression added to enable type checking */
    testGroup.targetList_ = targets2;
    /** @suppress {visibility} suppression added to enable type checking */
    testGroup.targetBox_ = new Box(10, 50, 80, 10);

    /** @suppress {visibility} suppression added to enable type checking */
    let target = testGroup.maybeCreateDummyTargetForPosition_(30, 40);
    assertFalse(boxOverlapsTargets(target.box_, testGroup.targetList_));
    assertTrue(testGroup.isInside(30, 40, target.box_));

    /** @suppress {visibility} suppression added to enable type checking */
    target = testGroup.maybeCreateDummyTargetForPosition_(45, 40);
    assertFalse(boxOverlapsTargets(target.box_, testGroup.targetList_));
    assertTrue(testGroup.isInside(45, 40, target.box_));

    testGroup.targetList_.push({box_: new Box(40, 50, 50, 40)});

    /** @suppress {visibility} suppression added to enable type checking */
    target = testGroup.maybeCreateDummyTargetForPosition_(30, 40);
    assertFalse(boxOverlapsTargets(target.box_, testGroup.targetList_));
    /** @suppress {visibility} suppression added to enable type checking */
    target = testGroup.maybeCreateDummyTargetForPosition_(45, 35);
    assertFalse(boxOverlapsTargets(target.box_, testGroup.targetList_));
  },


  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testMaybeCreateDummyTargetForPosition3BoxHasDecentSize() {
    const testGroup = new AbstractDragDrop();
    /** @suppress {visibility} suppression added to enable type checking */
    testGroup.targetList_ = targets3;
    /** @suppress {visibility} suppression added to enable type checking */
    testGroup.targetBox_ = new Box(0, 6, 6, 0);

    /** @suppress {visibility} suppression added to enable type checking */
    const target = testGroup.maybeCreateDummyTargetForPosition_(3, 3);
    assertFalse(boxOverlapsTargets(target.box_, testGroup.targetList_));
    assertTrue(testGroup.isInside(3, 3, target.box_));
    assertEquals('(1t, 5r, 5b, 1l)', target.box_.toString());
  },


  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testMaybeCreateDummyTargetForPosition4() {
    const testGroup = new AbstractDragDrop();
    /** @suppress {visibility} suppression added to enable type checking */
    testGroup.targetList_ = targets;
    /** @suppress {visibility} suppression added to enable type checking */
    testGroup.targetBox_ = new Box(0, 9, 10, 1);

    for (/** @suppress {visibility} suppression added to enable type checking */
         let x = testGroup.targetBox_.left; x < testGroup.targetBox_.right;
         x++) {
      for (/**
              @suppress {visibility} suppression added to enable type checking
            */
           let y = testGroup.targetBox_.top; y < testGroup.targetBox_.bottom;
           y++) {
        let inRealTarget = false;
        for (let i = 0; i < testGroup.targetList_.length; i++) {
          if (testGroup.isInside(x, y, testGroup.targetList_[i].box_)) {
            inRealTarget = true;
            break;
          }
        }
        if (!inRealTarget) {
          /**
           * @suppress {visibility} suppression added to enable type checking
           */
          const target = testGroup.maybeCreateDummyTargetForPosition_(x, y);
          if (target) {
            assertFalse(
                'Fake target for point(' + x + ',' + y + ') should ' +
                    'not overlap any real targets.',
                boxOverlapsTargets(target.box_, testGroup.targetList_));
            assertTrue(testGroup.isInside(x, y, target.box_));
          }
        }
      }
    }
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testMaybeCreateDummyTargetForPosition_NegativePositions() {
    const negTargets =
        [{box_: new Box(-20, 10, -5, 1)}, {box_: new Box(20, 10, 30, 1)}];

    const testGroup = new AbstractDragDrop();
    /** @suppress {visibility} suppression added to enable type checking */
    testGroup.targetList_ = negTargets;
    /** @suppress {visibility} suppression added to enable type checking */
    testGroup.targetBox_ = new Box(-20, 10, 30, 1);

    /** @suppress {visibility} suppression added to enable type checking */
    const target = testGroup.maybeCreateDummyTargetForPosition_(1, 5);
    assertFalse(boxOverlapsTargets(target.box_, testGroup.targetList_));
    assertTrue(testGroup.isInside(1, 5, target.box_));
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testMaybeCreateDummyTargetOutsideScrollableContainer() {
    const targets =
        [{box_: new Box(0, 3, 10, 1)}, {box_: new Box(20, 3, 30, 1)}];
    const target = targets[0];

    const testGroup = new AbstractDragDrop();
    /** @suppress {visibility} suppression added to enable type checking */
    testGroup.targetList_ = targets;
    /** @suppress {visibility} suppression added to enable type checking */
    testGroup.targetBox_ = new Box(0, 3, 30, 1);

    testGroup.addScrollableContainer(document.getElementById('container1'));
    /** @suppress {visibility} suppression added to enable type checking */
    const container = testGroup.scrollableContainers_[0];
    container.containedTargets_.push(target);
    /** @suppress {visibility} suppression added to enable type checking */
    container.box_ = new Box(0, 3, 5, 1);  // shorter than target
    target.scrollableContainer_ = container;

    // mouse cursor is below scrollable target but not the actual target
    /** @suppress {visibility} suppression added to enable type checking */
    const dummyTarget = testGroup.maybeCreateDummyTargetForPosition_(2, 7);
    // dummy target should not overlap the scrollable container
    assertFalse(boxOverlaps(dummyTarget.box_, container.box_));
    // but should overlap the actual target, since not all of it is visible
    assertTrue(boxOverlaps(dummyTarget.box_, target.box_));
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testMaybeCreateDummyTargetInsideScrollableContainer() {
    const targets =
        [{box_: new Box(0, 3, 10, 1)}, {box_: new Box(20, 3, 30, 1)}];
    const target = targets[0];

    const testGroup = new AbstractDragDrop();
    /** @suppress {visibility} suppression added to enable type checking */
    testGroup.targetList_ = targets;
    /** @suppress {visibility} suppression added to enable type checking */
    testGroup.targetBox_ = new Box(0, 3, 30, 1);

    testGroup.addScrollableContainer(document.getElementById('container1'));
    /** @suppress {visibility} suppression added to enable type checking */
    const container = testGroup.scrollableContainers_[0];
    container.containedTargets_.push(target);
    /** @suppress {visibility} suppression added to enable type checking */
    container.box_ = new Box(0, 3, 20, 1);  // longer than target
    target.scrollableContainer_ = container;

    // mouse cursor is below both the scrollable and the actual target
    /** @suppress {visibility} suppression added to enable type checking */
    const dummyTarget = testGroup.maybeCreateDummyTargetForPosition_(2, 15);
    // dummy target should overlap the scrollable container
    assertTrue(boxOverlaps(dummyTarget.box_, container.box_));
    // but not overlap the actual target
    assertFalse(boxOverlaps(dummyTarget.box_, target.box_));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testCalculateTargetBox() {
    let testGroup = new AbstractDragDrop();
    /** @suppress {visibility} suppression added to enable type checking */
    testGroup.targetList_ = [];
    array.forEach(
        targets, /**
                    @suppress {visibility} suppression added to enable type
                    checking
                  */
        function(target) {
          testGroup.targetList_.push(target);
          testGroup.calculateTargetBox_(target.box_);
        });
    assertTrue(Box.equals(testGroup.targetBox_, new Box(0, 9, 10, 1)));

    testGroup = new AbstractDragDrop();
    /** @suppress {visibility} suppression added to enable type checking */
    testGroup.targetList_ = [];
    array.forEach(
        targets2, /**
                     @suppress {visibility} suppression added to enable type
                     checking
                   */
        function(target) {
          testGroup.targetList_.push(target);
          testGroup.calculateTargetBox_(target.box_);
        });
    assertTrue(Box.equals(testGroup.targetBox_, new Box(10, 50, 80, 10)));

    testGroup = new AbstractDragDrop();
    /** @suppress {visibility} suppression added to enable type checking */
    testGroup.targetList_ = [];
    array.forEach(
        targets3, /**
                     @suppress {visibility} suppression added to enable type
                     checking
                   */
        function(target) {
          testGroup.targetList_.push(target);
          testGroup.calculateTargetBox_(target.box_);
        });
    assertTrue(Box.equals(testGroup.targetBox_, new Box(0, 6, 6, 0)));
  },


  /** @suppress {visibility} suppression added to enable type checking */
  testIsInside() {
    const add = new AbstractDragDrop();
    // The box in question.
    // 10,20+++++20,20
    //   +         |
    // 10,30-----20,30
    const box = new Box(20, 20, 30, 10);

    assertTrue(
        'A point somewhere in the middle of the box should be inside.',
        add.isInside(15, 25, box));

    assertTrue(
        'A point in top-left corner should be inside the box.',
        add.isInside(10, 20, box));

    assertTrue(
        'A point on top border should be inside the box.',
        add.isInside(15, 20, box));

    assertFalse(
        'A point in top-right corner should be outside the box.',
        add.isInside(20, 20, box));

    assertFalse(
        'A point on right border should be outside the box.',
        add.isInside(20, 25, box));

    assertFalse(
        'A point in bottom-right corner should be outside the box.',
        add.isInside(20, 30, box));

    assertFalse(
        'A point on bottom border should be outside the box.',
        add.isInside(15, 30, box));

    assertFalse(
        'A point in bottom-left corner should be outside the box.',
        add.isInside(10, 30, box));

    assertTrue(
        'A point on left border should be inside the box.',
        add.isInside(10, 25, box));

    add.dispose();
  },


  /** @suppress {visibility} suppression added to enable type checking */
  testAddingRemovingScrollableContainers() {
    const group = new AbstractDragDrop();
    const el1 = dom.createElement(TagName.DIV);
    const el2 = dom.createElement(dom.TagName.DIV);

    assertEquals(0, group.scrollableContainers_.length);

    group.addScrollableContainer(el1);
    assertEquals(1, group.scrollableContainers_.length);

    group.addScrollableContainer(el2);
    assertEquals(2, group.scrollableContainers_.length);

    group.removeAllScrollableContainers();
    assertEquals(0, group.scrollableContainers_.length);
  },


  /** @suppress {visibility} suppression added to enable type checking */
  testScrollableContainersCalculation() {
    const group = new AbstractDragDrop();
    const target = new AbstractDragDrop();

    group.addTarget(target);
    group.addScrollableContainer(document.getElementById('container1'));
    /** @suppress {visibility} suppression added to enable type checking */
    const container = group.scrollableContainers_[0];

    const item1 = new DragDropItem(document.getElementById('child1'));
    const item2 = new DragDropItem(document.getElementById('child2'));

    target.items_.push(item1);
    group.recalculateDragTargets();
    group.recalculateScrollableContainers();

    assertEquals(1, container.containedTargets_.length);
    assertEquals(container, group.targetList_[0].scrollableContainer_);

    target.items_.push(item2);
    group.recalculateDragTargets();
    assertEquals(1, container.containedTargets_.length);
    assertNull(group.targetList_[0].scrollableContainer_);

    group.recalculateScrollableContainers();
    assertEquals(2, container.containedTargets_.length);
    assertEquals(container, group.targetList_[1].scrollableContainer_);
  },


  /** @suppress {visibility} suppression added to enable type checking */
  testMouseDownEventDefaultAction() {
    const group = new AbstractDragDrop();
    const target = new AbstractDragDrop();
    group.addTarget(target);
    const item1 = new DragDropItem(document.getElementById('child1'));
    group.items_.push(item1);
    item1.setParent(group);
    group.init();

    const mousedownDefaultPrevented =
        !testingEvents.fireMouseDownEvent(item1.element);

    assertFalse(
        'Default action of mousedown event should not be cancelled.',
        mousedownDefaultPrevented);
  },


  // See http://b/7494613.
  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testMouseUpOutsideElement() {
    const group = new AbstractDragDrop();
    const target = new AbstractDragDrop();
    group.addTarget(target);
    const item1 = new DragDropItem(document.getElementById('child1'));
    group.items_.push(item1);
    item1.setParent(group);
    group.init();

    group.startDrag = functions.error('startDrag should not be called.');

    testingEvents.fireMouseDownEvent(item1.element);
    testingEvents.fireMouseUpEvent(item1.element.parentNode);
    // This should have no effect (not start a drag) since the previous event
    // should have cleared the listeners.
    testingEvents.fireMouseOutEvent(item1.element);

    group.dispose();
    target.dispose();
  },


  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testScrollBeforeMoveDrag() {
    const group = new AbstractDragDrop();
    const target = new AbstractDragDrop();

    group.addTarget(target);
    const container = document.getElementById('container1');
    group.addScrollableContainer(container);

    const childEl = document.getElementById('child1');
    const item = new DragDropItem(childEl);
    /** @suppress {visibility} suppression added to enable type checking */
    item.currentDragElement_ = childEl;

    target.items_.push(item);
    group.recalculateDragTargets();
    group.recalculateScrollableContainers();

    // Simulare starting a drag.
    const moveEvent = {
      'clientX': 8,
      'clientY': 10,
      'type': EventType.MOUSEMOVE,
      'relatedTarget': childEl,
      'preventDefault': function() {}
    };
    group.startDrag(moveEvent, item);

    // Simulate scrolling before the first move drag event.
    const scrollEvent = {'target': container};
    assertNotThrows(
        goog.bind(group.containerScrollHandler_, group, scrollEvent));
  },


  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testMouseMove_mouseOutBeforeThreshold() {
    // Setup dragdrop and item
    const itemEl = dom.createElement(TagName.DIV);
    const childEl = dom.createElement(TagName.DIV);
    itemEl.appendChild(childEl);
    const add = new AbstractDragDrop();
    const item = new DragDropItem(itemEl);
    item.setParent(add);
    add.items_.push(item);

    // Simulate maybeStartDrag
    /** @suppress {visibility} suppression added to enable type checking */
    item.startPosition_ = new Coordinate(10, 10);
    /** @suppress {visibility} suppression added to enable type checking */
    item.currentDragElement_ = itemEl;

    // Test
    let draggedItem = null;
    add.startDrag = function(event, item) {
      draggedItem = item;
    };

    let event = new testingEvents.Event(EventType.MOUSEOUT, childEl);
    // Drag distance is only 2.
    event.clientX = 8;
    event.clientY = 10;
    item.mouseMove_(event);
    assertEquals(
        'DragStart should not be fired for mouseout on child element.', null,
        draggedItem);

    event = new testingEvents.Event(EventType.MOUSEOUT, itemEl);
    // Drag distance is only 2.
    event.clientX = 8;
    event.clientY = 10;
    item.mouseMove_(event);
    assertEquals(
        'DragStart should be fired for mouseout on main element.', item,
        draggedItem);
  },


  testGetDragElementPosition() {
    const testGroup = new AbstractDragDrop();
    const sourceEl = dom.createElement(TagName.DIV);
    document.body.appendChild(sourceEl);

    let pageOffset = style.getPageOffset(sourceEl);
    /** @suppress {checkTypes} suppression added to enable type checking */
    let pos = testGroup.getDragElementPosition(sourceEl);
    assertEquals(
        'Drag element position should be source element page offset',
        pageOffset.x, pos.x);
    assertEquals(
        'Drag element position should be source element page offset',
        pageOffset.y, pos.y);

    sourceEl.style.marginLeft = '5px';
    sourceEl.style.marginTop = '7px';
    pageOffset = style.getPageOffset(sourceEl);
    /** @suppress {checkTypes} suppression added to enable type checking */
    pos = testGroup.getDragElementPosition(sourceEl);
    assertEquals(
        'Drag element position should be adjusted for source element ' +
            'margins',
        pageOffset.x - 10, pos.x);
    assertEquals(
        'Drag element position should be adjusted for source element ' +
            'margins',
        pageOffset.y - 14, pos.y);
  },


  testDragEndEvent() {
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    function testDragEndEventInternal(shouldContainItemData) {
      const testGroup = new AbstractDragDrop();

      const childEl = document.getElementById('child1');
      const item = new DragDropItem(childEl);
      /** @suppress {visibility} suppression added to enable type checking */
      item.currentDragElement_ = childEl;

      testGroup.items_.push(item);
      testGroup.recalculateDragTargets();

      // Simulate starting a drag
      const startEvent = {
        'clientX': 0,
        'clientY': 0,
        'type': EventType.MOUSEMOVE,
        'relatedTarget': childEl,
        'preventDefault': function() {}
      };
      testGroup.startDrag(startEvent, item);

      /**
       * @suppress {visibility,checkTypes} suppression added to enable type
       * checking
       */
      testGroup.activeTarget_ =
          new ActiveDropTarget(new Box(0, 0, 0, 0), testGroup, item, childEl);

      events.listen(
          testGroup, AbstractDragDrop.EventType.DRAGEND, function(event) {
            if (shouldContainItemData) {
              assertEquals(
                  'The drag end event should contain a drop target', testGroup,
                  event.dropTarget);
              assertEquals(
                  'The drag end event should contain a drop target item', item,
                  event.dropTargetItem);
              assertEquals(
                  'The drag end event should contain a drop target element',
                  childEl, event.dropTargetElement);
            } else {
              assertUndefined(
                  'The drag end event shouldn\'t contain a drop target',
                  event.dropTarget);
              assertUndefined(
                  'The drag end event shouldn\'t contain a drop target item',
                  event.dropTargetItem);
              assertUndefined(
                  'The drag end event shouldn\'t contain a drop target element',
                  event.dropTargetElement);
            }
          });

      testGroup.endDrag(
          {'clientX': 0, 'clientY': 0, 'dragCanceled': !shouldContainItemData});

      testGroup.dispose();
      item.dispose();
    }

    testDragEndEventInternal(false);
    testDragEndEventInternal(true);
  },


  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testDropEventHasBrowserEvent() {
    const testGroup = new AbstractDragDrop();

    const childEl = document.getElementById('child1');
    const item = new DragDropItem(childEl);
    /** @suppress {visibility} suppression added to enable type checking */
    item.currentDragElement_ = childEl;

    testGroup.items_.push(item);
    testGroup.recalculateDragTargets();

    // Simulate starting a drag
    const startBrowserEvent = {
      'clientX': 0,
      'clientY': 0,
      'type': EventType.MOUSEMOVE,
      'relatedTarget': childEl,
      'preventDefault': function() {},
    };
    testGroup.startDrag(startBrowserEvent, item);

    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    testGroup.activeTarget_ =
        new ActiveDropTarget(new Box(0, 0, 0, 0), testGroup, item, childEl);

    const endBrowserEvent = {
      'clientX': 0,
      'clientY': 0,
      'type': EventType.MOUSEUP,
      'ctrlKey': false,
      'altKey': true
    };

    events.listen(testGroup, AbstractDragDrop.EventType.DROP, function(event) {
      const browserEvent = event.browserEvent;
      assertEquals(
          'The drop event should contain the browser event', endBrowserEvent,
          browserEvent);
    });

    testGroup.endDrag({
      'clientX': 0,
      'clientY': 0,
      'dragCanceled': false,
      'browserEvent': endBrowserEvent
    });

    testGroup.dispose();
    item.dispose();
  },

});
