/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview
 * @suppress {missingRequire} Stubbing goog.async.Throttle
 */

goog.module('goog.ui.ac.CachingMatcherTest');
goog.setTestOnly();

const CachingMatcher = goog.require('goog.ui.ac.CachingMatcher');
const MockControl = goog.require('goog.testing.MockControl');
const mockmatchers = goog.require('goog.testing.mockmatchers');
const testSuite = goog.require('goog.testing.testSuite');

let ignoreArgument = mockmatchers.ignoreArgument;

/**
 * Fake version of Throttle which only fires when we call permitOne().
 * @suppress {missingProvide,checkTypes} suppression added to enable type
 * checking
 */
goog.async.Throttle = class {
  constructor(fn, time, self) {
    this.fn = fn;
    this.time = time;
    this.self = self;
    this.numFires = 0;
  }

  fire() {
    this.numFires++;
  }

  permitOne() {
    if (this.numFires == 0) {
      return;
    }
    this.fn.call(this.self);
    this.numFires = 0;
  }
};

// Actual tests.
let mockControl;
let mockMatcher;
let mockHandler;
let matcher;

testSuite({
  setUp() {
    mockControl = new MockControl();
    mockMatcher = {
      requestMatchingRows:
          mockControl.createFunctionMock('requestMatchingRows'),
    };
    mockHandler = mockControl.createFunctionMock('matchHandler');
    matcher = new CachingMatcher(mockMatcher);
  },

  tearDown() {
    mockControl.$tearDown();
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testLocalThenRemoteMatch() {
    // We immediately get the local match.
    mockHandler('foo', []);
    mockControl.$replayAll();
    matcher.requestMatchingRows('foo', 12, mockHandler);
    mockControl.$verifyAll();
    mockControl.$resetAll();

    // Now we run the remote match.
    mockHandler('foo', ['foo1', 'foo2'], ignoreArgument);
    mockMatcher.requestMatchingRows('foo', 100, ignoreArgument)
        .$does((token, maxResults, matchHandler) => {
          matchHandler('foo', ['foo1', 'foo2', 'bar3']);
        });
    mockControl.$replayAll();
    matcher.throttledTriggerBaseMatch_.permitOne();
    mockControl.$verifyAll();
    mockControl.$resetAll();
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testCacheSize() {
    matcher.setMaxCacheSize(4);

    // First we populate, but not overflow the cache.
    mockHandler('foo', []);
    mockHandler('foo', ['foo111', 'foo222'], ignoreArgument);
    mockMatcher.requestMatchingRows('foo', 100, ignoreArgument)
        .$does((token, maxResults, matchHandler) => {
          matchHandler('foo', ['foo111', 'foo222', 'bar333']);
        });
    mockControl.$replayAll();
    matcher.requestMatchingRows('foo', 12, mockHandler);
    matcher.throttledTriggerBaseMatch_.permitOne();
    mockControl.$verifyAll();
    mockControl.$resetAll();

    // Now we verify the cache is populated.
    mockHandler('foo1', ['foo111']);
    mockControl.$replayAll();
    matcher.requestMatchingRows('foo1', 12, mockHandler);
    mockControl.$verifyAll();
    mockControl.$resetAll();

    // Now we overflow the cache. Check that the remote results show the first
    // time we get them back, even though they overflow the cache.
    mockHandler('foo11', ['foo111']);
    mockHandler(
        'foo11', ['foo111', 'foo112', 'foo113', 'foo114'], ignoreArgument);
    mockMatcher.requestMatchingRows('foo11', 100, ignoreArgument)
        .$does((token, maxResults, matchHandler) => {
          matchHandler('foo11', ['foo111', 'foo112', 'foo113', 'foo114']);
        });
    mockControl.$replayAll();
    matcher.requestMatchingRows('foo11', 12, mockHandler);
    matcher.throttledTriggerBaseMatch_.permitOne();
    mockControl.$verifyAll();
    mockControl.$resetAll();

    // Now check that the cache is empty.
    mockHandler('foo11', []);
    mockControl.$replayAll();
    matcher.requestMatchingRows('foo11', 12, mockHandler);
    mockControl.$verifyAll();
    mockControl.$resetAll();
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testClearCache() {
    // First we populate the cache.
    mockHandler('foo', []);
    mockHandler('foo', ['foo111', 'foo222'], ignoreArgument);
    mockMatcher.requestMatchingRows('foo', 100, ignoreArgument)
        .$does((token, maxResults, matchHandler) => {
          matchHandler('foo', ['foo111', 'foo222', 'bar333']);
        });
    mockControl.$replayAll();
    matcher.requestMatchingRows('foo', 12, mockHandler);
    matcher.throttledTriggerBaseMatch_.permitOne();
    mockControl.$verifyAll();
    mockControl.$resetAll();

    // Now we verify the cache is populated.
    mockHandler('foo1', ['foo111']);
    mockControl.$replayAll();
    matcher.requestMatchingRows('foo1', 12, mockHandler);
    mockControl.$verifyAll();
    mockControl.$resetAll();

    // Now we clear the cache.
    matcher.clearCache();

    // Now check that the cache is empty.
    mockHandler('foo11', []);
    mockControl.$replayAll();
    matcher.requestMatchingRows('foo11', 12, mockHandler);
    mockControl.$verifyAll();
    mockControl.$resetAll();
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testSimilarMatchingDoesntReorderResults() {
    // Populate the cache. We get two prefix matches.
    mockHandler('ba', []);
    mockHandler('ba', ['bar', 'baz', 'bam'], ignoreArgument);
    mockMatcher.requestMatchingRows('ba', 100, ignoreArgument)
        .$does((token, maxResults, matchHandler) => {
          matchHandler('ba', ['bar', 'baz', 'bam']);
        });
    mockControl.$replayAll();
    matcher.requestMatchingRows('ba', 12, mockHandler);
    matcher.throttledTriggerBaseMatch_.permitOne();
    mockControl.$verifyAll();
    mockControl.$resetAll();

    // The user types another character. The local match gives us two similar
    // matches, but no prefix matches. The remote match returns a prefix match,
    // which would normally be ranked above the similar matches, but gets ranked
    // below the similar matches because the user hasn't typed any more
    // characters.
    mockHandler('bad', ['bar', 'baz', 'bam']);
    mockHandler(
        'bad', ['bar', 'baz', 'bam', 'bad', 'badder', 'baddest'],
        ignoreArgument);
    mockMatcher.requestMatchingRows('bad', 100, ignoreArgument)
        .$does((token, maxResults, matchHandler) => {
          matchHandler('bad', ['bad', 'badder', 'baddest']);
        });
    mockControl.$replayAll();
    matcher.requestMatchingRows('bad', 12, mockHandler);
    matcher.throttledTriggerBaseMatch_.permitOne();
    mockControl.$verifyAll();
    mockControl.$resetAll();

    // The user types yet another character, which allows the prefix matches to
    // jump to the top of the list of suggestions.
    mockHandler('badd', ['badder', 'baddest']);
    mockControl.$replayAll();
    matcher.requestMatchingRows('badd', 12, mockHandler);
    mockControl.$verifyAll();
    mockControl.$resetAll();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSetThrottleTime() {
    assertEquals(150, matcher.throttledTriggerBaseMatch_.time);
    matcher.setThrottleTime(234);
    assertEquals(234, matcher.throttledTriggerBaseMatch_.time);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetBaseMatcherMaxMatches() {
    mockHandler('foo', []);  // Local match
    mockMatcher.requestMatchingRows('foo', 789, ignoreArgument);
    mockControl.$replayAll();
    matcher.setBaseMatcherMaxMatches();
    matcher.requestMatchingRows('foo', 12, mockHandler);
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testSetLocalMatcher() {
    // Use a local matcher which just sorts all the rows alphabetically.
    function sillyMatcher(token, maxMatches, rows) {
      rows = rows.concat([]);
      rows.sort();
      return rows;
    }

    mockHandler('foo', []);
    mockHandler('foo', ['a', 'b', 'c'], ignoreArgument);
    mockMatcher.requestMatchingRows('foo', 100, ignoreArgument)
        .$does((token, maxResults, matchHandler) => {
          matchHandler('foo', ['b', 'a', 'c']);
        });
    mockControl.$replayAll();
    matcher.setLocalMatcher(sillyMatcher);
    matcher.requestMatchingRows('foo', 12, mockHandler);
    matcher.throttledTriggerBaseMatch_.permitOne();
    mockControl.$verifyAll();
    mockControl.$resetAll();
  },
});
