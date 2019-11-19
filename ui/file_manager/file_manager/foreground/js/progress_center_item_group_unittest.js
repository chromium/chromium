// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Mock LoadTimeData strings.
window.loadTimeData.data = {
  COPY_PROGRESS_SUMMARY: 'Copying...',
  ERROR_PROGRESS_SUMMARY: '1 Error.',
  ERROR_PROGRESS_SUMMARY_PLURAL: '$1 Errors.'
};
window.loadTimeData.getString = id => {
  return window.loadTimeData.data_[id] || id;
};

function testSimpleProgress() {
  const group =
      new ProgressCenterItemGroup(/* name */ 'test', /* quiet */ false);
  assertEquals(ProgressCenterItemGroup.State.EMPTY, group.state);

  const item = new ProgressCenterItem();
  item.id = 'test-item-1';
  item.message = 'TestItemMessage1';
  item.state = ProgressItemState.PROGRESSING;
  item.progressMax = 1.0;

  // Add an item.
  group.update(item);
  assertFalse(group.isAnimated(item.id));
  assertFalse(group.isSummarizedAnimated());
  assertEquals('TestItemMessage1', group.getItem(item.id).message);
  assertEquals('TestItemMessage1', group.getSummarizedItem(0).message);
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // Start an animation of the item.
  item.progressValue = 0.5;
  group.update(item);
  assertTrue(group.isAnimated(item.id));
  assertTrue(group.isSummarizedAnimated());
  assertEquals(0.5, group.getItem(item.id).progressValue);
  assertEquals(0.5, group.getSummarizedItem(0).progressValue);
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // Item is completed, but the animation is still on going.
  item.progressValue = 1.0;
  item.state = ProgressItemState.COMPLETED;
  group.update(item);
  assertTrue(group.isAnimated(item.id));
  assertTrue(group.isSummarizedAnimated());
  assertEquals(100, group.getItem(item.id).progressRateInPercent);
  assertEquals(100, group.getSummarizedItem(0).progressRateInPercent);
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // The animation of the item is completed.
  group.completeItemAnimation(item.id);
  assertFalse(group.isAnimated(item.id));
  assertTrue(group.isSummarizedAnimated());
  assertEquals(null, group.getItem(item.id));
  assertTrue(!!group.getSummarizedItem(0));
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // The animation of the summarized item is completed.
  group.completeSummarizedItemAnimation();
  assertEquals(ProgressCenterItemGroup.State.EMPTY, group.state);
}

function testCompleteAnimationDuringProgress() {
  const group =
      new ProgressCenterItemGroup(/* name */ 'test', /* quiet */ false);
  const item = new ProgressCenterItem();
  item.id = 'test-item-1';
  item.message = 'TestItemMessage1';
  item.state = ProgressItemState.PROGRESSING;
  item.progressMax = 1.0;

  // Add an item.
  group.update(item);
  assertFalse(group.isAnimated(item.id));
  assertFalse(group.isSummarizedAnimated());
  assertEquals('TestItemMessage1', group.getItem(item.id).message);
  assertEquals('TestItemMessage1', group.getSummarizedItem(0).message);
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // Start an animation of the item.
  item.progressValue = 0.5;
  group.update(item);
  assertTrue(group.isAnimated(item.id));
  assertTrue(group.isSummarizedAnimated());
  assertEquals(0.5, group.getItem(item.id).progressValue);
  assertEquals(0.5, group.getSummarizedItem(0).progressValue);
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // The animation of the item to 50% progress is completed.
  group.completeItemAnimation(item.id);
  assertFalse(group.isAnimated(item.id));
  assertTrue(group.isSummarizedAnimated());
  assertTrue(!!group.getItem(item.id));
  assertTrue(!!group.getSummarizedItem(0));
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // The animation of the summarized item to 50% progress is completed.
  group.completeSummarizedItemAnimation();
  assertFalse(group.isSummarizedAnimated());
  assertTrue(!!group.getSummarizedItem(0));
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // The item is completed. The animation to 100% progress starts.
  item.progressValue = 1.0;
  item.state = ProgressItemState.COMPLETED;
  group.update(item);
  assertTrue(group.isAnimated(item.id));
  assertTrue(group.isSummarizedAnimated());
  assertEquals(100, group.getItem(item.id).progressRateInPercent);
  assertEquals(100, group.getSummarizedItem(0).progressRateInPercent);
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // The animation of the summarized item to 100% progress is completed.
  group.completeSummarizedItemAnimation();
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // The animation of the item to 100% progress is completed.
  group.completeItemAnimation(item.id);
  assertFalse(group.isAnimated(item.id));
  assertFalse(group.isSummarizedAnimated());
  assertFalse(!!group.getItem(item.id));
  assertFalse(!!group.getSummarizedItem(0));
  assertEquals(ProgressCenterItemGroup.State.EMPTY, group.state);
}

function testAddMaxProgressItem() {
  const group =
      new ProgressCenterItemGroup(/* name */ 'test', /* quiet */ false);
  const item = new ProgressCenterItem();
  item.id = 'test-item-1';
  item.message = 'TestItemMessage1';
  item.state = ProgressItemState.PROGRESSING;
  item.progressMax = 1.0;
  item.progressValue = 1.0;

  // Add an item with 100% progress.
  group.update(item);
  assertFalse(group.isAnimated(item.id));
  assertFalse(group.isSummarizedAnimated());
  assertEquals('TestItemMessage1', group.getItem(item.id).message);
  assertEquals('TestItemMessage1', group.getSummarizedItem(0).message);
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // Complete the item without animation.
  item.state = ProgressItemState.COMPLETED;
  group.update(item);
  assertFalse(group.isAnimated(item.id));
  assertFalse(group.isSummarizedAnimated());
  assertEquals(null, group.getItem(item.id));
  assertEquals(null, group.getSummarizedItem(0));
  assertEquals(ProgressCenterItemGroup.State.EMPTY, group.state);
}

function testCompleteDuringAnimation() {
  const group =
      new ProgressCenterItemGroup(/* name */ 'test', /* quiet */ false);
  const item = new ProgressCenterItem();
  item.id = 'test-item-1';
  item.message = 'TestItemMessage1';
  item.state = ProgressItemState.PROGRESSING;
  item.progressMax = 1.0;
  item.progressValue = 0.0;

  // Add an item.
  group.update(item);
  assertFalse(group.isAnimated(item.id));
  assertFalse(group.isSummarizedAnimated());
  assertEquals('TestItemMessage1', group.getItem(item.id).message);
  assertEquals('TestItemMessage1', group.getSummarizedItem(0).message);
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // Update the progress of the item to 100%. The animation starts.
  item.progressValue = 1.0;
  group.update(item);
  assertTrue(group.isAnimated(item.id));
  assertTrue(group.isSummarizedAnimated());
  assertEquals('TestItemMessage1', group.getItem(item.id).message);
  assertEquals('TestItemMessage1', group.getSummarizedItem(0).message);
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // Complete the item. The animation is still on going.
  item.state = ProgressItemState.COMPLETED;
  group.update(item);
  assertTrue(group.isAnimated(item.id));
  assertTrue(group.isSummarizedAnimated());
  assertTrue(!!group.getItem(item.id));
  assertTrue(!!group.getSummarizedItem(0));
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);
}

function testTwoItems() {
  const group =
      new ProgressCenterItemGroup(/* name */ 'test', /* quiet */ false);
  const item1 = new ProgressCenterItem();
  item1.id = 'test-item-1';
  item1.message = 'TestItemMessage1';
  item1.state = ProgressItemState.PROGRESSING;
  item1.progressMax = 1.0;
  item1.progressValue = 0.0;
  item1.type = ProgressItemType.COPY;

  const item2 = new ProgressCenterItem();
  item2.id = 'test-item-2';
  item2.message = 'TestItemMessage2';
  item2.state = ProgressItemState.PROGRESSING;
  item2.progressMax = 2.0;
  item2.progressValue = 1.0;
  item2.type = ProgressItemType.COPY;

  // Item 1 is added.
  group.update(item1);
  assertFalse(group.isAnimated(item1.id));
  assertFalse(group.isSummarizedAnimated());
  assertEquals('TestItemMessage1', group.getItem(item1.id).message);
  assertEquals('TestItemMessage1', group.getSummarizedItem(0).message);
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // Item 2 is added.
  group.update(item2);
  assertFalse(group.isAnimated(item1.id));
  assertFalse(group.isAnimated(item2.id));
  assertTrue(group.isSummarizedAnimated());
  assertEquals('TestItemMessage1', group.getItem(item1.id).message);
  assertEquals('TestItemMessage2', group.getItem(item2.id).message);
  assertEquals('Copying...', group.getSummarizedItem(0).message);
  assertEquals('Copying... 1 Error.', group.getSummarizedItem(1).message);
  assertEquals('Copying... 2 Errors.', group.getSummarizedItem(2).message);
  assertEquals(3.0, group.getSummarizedItem(0).progressMax);
  assertEquals(1.0, group.getSummarizedItem(0).progressValue);
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // Item 1 is completed.
  item1.state = ProgressItemState.COMPLETED;
  item1.progressValue = 1.0;
  group.update(item1);
  assertTrue(group.isAnimated(item1.id));
  assertFalse(group.isAnimated(item2.id));
  assertTrue(group.isSummarizedAnimated());
  assertEquals('Copying...', group.getSummarizedItem(0).message);
  assertEquals(3.0, group.getSummarizedItem(0).progressMax);
  assertEquals(2.0, group.getSummarizedItem(0).progressValue);
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // Item 1's animation is completed.
  group.completeItemAnimation(item1.id);
  assertFalse(group.isAnimated(item1.id));
  assertTrue(group.isSummarizedAnimated());
  assertEquals('TestItemMessage2', group.getSummarizedItem(0).message);
  assertEquals(3.0, group.getSummarizedItem(0).progressMax);
  assertEquals(2.0, group.getSummarizedItem(0).progressValue);
  assertFalse(!!group.getItem(item1.id));
  assertTrue(!!group.getItem(item2.id));

  // Item 2 is completed.
  item2.state = ProgressItemState.COMPLETED;
  item2.progressValue = 2.0;
  group.update(item2);
  assertTrue(group.isSummarizedAnimated());
  assertEquals('TestItemMessage2', group.getSummarizedItem(0).message);
  assertEquals(ProgressItemState.COMPLETED, group.getSummarizedItem(0).state);
  assertEquals(3.0, group.getSummarizedItem(0).progressMax);
  assertEquals(3.0, group.getSummarizedItem(0).progressValue);
  assertTrue(!!group.getItem(item2.id));

  // Item 2's animation is completed.
  group.completeItemAnimation(item2.id);
  assertFalse(group.isAnimated(item2.id));
  assertFalse(!!group.getItem(item2.id));
  assertEquals(3.0, group.getSummarizedItem(0).progressMax);
  assertEquals(3.0, group.getSummarizedItem(0).progressValue);
  assertEquals(ProgressItemState.COMPLETED, group.getSummarizedItem(0).state);
  assertTrue(group.isSummarizedAnimated());
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // Summarized item's animation is completed.
  group.completeSummarizedItemAnimation();
  assertFalse(!!group.getSummarizedItem(0));
  assertFalse(group.isSummarizedAnimated());
  assertEquals(ProgressCenterItemGroup.State.EMPTY, group.state);
}

function testOneError() {
  const group =
      new ProgressCenterItemGroup(/* name */ 'test', /* quiet */ false);
  const item1 = new ProgressCenterItem();
  item1.id = 'test-item-1';
  item1.message = 'TestItemMessage1';
  item1.state = ProgressItemState.PROGRESSING;
  item1.progressMax = 2.0;
  item1.progressValue = 1.0;
  item1.type = ProgressItemType.COPY;

  // Item 1 is added.
  group.update(item1);

  // Item 1 becomes error.
  item1.message = 'Error.';
  item1.state = ProgressItemState.ERROR;
  group.update(item1);

  assertTrue(!!group.getItem(item1.id));
  assertFalse(group.isAnimated(item1.id));
  assertEquals(null, group.getSummarizedItem(0));
  assertFalse(group.isSummarizedAnimated());
  assertEquals(ProgressCenterItemGroup.State.INACTIVE, group.state);

  // Add another item without dismissing the error item.
  const item2 = new ProgressCenterItem();
  item2.id = 'test-item-2';
  item2.message = 'TestItemMessage2';
  item2.state = ProgressItemState.PROGRESSING;
  item2.progressMax = 4.0;
  item2.progressValue = 1.0;
  item2.type = ProgressItemType.COPY;

  // Item 2 is added.
  group.update(item2);

  assertTrue(!!group.getItem(item1.id));
  assertTrue(!!group.getItem(item2.id));
  assertEquals('Copying... 1 Error.', group.getSummarizedItem(0).message);
  assertEquals(4.0, group.getSummarizedItem(0).progressMax);
  assertEquals(1.0, group.getSummarizedItem(0).progressValue);
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // Dismiss the error item.
  group.dismissErrorItem(item1.id);

  assertFalse(!!group.getItem(item1.id));
  assertTrue(!!group.getItem(item2.id));
  assertEquals('TestItemMessage2', group.getSummarizedItem(0).message);
  assertEquals(4.0, group.getSummarizedItem(0).progressMax);
  assertEquals(1.0, group.getSummarizedItem(0).progressValue);
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);
}

function testOneItemWithError() {
  const group =
      new ProgressCenterItemGroup(/* name */ 'test', /* quiet */ false);
  const item1 = new ProgressCenterItem();
  item1.id = 'test-item-1';
  item1.message = 'TestItemMessage1';
  item1.state = ProgressItemState.PROGRESSING;
  item1.progressMax = 1.0;
  item1.progressValue = 0.0;
  item1.type = ProgressItemType.COPY;

  const item2 = new ProgressCenterItem();
  item2.id = 'test-item-2';
  item2.message = 'TestItemMessage2';
  item2.state = ProgressItemState.PROGRESSING;
  item2.progressMax = 2.0;
  item2.progressValue = 1.0;
  item2.type = ProgressItemType.COPY;

  // Item 1 is added.
  group.update(item1);

  // Item 2 is added.
  group.update(item2);

  // Item 2 becomes error.
  item2.state = ProgressItemState.ERROR;
  item2.message = 'Error message.';
  group.update(item2);
  assertFalse(group.isAnimated(item1.id));
  assertFalse(group.isAnimated(item2.id));
  assertFalse(group.isSummarizedAnimated());
  assertEquals('Copying... 1 Error.', group.getSummarizedItem(0).message);
  assertEquals('Copying... 2 Errors.', group.getSummarizedItem(1).message);
  assertEquals(1.0, group.getSummarizedItem(0).progressMax);
  assertEquals(0.0, group.getSummarizedItem(0).progressValue);
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // Item 1 is completed.
  item1.state = ProgressItemState.COMPLETED;
  item1.progressValue = 1.0;
  group.update(item1);
  assertTrue(group.isAnimated(item1.id));
  assertFalse(group.isAnimated(item2.id));
  assertTrue(group.isSummarizedAnimated());
  assertEquals('Copying... 1 Error.', group.getSummarizedItem(0).message);
  assertEquals(1.0, group.getSummarizedItem(0).progressMax);
  assertEquals(1.0, group.getSummarizedItem(0).progressValue);
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // Item 1's animation is completed.
  group.completeItemAnimation(item1.id);
  // Summarized item's animation is completed.
  group.completeSummarizedItemAnimation();

  assertFalse(group.isAnimated(item1.id));
  assertFalse(group.isSummarizedAnimated());
  assertFalse(!!group.getSummarizedItem(0));
  assertEquals(
      'Error message.',
      ProgressCenterItemGroup.getSummarizedErrorItem(group).message);
  assertEquals(
      '2 Errors.',
      ProgressCenterItemGroup.getSummarizedErrorItem(group, group).message);
  assertFalse(!!group.getItem(item1.id));
  assertTrue(!!group.getItem(item2.id));

  assertEquals(
      'Error message.',
      ProgressCenterItemGroup.getSummarizedErrorItem(group).message);
  assertFalse(group.isSummarizedAnimated());
  assertEquals(ProgressCenterItemGroup.State.INACTIVE, group.state);

  // Dismiss error item.
  group.dismissErrorItem(item2.id);
  assertFalse(!!group.getItem(item2.id));
  assertEquals(ProgressCenterItemGroup.State.EMPTY, group.state);
}

function testOneItemWithErrorDuringAnimation() {
  const group =
      new ProgressCenterItemGroup(/* name */ 'test', /* quiet */ false);
  const item1 = new ProgressCenterItem();
  item1.id = 'test-item-1';
  item1.message = 'TestItemMessage1';
  item1.state = ProgressItemState.PROGRESSING;
  item1.progressMax = 1.0;
  item1.progressValue = 0.0;
  item1.type = ProgressItemType.COPY;

  const item2 = new ProgressCenterItem();
  item2.id = 'test-item-2';
  item2.message = 'TestItemMessage2';
  item2.state = ProgressItemState.PROGRESSING;
  item2.progressMax = 2.0;
  item2.progressValue = 1.0;
  item2.type = ProgressItemType.COPY;

  // Item 1 is added.
  group.update(item1);

  // Item 2 is added.
  group.update(item2);

  // Item 2 starts an animation.
  item2.progressValue = 1.5;
  group.update(item2);
  assertTrue(group.isAnimated(item2.id));
  assertTrue(group.isSummarizedAnimated());

  // Item 2 enters the error state.
  item2.state = ProgressItemState.ERROR;
  item2.message = 'Error message.';
  group.update(item2);
  assertFalse(group.isAnimated(item2.id));
  assertFalse(group.isSummarizedAnimated());
  assertEquals('Copying... 1 Error.', group.getSummarizedItem(0).message);
  assertEquals('Copying... 2 Errors.', group.getSummarizedItem(1).message);
  assertEquals(1.0, group.getSummarizedItem(0).progressMax);
  assertEquals(0.0, group.getSummarizedItem(0).progressValue);
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);
}

function testTwoErrors() {
  const group =
      new ProgressCenterItemGroup(/* name */ 'test', /* quiet */ false);
  const item1 = new ProgressCenterItem();
  item1.id = 'test-item-1';
  item1.message = 'Error message 1';
  item1.state = ProgressItemState.ERROR;
  item1.type = ProgressItemType.COPY;

  const item2 = new ProgressCenterItem();
  item2.id = 'test-item-2';
  item2.message = 'Error message 2';
  item2.state = ProgressItemState.ERROR;
  item2.type = ProgressItemType.COPY;

  // Add an error item.
  group.update(item1);
  assertFalse(group.isAnimated(item1.id));
  assertFalse(group.isSummarizedAnimated());
  assertEquals(
      'Error message 1',
      ProgressCenterItemGroup.getSummarizedErrorItem(group).message);
  assertEquals(ProgressCenterItemGroup.State.INACTIVE, group.state);

  // Add another error item.
  group.update(item2);
  assertTrue(!!group.getItem(item1.id));
  assertTrue(!!group.getItem(item2.id));
  assertFalse(group.isAnimated(item2.id));
  assertFalse(group.isSummarizedAnimated());
  assertEquals(
      '2 Errors.',
      ProgressCenterItemGroup.getSummarizedErrorItem(group).message);
  assertEquals(ProgressCenterItemGroup.State.INACTIVE, group.state);

  // Dismiss Error message 1.
  group.dismissErrorItem(item1.id);

  assertFalse(!!group.getItem(item1.id));
  assertTrue(!!group.getItem(item2.id));
  assertEquals(
      'Error message 2',
      ProgressCenterItemGroup.getSummarizedErrorItem(group).message);
  assertEquals(ProgressCenterItemGroup.State.INACTIVE, group.state);
}

function testCancel() {
  const group =
      new ProgressCenterItemGroup(/* name */ 'test', /* quiet */ false);
  const item = new ProgressCenterItem();
  item.id = 'test-item-1';
  item.message = 'TestItemMessage1';
  item.state = ProgressItemState.PROGRESSING;
  item.progressMax = 1.0;
  item.progressValue = 0.0;
  item.type = ProgressItemType.COPY;

  // Add an item.
  group.update(item);

  // Start an animation of the item.
  item.progressValue = 0.5;
  group.update(item);

  // Cancel the item.
  item.state = ProgressItemState.CANCELED;
  group.update(item);
  assertFalse(group.isAnimated(item.id));
  assertFalse(group.isSummarizedAnimated());
  assertEquals(null, group.getItem(item.id));
  assertEquals(null, group.getSummarizedItem(0));
  assertEquals(ProgressCenterItemGroup.State.EMPTY, group.state);
}

function testCancelWithError() {
  const group =
      new ProgressCenterItemGroup(/* name */ 'test', /* quiet */ false);
  const item1 = new ProgressCenterItem();
  item1.id = 'test-item-1';
  item1.message = 'TestItemMessage1';
  item1.state = ProgressItemState.PROGRESSING;
  item1.progressMax = 1.0;
  item1.progressValue = 0.0;
  item1.type = ProgressItemType.COPY;

  const item2 = new ProgressCenterItem();
  item2.id = 'test-item-2';
  item2.message = 'Error message 2';
  item2.state = ProgressItemState.ERROR;
  item2.type = ProgressItemType.COPY;

  // Add an item.
  group.update(item1);

  // Start an animation of the item.
  item1.progressValue = 0.5;
  group.update(item1);

  // Add an error item.
  group.update(item2);

  // Cancel the item.
  item1.state = ProgressItemState.CANCELED;
  group.update(item1);
  assertFalse(group.isAnimated(item1.id));
  assertFalse(group.isAnimated(item2.id));
  assertFalse(group.isSummarizedAnimated());
  assertEquals(null, group.getItem(item1.id));
  assertTrue(!!group.getItem(item2.id));
  assertEquals(null, group.getSummarizedItem(0));
  assertEquals(
      'Error message 2',
      ProgressCenterItemGroup.getSummarizedErrorItem(group).message);
  assertEquals(ProgressCenterItemGroup.State.INACTIVE, group.state);
}

function testQuietItem() {
  const group =
      new ProgressCenterItemGroup(/* name */ 'test', /* quiet */ true);
  const item = new ProgressCenterItem();
  item.id = 'test-item-1';
  item.message = 'TestItemMessage1';
  item.state = ProgressItemState.PROGRESSING;
  item.progressMax = 1.0;
  item.quiet = true;

  // Add an item.
  group.update(item);
  assertFalse(group.isAnimated(item.id));
  assertFalse(group.isSummarizedAnimated());
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // Start an animation of the item.
  item.progressValue = 0.5;
  group.update(item);
  assertTrue(group.isAnimated(item.id));
  // Summarized item should not animated because the panel does not show
  // progress bar for quiet and summarized item.
  assertFalse(group.isSummarizedAnimated());
  assertEquals(0.5, group.getItem(item.id).progressValue);
  assertEquals(0.5, group.getSummarizedItem(0).progressValue);
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // Item is completed, but the animation is still on going.
  item.progressValue = 1.0;
  item.state = ProgressItemState.COMPLETED;
  group.update(item);
  assertTrue(group.isAnimated(item.id));
  assertFalse(group.isSummarizedAnimated());
  assertEquals(100, group.getItem(item.id).progressRateInPercent);
  assertEquals(100, group.getSummarizedItem(0).progressRateInPercent);
  assertEquals(ProgressCenterItemGroup.State.ACTIVE, group.state);

  // The animation of the item is completed.
  group.completeItemAnimation(item.id);
  assertFalse(group.isAnimated(item.id));
  assertFalse(group.isSummarizedAnimated());
  assertEquals(null, group.getItem(item.id));
  assertFalse(!!group.getSummarizedItem(0));
  assertEquals(ProgressCenterItemGroup.State.EMPTY, group.state);
}
