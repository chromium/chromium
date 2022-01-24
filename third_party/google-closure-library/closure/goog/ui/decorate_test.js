/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.decorateTest');
goog.setTestOnly();

const decorate = goog.require('goog.ui.decorate');
const registry = goog.require('goog.ui.registry');
const testSuite = goog.require('goog.testing.testSuite');

// Fake component and renderer implementations, for testing only.
// UnknownComponent has no default renderer or decorator registered.
class UnknownComponent {}

// FakeComponentX's default renderer is FakeRenderer.  It also has a
// decorator.
class FakeComponentX {
  constructor() {
    this.element = null;
  }

  decorate(element) {
    this.element = element;
  }
}

// FakeComponentY doesn't have an explicitly registered default
// renderer; it should inherit the default renderer from its superclass.
// It does have a decorator registered.
class FakeComponentY extends FakeComponentX {
  constructor() {
    super();
  }
}

// FakeComponentZ is just another component.  Its default renderer is
// FakeSingletonRenderer, but it has no decorator registered.
class FakeComponentZ {}

// FakeRenderer is a stateful renderer.
class FakeRenderer {}

// FakeSingletonRenderer is a stateless renderer that can be used as a
// singleton.
class FakeSingletonRenderer {
  static getInstance() {
    return instance;
  }
}

let instance = new FakeSingletonRenderer();

testSuite({
  setUp() {
    registry.setDefaultRenderer(FakeComponentX, FakeRenderer);
    registry.setDefaultRenderer(FakeComponentZ, FakeSingletonRenderer);

    registry.setDecoratorByClassName(
        'fake-component-x', () => new FakeComponentX());
    registry.setDecoratorByClassName(
        'fake-component-y', () => new FakeComponentY());
  },

  tearDown() {
    registry.reset();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testDecorate() {
    const dx = decorate(document.getElementById('x'));
    assertTrue(
        'Decorator for element with fake-component-x class must be ' +
            'a FakeComponentX',
        dx instanceof FakeComponentX);
    assertEquals(
        'Element x must have been decorated', document.getElementById('x'),
        dx.element);

    const dy = decorate(document.getElementById('y'));
    assertTrue(
        'Decorator for element with fake-component-y class must be ' +
            'a FakeComponentY',
        dy instanceof FakeComponentY);
    assertEquals(
        'Element y must have been decorated', document.getElementById('y'),
        dy.element);

    const dz = decorate(document.getElementById('z'));
    assertNull('Decorator for element with unknown class must be null', dz);

    const du = decorate(document.getElementById('u'));
    assertNull('Decorator for element without CSS class must be null', du);
  },
});
