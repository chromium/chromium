/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.labs.mockTest');
goog.setTestOnly('goog.labs.mockTest');

const TimeoutError = goog.require('goog.labs.mock.TimeoutError');
const VerificationError = goog.require('goog.labs.mock.VerificationError');
const array = goog.require('goog.array');
const mock = goog.require('goog.labs.mock');
const mockTimeout = goog.require('goog.labs.mock.timeout');
const string = goog.require('goog.string');
const testSuite = goog.require('goog.testing.testSuite');
/** @suppress {extraRequire} Declares globals */
goog.require('goog.labs.testing.AnythingMatcher');
/** @suppress {extraRequire} Declares globals */
goog.require('goog.labs.testing.GreaterThanMatcher');

const ParentClass = function() {};
ParentClass.prototype.method1 = function() {};
ParentClass.prototype.x = 1;
ParentClass.prototype.val = 0;
ParentClass.prototype.incrementVal = function() {
  this.val++;
};

const ChildClass = function() {};
goog.inherits(ChildClass, ParentClass);
ChildClass.prototype.method2 = function() {};
ChildClass.prototype.y = 2;

class ParentClassEs6 {
  /** Parent method */
  parent() {}
}

class ChildClassEs6 extends ParentClassEs6 {
  /** Child method */
  child() {}
}

/**
 * Asserts that the given string contains a list of others strings
 * in the given order.
 */
function assertContainsInOrder(str, var_args) {
  /** @suppress {checkTypes} suppression added to enable type checking */
  const expected = array.splice(arguments, 1);
  const indices = array.map(expected, function(val) {
    return str.indexOf(val);
  });

  for (let i = 0; i < expected.length; i++) {
    let msg = 'Missing "' + expected[i] + '" from "' + str + '"';
    assertTrue(msg, indices[i] != -1);

    if (i > 0) {
      msg = '"' + expected[i - 1] + '" should come before "' + expected[i] +
          '" in "' + str + '"';
      assertTrue(msg, indices[i] > indices[i - 1]);
    }
  }
}


testSuite({
  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testParentClass() {
    const parentMock = mock.mock(ParentClass);

    assertNotUndefined(parentMock.method1);
    assertUndefined(parentMock.method1());
    assertUndefined(parentMock.method2);
    assertNotUndefined(parentMock.x);
    assertUndefined(parentMock.y);
    assertTrue(
        'Mock should be an instance of the mocked class.',
        parentMock instanceof ParentClass);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testParentClassEs6() {
    const parentMock = mock.mock(ParentClassEs6);

    assertNotUndefined(parentMock.parent);
    assertUndefined(parentMock.parent());
    assertTrue(
        'Mock should be an instance of the mocked class.',
        parentMock instanceof ParentClassEs6);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testChildClass() {
    const childMock = mock.mock(ChildClass);

    assertNotUndefined(childMock.method1);
    assertUndefined(childMock.method1());
    assertNotUndefined(childMock.method2);
    assertUndefined(childMock.method2());
    assertNotUndefined(childMock.x);
    assertNotUndefined(childMock.y);
    assertTrue(
        'Mock should be an instance of the mocked class.',
        childMock instanceof ChildClass);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testChildClassEs6() {
    const childMock = mock.mock(ChildClassEs6);

    assertNotUndefined(childMock.parent);
    assertUndefined(childMock.parent());
    assertNotUndefined(childMock.child);
    assertUndefined(childMock.child());
    assertTrue(
        'Mock should be an instance of the mocked class.',
        childMock instanceof ChildClassEs6);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testParentClassInstance() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const parentMock = mock.mock(new ParentClass());

    assertNotUndefined(parentMock.method1);
    assertUndefined(parentMock.method1());
    assertUndefined(parentMock.method2);
    assertNotUndefined(parentMock.x);
    assertUndefined(parentMock.y);
    assertTrue(
        'Mock should be an instance of the mocked class.',
        parentMock instanceof ParentClass);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testParentClassEs6Instance() {
    const parentMock = mock.mock(new ParentClassEs6());

    assertNotUndefined(parentMock.parent);
    assertUndefined(parentMock.parent());
    assertTrue(
        'Mock should be an instance of the mocked class.',
        parentMock instanceof ParentClassEs6);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testChildClassInstance() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const childMock = mock.mock(new ChildClass());

    assertNotUndefined(childMock.method1);
    assertUndefined(childMock.method1());
    assertNotUndefined(childMock.method2);
    assertUndefined(childMock.method2());
    assertNotUndefined(childMock.x);
    assertNotUndefined(childMock.y);
    assertTrue(
        'Mock should be an instance of the mocked class.',
        childMock instanceof ChildClass);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testChildClassEs6Instance() {
    const childMock = mock.mock(new ChildClassEs6());

    assertNotUndefined(childMock.parent);
    assertUndefined(childMock.parent());
    assertNotUndefined(childMock.child);
    assertUndefined(childMock.child());
    assertTrue(
        'Mock should be an instance of the mocked class.',
        childMock instanceof ChildClassEs6);
  },

  testNonEnumerableProperties() {
    const mockObject = mock.mock({});
    assertNotUndefined(mockObject.toString);
    mock.when(mockObject).toString().then(function() {
      return 'toString';
    });
    assertEquals('toString', mockObject.toString());
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testBasicStubbing() {
    const obj = {
      method1: function(i) {
        return 2 * i;
      },
      method2: function(i, str) {
        return str;
      },
      method3: function(x) {
        return x;
      }
    };

    const mockObj = mock.mock(obj);
    mock.when(mockObj).method1(2).then(function(i) {
      return i;
    });

    assertEquals(4, obj.method1(2));
    assertEquals(2, mockObj.method1(2));
    assertUndefined(mockObj.method1(4));

    mock.when(mockObj).method2(1, 'hi').then(function(i) {
      return 'oh';
    });
    assertEquals('hi', obj.method2(1, 'hi'));
    assertEquals('oh', mockObj.method2(1, 'hi'));
    assertUndefined(mockObj.method2(3, 'foo'));

    mock.when(mockObj).method3(4).thenReturn(10);
    assertEquals(4, obj.method3(4));
    assertEquals(10, mockObj.method3(4));
    mock.verify(mockObj).method3(4);
    assertUndefined(mockObj.method3(5));
  },

  testMockFunctions() {
    function x(i) {
      return i;
    }

    const mockedFunc = mock.mockFunction(x);
    mock.when(mockedFunc)(100).thenReturn(10);
    mock.when(mockedFunc)(50).thenReturn(25);

    assertEquals(100, x(100));
    assertEquals(10, mockedFunc(100));
    assertEquals(25, mockedFunc(50));
  },

  testMockFunctionsWithNullableParameters() {
    const func = function(nullableObject) {
      return 0;
    };
    const mockedFunc = mock.mockFunction(func);
    mock.when(mockedFunc)(null).thenReturn(-1);

    assertEquals(0, func(null));
    assertEquals(-1, mockedFunc(null));
  },

  testMockConstructor() {
    const Ctor = function() {
      this.isMock = false;
    };
    const mockInstance = {isMock: true};
    const MockCtor = mock.mockConstructor(Ctor);
    mock.when(MockCtor)().thenReturn(mockInstance);
    assertEquals(mockInstance, new MockCtor());
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testMockConstructorCopiesProperties() {
    const Ctor = function() {};
    Ctor.myParam = true;
    const MockCtor = mock.mockConstructor(Ctor);
    assertTrue(MockCtor.myParam);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testStubbingConsecutiveCalls() {
    const obj = {
      method: function(i) {
        return i * 42;
      }
    };

    const mockObj = mock.mock(obj);
    mock.when(mockObj).method(1).thenReturn(3).thenReturn(4);

    assertEquals(42, obj.method(1));
    assertEquals(3, mockObj.method(1));
    assertEquals(4, mockObj.method(1));
    assertEquals(4, mockObj.method(1));

    const x = function(i) {
      return i;
    };
    const mockedFunc = mock.mockFunction(x);
    mock.when(mockedFunc)(100).thenReturn(10).thenReturn(25);

    assertEquals(100, x(100));
    assertEquals(10, mockedFunc(100));
    assertEquals(25, mockedFunc(100));
    assertEquals(25, mockedFunc(100));
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testStubbingMultipleObjectStubsNonConflictingArgsAllShouldWork() {
    const obj = {
      method: function(i) {
        return i * 2;
      }
    };
    const mockObj = mock.mock(obj);

    mock.when(mockObj).method(2).thenReturn(100);
    mock.when(mockObj).method(5).thenReturn(45);

    assertEquals(100, mockObj.method(2));
    assertEquals(45, mockObj.method(5));
  },


  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testStubbingMultipleObjectStubsConflictingArgsMostRecentShouldPrevail() {
    const obj = {
      method: function(i) {
        return i * 2;
      }
    };
    const mockObj = mock.mock(obj);

    mock.when(mockObj).method(2).thenReturn(100);
    mock.when(mockObj).method(2).thenReturn(45);

    assertEquals(45, mockObj.method(2));
  },

  testStubbingMultipleFunctionStubsNonConflictingArgsAllShouldWork() {
    const x = function(i) {
      return i;
    };
    const mockedFunc = mock.mockFunction(x);

    mock.when(mockedFunc)(100).thenReturn(10);
    mock.when(mockedFunc)(10).thenReturn(132);

    assertEquals(10, mockedFunc(100));
    assertEquals(132, mockedFunc(10));
  },


  testStubbingMultipleFunctionStubsConflictingArgsMostRecentShouldPrevail() {
    const x = function(i) {
      return i;
    };
    const mockedFunc = mock.mockFunction(x);

    mock.when(mockedFunc)(100).thenReturn(10);
    mock.when(mockedFunc)(100).thenReturn(132);

    assertEquals(132, mockedFunc(100));
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSpying() {
    const obj = {
      method1: function(i) {
        return 2 * i;
      },
      method2: function(i) {
        return 5 * i;
      }
    };

    const spyObj = mock.spy(obj);
    mock.when(spyObj).method1(2).thenReturn(5);

    assertEquals(2, obj.method1(1));
    assertEquals(5, spyObj.method1(2));
    mock.verify(spyObj).method1(2);
    assertEquals(2, spyObj.method1(1));
    mock.verify(spyObj).method1(1);
    assertEquals(20, spyObj.method2(4));
    mock.verify(spyObj).method2(4);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSpyingSelfInteraction() {
    class A {
      method1() {
        this.method2();
      }
      method2() {}
    }
    const spyObj = mock.spy(new A());

    spyObj.method1();
    mock.verify(spyObj).method2();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSpyParentClassInstance() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const parent = new ParentClass();
    const parentMock = mock.spy(parent);

    assertNotUndefined(parentMock.method1);
    assertUndefined(parentMock.method1());
    assertUndefined(parentMock.method2);
    assertNotUndefined(parentMock.x);
    assertUndefined(parentMock.y);
    assertTrue(
        'Mock should be an instance of the mocked class.',
        parentMock instanceof ParentClass);
    const incrementedOrigVal = parent.val + 1;
    parentMock.incrementVal();
    assertEquals(
        'Changes in the spied object should reflect in the spy.',
        incrementedOrigVal, parentMock.val);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSpyChildClassInstance() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const child = new ChildClass();
    const childMock = mock.spy(child);

    assertNotUndefined(childMock.method1);
    assertUndefined(childMock.method1());
    assertNotUndefined(childMock.method2);
    assertUndefined(childMock.method2());
    assertNotUndefined(childMock.x);
    assertNotUndefined(childMock.y);
    assertTrue(
        'Mock should be an instance of the mocked class.',
        childMock instanceof ParentClass);
    const incrementedOrigVal = child.val + 1;
    childMock.incrementVal();
    assertEquals(
        'Changes in the spied object should reflect in the spy.',
        incrementedOrigVal, childMock.val);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testVerifyForObjects() {
    const obj = {
      method1: function(i) {
        return 2 * i;
      },
      method2: function(i) {
        return 5 * i;
      }
    };

    const mockObj = mock.mock(obj);
    mock.when(mockObj).method1(2).thenReturn(5);

    assertEquals(5, mockObj.method1(2));
    mock.verify(mockObj).method1(2);
    const e = assertThrows(goog.partial(mock.verify(mockObj).method2, 2));
    assertTrue(e instanceof VerificationError);
  },

  testVerifyForFunctions() {
    const func = function(i) {
      return i;
    };

    const mockFunc = mock.mockFunction(func);
    mock.when(mockFunc)(2).thenReturn(55);
    assertEquals(55, mockFunc(2));
    mock.verify(mockFunc)(2);
    mock.verify(mockFunc)(lessThan(3));

    const e = assertThrows(goog.partial(mock.verify(mockFunc), 3));
    assertTrue(e instanceof VerificationError);
  },

  testVerifyForFunctionsWithNullableParameters() {
    const func = function(nullableObject) {};
    const mockFuncCalled = mock.mockFunction(func);
    const mockFuncNotCalled = mock.mockFunction(func);

    mockFuncCalled(null);

    mock.verify(mockFuncCalled)(null);
    const e = assertThrows(goog.partial(mock.verify(mockFuncNotCalled), null));
    assertTrue(e instanceof VerificationError);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testVerifyPassesWhenVerificationModeReturnsTrue() {
    const trueMode = {
      verify: function(number) {
        return true;
      },
      describe: function() {
        return '';
      }
    };

    const mockObj = mock.mock({doThing: function() {}});

    mock.verify(mockObj, trueMode).doThing();
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testVerifyFailsWhenVerificationModeReturnsFalse() {
    const falseMode = {
      verify: function(number) {
        return false;
      },
      describe: function() {
        return '';
      }
    };
    const mockObj = mock.mock({doThing: function() {}});

    assertThrows(mock.verify(mockObj, falseMode).doThing);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testVerificationErrorMessagePutsVerificationModeInRightPlace() {
    const modeDescription = 'test';
    const mode = {
      verify: function(number) {
        return false;
      },
      describe: function() {
        return modeDescription;
      }
    };
    const mockObj = mock.mock({methodName: function() {}});
    mockObj.methodName(2);

    /** @suppress {checkTypes} suppression added to enable type checking */
    const e = assertThrows(mock.verify(mockObj, mode).methodName);
    // The mode description should be between the expected method
    // invocation and a newline.
    assertTrue(
        string.contains(e.message, 'methodName() ' + modeDescription + '\n'));
  },


  /**
   * When a function invocation verification fails, it should show the failed
   * expectation call, as well as the recorded calls to the same method.
   * @suppress {strictMissingProperties,checkTypes} suppression added to enable
   * type checking
   */
  testVerificationErrorMessages() {
    const mockObj = mock.mock({
      method: function(i) {
        return i;
      }
    });

    // Failure when there are no recorded calls.
    let e = assertThrows(function() {
      mock.verify(mockObj).method(4);
    });
    assertTrue(e instanceof VerificationError);
    let expected = '\nExpected: method(4) at least 1 times\n' +
        'Recorded: No recorded calls';
    assertEquals(expected, e.message);


    // Failure when there are recorded calls with ints and functions
    // as arguments.
    const callback = function() {};
    const callbackId = mock.getUid(callback);

    mockObj.method(1);
    mockObj.method(2);
    mockObj.method(callback);

    e = assertThrows(function() {
      mock.verify(mockObj).method(3);
    });
    assertTrue(e instanceof VerificationError);

    expected = '\nExpected: method(3) at least 1 times\n' +
        'Recorded: method(1),\n' +
        '          method(2),\n' +
        '          method(<function #anonymous' + callbackId + '>)';
    assertEquals(expected, e.message);

    // With mockFunctions
    const mockCallback = mock.mockFunction(callback);
    e = assertThrows(function() {
      mock.verify(mockCallback)(5);
    });
    expected = '\nExpected: #mockFor<#anonymous' + callbackId +
        '>(5) at least' +
        ' 1 times\n' +
        'Recorded: No recorded calls';

    mockCallback(8);
    mock.verify(mockCallback)(8);
    assertEquals(expected, e.message);

    // Objects with circular references should not fail.
    const obj = {x: 1};
    obj.y = obj;

    mockCallback(obj);
    e = assertThrows(function() {
      mock.verify(mockCallback)(5);
    });
    assertTrue(e instanceof VerificationError);

    // Should respect string representation of different custom classes.
    const myClass = function() {};
    myClass.prototype.toString = function() {
      return '<superClass>';
    };

    const mockFunction = mock.mockFunction(function f() {});
    mockFunction(new myClass());

    e = assertThrows(function() {
      mock.verify(mockFunction)(5);
    });
    expected = '\nExpected: #mockFor<f>(5) at least 1 times\n' +
        'Recorded: #mockFor<f>(<superClass>)';
    assertEquals(expected, e.message);
  },

  async testWait() {
    const mockParent = mock.mock(ParentClass);

    setTimeout(/**
                  @suppress {strictMissingProperties} suppression added to
                  enable type checking
                */
               () => {
                 mockParent.method1();
               },
               0);

    await mock.waitAndVerify(mockParent).method1();
  },

  async testMockFunctionWait() {
    const mockFunc = mock.mockFunction();

    setTimeout(() => {
      mockFunc();
    }, 0);

    await mock.waitAndVerify(mockFunc)();
  },

  async testWaitOnMultipleMethodCalls() {
    const mockParent = mock.mock(ParentClass);
    const timeoutMode = mockTimeout.timeout(150);
    const verificationMode = mock.verification.times(2);

    setTimeout(/**
                  @suppress {strictMissingProperties} suppression added to
                  enable type checking
                */
               () => {
                 mockParent.method1();
               },
               0);
    setTimeout(/**
                  @suppress {strictMissingProperties} suppression added to
                  enable type checking
                */
               () => {
                 mockParent.method1();
               },
               0);

    await mock.waitAndVerify(mockParent, verificationMode, timeoutMode)
        .method1();
  },

  async testMockFunctionWaitOnMultipleMethodCalls() {
    const mockFunc = mock.mockFunction();
    const timeoutMode = mockTimeout.timeout(150);
    const verificationMode = mock.verification.times(2);

    setTimeout(() => {
      mockFunc();
    }, 0);
    setTimeout(() => {
      mockFunc();
    }, 0);

    await mock.waitAndVerify(mockFunc, verificationMode, timeoutMode)();
  },

  async testWaitOnDifferentFunctions() {
    const mockParent = mock.mock(ParentClass);

    setTimeout(/**
                  @suppress {strictMissingProperties} suppression added to
                  enable type checking
                */
               () => {
                 mockParent.incrementVal();
               },
               0);

    setTimeout(/**
                  @suppress {strictMissingProperties} suppression added to
                  enable type checking
                */
               () => {
                 mockParent.method1();
               },
               0);

    await mock.waitAndVerify(mockParent).method1();
    await mock.waitAndVerify(mockParent).incrementVal();
  },

  async testWaitOnSameFunctionWithDifferentArgs() {
    const mockParent = mock.mock(ParentClass);

    setTimeout(/**
                  @suppress {strictMissingProperties} suppression added to
                  enable type checking
                */
               () => {
                 mockParent.method1(1);
               },
               0);

    setTimeout(/**
                  @suppress {strictMissingProperties} suppression added to
                  enable type checking
                */
               () => {
                 mockParent.method1(2);
               },
               0);

    await mock.waitAndVerify(mockParent).method1(2);
    await mock.waitAndVerify(mockParent).method1(1);
  },

  async testMockFunctionWaitWithDifferentArgs() {
    const mockFunc = mock.mockFunction();

    setTimeout(() => {
      mockFunc(1);
    }, 0);

    setTimeout(() => {
      mockFunc(2);
    }, 0);

    await mock.waitAndVerify(mockFunc)(2);
    await mock.waitAndVerify(mockFunc)(1);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  async testWaitWithTimeoutMode() {
    const mockParent = mock.mock(ParentClass);
    const timeoutMode = mockTimeout.timeout(1);

    setTimeout(/**
                  @suppress {strictMissingProperties} suppression added to
                  enable type checking
                */
               () => {
                 mockParent.method1();
               },
               50);

    const e = await assertRejects(
        mock.waitAndVerify(mockParent, timeoutMode).method1());
    assertTrue(e instanceof TimeoutError);
    assertEquals(
        e.message,
        'Function call was either not invoked or never met criteria specified ' +
            'by provided verification mode. \n' +
            'Expected: method1() at least 1 times\n' +
            'Recorded: No recorded calls');
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  async testMockFunctionWaitWithTimeoutMode() {
    const func = function() {};
    const funcId = mock.getUid(func);
    const mockFunc = mock.mockFunction(func);
    const timeoutMode = mockTimeout.timeout(1);

    setTimeout(() => {
      mockFunc();
    }, 50);

    const e = await assertRejects(mock.waitAndVerify(mockFunc, timeoutMode)());
    assertTrue(e instanceof TimeoutError);
    assertEquals(
        e.message,
        'Function call was either not invoked or never met criteria specified ' +
            'by provided verification mode. \n' +
            'Expected: #mockFor<#anonymous' + funcId +
            '>() at least 1 times\n' +
            'Recorded: No recorded calls');
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  async testWaitWithVerificationMode() {
    const mockParent = mock.mock(ParentClass);
    const verificationMode = mock.verification.times(2);

    mockParent.method1();

    const e = await assertRejects(
        mock.waitAndVerify(mockParent, verificationMode).method1());
    assertEquals(
        e.message,
        'Function call was either not invoked or never met criteria specified ' +
            'by provided verification mode. \n' +
            'Expected: method1() 2 times\n' +
            'Recorded: method1()');
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  async testMockFunctionWaitWithVerificationMode() {
    const func = function() {};
    const funcId = mock.getUid(func);
    const mockFunc = mock.mockFunction(func);
    const verificationMode = mock.verification.times(2);

    mockFunc();

    const e =
        await assertRejects(mock.waitAndVerify(mockFunc, verificationMode)());
    assertEquals(
        e.message,
        'Function call was either not invoked or never met criteria specified ' +
            'by provided verification mode. \n' +
            'Expected: #mockFor<#anonymous' + funcId + '>() 2 times\n' +
            'Recorded: #mockFor<#anonymous' + funcId + '>()');
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  async testWaitOnSameMethodTwice() {
    const mockParent = mock.mock(ParentClass);

    mockParent.method1();

    await mock.waitAndVerify(mockParent).method1();
    await mock.waitAndVerify(mockParent).method1();
  },

  async testMockFunctionWaitOnSameMethodTwice() {
    const mockFunc = mock.mockFunction();

    mockFunc();

    await mock.waitAndVerify(mockFunc)();
    await mock.waitAndVerify(mockFunc)();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  async testWaitWithTimeoutAndVerificationMode() {
    const mockParent = mock.mock(ParentClass);
    const timeoutMode = mockTimeout.timeout(150);
    const verificationMode = mock.verification.times(2);

    setTimeout(/**
                  @suppress {strictMissingProperties} suppression added to
                  enable type checking
                */
               () => {
                 mockParent.method1();
               },
               50);

    setTimeout(/**
                  @suppress {strictMissingProperties} suppression added to
                  enable type checking
                */
               () => {
                 mockParent.method1();
               },
               250);

    const e = await assertRejects(
        mock.waitAndVerify(mockParent, timeoutMode, verificationMode)
            .method1());
    assertTrue(e instanceof TimeoutError);
    assertEquals(
        e.message,
        'Function call was either not invoked or never met criteria specified ' +
            'by provided verification mode. \n' +
            'Expected: method1() 2 times\n' +
            'Recorded: method1()');
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  async testMockFunctionWaitWithTimeoutAndVerificationMode() {
    const func = function() {};
    const funcId = mock.getUid(func);
    const mockFunc = mock.mockFunction(func);
    const timeoutMode = mockTimeout.timeout(150);
    const verificationMode = mock.verification.times(2);

    setTimeout(() => {
      mockFunc();
    }, 50);

    setTimeout(() => {
      mockFunc();
    }, 250);

    const e = await assertRejects(
        mock.waitAndVerify(mockFunc, timeoutMode, verificationMode)());
    assertTrue(e instanceof TimeoutError);
    assertEquals(
        e.message,
        'Function call was either not invoked or never met criteria specified ' +
            'by provided verification mode. \n' +
            'Expected: #mockFor<#anonymous' + funcId + '>() 2 times\n' +
            'Recorded: #mockFor<#anonymous' + funcId + '>()');
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  async testPassingVerificationModeBeforeTimeoutMode() {
    const mockParent = mock.mock(ParentClass);
    const timeoutMode = mockTimeout.timeout(150);
    const verificationMode = mock.verification.times(2);

    setTimeout(/**
                  @suppress {strictMissingProperties} suppression added to
                  enable type checking
                */
               () => {
                 mockParent.method1();
               },
               50);

    setTimeout(/**
                  @suppress {strictMissingProperties} suppression added to
                  enable type checking
                */
               () => {
                 mockParent.method1();
               },
               250);

    const e = await assertRejects(
        mock.waitAndVerify(mockParent, verificationMode, timeoutMode)
            .method1());
    assertTrue(e instanceof TimeoutError);
    assertEquals(
        e.message,
        'Function call was either not invoked or never met criteria specified ' +
            'by provided verification mode. \n' +
            'Expected: method1() 2 times\n' +
            'Recorded: method1()');
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  async testMockFunctionPassingVerificationModeBeforeTimeoutMode() {
    const func = function() {};
    const funcId = mock.getUid(func);
    const mockFunc = mock.mockFunction(func);
    const timeoutMode = mockTimeout.timeout(150);
    const verificationMode = mock.verification.times(2);

    setTimeout(() => {
      mockFunc();
    }, 50);

    setTimeout(() => {
      mockFunc();
    }, 250);

    const e = await assertRejects(
        mock.waitAndVerify(mockFunc, verificationMode, timeoutMode)());
    assertTrue(e instanceof TimeoutError);
    assertEquals(
        e.message,
        'Function call was either not invoked or never met criteria specified ' +
            'by provided verification mode. \n' +
            'Expected: #mockFor<#anonymous' + funcId + '>() 2 times\n' +
            'Recorded: #mockFor<#anonymous' + funcId + '>()');
  },


  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testMatchers() {
    const obj = {
      method1: function(i) {
        return 2 * i;
      },
      method2: function(i) {
        return 5 * i;
      }
    };

    const mockObj = mock.mock(obj);

    mock.when(mockObj).method1(greaterThan(4)).thenReturn(100);
    mock.when(mockObj).method1(lessThan(4)).thenReturn(40);

    assertEquals(100, mockObj.method1(5));
    assertEquals(100, mockObj.method1(6));
    assertEquals(40, mockObj.method1(2));
    assertEquals(40, mockObj.method1(1));
    assertUndefined(mockObj.method1(4));
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testMatcherVerify() {
    const obj = {
      method: function(i) {
        return 2 * i;
      }
    };

    // Using spy objects.
    const spy = mock.spy(obj);

    spy.method(6);

    mock.verify(spy).method(greaterThan(4));
    let e = assertThrows(goog.partial(mock.verify(spy).method, lessThan(4)));
    assertTrue(e instanceof VerificationError);

    // Using mocks
    const mockObj = mock.mock(obj);

    mockObj.method(8);

    mock.verify(mockObj).method(greaterThan(7));
    e = assertThrows(goog.partial(mock.verify(mockObj).method, lessThan(7)));
    assertTrue(e instanceof mock.VerificationError);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testMatcherVerifyCollision() {
    const obj = {
      method: function(i) {
        return 2 * i;
      }
    };
    const mockObj = mock.mock(obj);

    mock.when(mockObj).method(5).thenReturn(100);
    assertNotEquals(100, mockObj.method(greaterThan(2)));
  },

  testMatcherVerifyCollisionBetweenMatchers() {
    const obj = {
      method: function(i) {
        return 2 * i;
      }
    };
    const mockObj = mock.mock(obj);

    mock.when(mockObj).method(anything()).thenReturn(100);

    const e =
        assertThrows(goog.partial(mock.verify(mockObj).method, anything()));
    assertTrue(e instanceof VerificationError);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testVerifyForUnmockedMethods() {
    const Task = function() {};
    Task.prototype.run = function() {};

    const mockTask = mock.mock(Task);
    mockTask.run();

    mock.verify(mockTask).run();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testFormatMethodCall() {
    /** @suppress {visibility} suppression added to enable type checking */
    const formatMethodCall = mock.formatMethodCall_;
    assertEquals('alert()', formatMethodCall('alert'));
    assertEquals('sum(2, 4)', formatMethodCall('sum', [2, 4]));
    assertEquals('sum("2", "4")', formatMethodCall('sum', ['2', '4']));
    assertEquals(
        'call(<function unicorn>)',
        formatMethodCall('call', [function unicorn() {}]));

    const arg = {x: 1, y: {hello: 'world'}};
    assertEquals(
        'call(' + mock.formatValue_(arg) + ')',
        formatMethodCall('call', [arg]));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetFunctionName() {
    const f1 = function() {};
    const f2 = function() {};
    const named = function myName() {};

    assert(string.startsWith(mock.getFunctionName_(f1), '#anonymous'));
    assert(string.startsWith(mock.getFunctionName_(f2), '#anonymous'));
    assertNotEquals(mock.getFunctionName_(f1), mock.getFunctionName_(f2));
    assertEquals('myName', mock.getFunctionName_(named));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testFormatObject() {
    let obj;
    let obj2;
    let obj3;

    obj = {x: 1};
    assertEquals(
        '{"x":1 _id:' + mock.getUid(obj) + '}', mock.formatValue_(obj));
    assertEquals('{"x":1}', mock.formatValue_(obj, false /* id */));

    obj = {x: 'hello'};
    assertEquals(
        '{"x":"hello" _id:' + mock.getUid(obj) + '}', mock.formatValue_(obj));
    assertEquals('{"x":"hello"}', mock.formatValue_(obj, false /* id */));

    obj3 = {};
    obj2 = {y: obj3};
    obj3.x = obj2;
    assertEquals(
        '{"x":{"y":<recursive/dupe obj_' + mock.getUid(obj3) + '> ' +
            '_id:' + mock.getUid(obj2) + '} ' +
            '_id:' + mock.getUid(obj3) + '}',
        mock.formatValue_(obj3));
    assertEquals(
        '{"x":{"y":<recursive/dupe>}}',
        mock.formatValue_(obj3, false /* id */));


    obj = {x: function y() {}};
    assertEquals(
        '{"x":<function y> _id:' + mock.getUid(obj) + '}',
        mock.formatValue_(obj));
    assertEquals('{"x":<function y>}', mock.formatValue_(obj, false /* id */));
  },

  testGetUid() {
    const obj1 = {};
    const obj2 = {};
    const func1 = function() {};
    const func2 = function() {};

    assertNotEquals(mock.getUid(obj1), mock.getUid(obj2));
    assertNotEquals(mock.getUid(func1), mock.getUid(func2));
    assertNotEquals(mock.getUid(obj1), mock.getUid(func2));
    assertEquals(mock.getUid(obj1), mock.getUid(obj1));
    assertEquals(mock.getUid(func1), mock.getUid(func1));
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testMockEs6ClassMethods() {
    const Foo = class {
      a() {
        fail('real object should never be called');
      }
    };

    const mockObj = mock.mock(Foo);
    mock.when(mockObj).a().thenReturn('a');
    assertThrowsJsUnitException(function() {
      new Foo().a();
    });
    assertEquals('a', mockObj.a());
    mock.verify(mockObj).a();
  },

});
