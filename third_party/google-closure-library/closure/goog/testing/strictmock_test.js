/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.StrictMockTest');
goog.setTestOnly();

const StrictMock = goog.require('goog.testing.StrictMock');
const testSuite = goog.require('goog.testing.testSuite');

// The object that we will be mocking
class RealObject {
  a() {
    fail('real object should never be called');
  }

  b() {
    fail('real object should never be called');
  }

  c() {
    fail('real object should never be called');
  }
}

let mock;

testSuite({
  setUp() {
    const obj = new RealObject();
    mock = new StrictMock(obj);
  },

  /**
     @suppress {strictMissingProperties,missingProperties} suppression added to
     enable type checking
   */
  testMockFunction() {
    const mock = new StrictMock(RealObject);
    mock.a();
    mock.b();
    mock.c();
    mock.$replay();
    mock.a();
    mock.b();
    mock.c();
    mock.$verify();

    mock.$reset();

    assertThrows(/**
                    @suppress {missingProperties} suppression added to enable
                    type checking
                  */
                 () => {
                   mock.x();
                 });
  },

  /**
     @suppress {strictMissingProperties,missingProperties} suppression added to
     enable type checking
   */
  testSimpleExpectations() {
    mock.a();
    mock.$replay();
    mock.a();
    mock.$verify();

    mock.$reset();

    mock.a();
    mock.b();
    mock.a();
    mock.a();
    mock.$replay();
    mock.a();
    mock.b();
    mock.a();
    mock.a();
    mock.$verify();
  },

  /**
     @suppress {strictMissingProperties,missingProperties} suppression added to
     enable type checking
   */
  testFailToSetExpectation() {
    mock.$replay();
    assertThrowsJsUnitException(goog.bind(mock.a, mock));

    mock.$reset();

    mock.$replay();
    assertThrowsJsUnitException(goog.bind(mock.b, mock));
  },

  /**
     @suppress {strictMissingProperties,missingProperties} suppression added to
     enable type checking
   */
  testUnexpectedCall() {
    mock.a();
    mock.$replay();
    mock.a();
    assertThrowsJsUnitException(goog.bind(mock.a, mock));

    mock.$reset();

    mock.a();
    mock.$replay();
    assertThrowsJsUnitException(goog.bind(mock.b, mock));
  },

  /**
     @suppress {strictMissingProperties,missingProperties} suppression added to
     enable type checking
   */
  testNotEnoughCalls() {
    mock.a();
    mock.$replay();
    assertThrowsJsUnitException(goog.bind(mock.$verify, mock));

    mock.$reset();

    mock.a();
    mock.b();
    mock.$replay();
    mock.a();
    assertThrowsJsUnitException(goog.bind(mock.$verify, mock));
  },

  /**
     @suppress {strictMissingProperties,missingProperties} suppression added to
     enable type checking
   */
  testOutOfOrderCalls() {
    mock.a();
    mock.b();
    mock.$replay();
    assertThrowsJsUnitException(goog.bind(mock.b, mock));
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testVerify() {
    mock.a();
    mock.$replay();
    mock.a();
    mock.$verify();

    mock.$reset();

    mock.a();
    mock.$replay();
    assertThrowsJsUnitException(goog.bind(mock.$verify, mock));
  },

  /**
     @suppress {strictMissingProperties,missingProperties} suppression added to
     enable type checking
   */
  testArgumentMatching() {
    mock.a('foo');
    mock.b('bar');
    mock.$replay();
    mock.a('foo');
    assertThrowsJsUnitException(/**
                                   @suppress {missingProperties} suppression
                                   added to enable type checking
                                 */
                                () => {
                                  mock.b('foo');
                                });

    mock.$reset();
    mock.a('foo');
    mock.a('bar');
    mock.$replay();
    mock.a('foo');
    mock.a('bar');
    mock.$verify();

    mock.$reset();
    mock.a('foo');
    mock.a('bar');
    mock.$replay();
    assertThrowsJsUnitException(/**
                                   @suppress {strictMissingProperties}
                                   suppression added to enable type checking
                                 */
                                () => {
                                  mock.a('bar');
                                });
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testReturnValue() {
    mock.a().$returns(5);
    mock.$replay();

    assertEquals('Mock should return the right value', 5, mock.a());

    mock.$verify();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testMultipleReturnValues() {
    mock.a().$returns(3);
    mock.a().$returns(2);
    mock.a().$returns(1);

    mock.$replay();

    assertArrayEquals(
        'Mock should return the right value sequence', [3, 2, 1],
        [mock.a(), mock.a(), mock.a()]);

    mock.$verify();
  },

  /**
     @suppress {strictMissingProperties,missingProperties} suppression added to
     enable type checking
   */
  testAtMostOnce() {
    // Zero times SUCCESS.
    mock.a().$atMostOnce();
    mock.$replay();
    mock.$verify();

    mock.$reset();

    // One time SUCCESS.
    mock.a().$atMostOnce();
    mock.$replay();
    mock.a();
    mock.$verify();

    mock.$reset();

    // Many times FAIL.
    mock.a().$atMostOnce();
    mock.$replay();
    mock.a();
    assertThrowsJsUnitException(goog.bind(mock.a, mock));

    mock.$reset();

    // atMostOnce only lasts until a new method is called.
    mock.a().$atMostOnce();
    mock.b();
    mock.a();
    mock.$replay();
    mock.b();
    assertThrowsJsUnitException(goog.bind(mock.$verify, mock));
  },

  /**
     @suppress {strictMissingProperties,missingProperties} suppression added to
     enable type checking
   */
  testAtLeastOnce() {
    // atLeastOnce does not mean zero times
    mock.a().$atLeastOnce();
    mock.$replay();
    assertThrowsJsUnitException(goog.bind(mock.$verify, mock));

    mock.$reset();

    // atLeastOnce does mean three times
    mock.a().$atLeastOnce();
    mock.$replay();
    mock.a();
    mock.a();
    mock.a();
    mock.$verify();

    mock.$reset();

    // atLeastOnce only lasts until a new method is called
    mock.a().$atLeastOnce();
    mock.b();
    mock.a();
    mock.$replay();
    mock.a();
    mock.a();
    mock.b();
    mock.a();
    assertThrowsJsUnitException(goog.bind(mock.a, mock));
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testAtLeastOnceWithArgs() {
    mock.a('asdf').$atLeastOnce();
    mock.a('qwert');
    mock.$replay();
    mock.a('asdf');
    mock.a('asdf');
    mock.a('qwert');
    mock.$verify();

    mock.$reset();

    mock.a('asdf').$atLeastOnce();
    mock.a('qwert');
    mock.$replay();
    mock.a('asdf');
    mock.a('asdf');
    assertThrowsJsUnitException(/**
                                   @suppress {strictMissingProperties}
                                   suppression added to enable type checking
                                 */
                                () => {
                                  mock.a('zxcv');
                                });
    assertThrowsJsUnitException(goog.bind(mock.$verify, mock));
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testAnyTimes() {
    mock.a().$anyTimes();
    mock.$replay();
    mock.$verify();

    mock.$reset();

    mock.a().$anyTimes();
    mock.$replay();
    mock.a();
    mock.a();
    mock.a();
    mock.a();
    mock.a();
    mock.$verify();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testAnyTimesWithArguments() {
    mock.a('foo').$anyTimes();
    mock.$replay();
    mock.$verify();

    mock.$reset();

    mock.a('foo').$anyTimes();
    mock.a('bar').$anyTimes();
    mock.$replay();
    mock.a('foo');
    mock.a('foo');
    mock.a('foo');
    mock.a('bar');
    mock.a('bar');
    mock.$verify();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testZeroTimes() {
    mock.a().$times(0);
    mock.$replay();
    mock.$verify();

    mock.$reset();

    mock.a().$times(0);
    mock.$replay();
    assertThrowsJsUnitException(/**
                                   @suppress {strictMissingProperties}
                                   suppression added to enable type checking
                                 */
                                () => {
                                  mock.a();
                                });
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testZeroTimesWithArguments() {
    mock.a('foo').$times(0);
    mock.$replay();
    mock.$verify();

    mock.$reset();

    mock.a('foo').$times(0);
    mock.$replay();
    assertThrowsJsUnitException(/**
                                   @suppress {strictMissingProperties}
                                   suppression added to enable type checking
                                 */
                                () => {
                                  mock.a('foo');
                                });
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testTooManyCalls() {
    mock.a().$times(2);
    mock.$replay();
    mock.a();
    mock.a();
    assertThrowsJsUnitException(/**
                                   @suppress {strictMissingProperties}
                                   suppression added to enable type checking
                                 */
                                () => {
                                  mock.a();
                                });
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testTooManyCallsWithArguments() {
    mock.a('foo').$times(2);
    mock.$replay();
    mock.a('foo');
    mock.a('foo');
    assertThrowsJsUnitException(/**
                                   @suppress {strictMissingProperties}
                                   suppression added to enable type checking
                                 */
                                () => {
                                  mock.a('foo');
                                });
  },

  /**
     @suppress {strictMissingProperties,missingProperties} suppression added to
     enable type checking
   */
  testMultipleSkippedAnyTimes() {
    mock.a().$anyTimes();
    mock.b().$anyTimes();
    mock.c().$anyTimes();
    mock.$replay();
    mock.c();
    mock.$verify();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testMultipleSkippedAnyTimesWithArguments() {
    mock.a('foo').$anyTimes();
    mock.a('bar').$anyTimes();
    mock.a('baz').$anyTimes();
    mock.$replay();
    mock.a('baz');
    mock.$verify();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testVerifyThrows() {
    mock.a(1);
    mock.$replay();
    mock.a(1);
    try {
      mock.a(2);
      fail('bad mock, should fail');
    } catch (ex) {
      // this could be an event handler, for example
    }
    assertThrowsJsUnitException(goog.bind(mock.$verify, mock));
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testThrows() {
    mock.a().$throws('exception!');
    mock.$replay();
    assertThrows(goog.bind(mock.a, mock));
    mock.$verify();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testDoes() {
    mock.a(1, 2).$does((a, b) => a + b);
    mock.$replay();
    assertEquals('Mock should call the function', 3, mock.a(1, 2));
    mock.$verify();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testErrorMessageForBadArgs() {
    mock.a();
    mock.$anyTimes();

    mock.$replay();

    const e =
        assertThrowsJsUnitException(/**
                                       @suppress {strictMissingProperties}
                                       suppression added to enable type checking
                                     */
                                    () => {
                                      mock.a('a');
                                    });

    assertContains('Bad arguments to a()', e.message);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  async testWaitAndVerify() {
    mock.a();
    mock.$replay();

    setTimeout(/**
                  @suppress {strictMissingProperties} suppression added to
                  enable type checking
                */
               () => {
                 mock.a();
               },
               0);
    await mock.$waitAndVerify();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  async testWaitAndVerify_Multiple() {
    mock.a().$times(2);
    mock.$replay();

    setTimeout(/**
                  @suppress {strictMissingProperties} suppression added to
                  enable type checking
                */
               () => {
                 mock.a();
               },
               0);
    setTimeout(/**
                  @suppress {strictMissingProperties} suppression added to
                  enable type checking
                */
               () => {
                 mock.a();
               },
               50);
    await mock.$waitAndVerify();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  async testWaitAndVerify_Never() {
    mock.a().$never();
    mock.$replay();

    await mock.$waitAndVerify();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  async testWaitAndVerify_Synchronous() {
    mock.a();
    mock.$replay();

    mock.a();
    await mock.$waitAndVerify();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  async testWaitAndVerify_Exception() {
    mock.a();
    mock.$replay();

    setTimeout(() => {
      assertThrowsJsUnitException(/**
                                     @suppress {strictMissingProperties}
                                     suppression added to enable type checking
                                   */
                                  () => {
                                    mock.a(false);
                                  });
    }, 0);
    await assertRejects(mock.$waitAndVerify());
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  async testWaitAndVerify_Reset() {
    mock.a();
    mock.$replay();

    setTimeout(/**
                  @suppress {strictMissingProperties} suppression added to
                  enable type checking
                */
               () => {
                 mock.a();
               },
               0);
    await mock.$waitAndVerify();
    mock.$reset();
    mock.a();
    mock.$replay();

    setTimeout(/**
                  @suppress {strictMissingProperties} suppression added to
                  enable type checking
                */
               () => {
                 mock.a();
               },
               0);
    await mock.$waitAndVerify();
  },
});
