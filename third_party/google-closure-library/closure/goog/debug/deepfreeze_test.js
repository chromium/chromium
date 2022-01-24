/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.deepFreezeTest');
goog.setTestOnly();

const asserts = goog.require('goog.asserts');
const testSuite = goog.require('goog.testing.testSuite');
const {deepFreeze} = goog.require('goog.debug.deepFreeze');

testSuite({
  testDeepFreeze: {
    testObject() {
      const a = {
        'b': 'c',
        'c': true,
        'd': false,
        'e': null,
        'f': 5,
        'g': undefined,
      };
      const f = deepFreeze(a);
      assertEquals('c', f.b);
      assertTrue(f.c);
      assertFalse(f.d);
      assertNull(f.e);
      assertEquals(5, f.f);
      assertTrue(f.hasOwnProperty('g'));
      assertThrows(() => {
        f.e = 5;
      });
      assertThrows(() => {
        console.log(a.e);
      });
      assertEquals('c', f.b);
      assertTrue(f.c);
      assertFalse(f.d);
      assertNull(f.e);
      assertEquals(5, f.f);
    },

    testObjectWithArray() {
      const a = {
        'b': 'c',
        'c': ['d', 'e', 'f'],
      };
      const f = deepFreeze(a);
      assertEquals('c', f.b);
      assertEquals('d', f.c[0]);
      assertEquals('e', f.c[1]);
      assertEquals('f', f.c[2]);
      asserts.assertArray(f.c);
      assertThrows(() => {
        f.c = 'hello world!';
      });
      assertThrows(() => {
        console.log(a.c);
      });
      assertEquals('c', f.b);
      assertEquals('d', f.c[0]);
      assertEquals('e', f.c[1]);
      assertEquals('f', f.c[2]);
    },

    testObjectWithChildObjects() {
      const a = {
        'c': {
          'd': 'e',
          'r': 'f',
        },
      };
      const f = deepFreeze(a);
      assertEquals('e', f.c.d);
      assertEquals('f', f.c.r);
      assertThrows(() => {
        f.c.r = 10;
      });
      assertThrows(() => {
        f.c = {};
      });
      assertThrows(() => {
        console.log(a.c);
      });

      assertThrows(() => {
        a.c = {};
      });
      assertEquals('e', f.c.d);
      assertEquals('f', f.c.r);
    },

    /**
       @suppress {missingProperties} suppression added to enable type checking
     */
    testObjectWithChildMultipath() {
      const a = {
        'c': {
          'd': 'e',
          'r': 'f',
        },
      };
      const d = {'a': 'b'};
      // Create two ways to access object d. This should not throw when frozen,
      // and both paths to this same object should throw.
      a.c['g'] = d;
      a.c['h'] = d;
      const f = deepFreeze(a);
      assertEquals('e', f.c.d);
      assertEquals('f', f.c.r);
      assertEquals('b', f.c.g.a);
      assertEquals('b', f.c.h.a);
      assertThrows(() => {
        f.c.r = 10;
      });
      assertThrows(() => {
        f.c = {};
      });
      assertThrows(() => {
        console.log(a.c);
      });
      assertThrows(() => {
        /**
         * @suppress {missingProperties} suppression added to enable type
         * checking
         */
        f.c.g.a = 'c';
      });
      assertThrows(() => {
        /**
         * @suppress {missingProperties} suppression added to enable type
         * checking
         */
        f.c.h.a = 'c';
      });

      assertThrows(() => {
        a.c = {};
      });
      assertEquals('e', f.c.d);
      assertEquals('f', f.c.r);
    },

    testObjectWithSymbolKeys() {
      const s = Symbol(5);
      const a = {};
      a[s] = 'hello world';
      const f = deepFreeze(a);
      assertEquals('hello world', f[s]);
      // The below check would be an assertThrows, but the Symbol polyfill for
      // IE doesn't throw when changing a frozen symbol key's value.
      try {
        f[s] = 'new thing!';
      } catch (expectedInStrictMode) {
      }
      assertEquals('hello world', f[s]);
      assertThrows(() => {
        console.log(a[s]);
      });
    },

    /** @suppress {checkTypes} suppression added to enable type checking */
    testObjectWithSymbolValues() {
      const s = Symbol(5);
      if (s instanceof Object) {
        // The below test doesn't work in IE as the Symbol polyfill is detected
        // to be an object that is not an object literal.
        // TODO(user): run this test on IE.
        return;
      }
      const a = {
        's': s,
      };
      const f = deepFreeze(a);
      assertEquals(s, f.s);

      assertThrows(() => {
        f.s = Symbol(10);
      });
      assertThrows(() => {
        console.log(a.s);
      });
      assertEquals(s, f.s);
    },

    testArray() {
      const a = new Array(5);
      a[2] = 5;
      const f = deepFreeze(a);
      assertEquals(a.length, f.length);
      assertEquals(5, f[2]);
      asserts.assertArray(f);
      assertThrows(() => {
        f[2] = 'hello world!';
      });
      assertThrows(() => {
        console.log(a[2]);
      });
    },

    testFailsWithFunctions() {
      assertThrows(() => {
        deepFreeze(function() {
          console.log('');
        });
      });
    },

    testFailsWithFunctionInObject() {
      assertThrows(() => {
        deepFreeze({
          a: function() {
            console.log('');
          },
        });
      });
    },

    testFailsWithClasses() {
      assertThrows(() => {
        class C {
          hello() {
            console.log('Hello');
          }
        }
        const b = new C();
        deepFreeze(b);
      });
    },

    testFailsWithClassInObject() {
      assertThrows(() => {
        class C {
          hello() {
            console.log('Hello');
          }
        }
        const b = new C();
        deepFreeze({
          'b': b,
        });
      });
    },

    testFailsWithClassDefinition() {
      assertThrows(() => {
        const b = class C {};
        deepFreeze(b);
      });
    },

    testFailsWithClassDefinitionInObject() {
      assertThrows(() => {
        const b = class C {};
        deepFreeze({
          'b': b,
        });
      });
    },

    testFailsWithGetters() {
      assertThrows(() => {
        deepFreeze({
          get aThing() {
            return 5;
          },
        });
      });
    },

    testFailsWithSetters() {
      assertThrows(() => {
        deepFreeze({
          a: 5,
          /**
             @suppress {undefinedVars} suppression added to enable type
             checking
           */
          set aThing(v) {
            a++;
          },
        });
      });
    },


    testFailsWithCyclicReferences() {
      const a = {b: {}};
      a.b.c = a;
      assertThrows(() => {
        deepFreeze(a);
      });
    },
  },
});
