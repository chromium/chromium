/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.MockClassFactoryTest');
goog.setTestOnly('goog.testing.MockClassFactoryTest');

const LooseMock = goog.require('goog.testing.LooseMock');
const MockClassFactory = goog.require('goog.testing.MockClassFactory');
const StrictMock = goog.require('goog.testing.StrictMock');
const testSuite = goog.require('goog.testing.testSuite');
const testing = goog.require('goog.testing');

/** A fake namespace. */
const fake = {};

// Classes that will be mocked.  A base class and child class are used to
// test inheritance.
fake.BaseClass = function(a) {
  fail('real object should never be called');
};

fake.BaseClass.prototype.foo = function() {
  fail('real object should never be called');
};

fake.BaseClass.prototype.toString = function() {
  return 'foo';
};

fake.BaseClass.prototype.toLocaleString = function() {
  return 'bar';
};

fake.BaseClass.prototype.overridden = function() {
  return 42;
};

fake.ChildClass = function(a) {
  fail('real object should never be called');
};
goog.inherits(fake.ChildClass, fake.BaseClass);

fake.ChildClass.staticFoo = function() {
  fail('real object should never be called');
};

fake.ChildClass.prototype.bar = function() {
  fail('real object should never be called');
};

fake.ChildClass.staticProperty = 'staticPropertyOnClass';

function TopLevelBaseClass() {}

fake.ChildClass.prototype.overridden = function() {
  /**
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  const superResult = fake.ChildClass.base(this, 'overridden');
  if (superResult != 42) {
    fail('super method not invoked or returned wrong value');
  }
  return superResult + 1;
};

const mockClassFactory = new MockClassFactory();
const matchers = testing.mockmatchers;


testSuite({
  tearDown() {
    mockClassFactory.reset();
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testGetStrictMockClass() {
    const mock1 = mockClassFactory.getStrictMockClass(fake, fake.BaseClass, 1);
    mock1.foo();
    mock1.$replay();

    const mock2 = mockClassFactory.getStrictMockClass(fake, fake.BaseClass, 2);
    mock2.foo();
    mock2.$replay();

    const mock3 = mockClassFactory.getStrictMockClass(fake, fake.ChildClass, 3);
    mock3.foo();
    mock3.bar();
    mock3.$replay();

    /** @suppress {checkTypes} suppression added to enable type checking */
    const instance1 = new fake.BaseClass(1);
    instance1.foo();
    mock1.$verify();

    /** @suppress {checkTypes} suppression added to enable type checking */
    const instance2 = new fake.BaseClass(2);
    instance2.foo();
    mock2.$verify();

    /** @suppress {checkTypes} suppression added to enable type checking */
    const instance3 = new fake.ChildClass(3);
    instance3.foo();
    instance3.bar();
    mock3.$verify();

    assertThrows(/**
                    @suppress {checkTypes} suppression added to enable type
                    checking
                  */
                 function() {
                   new fake.BaseClass(-1);
                 });
    assertTrue(instance1 instanceof fake.BaseClass);
    assertTrue(instance2 instanceof fake.BaseClass);
    assertTrue(instance3 instanceof fake.ChildClass);
  },

  /** @suppress {uselessCode} suppression added to enable type checking */
  testGetStrictMockClassCreatesAllProxies() {
    const mock1 = mockClassFactory.getStrictMockClass(fake, fake.BaseClass, 1);
    // toString(), toLocaleString() and others are treaded specially in
    // createProxy_().
    mock1.toString();
    mock1.toLocaleString();
    mock1.$replay();

    /** @suppress {checkTypes} suppression added to enable type checking */
    const instance1 = new fake.BaseClass(1);
    instance1.toString();
    instance1.toLocaleString();
    mock1.$verify();
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testGetLooseMockClass() {
    const mock1 = mockClassFactory.getLooseMockClass(fake, fake.BaseClass, 1);
    mock1.foo().$anyTimes().$returns(3);
    mock1.$replay();

    const mock2 = mockClassFactory.getLooseMockClass(fake, fake.BaseClass, 2);
    mock2.foo().$times(3);
    mock2.$replay();

    const mock3 = mockClassFactory.getLooseMockClass(fake, fake.ChildClass, 3);
    mock3.foo().$atLeastOnce().$returns(5);
    mock3.bar().$atLeastOnce();
    mock3.$replay();

    /** @suppress {checkTypes} suppression added to enable type checking */
    const instance1 = new fake.BaseClass(1);
    assertEquals(3, instance1.foo());
    assertEquals(3, instance1.foo());
    assertEquals(3, instance1.foo());
    assertEquals(3, instance1.foo());
    assertEquals(3, instance1.foo());
    mock1.$verify();

    /** @suppress {checkTypes} suppression added to enable type checking */
    const instance2 = new fake.BaseClass(2);
    instance2.foo();
    instance2.foo();
    instance2.foo();
    mock2.$verify();

    /** @suppress {checkTypes} suppression added to enable type checking */
    const instance3 = new fake.ChildClass(3);
    assertEquals(5, instance3.foo());
    assertEquals(5, instance3.foo());
    instance3.bar();
    mock3.$verify();

    assertThrows(/**
                    @suppress {checkTypes} suppression added to enable type
                    checking
                  */
                 function() {
                   new fake.BaseClass(-1);
                 });
    assertTrue(instance1 instanceof fake.BaseClass);
    assertTrue(instance2 instanceof fake.BaseClass);
    assertTrue(instance3 instanceof fake.ChildClass);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testGetStrictStaticMock() {
    const staticMock =
        mockClassFactory.getStrictStaticMock(fake, fake.ChildClass);
    const mock = mockClassFactory.getStrictMockClass(fake, fake.ChildClass, 1);

    mock.foo();
    mock.bar();
    staticMock.staticFoo();
    mock.$replay();
    staticMock.$replay();

    /** @suppress {checkTypes} suppression added to enable type checking */
    const instance = new fake.ChildClass(1);
    instance.foo();
    instance.bar();
    fake.ChildClass.staticFoo();
    mock.$verify();
    staticMock.$verify();

    assertTrue(instance instanceof fake.BaseClass);
    assertTrue(instance instanceof fake.ChildClass);
    assertThrows(function() {
      mockClassFactory.getLooseStaticMock(fake, fake.ChildClass);
    });
  },

  testGetStrictStaticMockKeepsStaticProperties() {
    const OriginalChildClass = fake.ChildClass;
    const staticMock =
        mockClassFactory.getStrictStaticMock(fake, fake.ChildClass);
    assertTrue(staticMock instanceof StrictMock);
    assertEquals(
        OriginalChildClass.staticProperty, fake.ChildClass.staticProperty);
  },

  testGetLooseStaticMockKeepsStaticProperties() {
    const OriginalChildClass = fake.ChildClass;
    const staticMock =
        mockClassFactory.getLooseStaticMock(fake, fake.ChildClass);
    assertTrue(staticMock instanceof LooseMock);
    assertEquals(
        OriginalChildClass.staticProperty, fake.ChildClass.staticProperty);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testGetLooseStaticMock() {
    const staticMock =
        mockClassFactory.getLooseStaticMock(fake, fake.ChildClass);
    const mock = mockClassFactory.getStrictMockClass(fake, fake.ChildClass, 1);

    mock.foo();
    mock.bar();
    staticMock.staticFoo().$atLeastOnce();
    mock.$replay();
    staticMock.$replay();

    /** @suppress {checkTypes} suppression added to enable type checking */
    const instance = new fake.ChildClass(1);
    instance.foo();
    instance.bar();
    fake.ChildClass.staticFoo();
    fake.ChildClass.staticFoo();
    mock.$verify();
    staticMock.$verify();

    assertTrue(instance instanceof fake.BaseClass);
    assertTrue(instance instanceof fake.ChildClass);
    assertThrows(function() {
      mockClassFactory.getStrictStaticMock(fake, fake.ChildClass);
    });
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testFlexibleClassMockInstantiation() {
    // This mock should be returned for all instances created with a number
    // as the first argument.
    const mock = mockClassFactory.getStrictMockClass(
        fake, fake.ChildClass, matchers.isNumber);
    mock.foo();  // Will be called by the first mock instance.
    mock.foo();  // Will be called by the second mock instance.
    mock.$replay();

    /** @suppress {checkTypes} suppression added to enable type checking */
    const instance1 = new fake.ChildClass(1);
    /** @suppress {checkTypes} suppression added to enable type checking */
    const instance2 = new fake.ChildClass(2);
    instance1.foo();
    instance2.foo();
    assertThrows(/**
                    @suppress {checkTypes} suppression added to enable type
                    checking
                  */
                 function() {
                   new fake.ChildClass('foo');
                 });
    mock.$verify();
  },

  testGoogBaseCall() {
    const overriddenFn = fake.ChildClass.prototype.overridden;
    const mock = mockClassFactory.getLooseMockClass(fake, fake.ChildClass, 1);
    /** @suppress {checkTypes} suppression added to enable type checking */
    const instance1 = new fake.ChildClass(1);
    assertTrue(43 == overriddenFn.call(instance1));
  },

});
