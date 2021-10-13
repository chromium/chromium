/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.events.EventMatcherTest');
goog.setTestOnly();

const EventMatcher = goog.require('goog.testing.events.EventMatcher');
const GoogEvent = goog.require('goog.events.Event');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  /** @suppress {checkTypes} suppression added to enable type checking */
  testEventMatcher() {
    const matcher = new EventMatcher('foo');
    assertFalse(matcher.matches(undefined));
    assertFalse(matcher.matches(null));
    assertFalse(matcher.matches({type: 'foo'}));
    assertFalse(matcher.matches(new GoogEvent('bar')));

    assertTrue(matcher.matches(new GoogEvent('foo')));
    const FooEvent = function() {
      GoogEvent.call(this, 'foo');
    };
    goog.inherits(FooEvent, GoogEvent);
    assertTrue(matcher.matches(new FooEvent()));
  },
});
