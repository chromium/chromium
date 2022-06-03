/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.delegate.DelegateRegistryTest');
goog.setTestOnly();

const DelegateRegistry = goog.require('goog.delegate.DelegateRegistry');
const testSuite = goog.require('goog.testing.testSuite');
const {zip} = goog.require('goog.array');

/** Always throws. */
const THROW = () => {
  throw new Error('Unexpected call');
};

class Foo {
  constructor(arg = undefined) {
    this.arg = arg;
  }
  toString() {
    return `Foo(${this.arg !== undefined ? this.arg : ''})`;
  }
}

class Bar {
  constructor(arg = undefined) {
    this.arg = arg;
  }
  toString() {
    return `Bar(${this.arg !== undefined ? this.arg : ''})`;
  }
}

testSuite({
  shouldRunTests() {
    return typeof Array.prototype.map == 'function';
  },

  testBasic_registerNothing() {
    /** @type {!DelegateRegistry<!Foo>} */
    const registry = new DelegateRegistry();

    assertArrayEquals([], registry.delegates(THROW));
    assertUndefined(registry.delegate(THROW));
  },

  testBasic_registerClass() {
    /** @type {!DelegateRegistry<!Foo>} */
    const registry = new DelegateRegistry().expectAtMostOneDelegate();

    registry.registerClass(Foo);

    const delegates = registry.delegates();
    assertEquals('Foo()', delegates.join(' '));
    // Make sure we get a new instance.
    assertNotEquals(delegates[0], registry.delegates()[0]);

    assertEquals('Foo(42)', registry.delegates(ctor => new ctor(42)).join(' '));

    const delegate = registry.delegate();
    assertEquals('Foo()', delegate.toString());
    assertNotEquals(delegates[0], delegate);

    assertEquals('Foo(42)', registry.delegate(ctor => new ctor(42)).toString());
  },

  testBasic_registerInstance() {
    /** @type {!DelegateRegistry<!Foo>} */
    const registry = new DelegateRegistry();
    const delegate = new Foo(23);

    registry.registerInstance(delegate);
    assertArrayEquals([delegate], registry.delegates(THROW));
    assertEquals(delegate, registry.delegate(THROW));
  },

  testBasic_registerMultiple() {
    /** @type {!DelegateRegistry<!Foo|!Bar>} */
    const registry = new DelegateRegistry();

    registry.registerClass(Bar);
    registry.registerInstance(new Foo(23));
    registry.registerClass(Foo);

    const delegates = registry.delegates();
    assertEquals('Bar() Foo(23) Foo()', delegates.join(' '));
    const delegates2 = registry.delegates();
    assertArrayEquals(
        [false, true, false],
        zip(delegates, delegates2).map(([a, b]) => a == b));

    assertEquals(
        'Bar(42) Foo(23) Foo(42)',
        registry.delegates(ctor => new ctor(42)).join(' '));

    assertEquals('Bar()', registry.delegate().toString());
    assertNotEquals(delegates[0], registry.delegate());
  },

  testPrioritized_registerClass() {
    /** @type {!DelegateRegistry.Prioritized<?>} */
    const registry = new DelegateRegistry.Prioritized();
    registry.registerClass(Foo, 0);
    registry.registerClass(Bar, 10);
    assertEquals(
        'Bar(2) Foo(2)', registry.delegates(ctor => new ctor(2)).join(' '));
  },

  testPrioritized_registerInstance() {
    /** @type {!DelegateRegistry.Prioritized<string>} */
    const registry = new DelegateRegistry.Prioritized();
    registry.registerInstance('o', 10);
    registry.registerInstance('l', 20);
    registry.registerInstance('r', -5);
    registry.registerInstance('s', 5);
    registry.registerInstance('c', 25);
    registry.registerInstance('e', -10);
    registry.registerInstance('u', 0);
    assertEquals('closure', registry.delegates(THROW).join(''));
    assertEquals('c', registry.delegate(THROW));

    // Duplicate priority
    assertThrows(() => registry.registerInstance('x', 5));
  },

  testAllowLateRegistration() {
    /** @type {!DelegateRegistry<!Foo|!Bar>} */
    const registry = new DelegateRegistry();
    assertUndefined(registry.delegate(THROW));

    assertThrows(() => registry.registerClass(Bar));
    assertThrows(() => registry.registerInstance(new Bar()));

    assertEquals(registry, registry.allowLateRegistration());

    registry.registerClass(Foo);
    registry.registerInstance(new Foo(23));

    assertEquals('Foo() Foo(23)', registry.delegates().join(' '));
  },

  testCacheInstantiation() {
    /** @type {!DelegateRegistry<!Foo>} */
    const registry = new DelegateRegistry();
    registry.registerClass(Foo);
    registry.registerInstance(new Foo(5));
    const d1 = registry.delegates(ctor => new ctor(42));
    assertEquals('Foo(42) Foo(5)', d1.join(' '));
    const d2 = registry.delegates(ctor => new ctor(23));
    assertEquals('Foo(23) Foo(5)', d2.join(' '));
    assertEquals(d1[1], d2[1]);

    assertEquals(registry, registry.cacheInstantiation());
    const d3 = registry.delegates(ctor => new ctor(56));
    assertEquals('Foo(56) Foo(5)', d3.join(' '));
    assertArrayEquals(d3, registry.delegates(THROW));
  },

  testCacheInstantiation_withLateRegistration() {
    /** @type {!DelegateRegistry<!Foo>} */
    const registry =
        new DelegateRegistry().cacheInstantiation().allowLateRegistration();
    registry.registerClass(Foo);
    registry.registerClass(Foo);
    assertEquals('Foo(42)', registry.delegate(ctor => new ctor(42)).toString());
    assertEquals(
        'Foo(42) Foo(3)', registry.delegates(ctor => new ctor(3)).join(' '));
    registry.registerClass(Foo);
    assertEquals(
        'Foo(42) Foo(3) Foo(99)',
        registry.delegates(ctor => new ctor(99)).join(' '));
  },

  testExpectAtMostOneDelegate() {
    /** @type {!DelegateRegistry<!Foo>} */
    const registry = new DelegateRegistry();
    assertEquals(registry, registry.expectAtMostOneDelegate());
    registry.registerClass(Foo);
    assertThrows(() => registry.registerClass(Foo));
  },
});
