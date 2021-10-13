/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.memoizeTest');
goog.setTestOnly();

const memoize = goog.require('goog.memoize');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testNoArgs() {
    let called = 0;
    const f = memoize(() => {
      called++;
      return 10;
    });

    assertEquals('f() 1st call', 10, f());
    assertEquals('f() 2nd call', 10, f());
    assertEquals('f() 3rd call', 10, f.call());
    assertEquals('f() called once', 1, called);
  },

  testOneOptionalArgSimple() {
    let called = 0;
    const f = memoize(function(opt_x) {
      called++;
      return arguments.length == 0 ? 'no args' : opt_x;
    });

    assertEquals('f() 1st call', 'no args', f());
    assertEquals('f() 2nd call', 'no args', f());
    assertEquals('f(0) 1st call', 0, f(0));
    assertEquals('f(0) 2nd call', 0, f(0));
    assertEquals('f("") 1st call', '', f(''));
    assertEquals('f("") 2nd call', '', f(''));
    assertEquals('f("0") 1st call', '0', f('0'));
    assertEquals('f("0") 1st call', '0', f('0'));
    assertEquals('f(null) 1st call', null, f(null));
    assertEquals('f(null) 2nd call', null, f(null));
    assertEquals('f(undefined) 1st call', undefined, f(undefined));
    assertEquals('f(undefined) 2nd call', undefined, f(undefined));

    assertEquals('f(opt_x) called 6 times', 6, called);
  },

  testProtoFunctions() {
    let fcalled = 0;
    let gcalled = 0;
    const Class = function(x) {
      this.x = x;
      this.f = memoize(function(y) {
        fcalled++;
        return this.x + y;
      });
    };
    Class.prototype.g = memoize(function(z) {
      gcalled++;
      return this.x - z;
    });

    /** @suppress {checkTypes} suppression added to enable type checking */
    const obj1 = new Class(10);
    /** @suppress {checkTypes} suppression added to enable type checking */
    const obj2 = new Class(20);

    assertEquals('10+1', 11, obj1.f(1));
    assertEquals('10+2', 12, obj1.f(2));
    assertEquals('10+2 again', 12, obj1.f(2));
    assertEquals('f called twice', 2, fcalled);

    assertEquals('10-1', 9, obj1.g(1));
    assertEquals('10-2', 8, obj1.g(2));
    assertEquals('10-2 again', 8, obj1.g(2));
    assertEquals('g called twice', 2, gcalled);

    assertEquals('20+1', 21, obj2.f(1));
    assertEquals('20+2', 22, obj2.f(2));
    assertEquals('20+2 again', 22, obj2.f(2));
    assertEquals('f called 4 times', 4, fcalled);

    assertEquals('20-1', 19, obj2.g(1));
    assertEquals('20-2', 18, obj2.g(2));
    assertEquals('20-2 again', 18, obj2.g(2));
    assertEquals('g called 4 times', 4, gcalled);
  },

  testCustomSerializer() {
    let called = 0;
    const serializer = (this_context, args) => String(args[0].getTime());
    const getYear = memoize((date) => {
      called++;
      return date.getFullYear();
    }, serializer);

    assertEquals(
        'getYear(2008, 0, 1), 1st', 2008, getYear(new Date(2008, 0, 1)));
    assertEquals(
        'getYear(2008, 0, 1), 2nd', 2008, getYear(new Date(2008, 0, 1)));
    assertEquals('getYear called once', 1, called);

    assertEquals('getYear(2007, 0, 1)', 2007, getYear(new Date(2007, 0, 1)));
    assertEquals('getYear called twice', 2, called);
  },

  testClearCache() {
    let computed = 0;
    const identity = memoize((x) => {
      computed++;
      return x;
    });
    assertEquals('identity(1)==1', 1, identity(1));
    assertEquals('identity(1)==1', 1, identity(1));
    assertEquals('identity(1)==1', 1, identity(1));
    assertEquals('Expected memozation', 1, computed);

    memoize.clearCache(globalThis);
    assertEquals('identity(1)==1', 1, identity(1));
    assertEquals('identity(1)==1', 1, identity(1));
    assertEquals('Expected cleared memoization cache', 2, computed);
  },

  testDisableMemoize() {
    let computed = 0;
    const identity = memoize((x) => {
      computed++;
      return x;
    });

    assertEquals('return value on first call', 1, identity(1));
    assertEquals('return value on second call (memoized)', 1, identity(1));
    assertEquals('computed once', 1, computed);

    memoize.ENABLE_MEMOIZE = false;

    try {
      assertEquals('return value after disabled memoization', 1, identity(1));
      assertEquals('computed again', 2, computed);
    } finally {
      memoize.ENABLE_MEMOIZE = true;
    }

    assertEquals('return value after reenabled memoization', 1, identity(1));
    assertEquals('not computed again', 2, computed);
  },
});
