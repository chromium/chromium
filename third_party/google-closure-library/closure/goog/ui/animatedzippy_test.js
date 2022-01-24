/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.AnimatedZippyTest');
goog.setTestOnly();

const AnimatedZippy = goog.require('goog.ui.AnimatedZippy');
const Animation = goog.require('goog.fx.Animation');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const Role = goog.require('goog.a11y.aria.Role');
const State = goog.require('goog.a11y.aria.State');
const Transition = goog.require('goog.fx.Transition');
const Zippy = goog.require('goog.ui.Zippy');
const aria = goog.require('goog.a11y.aria');
const asserts = goog.require('goog.asserts');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const functions = goog.require('goog.functions');
const testSuite = goog.require('goog.testing.testSuite');
const testingAsserts = goog.require('goog.testing.asserts');

let animatedZippy;
let animatedZippyHeaderEl;
let propertyReplacer;

testSuite({
  setUp() {
    animatedZippyHeaderEl = dom.getElement('t1');
    asserts.assert(animatedZippyHeaderEl);
    animatedZippy =
        new AnimatedZippy(animatedZippyHeaderEl, dom.getElement('c1'));

    propertyReplacer = new PropertyReplacer();
  },

  tearDown() {
    propertyReplacer.reset();
    animatedZippy.dispose();
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testConstructor() {
    assertNotNull('must not be null', animatedZippy);
    assertEquals(aria.getRole(animatedZippyHeaderEl), Role.TAB);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testConstructorAriaRoleOverride() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    animatedZippy = new AnimatedZippy(
        animatedZippyHeaderEl, dom.getElement('c1'), null, null, Role.BUTTON);
    assertEquals(aria.getRole(animatedZippyHeaderEl), Role.BUTTON);
  },

  testExpandCollapse() {
    let animationsPlayed = 0;
    let toggleEventsFired = 0;

    propertyReplacer.replace(Animation.prototype, 'play', function() {
      animationsPlayed++;
      this.dispatchAnimationEvent(Transition.EventType.END);
    });
    propertyReplacer.replace(
        AnimatedZippy.prototype, 'onAnimate_', functions.NULL);

    events.listenOnce(
        animatedZippy,
        Zippy.Events.TOGGLE, /**
                                @suppress {checkTypes} suppression added to
                                enable type checking
                              */
        (e) => {
          toggleEventsFired++;
          assertTrue('TOGGLE event must be for expansion', e.expanded);
          assertEquals(
              'expanded must be true', true, animatedZippy.isExpanded());
          assertEquals(
              'aria-expanded must be true', 'true',
              aria.getState(animatedZippyHeaderEl, State.EXPANDED));
        });

    animatedZippy.expand();

    events.listenOnce(
        animatedZippy,
        Zippy.Events.TOGGLE, /**
                                @suppress {checkTypes} suppression added to
                                enable type checking
                              */
        (e) => {
          toggleEventsFired++;
          assertFalse('TOGGLE event must be for collapse', e.expanded);
          assertEquals(
              'expanded must be false', false, animatedZippy.isExpanded());
          assertEquals(
              'aria-expanded must be false', 'false',
              aria.getState(animatedZippyHeaderEl, State.EXPANDED));
        });

    animatedZippy.collapse();

    assertEquals('animations must play', 2, animationsPlayed);
    assertEquals('TOGGLE events must fire', 2, toggleEventsFired);
  },

  testExpandCollapseWhileAnimationPlaying() {
    let animationsRunning = 0;
    let lastAnimation = null;

    propertyReplacer.replace(Animation.prototype, 'play', function() {
      animationsRunning++;
      lastAnimation = this;
    });
    propertyReplacer.replace(Animation.prototype, 'stop', function() {
      animationsRunning--;
      this.dispatchAnimationEvent(Transition.EventType.END);
    });
    propertyReplacer.replace(
        AnimatedZippy.prototype, 'onAnimate_', functions.NULL);

    // Expand when expanding animation is playing.
    animatedZippy.expand();
    animatedZippy.expand();
    assertEquals('exactly 1 animation must be running', 1, animationsRunning);
    lastAnimation.stop();
    assertEquals('animation must have finished', 0, animationsRunning);
    assertEquals('expanded must be true', animatedZippy.isExpanded(), true);

    // Expand when collapsing animation is playing.
    animatedZippy.collapse();
    animatedZippy.expand();
    assertEquals('exactly 1 animation must be running', 1, animationsRunning);
    lastAnimation.stop();
    assertEquals('animation must have finished', 0, animationsRunning);
    assertEquals('expanded must be true', animatedZippy.isExpanded(), true);

    // Collapse when collapsing animation is playing.
    animatedZippy.collapse();
    animatedZippy.collapse();
    assertEquals('exactly 1 animation must be running', 1, animationsRunning);
    lastAnimation.stop();
    assertEquals('animation must have finished', 0, animationsRunning);
    assertEquals('expanded must be false', animatedZippy.isExpanded(), false);

    // Collapse when expanding animation is playing.
    animatedZippy.expand();
    animatedZippy.collapse();
    assertEquals('exactly 1 animation must be running', 1, animationsRunning);
    lastAnimation.stop();
    assertEquals('animation must have finished', 0, animationsRunning);
    assertEquals('expanded must be false', animatedZippy.isExpanded(), false);
  },

  /**
   * Tests the TOGGLE_ANIMATION_BEGIN event.
   * @suppress {checkTypes} suppression
   *      added to enable type checking
   */
  testToggleBegin() {
    let animationsPlayed = 0;
    let toggleEventsFired = 0;

    propertyReplacer.replace(Animation.prototype, 'play', function() {
      animationsPlayed++;
      this.dispatchAnimationEvent(Transition.EventType.BEGIN);
      this.dispatchAnimationEvent(Transition.EventType.END);
    });
    propertyReplacer.replace(
        AnimatedZippy.prototype, 'onAnimate_', functions.NULL);

    events.listenOnce(
        animatedZippy,
        AnimatedZippy.Events
            .TOGGLE_ANIMATION_BEGIN, /**
                                        @suppress {checkTypes} suppression added
                                        to enable type checking
                                      */
        (e) => {
          toggleEventsFired++;
          assertTrue(
              'TOGGLE_ANIMATION_BEGIN event must be for expansion', e.expanded);
          assertEquals(
              'expanded must be false', false, animatedZippy.isExpanded());
          assertEquals(
              'aria-expanded must be true', 'true',
              aria.getState(animatedZippyHeaderEl, State.EXPANDED));
        });

    animatedZippy.expand();

    events.listenOnce(
        animatedZippy,
        AnimatedZippy.Events
            .TOGGLE_ANIMATION_BEGIN, /**
                                        @suppress {checkTypes} suppression added
                                        to enable type checking
                                      */
        (e) => {
          toggleEventsFired++;
          assertFalse(
              'TOGGLE_ANIMATION_BEGIN event must be for collapse', e.expanded);
          assertEquals(
              'expanded must be true', true, animatedZippy.isExpanded());
          assertEquals(
              'aria-expanded must be false', 'false',
              aria.getState(animatedZippyHeaderEl, State.EXPANDED));
        });

    animatedZippy.collapse();

    assertEquals('animations must play', 2, animationsPlayed);
    assertEquals(
        'TOGGLE_ANIMATION_BEGIN events must fire', 2, toggleEventsFired);
  },

  /**
   * Tests the TOGGLE_ANIMATION_END event.
   * @suppress {checkTypes} suppression
   *      added to enable type checking
   */
  testToggleEnd() {
    let animationsPlayed = 0;
    let toggleEventsFired = 0;

    propertyReplacer.replace(Animation.prototype, 'play', function() {
      animationsPlayed++;
      this.dispatchAnimationEvent(Transition.EventType.END);
    });
    propertyReplacer.replace(
        AnimatedZippy.prototype, 'onAnimate_', functions.NULL);

    events.listenOnce(
        animatedZippy,
        AnimatedZippy.Events
            .TOGGLE_ANIMATION_END, /**
                                      @suppress {checkTypes} suppression added
                                      to enable type checking
                                    */
        (e) => {
          toggleEventsFired++;
          assertTrue(
              'TOGGLE_ANIMATION_END event must be for expansion', e.expanded);
          assertEquals(
              'expanded must be true', true, animatedZippy.isExpanded());
          assertEquals(
              'aria-expanded must be true', 'true',
              aria.getState(animatedZippyHeaderEl, State.EXPANDED));
        });

    animatedZippy.expand();

    events.listenOnce(
        animatedZippy,
        AnimatedZippy.Events
            .TOGGLE_ANIMATION_END, /**
                                      @suppress {checkTypes} suppression added
                                      to enable type checking
                                    */
        (e) => {
          toggleEventsFired++;
          assertFalse(
              'TOGGLE_ANIMATION_END event must be for collapse', e.expanded);
          assertEquals(
              'expanded must be false', false, animatedZippy.isExpanded());
          assertEquals(
              'aria-expanded must be false', 'false',
              aria.getState(animatedZippyHeaderEl, State.EXPANDED));
        });

    animatedZippy.collapse();

    assertEquals('animations must play', 2, animationsPlayed);
    assertEquals('TOGGLE_ANIMATION_END events must fire', 2, toggleEventsFired);
  },
});
