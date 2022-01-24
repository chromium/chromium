/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.history.Html5HistoryTest');
goog.setTestOnly();

const EventType = goog.require('goog.events.EventType');
const HistoryEventType = goog.require('goog.history.EventType');
const Html5History = goog.require('goog.history.Html5History');
const MockControl = goog.require('goog.testing.MockControl');
const Timer = goog.require('goog.Timer');
const events = goog.require('goog.events');
const jsunit = goog.require('goog.testing.jsunit');
const mockmatchers = goog.require('goog.testing.mockmatchers');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

// Delay running the tests after page load. This test has some asynchronous
// behavior that interacts with page load detection.
/** @suppress {constantProperty} suppression added to enable type checking */
jsunit.AUTO_RUN_DELAY_IN_MS = 500;

let mockControl;
let mockWindow;

let html5History;

// Regression test for b/18663922.

// Regression test for b/18663922.

testSuite({
  /** @suppress {checkTypes} suppression added to enable type checking */
  setUp() {
    mockControl = new MockControl();

    mockWindow = {location: {}};
    mockWindow.attachEvent = mockControl.createFunctionMock();
    mockWindow
        .attachEvent(mockmatchers.ignoreArgument, mockmatchers.ignoreArgument)
        .$anyTimes();
    const mockHistoryIsSupportedMethod =
        mockControl.createMethodMock(Html5History, 'isSupported');
    mockHistoryIsSupportedMethod(mockWindow).$returns(true).$anyTimes();
  },

  tearDown() {
    if (html5History) {
      html5History.dispose();
      html5History = null;
    }
    mockControl.$tearDown();
  },

  testGetTokenWithoutUsingFragment() {
    mockWindow.location.pathname = '/test/something';

    mockControl.$replayAll();
    /** @suppress {checkTypes} suppression added to enable type checking */
    html5History = new Html5History(mockWindow);
    html5History.setUseFragment(false);

    assertEquals('test/something', html5History.getToken());
    mockControl.$verifyAll();
  },

  testGetTokenWithoutUsingFragmentWithCustomPathPrefix() {
    mockWindow.location.pathname = '/test/something';

    mockControl.$replayAll();
    /** @suppress {checkTypes} suppression added to enable type checking */
    html5History = new Html5History(mockWindow);
    html5History.setUseFragment(false);
    html5History.setPathPrefix('/test/');

    assertEquals('something', html5History.getToken());
    mockControl.$verifyAll();
  },

  testGetTokenWithoutUsingFragmentWithCustomTransformer() {
    mockWindow.location.pathname = '/test/something';
    const mockTransformer =
        mockControl.createLooseMock(Html5History.TokenTransformer);
    mockTransformer.retrieveToken('/', mockWindow.location).$returns('abc/1');

    mockControl.$replayAll();
    /** @suppress {checkTypes} suppression added to enable type checking */
    html5History = new Html5History(mockWindow, mockTransformer);
    html5History.setUseFragment(false);

    assertEquals('abc/1', html5History.getToken());
    mockControl.$verifyAll();
  },

  testGetTokenWithoutUsingFragmentWithCustomTransformerAndPrefix() {
    mockWindow.location.pathname = '/test/something';
    const mockTransformer =
        mockControl.createLooseMock(Html5History.TokenTransformer);
    mockTransformer.retrieveToken('/test/', mockWindow.location)
        .$returns('abc/1');

    mockControl.$replayAll();
    /** @suppress {checkTypes} suppression added to enable type checking */
    html5History = new Html5History(mockWindow, mockTransformer);
    html5History.setUseFragment(false);
    html5History.setPathPrefix('/test/');

    assertEquals('abc/1', html5History.getToken());
    mockControl.$verifyAll();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetUrlWithoutUsingFragment() {
    mockWindow.location.search = '?q=something';

    mockControl.$replayAll();
    /** @suppress {checkTypes} suppression added to enable type checking */
    html5History = new Html5History(mockWindow);
    html5History.setUseFragment(false);

    assertEquals('/some/token?q=something', html5History.getUrl_('some/token'));
    mockControl.$verifyAll();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetUrlWithoutUsingFragmentWithCustomPathPrefix() {
    mockWindow.location.search = '?q=something';

    mockControl.$replayAll();
    /** @suppress {checkTypes} suppression added to enable type checking */
    html5History = new Html5History(mockWindow);
    html5History.setUseFragment(false);
    html5History.setPathPrefix('/test/');

    assertEquals(
        '/test/some/token?q=something', html5History.getUrl_('some/token'));
    mockControl.$verifyAll();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetUrlWithoutUsingFragmentWithCustomTransformer() {
    mockWindow.location.search = '?q=something';
    const mockTransformer =
        mockControl.createLooseMock(Html5History.TokenTransformer);
    mockTransformer.createUrl('some/token', '/', mockWindow.location)
        .$returns('/something/else/?different');

    mockControl.$replayAll();
    /** @suppress {checkTypes} suppression added to enable type checking */
    html5History = new Html5History(mockWindow, mockTransformer);
    html5History.setUseFragment(false);

    assertEquals(
        '/something/else/?different', html5History.getUrl_('some/token'));
    mockControl.$verifyAll();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetUrlWithoutUsingFragmentWithCustomTransformerAndPrefix() {
    mockWindow.location.search = '?q=something';
    const mockTransformer =
        mockControl.createLooseMock(Html5History.TokenTransformer);
    mockTransformer.createUrl('some/token', '/test/', mockWindow.location)
        .$returns('/something/else/?different');

    mockControl.$replayAll();
    /** @suppress {checkTypes} suppression added to enable type checking */
    html5History = new Html5History(mockWindow, mockTransformer);
    html5History.setUseFragment(false);
    html5History.setPathPrefix('/test/');

    assertEquals(
        '/something/else/?different', html5History.getUrl_('some/token'));
    mockControl.$verifyAll();
  },

  testNavigateFiresOnceOnNavigation() {
    const history = new Html5History;
    const onNavigate = recordFunction();
    history.setEnabled(true);
    history.listen(HistoryEventType.NAVIGATE, onNavigate);

    // Simulate that the user navigates in the history.
    /**
     * @suppress {checkTypes,const} suppression added to enable type checking
     */
    location = '#' + Date.now();

    return Timer.promise(0)
        .then(/**
                 @suppress {strictMissingProperties}
                 suppression added to enable type checking
               */
              () => {
                // NAVIGATE should fire once with
                // isNavigation=true.
                onNavigate.assertCallCount(1);
                assertTrue(
                    onNavigate.getLastCall().getArgument(0).isNavigation);
                return Timer.promise(0).then(() => {
                  // NAVIGATE should not fire again after the
                  // current JS execution context.
                  onNavigate.assertCallCount(1);
                });
              });
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testNavigateFiresOnceWithoutPopstate() {
    const history = new Html5History;
    const onNavigate = recordFunction();
    history.setEnabled(true);
    history.listen(HistoryEventType.NAVIGATE, onNavigate);

    // Removing POPSTATE to ensure NAVIGATE is triggered in browsers that don't
    // support it.
    assertTrue(events.unlisten(
        window, EventType.POPSTATE, history.onHistoryEvent_, false, history));

    // Simulate that the user navigates in the history.
    /**
     * @suppress {checkTypes,const} suppression added to enable type checking
     */
    location = '#' + Date.now();

    return Timer.promise(0)
        .then(/**
                 @suppress {strictMissingProperties}
                 suppression added to enable type checking
               */
              () => {
                // NAVIGATE should fire once with
                // isNavigation=true.
                onNavigate.assertCallCount(1);
                assertTrue(
                    onNavigate.getLastCall().getArgument(0).isNavigation);
                return Timer.promise(0).then(() => {
                  // NAVIGATE should not fire again after the
                  // current JS execution context.
                  onNavigate.assertCallCount(1);
                });
              });
  },
});
