// Copyright 2012 The Closure Library Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

goog.module('goog.defineClassTest');
goog.setTestOnly();

const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testSuper() {
    /** @constructor @struct */
    function SomeSuper() {}

    const SomeClass = goog.defineClass(SomeSuper, {
      constructor: function() {},
    });

    assertTrue(new SomeClass() instanceof SomeClass);
    assertTrue(new SomeClass() instanceof SomeSuper);
  },

  testInstanceProp() {
    const SomeClass = goog.defineClass(null, {
      constructor: function() {
        this.falseProp = false;
      },
    });

    assertEquals(new SomeClass().falseProp, false);
  },

  testPrototypeProp() {
    const SomeClass = goog.defineClass(null, {
      constructor: function() {},
      trueMethod: function() {
        return true;
      },
    });

    assertEquals(new SomeClass().trueMethod(), true);
    assertEquals(SomeClass.prototype.trueMethod(), true);
  },

  /** @suppress {missingProperties} */
  testStaticProp() {
    const SomeClass = goog.defineClass(null, {
      constructor: function() {},
      statics: {someProp: 100},
    });

    assertEquals(new SomeClass().statics, undefined);
    assertEquals(new SomeClass().someProp, undefined);
    assertEquals(SomeClass.someProp, 100);
  },

  /** @suppress {missingProperties} */
  testStaticPropFn() {
    const SomeClass = goog.defineClass(null, {
      constructor: function() {},
      statics: function(cls) {
        cls.someProp = 100;
      },
    });

    assertEquals(new SomeClass().statics, undefined);
    assertEquals(new SomeClass().someProp, undefined);
    assertEquals(SomeClass.someProp, 100);
  },

  testUid() {
    const SomeClass = goog.defineClass(null, {constructor: function() {}});

    const obj1 = new SomeClass();
    const obj2 = new SomeClass();
    assertEquals(goog.getUid(obj1), goog.getUid(obj1));
    assertEquals(goog.getUid(obj2), goog.getUid(obj2));
    assertNotEquals(goog.getUid(obj1), goog.getUid(obj2));
  },
});
