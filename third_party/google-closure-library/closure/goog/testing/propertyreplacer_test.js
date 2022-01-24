/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.PropertyReplacerTest');
goog.setTestOnly();

const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TagName = goog.require('goog.dom.TagName');
const asserts = goog.require('goog.testing.asserts');
const dom = goog.require('goog.dom');
const isVersion = goog.require('goog.userAgent.product.isVersion');
const product = goog.require('goog.userAgent.product');
const testSuite = goog.require('goog.testing.testSuite');

function isSafari8() {
  return product.SAFARI && isVersion('8.0');
}

testSuite({
  // Test PropertyReplacer with JavaScript objects.
  /**
     @suppress {strictMissingProperties,missingProperties} suppression added to
     enable type checking
   */
  testSetJsProperties() {
    const stubs = new PropertyReplacer();
    const x = {a: 1, b: undefined};

    // Setting simple values.
    stubs.set(x, 'num', 1);
    assertEquals('x.num = 1', 1, x.num);
    stubs.set(x, 'undef', undefined);
    assertTrue('x.undef = undefined', 'undef' in x && x.undef === undefined);
    stubs.set(x, 'null', null);
    assertTrue('x["null"] = null', x['null'] === null);

    // Setting a simple value that existed originally.
    stubs.set(x, 'b', null);
    assertTrue('x.b = null', x.b === null);

    // Setting a complex value.
    stubs.set(x, 'obj', {});
    assertEquals('x.obj = {}', 'object', typeof x.obj);
    stubs.set(x.obj, 'num', 2);
    assertEquals('x.obj.num = 2', 2, x.obj.num);

    // Overwriting a leaf.
    stubs.set(x.obj, 'num', 3);
    assertEquals('x.obj.num = 3', 3, x.obj.num);

    // Overwriting a non-leaf.
    stubs.set(x, 'obj', {});
    assertFalse('x.obj = {} again', 'num' in x.obj);

    // Setting a function.
    stubs.set(x, 'func', (n) => n + 1);
    assertEquals('x.func = lambda n: n+1', 11, x.func(10));

    // Setting a constructor and a prototype method.
    stubs.set(x, 'Class', function(num) {
      this.num = num;
    });
    stubs.set(x.Class.prototype, 'triple', function() {
      return this.num * 3;
    });
    assertEquals('prototype method', 12, (new x.Class(4)).triple());

    // Cleaning up with UnsetAll() twice. The second run should have no effect.
    for (let i = 0; i < 2; i++) {
      stubs.reset();
      assertEquals('x.a preserved', 1, x.a);
      assertTrue('x.b reset', 'b' in x && x.b === undefined);
      assertFalse('x.num removed', 'num' in x);
      assertFalse('x.undef removed', 'undef' in x);
      assertFalse('x["null"] removed', 'null' in x);
      assertFalse('x.obj removed', 'obj' in x);
      assertFalse('x.func removed', 'func' in x);
      assertFalse('x.Class removed', 'Class' in x);
    }
  },

  // Test removing JavaScript object properties.
  testRemoveJsProperties() {
    const stubs = new PropertyReplacer();
    const orig = {'a': 1, 'b': undefined};
    const x = {'a': 1, 'b': undefined};

    stubs.remove(x, 'a');
    assertFalse('x.a removed', 'a' in x);
    assertTrue('x.b not removed', 'b' in x);
    stubs.reset();
    assertObjectEquals('x.a reset', x, orig);

    stubs.remove(x, 'b');
    assertFalse('x.b removed', 'b' in x);
    stubs.reset();
    assertObjectEquals('x.b reset', x, orig);

    stubs.set(x, 'c', 2);
    stubs.remove(x, 'c');
    assertObjectEquals('x.c added then removed', x, orig);
    stubs.reset();
    assertObjectEquals('x.c reset', x, orig);

    stubs.remove(x, 'b');
    stubs.set(x, 'b', undefined);
    assertObjectEquals('x.b removed then added', x, orig);
    stubs.reset();
    assertObjectEquals('x.b reset', x, orig);

    stubs.remove(x, 'd');
    assertObjectEquals('removing non-existing key', x, orig);
    stubs.reset();
    assertObjectEquals('reset removing non-existing key', x, orig);
  },

  // Test PropertyReplacer with prototype chain.
  /**
     @suppress {checkTypes,missingProperties} suppression added to enable type
     checking
   */
  testPrototype() {
    let stubs = new PropertyReplacer();

    // Simple inheritance.
    const a = {a: 0};
    function B() {}
    B.prototype = a;
    /** @suppress {checkTypes} suppression added to enable type checking */
    const b = new B();

    stubs.set(a, 0, 1);
    stubs.set(b, 0, 2);
    stubs.reset();
    assertEquals('a.a == 0', 0, a['a']);
    assertEquals('b.a == 0', 0, b['a']);

    // Inheritance with goog.inherits.
    const c = {a: 0};
    function C() {}
    C.prototype = c;
    function D() {}
    goog.inherits(D, C);
    /** @suppress {checkTypes} suppression added to enable type checking */
    const d = new D();

    stubs = new PropertyReplacer();
    stubs.set(c, 'a', 1);
    stubs.set(d, 'a', 2);
    stubs.reset();
    assertEquals('c.a == 0', 0, c['a']);
    assertEquals('d.a == 0', 0, d['a']);

    // Setting prototype fields.
    stubs.set(B.prototype, 'c', 'z');
    assertEquals('b.c=="z"', 'z', b.c);
    stubs.reset();
    assertFalse('b.c deleted', 'c' in b);

    // Setting Object.prototype's fields.
    stubs.set(Object.prototype, 'one', 1);
    assertEquals('b.one==1', 1, b.one);
    stubs.reset();
    assertFalse('b.one deleted', 'one' in b);

    stubs.set(Object.prototype, 'two', 2);
    stubs.remove(b, 'two');
    assertEquals('b.two==2', 2, b.two);
    stubs.remove(Object.prototype, 'two');
    assertFalse('b.two deleted', 'two' in b);
    stubs.reset();
    assertFalse('Object prototype reset', 'two' in b);
  },

  // Test replacing function properties.
  /** @suppress {missingProperties} suppression added to enable type checking */
  testFunctionProperties() {
    const stubs = new PropertyReplacer();
    stubs.set(Array, 'x', 1);
    assertEquals('Array.x==1', 1, Array.x);
    stubs.reset();
    assertFalse('Array.x deleted', 'x' in Array);

    stubs.set(Math.random, 'x', 1);
    assertEquals('Math.random.x==1', 1, Math.random.x);
    stubs.reset();
    assertFalse('Math.random.x deleted', 'x' in Math.random);
  },

  // Test the hasKey_ private method.
  /** @suppress {checkTypes} suppression added to enable type checking */
  testHasKey() {
    /** @suppress {visibility} suppression added to enable type checking */
    const f = PropertyReplacer.hasKey_;

    assertFalse('{}.a', f({}, 'a'));
    assertTrue('{a:0}.a', f({a: 0}, 'a'));

    function C() {}
    C.prototype.a = 0;
    assertFalse('C.prototype.a set, is C.a own?', f(C, 'a'));
    assertTrue('C.prototype.a', f(C.prototype, 'a'));
    assertFalse('C.a not set', f(C, 'a'));
    C.a = 0;
    assertTrue('C.a set', f(C, 'a'));

    /** @suppress {checkTypes} suppression added to enable type checking */
    const c = new C();
    assertFalse('C().a, inherited', f(c, 'a'));
    c.a = 0;
    assertTrue('C().a, own', f(c, 'a'));

    assertFalse('window, invalid key', f(window, 'no such key'));
    assertTrue('window, global variable', f(window, 'goog'));
    assertTrue('window, build-in key', f(window, 'location'));

    assertFalse('document, invalid key', f(document, 'no such key'));

    const div = dom.createElement(TagName.DIV);
    assertFalse('div, invalid key', f(div, 'no such key'));
    div['a'] = 0;
    assertTrue('div, key added by JS', f(div, 'a'));

    // hasKey_ returns false for these DOM properties on Safari 8. See
    // b/22044928.
    if (!isSafari8()) {
      assertTrue('div.className', f(div, 'className'));
      assertTrue('document.body', f(document, 'body'));
    }

    assertFalse('Date().getTime', f(new Date(), 'getTime'));
    assertTrue('Date.prototype.getTime', f(Date.prototype, 'getTime'));

    assertFalse('Math, invalid key', f(Math, 'no such key'));
    assertTrue('Math.random', f(Math, 'random'));

    function Parent() {}
    Parent.prototype.a = 0;
    function Child() {}
    goog.inherits(Child, Parent);
    assertFalse('goog.inherits, parent prototype', f(new Child, 'a'));
    Child.prototype.a = 1;
    assertFalse('goog.inherits, child prototype', f(new Child, 'a'));

    function OverwrittenProto() {}
    OverwrittenProto.prototype = {a: 0};
    assertFalse(f(new OverwrittenProto, 'a'));
  },

  // Test PropertyReplacer with DOM objects' built in attributes.
  testDomBuiltInAttributes() {
    const div = dom.createElement(TagName.DIV);
    div.id = 'old-id';

    const stubs = new PropertyReplacer();
    stubs.set(div, 'id', 'new-id');
    stubs.set(div, 'className', 'test-class');
    assertEquals('div.id == "new-id"', 'new-id', div.id);
    assertEquals('div.className == "test-class"', 'test-class', div.className);

    stubs.remove(div, 'className');

    // Removal of these DOM properties is not supported in Safari 8. See
    // b/22044928.
    if (!isSafari8()) {
      // '' in Firefox, undefined in Chrome.
      assertEvaluatesToFalse('div.className is empty', div.className);
      stubs.reset();
      assertEquals('div.id == "old-id"', 'old-id', div.id);
      assertEquals('div.name == ""', '', div.className);
    }
  },

  // Test PropertyReplacer with DOM objects' custom attributes.
  /** @suppress {missingProperties} suppression added to enable type checking */
  testDomCustomAttributes() {
    const div = dom.createElement(TagName.DIV);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    div.attr1 = 'old';

    const stubs = new PropertyReplacer();
    stubs.set(div, 'attr1', 'new');
    stubs.set(div, 'attr2', 'new');
    assertEquals('div.attr1 == "new"', 'new', div.attr1);
    assertEquals('div.attr2 == "new"', 'new', div.attr2);

    stubs.set(div, 'attr3', 'new');
    stubs.remove(div, 'attr3');
    assertEquals('div.attr3 == undefined', undefined, div.attr3);

    stubs.reset();
    assertEquals('div.attr1 == "old"', 'old', div.attr1);
    assertEquals('div.attr2 == undefined', undefined, div.attr2);
  },

  // Test PropertyReplacer trying to override a read-only property.
  testReadOnlyProperties() {
    const stubs = new PropertyReplacer();

    // Function.prototype.length should be read-only.
    const foo = (_) => {};
    assertThrows(
        'Trying to set a read-only property fails silently.',
        goog.bind(stubs.set, stubs, foo, 'length', 10));
    assertThrows(
        'Trying to replace a read-only property fails silently.',
        goog.bind(stubs.replace, stubs, foo, 'length', 10));

    // Array length should be undeletable.
    const a = [1, 2, 3];
    assertThrows(
        'Trying to remove a read-only property fails silently.',
        goog.bind(stubs.remove, stubs, a, 'length'));

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    window.foo = foo;
    assertThrows(
        'Trying to set a read-only property by path fails silently.',
        goog.bind(stubs.setPath, stubs, 'window.foo.length', 10));
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    window.foo = undefined;
  },

  // Test PropertyReplacer trying to override a style property doesn't trigger
  // read-only exception.
  testSettingStyleProperties() {
    const stubs = new PropertyReplacer();

    const div = document.createElement('div');
    // Ensures setting a pixel style value doesn't trigger the read-only
    // property exception, considering div.style.margin will return "0px"
    // instead of just "0".
    assertNotThrows(
        'Trying to set a read-only property fails silently.',
        goog.bind(stubs.set, stubs, div.style, 'margin', '0'));
  },

  // Test PropertyReplacer trying to override a sealed property.
  testSealedProperties() {
    if (typeof Object.seal !== 'function') {
      return;
    }

    const stubs = new PropertyReplacer();
    const sealed = Object.seal({a: 1});
    assertThrows(
        'Trying to set a new sealed property fails silently.',
        goog.bind(stubs.set, stubs, sealed, 'b', 2));
    assertNotThrows(
        'Trying to remove a new sealed property fails.',
        goog.bind(stubs.remove, stubs, sealed, 'b'));
    assertNotThrows(
        'Trying to remove a sealed property fails.',
        goog.bind(stubs.remove, stubs, sealed, 'a'));

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    window.sealed = sealed;
    assertThrows(
        'Trying to set a new sealed property by path fails silently in strict ' +
            'mode.',
        goog.bind(stubs.setPath, stubs, 'window.sealed.b', 2));

    (function() {
      // Test Object.seal() in strict mode, where the assignment itself throws
      // the error rather than our explicit consistency check.
      'use strict';

      const sealed = Object.seal({a: 1});
      assertThrows(
          'Trying to set a new sealed property fails silently in strict mode.',
          goog.bind(stubs.set, stubs, sealed, 'b', 2));
      assertNotThrows(
          'Trying to remove a new sealed property fails in strict mode.',
          goog.bind(stubs.remove, stubs, sealed, 'b'));
      assertNotThrows(
          'Trying to remove a sealed property fails in strict mode.',
          goog.bind(stubs.remove, stubs, sealed, 'a'));

      /**
       * @suppress {strictMissingProperties} suppression added to enable type
       * checking
       */
      window.sealed = sealed;
      assertThrows(
          'Trying to set a new sealed property by path fails silently in ' +
              'strict mode.',
          goog.bind(stubs.setPath, stubs, 'window.sealed.b', 2));
    })();

    delete window.sealed;
  },

  // Test PropertyReplacer trying to override a frozen property.
  testFrozenProperty() {
    if (typeof Object.freeze !== 'function') {
      return;
    }

    const stubs = new PropertyReplacer();
    const frozen = Object.freeze({a: 1});
    assertThrows(
        'Trying to set a new frozen property fails silently.',
        goog.bind(stubs.set, stubs, frozen, 'b', 2));
    assertThrows(
        'Trying to set a frozen property fails silently.',
        goog.bind(stubs.set, stubs, frozen, 'a', 2));
    assertThrows(
        'Trying to replace a frozen property fails silently.',
        goog.bind(stubs.replace, stubs, frozen, 'a', 2));
    assertNotThrows(
        'Trying to remove a new frozen property fails.',
        goog.bind(stubs.remove, stubs, frozen, 'b'));
    assertThrows(
        'Trying to remove a frozen property fails silently.',
        goog.bind(stubs.remove, stubs, frozen, 'a'));

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    window.frozen = frozen;
    assertThrows(
        'Trying to set a frozen property by path fails silently.',
        goog.bind(stubs.setPath, stubs, 'window.frozen.a', 2));

    (function() {
      // Test Object.freeze() in strict mode, where the assignment itself throws
      // the error rather than our explicit consistency check.
      'use strict';

      const frozen = Object.freeze({a: 1});
      assertThrows(
          'Trying to set a new frozen property fails silently in strict mode.',
          goog.bind(stubs.set, stubs, frozen, 'b', 2));
      assertThrows(
          'Trying to ovewrite a frozen property fails silently in strict mode.',
          goog.bind(stubs.set, stubs, frozen, 'a', 2));
      assertThrows(
          'Trying to replace a frozen property fails silently in strict mode.',
          goog.bind(stubs.replace, stubs, frozen, 'a', 2));
      assertNotThrows(
          'Trying to remove a new frozen property fails in strict mode.',
          goog.bind(stubs.remove, stubs, frozen, 'b'));
      assertThrows(
          'Trying to remove a frozen property fails silently in strict mode.',
          goog.bind(stubs.remove, stubs, frozen, 'a'));

      /**
       * @suppress {strictMissingProperties} suppression added to enable type
       * checking
       */
      window.frozen = frozen;
      assertThrows(
          'Trying to set a new frozen property by path fails silently in ' +
              'strict mode.',
          goog.bind(stubs.setPath, stubs, 'window.frozen.b', 2));
      assertThrows(
          'Trying to set a frozen property by path fails silently in strict ' +
              'mode.',
          goog.bind(stubs.setPath, stubs, 'window.frozen.a', 2));
    })();

    delete window.frozen;
  },

  // Test PropertyReplacer overriding Math.random.
  testMathRandom() {
    const stubs = new PropertyReplacer();
    stubs.set(Math, 'random', () => -1);
    assertEquals('mocked Math.random', -1, Math.random());

    stubs.reset();
    assertNotEquals('original Math.random', -1, Math.random());
  },

  // Tests the replace method of PropertyReplacer.
  testReplace() {
    const stubs = new PropertyReplacer();
    function C() {
      this.a = 1;
    }
    C.prototype.b = 1;
    C.prototype.toString = () => 'obj';
    /** @suppress {checkTypes} suppression added to enable type checking */
    const obj = new C();

    stubs.replace(obj, 'a', 2);
    assertEquals(
        'successfully replaced the own property of an object', 2, obj.a);

    stubs.replace(obj, 'b', 2);
    assertEquals(
        'successfully replaced the property in the prototype', 2, obj.b);

    let error = assertThrows(
        'cannot replace missing key',
        goog.bind(stubs.replace, stubs, obj, 'unknown', 1));
    // Using assertContains instead of assertEquals because Opera 10.0 adds
    // the stack trace to the error message.
    assertEquals(
        'error message for missing key',
        'Cannot replace missing property "unknown" in obj', error.message);
    assertFalse('object not touched', 'unknown' in obj);

    error = assertThrows(
        'cannot change value type',
        goog.bind(stubs.replace, stubs, obj, 'a', '3'));
    assertContains(
        'error message for type mismatch',
        'Cannot replace property "a" in obj with a value of different type',
        error.message);

    assertThrows(
        'cannot change value type to null',
        goog.bind(stubs.replace, stubs, obj, 'a', null));

    assertThrows(
        'cannot change value type to undefined',
        goog.bind(stubs.replace, stubs, obj, 'a', undefined));
  },

  testReplaceAllowNullOrUndefined() {
    const stubs = new PropertyReplacer();
    const obj = {value: 1, zero: 0, emptyString: ''};

    stubs.replace(obj, 'value', undefined, true);
    assertUndefined(
        'Expected int value to be replaced with undefined', obj.value);

    stubs.replace(obj, 'value', 'b', true);
    assertEquals(
        'Expected undefined value to be replace with string', 'b', obj.value);

    stubs.replace(obj, 'value', null, true);
    assertNull('Expected string value to be replaced with null', obj.value);

    stubs.replace(obj, 'value', 1, true);
    assertEquals(
        'Expected null value to be replaced with non-null', 1, obj.value);

    // Replacing 0 with a string or empty string with a number is not allowed.
    assertThrows(
        'Cannot change value type',
        goog.bind(stubs.replace, stubs, obj, 'zero', 'a', true));

    assertThrows(
        'Cannot change value type',
        goog.bind(stubs.replace, stubs, obj, 'emptyString', 1, true));
  },

  // Tests altering complete namespace paths.
  /** @suppress {missingProperties} suppression added to enable type checking */
  testSetPath() {
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    globalThis.a = {b: {}};
    const stubs = new PropertyReplacer();

    stubs.setPath('a.b.c.d', 1);
    assertObjectEquals('a.b.c.d=1', {b: {c: {d: 1}}}, globalThis.a);
    stubs.setPath('a.b.e', 2);
    assertObjectEquals('a.b.e=2', {b: {c: {d: 1}, e: 2}}, globalThis.a);
    stubs.setPath('a.f', 3);
    assertObjectEquals('a.f=3', {b: {c: {d: 1}, e: 2}, f: 3}, globalThis.a);
    stubs.setPath('a.f.g', 4);
    assertObjectEquals(
        'a.f.g=4', {b: {c: {d: 1}, e: 2}, f: {g: 4}}, globalThis.a);
    stubs.setPath('a', 5);
    assertEquals('a=5', 5, globalThis.a);

    stubs.setPath('x.y.z', 5);
    assertObjectEquals('x.y.z=5', {y: {z: 5}}, globalThis.x);

    stubs.reset();
    assertObjectEquals('a.* reset', {b: {}}, globalThis.a);
    // NOTE: it's impossible to delete global variables in Internet Explorer,
    // so ('x' in globalThis) would be true.
    assertUndefined('x.* reset', globalThis.x);
  },

  // Tests altering paths with functions in them.
  /**
     @suppress {missingProperties,checkTypes} suppression added to enable type
     checking
   */
  testSetPathWithFunction() {
    const f = function() {};
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    globalThis.a = {b: f};
    const stubs = new PropertyReplacer();

    stubs.setPath('a.b.c', 1);
    assertEquals('a.b.c=1, f kept', f, globalThis.a.b);
    assertEquals('a.b.c=1, c set', 1, globalThis.a.b.c);

    stubs.setPath('a.b.prototype.d', 2);
    assertEquals('a.b.prototype.d=2, b kept', f, globalThis.a.b);
    assertEquals('a.b.prototype.d=2, c kept', 1, globalThis.a.b.c);
    assertFalse('a.b.prototype.d=2, a.b.d not set', 'd' in globalThis.a.b);
    assertTrue('a.b.prototype.d=2, proto set', 'd' in globalThis.a.b.prototype);
    assertEquals('a.b.prototype.d=2, d set', 2, new globalThis.a.b().d);

    const invalidSetPath = () => {
      stubs.setPath('a.prototype.e', 3);
    };
    assertThrows('setting the prototype of a non-function', invalidSetPath);

    stubs.reset();
    assertObjectEquals('a.b.c reset', {b: f}, globalThis.a);
    assertObjectEquals('a.b.prototype reset', {}, globalThis.a.b.prototype);
  },

  // Tests restoring original attribute values with restore() rather than
  // reset().
  /**
     @suppress {strictMissingProperties,missingProperties} suppression added to
     enable type checking
   */
  testRestore() {
    const stubs = new PropertyReplacer();
    const x = {a: 1, b: undefined};

    // Setting simple value.
    stubs.set(x, 'num', 1);
    assertEquals('x.num = 1', 1, x.num);
    stubs.restore(x, 'num');
    assertFalse('x.num removed', 'num' in x);

    // Setting undefined value.
    stubs.set(x, 'undef', undefined);
    assertTrue('x.undef = undefined', 'undef' in x && x.undef === undefined);
    stubs.restore(x, 'undef');
    assertFalse('x.undef removed', 'undef' in x);

    // Setting null value.
    stubs.set(x, 'null', null);
    assertTrue('x["null"] = null', x['null'] === null);
    stubs.restore(x, 'null');
    assertFalse('x["null"] removed', 'null' in x);

    // Setting a simple value that existed originally.
    stubs.set(x, 'b', null);
    assertTrue('x.b = null', x.b === null);

    // Setting a complex value.
    stubs.set(x, 'obj', {});
    assertEquals('x.obj = {}', 'object', typeof x.obj);
    stubs.set(x.obj, 'num', 2);
    assertEquals('x.obj.num = 2', 2, x.obj.num);
    stubs.restore(x.obj, 'num');
    assertFalse('x.obj.num removed', 'num' in x.obj);
    stubs.restore(x, 'obj');
    assertFalse('x.obj removed', 'obj' in x);

    // Setting a function.
    stubs.set(x, 'func', (n) => n + 1);
    assertEquals('x.func = lambda n: n+1', 11, x.func(10));
    stubs.restore(x, 'func');
    assertFalse('x.func removed', 'func' in x);

    // Setting a constructor and a prototype method.
    stubs.set(x, 'Class', function(num) {
      this.num = num;
    });
    stubs.set(x.Class.prototype, 'triple', function() {
      return this.num * 3;
    });
    assertEquals('prototype method', 12, (new x.Class(4)).triple());
    stubs.restore(x, 'Class');
    assertFalse('x.Class removed', 'Class' in x);

    // Final cleanup with reset(). This should have no effect:
    // all assertions about the original state shall still hold.
    stubs.reset();
    assertEquals('x.a preserved', 1, x.a);
    assertTrue('x.b reset', 'b' in x && x.b === undefined);
    assertFalse('x.num removed', 'num' in x);
    assertFalse('x.undef removed', 'undef' in x);
    assertFalse('x["null"] removed', 'null' in x);
    assertFalse('x.obj removed', 'obj' in x);
    assertFalse('x.func removed', 'func' in x);
    assertFalse('x.Class removed', 'Class' in x);
  },

  // Tests restore() with invalid arguments.
  testRestoreWithInvalidArguments() {
    const stubs = new PropertyReplacer();
    const x = {a: 1, b: undefined};
    const y = {a: 1};

    stubs.set(x, 'a', 42);

    assertThrows(
        'Trying to restore state of an unmodified property',
        goog.bind(stubs.restore, stubs, x, 'b'));
    assertThrows(
        'Trying to restore state of a non-existing property',
        goog.bind(stubs.restore, stubs, x, 'not_here'));
    assertThrows(
        'Trying to restore state of an unmodified object',
        goog.bind(stubs.restore, stubs, y, 'a'));
  },
});
