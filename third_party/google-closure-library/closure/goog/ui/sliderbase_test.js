/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

// TODO(user): We're trying to migrate all ES5 subclasses of Closure
// Library to ES6. In ES6 this cannot be referenced before super is called. This
// file has at least one this before a super call (in ES5) and cannot be
// automatically upgraded to ES6 as a result. Please fix this if you have a
// chance. Note: This can sometimes be caused by not calling the super
// constructor at all. You can run the conversion tool yourself to see what it
// does on this file: blaze run //javascript/refactoring/es6_classes:convert.

goog.module('goog.ui.SliderBaseTest');
goog.setTestOnly();

const Animation = goog.require('goog.fx.Animation');
const Component = goog.require('goog.ui.Component');
const Coordinate = goog.require('goog.math.Coordinate');
const EventType = goog.require('goog.events.EventType');
const KeyCodes = goog.require('goog.events.KeyCodes');
const MockClock = goog.require('goog.testing.MockClock');
const MockControl = goog.require('goog.testing.MockControl');
const SliderBase = goog.require('goog.ui.SliderBase');
const State = goog.require('goog.a11y.aria.State');
const TagName = goog.require('goog.dom.TagName');
const aria = goog.require('goog.a11y.aria');
const bidi = goog.require('goog.style.bidi');
const classlist = goog.require('goog.dom.classlist');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const mockmatchers = goog.require('goog.testing.mockmatchers');
const recordFunction = goog.require('goog.testing.recordFunction');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');
const userAgent = goog.require('goog.userAgent');

let oneThumbSlider;
let oneThumbSliderRtl;
let oneChangeEventCount;

let twoThumbSlider;
let twoThumbSliderRtl;
let twoChangeEventCount;

let mockClock;
let mockAnimation;

/** A basic class to implement the abstract SliderBase for testing. */
class OneThumbSlider extends SliderBase {
  /**
   * @param {boolean=} testOnlyIsRightToLeft This parameter is necessary to
   *     tell if the slider is rendered right-to-left when creating thumbs
   *     (before entering the document). Used only for test purposes.
   */
  constructor(testOnlyIsRightToLeft = false) {
    super();
    this.testOnlyIsRightToLeft_ = testOnlyIsRightToLeft;
    SliderBase.call(
        this, undefined /* domHelper */,
        (value) => value > 5 ? 'A big value.' : 'A small value.');
  }

  /** @override */
  createThumbs() {
    const dirSuffix = this.testOnlyIsRightToLeft_ ? 'Rtl' : '';
    /** @suppress {checkTypes} suppression added to enable type checking */
    this.valueThumb = this.extentThumb = dom.getElement(`thumb${dirSuffix}`);
  }

  /** @override */
  getCssClass(orientation) {
    // Avoid compiler check on getCssName parameters
    return goog['getCssName']('test-slider', orientation);
  }
}

/** A basic class to implement the abstract SliderBase for testing. */
class TwoThumbSlider extends SliderBase {
  /**
   * @param {boolean=} testOnlyIsRightToLeft This parameter is necessary to
   *     tell if the slider is rendered right-to-left when creating thumbs
   *     (before entering the document). Used only for test purposes.
   */
  constructor(testOnlyIsRightToLeft = false) {
    super();
    this.testOnlyIsRightToLeft_ = testOnlyIsRightToLeft;
    SliderBase.call(this);
  }

  /** @override */
  createThumbs() {
    const dirSuffix = this.testOnlyIsRightToLeft_ ? 'Rtl' : '';
    /** @suppress {checkTypes} suppression added to enable type checking */
    this.valueThumb = dom.getElement(`valueThumb${dirSuffix}`);
    /** @suppress {checkTypes} suppression added to enable type checking */
    this.extentThumb = dom.getElement(`extentThumb${dirSuffix}`);
    /** @suppress {checkTypes} suppression added to enable type checking */
    this.rangeHighlight = dom.getElement(`rangeHighlight${dirSuffix}`);
  }

  /** @override */
  getCssClass(orientation) {
    // Avoid compiler check on getCssName parameters
    return goog['getCssName']('test-slider', orientation);
  }
}

/**
 * Basic class that implements the AnimationFactory interface for testing.
 * @implements {SliderBase.AnimationFactory}
 */
class AnimationFactory {
  /**
   * @param {!Animation|!Array<!Animation>} testAnimations The test animations
   *     to use.
   */
  constructor(testAnimations) {
    this.testAnimations = testAnimations;
  }

  /**
   * @override
   * @suppress {checkTypes} suppression added to enable type checking
   */
  createAnimations() {
    return this.testAnimations;
  }
}

/**
 * Verifies that rangeHighlight position and size are correct for the given
 * startValue and endValue. Assumes slider has default min/max values [0, 100],
 * width of 1020px, and thumb widths of 20px, with rangeHighlight drawn from
 * the centers of the thumbs.
 * @param {number} rangeHighlight The range highlight.
 * @param {number} startValue The start value.
 * @param {number} endValue The end value.
 * @suppress {missingProperties} suppression added to enable type checking
 */
function assertHighlightedRange(rangeHighlight, startValue, endValue) {
  const rangeStr = `[${startValue}, ${endValue}]`;
  const rangeStart = 10 + 10 * startValue;
  assertEquals(
      `Range highlight for ${rangeStr} should start at ${rangeStart}` +
          'px.',
      rangeStart, rangeHighlight.offsetLeft);
  const rangeSize = 10 * (endValue - startValue);
  assertEquals(
      `Range highlight for ${rangeStr} should have size ${rangeSize}` +
          'px.',
      rangeSize, rangeHighlight.offsetWidth);
}

testSuite({
  setUp() {
    const sandBox = dom.getElement('sandbox');
    mockClock = new MockClock(true);

    const oneThumbElem = dom.createDom(
        TagName.DIV, {'id': 'oneThumbSlider'},
        dom.createDom(TagName.SPAN, {'id': 'thumb'}));
    sandBox.appendChild(oneThumbElem);
    oneThumbSlider = new OneThumbSlider();
    oneThumbSlider.decorate(oneThumbElem);
    oneChangeEventCount = 0;
    events.listen(oneThumbSlider, Component.EventType.CHANGE, () => {
      oneChangeEventCount++;
    });

    const twoThumbElem = dom.createDom(
        TagName.DIV, {'id': 'twoThumbSlider'},
        dom.createDom(TagName.DIV, {'id': 'rangeHighlight'}),
        dom.createDom(TagName.SPAN, {'id': 'valueThumb'}),
        dom.createDom(TagName.SPAN, {'id': 'extentThumb'}));
    sandBox.appendChild(twoThumbElem);
    twoThumbSlider = new TwoThumbSlider();
    twoThumbSlider.decorate(twoThumbElem);
    twoChangeEventCount = 0;
    events.listen(twoThumbSlider, Component.EventType.CHANGE, () => {
      twoChangeEventCount++;
    });

    const sandBoxRtl = dom.createDom(
        TagName.DIV, {'dir': 'rtl', 'style': 'position:absolute;'});
    sandBox.appendChild(sandBoxRtl);

    const oneThumbElemRtl = dom.createDom(
        TagName.DIV, {'id': 'oneThumbSliderRtl'},
        dom.createDom(TagName.SPAN, {'id': 'thumbRtl'}));
    sandBoxRtl.appendChild(oneThumbElemRtl);
    oneThumbSliderRtl = new OneThumbSlider(true /* testOnlyIsRightToLeft */);
    oneThumbSliderRtl.enableFlipForRtl(true);
    oneThumbSliderRtl.decorate(oneThumbElemRtl);
    events.listen(oneThumbSliderRtl, Component.EventType.CHANGE, () => {
      oneChangeEventCount++;
    });

    const twoThumbElemRtl = dom.createDom(
        TagName.DIV, {'id': 'twoThumbSliderRtl'},
        dom.createDom(TagName.DIV, {'id': 'rangeHighlightRtl'}),
        dom.createDom(TagName.SPAN, {'id': 'valueThumbRtl'}),
        dom.createDom(TagName.SPAN, {'id': 'extentThumbRtl'}));
    sandBoxRtl.appendChild(twoThumbElemRtl);
    twoThumbSliderRtl = new TwoThumbSlider(true /* testOnlyIsRightToLeft */);
    twoThumbSliderRtl.enableFlipForRtl(true);
    twoThumbSliderRtl.decorate(twoThumbElemRtl);
    twoChangeEventCount = 0;
    events.listen(twoThumbSliderRtl, Component.EventType.CHANGE, () => {
      twoChangeEventCount++;
    });
  },

  tearDown() {
    oneThumbSlider.dispose();
    twoThumbSlider.dispose();
    oneThumbSliderRtl.dispose();
    twoThumbSliderRtl.dispose();
    mockClock.dispose();
    dom.removeChildren(dom.getElement('sandbox'));
  },

  testGetAndSetValue() {
    oneThumbSlider.setValue(30);
    assertEquals(30, oneThumbSlider.getValue());
    assertEquals(
        'Setting valid value must dispatch only a single change event.', 1,
        oneChangeEventCount);

    oneThumbSlider.setValue(30);
    assertEquals(30, oneThumbSlider.getValue());
    assertEquals(
        'Setting to same value must not dispatch change event.', 1,
        oneChangeEventCount);

    oneThumbSlider.setValue(-30);
    assertEquals(
        'Setting invalid value must not change value.', 30,
        oneThumbSlider.getValue());
    assertEquals(
        'Setting invalid value must not dispatch change event.', 1,
        oneChangeEventCount);

    // Value thumb can't go past extent thumb, so we must move that first to
    // allow setting value.
    twoThumbSlider.setExtent(70);
    twoChangeEventCount = 0;
    twoThumbSlider.setValue(60);
    assertEquals(60, twoThumbSlider.getValue());
    assertEquals(
        'Setting valid value must dispatch only a single change event.', 1,
        twoChangeEventCount);

    twoThumbSlider.setValue(60);
    assertEquals(60, twoThumbSlider.getValue());
    assertEquals(
        'Setting to same value must not dispatch change event.', 1,
        twoChangeEventCount);

    twoThumbSlider.setValue(-60);
    assertEquals(
        'Setting invalid value must not change value.', 60,
        twoThumbSlider.getValue());
    assertEquals(
        'Setting invalid value must not dispatch change event.', 1,
        twoChangeEventCount);
  },

  testGetAndSetValueRtl() {
    const thumbElement = dom.getElement('thumbRtl');
    assertEquals(0, bidi.getOffsetStart(thumbElement));
    assertEquals('', thumbElement.style.left);
    assertEquals('0px', thumbElement.style.right);

    oneThumbSliderRtl.setValue(30);
    assertEquals(30, oneThumbSliderRtl.getValue());
    assertEquals(
        'Setting valid value must dispatch only a single change event.', 1,
        oneChangeEventCount);

    assertEquals('', thumbElement.style.left);
    assertEquals('294px', thumbElement.style.right);

    oneThumbSliderRtl.setValue(30);
    assertEquals(30, oneThumbSliderRtl.getValue());
    assertEquals(
        'Setting to same value must not dispatch change event.', 1,
        oneChangeEventCount);

    oneThumbSliderRtl.setValue(-30);
    assertEquals(
        'Setting invalid value must not change value.', 30,
        oneThumbSliderRtl.getValue());
    assertEquals(
        'Setting invalid value must not dispatch change event.', 1,
        oneChangeEventCount);

    // Value thumb can't go past extent thumb, so we must move that first to
    // allow setting value.
    const valueThumbElement = dom.getElement('valueThumbRtl');
    const extentThumbElement = dom.getElement('extentThumbRtl');
    assertEquals(0, bidi.getOffsetStart(valueThumbElement));
    assertEquals(0, bidi.getOffsetStart(extentThumbElement));
    assertEquals('', valueThumbElement.style.left);
    assertEquals('0px', valueThumbElement.style.right);
    assertEquals('', extentThumbElement.style.left);
    assertEquals('0px', extentThumbElement.style.right);

    twoThumbSliderRtl.setExtent(70);
    twoChangeEventCount = 0;
    twoThumbSliderRtl.setValue(60);
    assertEquals(60, twoThumbSliderRtl.getValue());
    assertEquals(
        'Setting valid value must dispatch only a single change event.', 1,
        twoChangeEventCount);

    twoThumbSliderRtl.setValue(60);
    assertEquals(60, twoThumbSliderRtl.getValue());
    assertEquals(
        'Setting to same value must not dispatch change event.', 1,
        twoChangeEventCount);

    assertEquals('', valueThumbElement.style.left);
    assertEquals('600px', valueThumbElement.style.right);
    assertEquals('', extentThumbElement.style.left);
    assertEquals('700px', extentThumbElement.style.right);

    twoThumbSliderRtl.setValue(-60);
    assertEquals(
        'Setting invalid value must not change value.', 60,
        twoThumbSliderRtl.getValue());
    assertEquals(
        'Setting invalid value must not dispatch change event.', 1,
        twoChangeEventCount);
  },

  testGetAndSetExtent() {
    // Note(user): With a one thumb slider the API only really makes sense if
    // you always use setValue since there is no extent.

    twoThumbSlider.setExtent(7);
    assertEquals(7, twoThumbSlider.getExtent());
    assertEquals(
        'Setting valid value must dispatch only a single change event.', 1,
        twoChangeEventCount);

    twoThumbSlider.setExtent(7);
    assertEquals(7, twoThumbSlider.getExtent());
    assertEquals(
        'Setting to same value must not dispatch change event.', 1,
        twoChangeEventCount);

    twoThumbSlider.setExtent(-7);
    assertEquals(
        'Setting invalid value must not change value.', 7,
        twoThumbSlider.getExtent());
    assertEquals(
        'Setting invalid value must not dispatch change event.', 1,
        twoChangeEventCount);
  },

  testUpdateValueExtent() {
    twoThumbSlider.setValueAndExtent(30, 50);

    assertNotNull(twoThumbSlider.getElement());
    assertEquals(
        'Setting value results in updating aria-valuenow', '30',
        aria.getState(twoThumbSlider.getElement(), State.VALUENOW));
    assertEquals(30, twoThumbSlider.getValue());
    assertEquals(50, twoThumbSlider.getExtent());
  },

  testValueText() {
    oneThumbSlider.setValue(10);
    assertEquals(
        'Setting value results in correct aria-valuetext', 'A big value.',
        aria.getState(oneThumbSlider.getElement(), State.VALUETEXT));
    oneThumbSlider.setValue(2);
    assertEquals(
        'Updating value results in updated aria-valuetext', 'A small value.',
        aria.getState(oneThumbSlider.getElement(), State.VALUETEXT));
  },

  testGetValueText() {
    oneThumbSlider.setValue(10);
    assertEquals(
        'Getting the text value gets the correct description', 'A big value.',
        oneThumbSlider.getTextValue());
    oneThumbSlider.setValue(2);
    assertEquals(
        'Getting the updated text value gets the correct updated description',
        'A small value.', oneThumbSlider.getTextValue());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRangeListener() {
    const slider = new SliderBase;
    /** @suppress {visibility} suppression added to enable type checking */
    slider.updateUi_ = slider.updateAriaStates = () => {};
    slider.rangeModel.setValue(0);

    const f = recordFunction();
    events.listen(slider, Component.EventType.CHANGE, f);

    slider.rangeModel.setValue(50);
    assertEquals(1, f.getCallCount());

    slider.exitDocument();
    slider.rangeModel.setValue(0);
    assertEquals(
        'The range model listener should not have been removed so we ' +
            'should have gotten a second event dispatch',
        2, f.getCallCount());
  },

  testKeyHandlingTests() {
    twoThumbSlider.setValue(0);
    twoThumbSlider.setExtent(100);
    assertEquals(0, twoThumbSlider.getValue());
    assertEquals(100, twoThumbSlider.getExtent());

    testingEvents.fireKeySequence(twoThumbSlider.getElement(), KeyCodes.RIGHT);
    assertEquals(1, twoThumbSlider.getValue());
    assertEquals(99, twoThumbSlider.getExtent());

    testingEvents.fireKeySequence(twoThumbSlider.getElement(), KeyCodes.RIGHT);
    assertEquals(2, twoThumbSlider.getValue());
    assertEquals(98, twoThumbSlider.getExtent());

    testingEvents.fireKeySequence(twoThumbSlider.getElement(), KeyCodes.LEFT);
    assertEquals(1, twoThumbSlider.getValue());
    assertEquals(98, twoThumbSlider.getExtent());

    testingEvents.fireKeySequence(twoThumbSlider.getElement(), KeyCodes.LEFT);
    assertEquals(0, twoThumbSlider.getValue());
    assertEquals(98, twoThumbSlider.getExtent());

    testingEvents.fireKeySequence(
        twoThumbSlider.getElement(), KeyCodes.RIGHT, {shiftKey: true});
    assertEquals(10, twoThumbSlider.getValue());
    assertEquals(90, twoThumbSlider.getExtent());

    testingEvents.fireKeySequence(
        twoThumbSlider.getElement(), KeyCodes.RIGHT, {shiftKey: true});
    assertEquals(20, twoThumbSlider.getValue());
    assertEquals(80, twoThumbSlider.getExtent());

    testingEvents.fireKeySequence(
        twoThumbSlider.getElement(), KeyCodes.LEFT, {shiftKey: true});
    assertEquals(10, twoThumbSlider.getValue());
    assertEquals(80, twoThumbSlider.getExtent());

    testingEvents.fireKeySequence(
        twoThumbSlider.getElement(), KeyCodes.LEFT, {shiftKey: true});
    assertEquals(0, twoThumbSlider.getValue());
    assertEquals(80, twoThumbSlider.getExtent());
  },

  testKeyHandlingLargeStepSize() {
    twoThumbSlider.setValue(0);
    twoThumbSlider.setExtent(100);
    twoThumbSlider.setStep(5);
    assertEquals(0, twoThumbSlider.getValue());
    assertEquals(100, twoThumbSlider.getExtent());

    testingEvents.fireKeySequence(twoThumbSlider.getElement(), KeyCodes.RIGHT);
    assertEquals(5, twoThumbSlider.getValue());
    assertEquals(95, twoThumbSlider.getExtent());

    testingEvents.fireKeySequence(twoThumbSlider.getElement(), KeyCodes.RIGHT);
    assertEquals(10, twoThumbSlider.getValue());
    assertEquals(90, twoThumbSlider.getExtent());

    testingEvents.fireKeySequence(twoThumbSlider.getElement(), KeyCodes.LEFT);
    assertEquals(5, twoThumbSlider.getValue());
    assertEquals(90, twoThumbSlider.getExtent());

    testingEvents.fireKeySequence(twoThumbSlider.getElement(), KeyCodes.LEFT);
    assertEquals(0, twoThumbSlider.getValue());
    assertEquals(90, twoThumbSlider.getExtent());
  },

  testKeyHandlingRtl() {
    twoThumbSliderRtl.setValue(0);
    twoThumbSliderRtl.setExtent(100);
    assertEquals(0, twoThumbSliderRtl.getValue());
    assertEquals(100, twoThumbSliderRtl.getExtent());

    testingEvents.fireKeySequence(
        twoThumbSliderRtl.getElement(), KeyCodes.RIGHT);
    assertEquals(0, twoThumbSliderRtl.getValue());
    assertEquals(99, twoThumbSliderRtl.getExtent());

    testingEvents.fireKeySequence(
        twoThumbSliderRtl.getElement(), KeyCodes.RIGHT);
    assertEquals(0, twoThumbSliderRtl.getValue());
    assertEquals(98, twoThumbSliderRtl.getExtent());

    testingEvents.fireKeySequence(
        twoThumbSliderRtl.getElement(), KeyCodes.LEFT);
    assertEquals(1, twoThumbSliderRtl.getValue());
    assertEquals(98, twoThumbSliderRtl.getExtent());

    testingEvents.fireKeySequence(
        twoThumbSliderRtl.getElement(), KeyCodes.LEFT);
    assertEquals(2, twoThumbSliderRtl.getValue());
    assertEquals(98, twoThumbSliderRtl.getExtent());

    testingEvents.fireKeySequence(
        twoThumbSliderRtl.getElement(), KeyCodes.RIGHT, {shiftKey: true});
    assertEquals(0, twoThumbSliderRtl.getValue());
    assertEquals(90, twoThumbSliderRtl.getExtent());

    testingEvents.fireKeySequence(
        twoThumbSliderRtl.getElement(), KeyCodes.RIGHT, {shiftKey: true});
    assertEquals(0, twoThumbSliderRtl.getValue());
    assertEquals(80, twoThumbSliderRtl.getExtent());

    testingEvents.fireKeySequence(
        twoThumbSliderRtl.getElement(), KeyCodes.LEFT, {shiftKey: true});
    assertEquals(10, twoThumbSliderRtl.getValue());
    assertEquals(80, twoThumbSliderRtl.getExtent());

    testingEvents.fireKeySequence(
        twoThumbSliderRtl.getElement(), KeyCodes.LEFT, {shiftKey: true});
    assertEquals(20, twoThumbSliderRtl.getValue());
    assertEquals(80, twoThumbSliderRtl.getExtent());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testRangeHighlight() {
    const rangeHighlight = dom.getElement('rangeHighlight');

    // Test [0, 100]
    twoThumbSlider.setValue(0);
    twoThumbSlider.setExtent(100);
    assertHighlightedRange(rangeHighlight, 0, 100);

    // Test [25, 75]
    twoThumbSlider.setValue(25);
    twoThumbSlider.setExtent(50);
    assertHighlightedRange(rangeHighlight, 25, 75);

    // Test [50, 50]
    twoThumbSlider.setValue(50);
    twoThumbSlider.setExtent(0);
    assertHighlightedRange(rangeHighlight, 50, 50);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testRangeHighlightAnimation() {
    let animationDelay =
        160;  // Delay in ms, is a bit higher than actual delay.
    if (userAgent.IE) {
      // For some reason, (probably due to how timing works), IE7 and IE8
      // will not stop if we don't wait for it.
      animationDelay = 250;
    }

    const rangeHighlight = dom.getElement('rangeHighlight');
    twoThumbSlider.setValue(0);
    twoThumbSlider.setExtent(100);

    // Animate right thumb, final range is [0, 75]
    twoThumbSlider.animatedSetValue(75);
    assertHighlightedRange(rangeHighlight, 0, 100);
    mockClock.tick(animationDelay);
    assertHighlightedRange(rangeHighlight, 0, 75);

    // Animate left thumb, final range is [25, 75]
    twoThumbSlider.animatedSetValue(25);
    assertHighlightedRange(rangeHighlight, 0, 75);
    mockClock.tick(animationDelay);
    assertHighlightedRange(rangeHighlight, 25, 75);
  },

  /**
   * Verifies that no error occurs and that the range highlight is sized
   * correctly for a zero-size slider (i.e. doesn't attempt to set a
   * negative size). The test tries to resize the slider from its original
   * size to 0, then checks that the range highlight's size is correctly set
   * to 0. The size verification is needed because Webkit/Gecko outright
   * ignore calls to set negative sizes on an element, leaving it at its
   * former size. IE throws an error in the same situation.
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  testRangeHighlightForZeroSizeSlider() {
    // Make sure range highlight spans whole slider before zeroing width.
    twoThumbSlider.setExtent(100);
    twoThumbSlider.getElement().style.width = 0;

    // The setVisible call is used to force a UI update.
    twoThumbSlider.setVisible(true);
    assertEquals(
        'Range highlight size should be 0 when slider size is 0', 0,
        dom.getElement('rangeHighlight').offsetWidth);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testAnimatedSetValueAnimatesFactoryCreatedAnimations() {
    // Create and set the factory.
    const ignore = mockmatchers.ignoreArgument;
    const mockControl = new MockControl();
    const mockAnimation1 = mockControl.createLooseMock(Animation);
    const mockAnimation2 = mockControl.createLooseMock(Animation);
    const testAnimations = [mockAnimation1, mockAnimation2];
    oneThumbSlider.setAdditionalAnimations(
        new AnimationFactory(testAnimations));

    // Expect the animations to be played.
    mockAnimation1.play(false);
    mockAnimation2.play(false);
    mockAnimation1.addEventListener(ignore, ignore, ignore);
    mockAnimation2.addEventListener(ignore, ignore, ignore);

    // Animate and verify.
    mockControl.$replayAll();
    oneThumbSlider.animatedSetValue(50);
    mockControl.$verifyAll();
    mockControl.$resetAll();
    mockControl.$tearDown();
  },

  testMouseWheelEventHandlerEnable() {
    // Mouse wheel handling should be enabled by default.
    assertTrue(oneThumbSlider.isHandleMouseWheel());

    // Test disabling the mouse wheel handler
    oneThumbSlider.setHandleMouseWheel(false);
    assertFalse(oneThumbSlider.isHandleMouseWheel());

    // Test that enabling again works fine.
    oneThumbSlider.setHandleMouseWheel(true);
    assertTrue(oneThumbSlider.isHandleMouseWheel());

    // Test that mouse wheel handling can be disabled before rendering a
    // slider.
    const wheelDisabledElem =
        dom.createDom(TagName.DIV, {}, dom.createDom(TagName.SPAN));
    const wheelDisabledSlider = new OneThumbSlider();
    wheelDisabledSlider.setHandleMouseWheel(false);
    wheelDisabledSlider.decorate(wheelDisabledElem);
    assertFalse(wheelDisabledSlider.isHandleMouseWheel());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testDisabledAndEnabledSlider() {
    // Check that a slider is enabled by default
    assertTrue(oneThumbSlider.isEnabled());

    /** @suppress {visibility} suppression added to enable type checking */
    const listenerCount = oneThumbSlider.getHandler().getListenerCount();
    // Disable the slider and check its state
    oneThumbSlider.setEnabled(false);
    assertFalse(oneThumbSlider.isEnabled());
    assertTrue(classlist.contains(
        oneThumbSlider.getElement(), 'goog-slider-disabled'));
    assertEquals(0, oneThumbSlider.getHandler().getListenerCount());

    // setValue should work unaffected even when the slider is disabled.
    oneThumbSlider.setValue(30);
    assertEquals(30, oneThumbSlider.getValue());
    assertEquals(
        'Setting valid value must dispatch a change event ' +
            'even when slider is disabled.',
        1, oneChangeEventCount);

    // Test the transition from disabled to enabled
    oneThumbSlider.setEnabled(true);
    assertTrue(oneThumbSlider.isEnabled());
    assertFalse(classlist.contains(
        oneThumbSlider.getElement(), 'goog-slider-disabled'));
    assertTrue(listenerCount == oneThumbSlider.getHandler().getListenerCount());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testBlockIncrementingWithEnableAndDisabled() {
    const doc = dom.getOwnerDocument(oneThumbSlider.getElement());
    // Case when slider is not disabled between the mouse down and up
    // events.
    testingEvents.fireMouseDownEvent(oneThumbSlider.getElement());
    assertEquals(
        1,
        events
            .getListeners(
                oneThumbSlider.getElement(), EventType.MOUSEMOVE, false)
            .length);
    assertEquals(1, events.getListeners(doc, EventType.MOUSEUP, true).length);

    testingEvents.fireMouseUpEvent(oneThumbSlider.getElement());

    assertEquals(
        0,
        events
            .getListeners(
                oneThumbSlider.getElement(), EventType.MOUSEMOVE, false)
            .length);
    assertEquals(0, events.getListeners(doc, EventType.MOUSEUP, true).length);

    // Case when the slider is disabled between the mouse down and up
    // events.
    testingEvents.fireMouseDownEvent(oneThumbSlider.getElement());
    assertEquals(
        1,
        events
            .getListeners(
                oneThumbSlider.getElement(), EventType.MOUSEMOVE, false)
            .length);
    assertEquals(1, events.getListeners(doc, EventType.MOUSEUP, true).length);

    oneThumbSlider.setEnabled(false);

    assertEquals(
        0,
        events
            .getListeners(
                oneThumbSlider.getElement(), EventType.MOUSEMOVE, false)
            .length);
    assertEquals(0, events.getListeners(doc, EventType.MOUSEUP, true).length);
    assertEquals(1, oneThumbSlider.getHandler().getListenerCount());

    testingEvents.fireMouseUpEvent(oneThumbSlider.getElement());
    assertEquals(
        0,
        events
            .getListeners(
                oneThumbSlider.getElement(), EventType.MOUSEMOVE, false)
            .length);
    assertEquals(0, events.getListeners(doc, EventType.MOUSEUP, true).length);
  },

  testMouseClickWithMoveToPointEnabled() {
    const stepSize = 20;
    oneThumbSlider.setStep(stepSize);
    oneThumbSlider.setMoveToPointEnabled(true);
    const initialValue = oneThumbSlider.getValue();

    // Figure out the number of pixels per step.
    const numSteps = Math.round(
        (oneThumbSlider.getMaximum() - oneThumbSlider.getMinimum()) / stepSize);
    const size = style.getSize(oneThumbSlider.getElement());
    const pixelsPerStep = Math.round(size.width / numSteps);

    const coords = style.getClientPosition(oneThumbSlider.getElement());
    coords.x += pixelsPerStep / 2;

    // Case when value is increased
    testingEvents.fireClickSequence(
        oneThumbSlider.getElement(),
        /* opt_button */ undefined, coords);
    assertEquals(oneThumbSlider.getValue(), initialValue + stepSize);

    // Case when value is decreased
    testingEvents.fireClickSequence(
        oneThumbSlider.getElement(),
        /* opt_button */ undefined, coords);
    assertEquals(oneThumbSlider.getValue(), initialValue);

    // Case when thumb is clicked
    testingEvents.fireClickSequence(oneThumbSlider.getElement());
    assertEquals(oneThumbSlider.getValue(), initialValue);
  },

  testNonIntegerStepSize() {
    const stepSize = 0.02;
    oneThumbSlider.setStep(stepSize);
    oneThumbSlider.setMinimum(-1);
    oneThumbSlider.setMaximum(1);
    oneThumbSlider.setValue(0.7);
    assertRoughlyEquals(0.7, oneThumbSlider.getValue(), 0.000001);
    oneThumbSlider.setValue(0.3);
    assertRoughlyEquals(0.3, oneThumbSlider.getValue(), 0.000001);
  },

  testSingleThumbSliderHasZeroExtent() {
    const stepSize = 0.02;
    oneThumbSlider.setStep(stepSize);
    oneThumbSlider.setMinimum(-1);
    oneThumbSlider.setMaximum(1);
    oneThumbSlider.setValue(0.7);
    assertEquals(0, oneThumbSlider.getExtent());
    oneThumbSlider.setValue(0.3);
    assertEquals(0, oneThumbSlider.getExtent());
  },

  /** Tests getThumbCoordinateForValue method. */
  testThumbCoordinateForValueWithHorizontalSlider() {
    // Make sure the y-coordinate stays the same for the horizontal slider.
    /** @suppress {visibility} suppression added to enable type checking */
    const originalY = style.getPosition(oneThumbSlider.valueThumb).y;
    /** @suppress {visibility} suppression added to enable type checking */
    const width = oneThumbSlider.getElement().clientWidth -
        oneThumbSlider.valueThumb.offsetWidth;
    const range = oneThumbSlider.getMaximum() - oneThumbSlider.getMinimum();

    // Verify coordinate for a particular value.
    const value = 20;
    const expectedX = Math.round(value / range * width);
    const expectedCoord = new Coordinate(expectedX, originalY);
    let coord = oneThumbSlider.getThumbCoordinateForValue(value);
    assertObjectEquals(expectedCoord, coord);

    // Verify this works regardless of current position.
    oneThumbSlider.setValue(value / 2);
    coord = oneThumbSlider.getThumbCoordinateForValue(value);
    assertObjectEquals(expectedCoord, coord);
  },

  testThumbCoordinateForValueWithVerticalSlider() {
    // Make sure the x-coordinate stays the same for the vertical slider.
    oneThumbSlider.setOrientation(SliderBase.Orientation.VERTICAL);
    /** @suppress {visibility} suppression added to enable type checking */
    const originalX = style.getPosition(oneThumbSlider.valueThumb).x;
    /** @suppress {visibility} suppression added to enable type checking */
    const height = oneThumbSlider.getElement().clientHeight -
        oneThumbSlider.valueThumb.offsetHeight;
    const range = oneThumbSlider.getMaximum() - oneThumbSlider.getMinimum();

    // Verify coordinate for a particular value.
    const value = 20;
    const expectedY = height - Math.round(value / range * height);
    const expectedCoord = new Coordinate(originalX, expectedY);
    let coord = oneThumbSlider.getThumbCoordinateForValue(value);
    assertObjectEquals(expectedCoord, coord);

    // Verify this works regardless of current position.
    oneThumbSlider.setValue(value / 2);
    coord = oneThumbSlider.getThumbCoordinateForValue(value);
    assertObjectEquals(expectedCoord, coord);
  },

  /**
   * Tests getValueFromMousePosition method.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testValueFromMousePosition() {
    const value = 30;
    oneThumbSlider.setValue(value);
    /** @suppress {visibility} suppression added to enable type checking */
    const offset = style.getPageOffset(oneThumbSlider.valueThumb);
    /** @suppress {visibility} suppression added to enable type checking */
    const size = style.getSize(oneThumbSlider.valueThumb);
    offset.x += size.width / 2;
    offset.y += size.height / 2;
    let e = null;
    events.listen(oneThumbSlider, EventType.MOUSEMOVE, (evt) => {
      e = evt;
    });
    testingEvents.fireMouseMoveEvent(oneThumbSlider, offset);
    assertNotEquals(e, null);
    assertRoughlyEquals(
        value, Math.round(oneThumbSlider.getValueFromMousePosition(e)), 1);
    // Verify this works regardless of current position.
    oneThumbSlider.setValue(value / 2);
    assertRoughlyEquals(
        value, Math.round(oneThumbSlider.getValueFromMousePosition(e)), 1);
  },

  /**
     Tests ignoring click event after mousedown event.
     @suppress {checkTypes} suppression added to enable type checking
   */
  testClickAfterMousedown() {
    // Get the center of the thumb at value zero.
    oneThumbSlider.setValue(0);
    /** @suppress {visibility} suppression added to enable type checking */
    const offset = style.getPageOffset(oneThumbSlider.valueThumb);
    /** @suppress {visibility} suppression added to enable type checking */
    const size = style.getSize(oneThumbSlider.valueThumb);
    offset.x += size.width / 2;
    offset.y += size.height / 2;

    const sliderElement = oneThumbSlider.getElement();
    const width = sliderElement.clientWidth - size.width;
    const range = oneThumbSlider.getMaximum() - oneThumbSlider.getMinimum();
    const offsetXAtZero = offset.x;

    // Temporarily control time.
    let theTime = Date.now();
    const saveGoogNow = Date.now;
    Date.now = () => theTime;

    // set coordinate for a particular value.
    const valueOne = 10;
    offset.x = offsetXAtZero + Math.round(valueOne / range * width);
    testingEvents.fireMouseDownEvent(sliderElement, null, offset);
    assertEquals(valueOne, oneThumbSlider.getValue());

    // Verify a click event with another value that follows quickly is
    // ignored.
    /** @suppress {visibility} suppression added to enable type checking */
    theTime += oneThumbSlider.MOUSE_DOWN_DELAY_ / 2;
    const valueTwo = 20;
    offset.x = offsetXAtZero + Math.round(valueTwo / range * width);
    testingEvents.fireClickEvent(sliderElement, null, offset);
    assertEquals(valueOne, oneThumbSlider.getValue());

    // Verify a click later in time does move the thumb.
    /** @suppress {visibility} suppression added to enable type checking */
    theTime += oneThumbSlider.MOUSE_DOWN_DELAY_;
    testingEvents.fireClickEvent(sliderElement, null, offset);
    assertEquals(valueTwo, oneThumbSlider.getValue());

    Date.now = saveGoogNow;
  },

  /**
   * Tests dragging events.
   * @suppress {visibility} suppression added to
   *      enable type checking
   */
  testDragEvents() {
    /** @suppress {visibility} suppression added to enable type checking */
    const offset = style.getPageOffset(oneThumbSlider.valueThumb);
    /** @suppress {visibility} suppression added to enable type checking */
    const size = style.getSize(oneThumbSlider.valueThumb);
    offset.x += size.width / 2;
    offset.y += size.height / 2;
    let event_types = [];
    const handler = (evt) => {
      event_types.push(evt.type);
    };

    events.listen(
        oneThumbSlider,
        [
          SliderBase.EventType.DRAG_START,
          SliderBase.EventType.DRAG_END,
          SliderBase.EventType.DRAG_VALUE_START,
          SliderBase.EventType.DRAG_VALUE_END,
          SliderBase.EventType.DRAG_EXTENT_START,
          SliderBase.EventType.DRAG_EXTENT_END,
          Component.EventType.CHANGE,
        ],
        handler);

    // Since the order of the events between value and extent is not
    // guaranteed across browsers, we need to allow for both here and once
    // we have them all, make sure that they were different.
    function isValueOrExtentDragStart(type) {
      return type == SliderBase.EventType.DRAG_VALUE_START ||
          type == SliderBase.EventType.DRAG_EXTENT_START;
    }
    function isValueOrExtentDragEnd(type) {
      return type == SliderBase.EventType.DRAG_VALUE_END ||
          type == SliderBase.EventType.DRAG_EXTENT_END;
    }

    // Test that dragging the thumb calls all the correct events.
    testingEvents.fireMouseDownEvent(oneThumbSlider.valueThumb);
    offset.x += 100;
    testingEvents.fireMouseMoveEvent(oneThumbSlider.valueThumb, offset);
    testingEvents.fireMouseUpEvent(oneThumbSlider.valueThumb);

    assertEquals(9, event_types.length);

    assertEquals(SliderBase.EventType.DRAG_START, event_types[0]);
    assertTrue(isValueOrExtentDragStart(event_types[1]));

    assertEquals(SliderBase.EventType.DRAG_START, event_types[2]);
    assertTrue(isValueOrExtentDragStart(event_types[3]));

    assertEquals(Component.EventType.CHANGE, event_types[4]);

    assertEquals(SliderBase.EventType.DRAG_END, event_types[5]);
    assertTrue(isValueOrExtentDragEnd(event_types[6]));

    assertEquals(SliderBase.EventType.DRAG_END, event_types[7]);
    assertTrue(isValueOrExtentDragEnd(event_types[8]));

    assertFalse(event_types[1] == event_types[3]);
    assertFalse(event_types[6] == event_types[8]);

    // Test that clicking the thumb without moving the mouse does not cause
    // a CHANGE event between DRAG_START/DRAG_END.
    event_types = [];
    testingEvents.fireMouseDownEvent(oneThumbSlider.valueThumb);
    testingEvents.fireMouseUpEvent(oneThumbSlider.valueThumb);

    assertEquals(8, event_types.length);

    assertEquals(SliderBase.EventType.DRAG_START, event_types[0]);
    assertTrue(isValueOrExtentDragStart(event_types[1]));

    assertEquals(SliderBase.EventType.DRAG_START, event_types[2]);
    assertTrue(isValueOrExtentDragStart(event_types[3]));

    assertEquals(SliderBase.EventType.DRAG_END, event_types[4]);
    assertTrue(isValueOrExtentDragEnd(event_types[5]));

    assertEquals(SliderBase.EventType.DRAG_END, event_types[6]);
    assertTrue(isValueOrExtentDragEnd(event_types[7]));

    assertFalse(event_types[1] == event_types[3]);
    assertFalse(event_types[5] == event_types[7]);

    // Early listener removal, do not wait for tearDown, to avoid building
    // up arrays of events unnecessarilly in further tests.
    events.removeAll(oneThumbSlider);
  },

  /**
   * Tests dragging events updates the value correctly in LTR mode based on
   * the amount of space remaining to the right of the thumb.
   * @suppress {visibility} suppression added to enable type checking
   */
  testDragEventsUpdatesValue() {
    // Get the center of the thumb at minimum value.
    oneThumbSlider.setMinimum(100);
    oneThumbSlider.setMaximum(300);
    oneThumbSlider.setValue(100);

    /** @suppress {visibility} suppression added to enable type checking */
    const offset = style.getPageOffset(oneThumbSlider.valueThumb);
    const offsetXAtZero = offset.x;

    const sliderElement = oneThumbSlider.getElementStrict();
    /** @suppress {visibility} suppression added to enable type checking */
    const thumbSize = style.getSize(oneThumbSlider.valueThumb);
    const width = sliderElement.clientWidth - thumbSize.width;
    const range = oneThumbSlider.getMaximum() - oneThumbSlider.getMinimum();

    // Test that dragging the thumb calls all the correct events.
    testingEvents.fireMouseDownEvent(oneThumbSlider.valueThumb);

    // Scroll to 30 in the range of 0-200. Given that this is LTR mode, that
    // means the value will be 100 + 30 = 130.
    offset.x = offsetXAtZero + Math.round(30 / range * width);
    testingEvents.fireMouseMoveEvent(oneThumbSlider.valueThumb, offset);
    assertEquals(130, oneThumbSlider.getValue());

    // Scroll to 70 in the range of 0-200. Given that this is LTR mode, that
    // means the value will be 100 + 70 = 170.
    offset.x = offsetXAtZero + Math.round(70 / range * width);
    testingEvents.fireMouseMoveEvent(oneThumbSlider.valueThumb, offset);
    assertEquals(170, oneThumbSlider.getValue());

    testingEvents.fireMouseUpEvent(oneThumbSlider.valueThumb);
  },

  /**
   * Tests dragging events updates the value correctly in RTL mode based on
   * the amount of space remaining to the left of the thumb.
   * @suppress {visibility} suppression added to enable type checking
   */
  testDragEventsInRtlModeUpdatesValue() {
    // Get the center of the thumb at minimum value.
    oneThumbSliderRtl.setMinimum(100);
    oneThumbSliderRtl.setMaximum(300);
    oneThumbSliderRtl.setValue(100);

    /** @suppress {visibility} suppression added to enable type checking */
    const offset = style.getPageOffset(oneThumbSliderRtl.valueThumb);
    let offsetXAtZero = offset.x;
    // Extra half of the thumb width in IE8 due to a browser bug where the
    // thumb offsetWidth is incorrectly calculated as 0 in test files.
    /** @suppress {visibility} suppression added to enable type checking */
    const thumbSize = style.getSize(oneThumbSliderRtl.valueThumb);
    if (userAgent.IE && !userAgent.isVersionOrHigher('9')) {
      offsetXAtZero += thumbSize.width / 2;
    }

    const sliderElement = oneThumbSliderRtl.getElementStrict();
    const width = sliderElement.clientWidth - thumbSize.width;
    const range =
        oneThumbSliderRtl.getMaximum() - oneThumbSliderRtl.getMinimum();

    // Test that dragging the thumb calls all the correct events.
    testingEvents.fireMouseDownEvent(oneThumbSliderRtl.valueThumb);

    // Scroll to 30 in the range of 0-200. Given that this is RTL mode, that
    // means the value will be 100 - (-30) = 130.
    offset.x = offsetXAtZero - Math.round(30 / range * width);
    testingEvents.fireMouseMoveEvent(oneThumbSliderRtl.valueThumb, offset);
    assertEquals(130, oneThumbSliderRtl.getValue());

    // Scroll to 70 in the range of 0-200. Given that this is RTL mode, that
    // means the value will be 100 - (-70) = 170.
    offset.x = offsetXAtZero - Math.round(70 / range * width);
    testingEvents.fireMouseMoveEvent(oneThumbSliderRtl.valueThumb, offset);
    assertEquals(170, oneThumbSliderRtl.getValue());

    testingEvents.fireMouseUpEvent(oneThumbSliderRtl.valueThumb);
  },

  /** Tests animationend event after click. */
  testAnimationEndEventAfterClick() {
    /** @suppress {visibility} suppression added to enable type checking */
    const offset = style.getPageOffset(oneThumbSlider.valueThumb);
    /** @suppress {visibility} suppression added to enable type checking */
    const size = style.getSize(oneThumbSlider.valueThumb);
    offset.x += size.width / 2;
    offset.y += size.height / 2;
    const event_types = [];
    const handler = (evt) => {
      event_types.push(evt.type);
    };
    let animationDelay =
        160;  // Delay in ms, is a bit higher than actual delay.
    if (userAgent.IE) {
      // For some reason, (probably due to how timing works), IE7 and IE8
      // will not stop if we don't wait for it.
      animationDelay = 250;
    }
    oneThumbSlider.setMoveToPointEnabled(true);
    events.listen(oneThumbSlider, SliderBase.EventType.ANIMATION_END, handler);

    function isAnimationEndType(type) {
      return type == SliderBase.EventType.ANIMATION_END;
    }
    offset.x += 100;
    testingEvents.fireClickSequence(
        oneThumbSlider.getElement(), /* opt_button */ undefined, offset);

    mockClock.tick(animationDelay);
    assertEquals(1, event_types.length);
    assertTrue(isAnimationEndType(event_types[0]));
    events.removeAll(oneThumbSlider);
  },

  /**
   * Tests that focus will be on the top level element when clicking the
   * slider if `focusElementOnSliderDrag` is true.
   */
  testFocusOnSliderAfterClickIfFocusElementOnSliderDragTrue() {
    const sliderElement = oneThumbSlider.getElement();
    const coords = style.getClientPosition(sliderElement);
    testingEvents.fireClickSequence(
        sliderElement, /* opt_button */ undefined, coords);

    const activeElement = oneThumbSlider.getDomHelper().getActiveElement();
    assertEquals(sliderElement, activeElement);
  },

  /**
   * Tests that focus will not be on the top level element when clicking the
   * slider if `focusElementOnSliderDrag` is false.
   */
  testFocusNotOnSliderAfterClickIfFocusElementOnSliderDragFalse() {
    oneThumbSlider.setFocusElementOnSliderDrag(false);
    const sliderElement = oneThumbSlider.getElement();
    const coords = style.getClientPosition(sliderElement);
    testingEvents.fireClickSequence(
        sliderElement, /* opt_button */ undefined, coords);

    const activeElement = oneThumbSlider.getDomHelper().getActiveElement();
    assertNotEquals(sliderElement, activeElement);
  },
});
