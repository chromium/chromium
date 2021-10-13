/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.LooseMockTest');
goog.setTestOnly();

const LooseMock = goog.require('goog.testing.LooseMock');
const mockmatchers = goog.require('goog.testing.mockmatchers');
const testSuite = goog.require('goog.testing.testSuite');

// The object that we will be mocking
class RealObject {
  a() {
    fail('real object should never be called');
  }

  b() {
    fail('real object should never be called');
  }
}

let mock;

// Most of the basic functionality is tested in strictmock_test; these tests
// cover the cases where loose mocks are different from strict mocks

testSuite({
  setUp() {
    const obj = new RealObject();
    mock = new LooseMock(obj);
  },

  /**
     @suppress {strictMissingProperties,missingProperties} suppression added to
     enable type checking
   */
  testSimpleExpectations() {
    mock.a(5);
    mock.b();
    mock.$replay();
    mock.a(5);
    mock.b();
    mock.$verify();

    mock.$reset();

    mock.a();
    mock.b();
    mock.$replay();
    mock.b();
    mock.a();
    mock.$verify();

    mock.$reset();

    mock.a(5).$times(2);
    mock.a(5);
    mock.a(2);
    mock.$replay();
    mock.a(5);
    mock.a(5);
    mock.a(5);
    mock.a(2);
    mock.$verify();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testMultipleExpectations() {
    mock.a().$returns(1);
    mock.a().$returns(2);
    mock.$replay();
    assertEquals(1, mock.a());
    assertEquals(2, mock.a());
    mock.$verify();
  },

  /**
     @suppress {strictMissingProperties,missingProperties} suppression added to
     enable type checking
   */
  testMultipleExpectationArgs() {
    mock.a('asdf').$anyTimes();
    mock.a('qwer').$anyTimes();
    mock.b().$times(3);
    mock.$replay();
    mock.a('asdf');
    mock.b();
    mock.a('asdf');
    mock.a('qwer');
    mock.b();
    mock.a('qwer');
    mock.b();
    mock.$verify();

    mock.$reset();

    mock.a('asdf').$anyTimes();
    mock.a('qwer').$anyTimes();
    mock.$replay();
    mock.a('asdf');
    mock.a('qwer');
    goog.bind(mock.a, mock, 'asdf');
    goog.bind(mock.$verify, mock);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSameMethodOutOfOrder() {
    mock.a('foo').$returns(1);
    mock.a('bar').$returns(2);
    mock.$replay();
    assertEquals(2, mock.a('bar'));
    assertEquals(1, mock.a('foo'));
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSameMethodDifferentReturnValues() {
    mock.a('foo').$returns(1).$times(2);
    mock.a('foo').$returns(3);
    mock.a('bar').$returns(2);
    mock.$replay();
    assertEquals(1, mock.a('foo'));
    assertEquals(2, mock.a('bar'));
    assertEquals(1, mock.a('foo'));
    assertEquals(3, mock.a('foo'));
    assertThrowsJsUnitException(/**
                                   @suppress {strictMissingProperties}
                                   suppression added to enable type checking
                                 */
                                () => {
                                  mock.a('foo');
                                  mock.$verify();
                                });
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSameMethodBrokenExpectations() {
    // This is a weird corner case.
    // No way to ever make this verify no matter what you call after replaying,
    // because the second expectation of mock.a('foo') will be masked by
    // the first expectation that can be called any number of times, and so we
    // can never satisfy that second expectation.
    mock.a('foo').$returns(1).$anyTimes();
    mock.a('bar').$returns(2);
    mock.a('foo').$returns(3);

    // LooseMock can detect this case and fail on $replay.
    assertThrowsJsUnitException(goog.bind(mock.$replay, mock));
    mock.$reset();

    // This is a variant of the corner case above, but it's harder to determine
    // that the expectation to mock.a('bar') can never be satisfied. So we don't
    // fail on $replay, but we do fail on $verify.
    mock.a(mockmatchers.isString).$returns(1).$anyTimes();
    mock.a('bar').$returns(2);
    mock.$replay();

    assertEquals(1, mock.a('foo'));
    assertEquals(1, mock.a('bar'));
    assertThrowsJsUnitException(goog.bind(mock.$verify, mock));
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSameMethodMultipleAnyTimes() {
    mock.a('foo').$returns(1).$anyTimes();
    mock.a('foo').$returns(2).$anyTimes();
    mock.$replay();
    assertEquals(1, mock.a('foo'));
    assertEquals(1, mock.a('foo'));
    assertEquals(1, mock.a('foo'));
    // Note we'll never return 2 but that's ok.
    mock.$verify();
  },

  /**
     @suppress {strictMissingProperties,missingProperties} suppression added to
     enable type checking
   */
  testFailingFast() {
    mock.a().$anyTimes();
    mock.$replay();
    mock.a();
    mock.a();
    assertThrowsJsUnitException(goog.bind(mock.b, mock));
    mock.$reset();

    // too many
    mock.a();
    mock.b();
    mock.$replay();
    mock.a();
    mock.b();

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    const e = assertThrowsJsUnitException(goog.bind(mock.a, mock));

    assertContains('Too many calls to a', e.message);
  },

  /**
     @suppress {strictMissingProperties,missingProperties} suppression added to
     enable type checking
   */
  testTimes() {
    mock.a().$times(3);
    mock.b().$times(2);
    mock.$replay();
    mock.a();
    mock.b();
    mock.b();
    mock.a();
    mock.a();
    mock.$verify();
  },

  /**
     @suppress {strictMissingProperties,missingProperties} suppression added to
     enable type checking
   */
  testFailingSlow() {
    // not enough
    mock.a().$times(3);
    mock.$replay();
    mock.a();
    mock.a();
    assertThrowsJsUnitException(goog.bind(mock.$verify, mock));

    mock.$reset();

    // not enough, interleaved order
    mock.a().$times(3);
    mock.b().$times(3);
    mock.$replay();
    mock.a();
    mock.b();
    mock.a();
    mock.b();
    assertThrowsJsUnitException(goog.bind(mock.$verify, mock));

    mock.$reset();
    // bad args
    mock.a('asdf').$anyTimes();
    mock.$replay();
    mock.a('asdf');
    assertThrowsJsUnitException(goog.bind(mock.a, mock, 'qwert'));
    assertThrowsJsUnitException(goog.bind(mock.$verify, mock));
  },

  /**
     @suppress {strictMissingProperties,missingProperties} suppression added to
     enable type checking
   */
  testArgsAndReturns() {
    mock.a('asdf').$atLeastOnce().$returns(5);
    mock.b('qwer').$times(2).$returns(3);
    mock.$replay();
    assertEquals(5, mock.a('asdf'));
    assertEquals(3, mock.b('qwer'));
    assertEquals(5, mock.a('asdf'));
    assertEquals(5, mock.a('asdf'));
    assertEquals(3, mock.b('qwer'));
    mock.$verify();
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
     @suppress {strictMissingProperties,missingProperties} suppression added to
     enable type checking
   */
  testIgnoresExtraCalls() {
    mock = new LooseMock(RealObject, true);
    mock.a();
    mock.$replay();
    mock.a();
    mock.b();  // doesn't throw
    mock.$verify();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSkipAnyTimes() {
    mock = new LooseMock(RealObject);
    mock.a(1).$anyTimes();
    mock.a(2).$anyTimes();
    mock.a(3).$anyTimes();
    mock.$replay();
    mock.a(1);
    mock.a(3);
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
