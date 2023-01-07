// Copyright 2006 The Closure Library Authors. All Rights Reserved.
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


/**
 * @fileoverview Unit tests for Closure's base.js that require debug loading.
 * @suppress {missingRequire}
 */

goog.provide('goog.baseDebugLoaderTest');

goog.setTestOnly('goog.baseDebugLoaderTest');

// Used to test dynamic loading works, see testRequire*
goog.require('goog.Timer');
goog.require('goog.dom');
goog.require('goog.functions');
goog.require('goog.object');
goog.require('goog.test_module');
goog.require('goog.testing.PropertyReplacer');
goog.require('goog.testing.jsunit');

// NOTE: using computed properties to avoid compiler checks
const earlyTestModuleGet = goog['module'].get('goog.test_module');

var stubs = new goog.testing.PropertyReplacer();
var originalGoogBind = goog.bind;
var autoLoadDep = true;

/**
 * @param {string} path
 * @param {string} relativePath
 * @param {!Array<string>} provides
 * @param {!Array<string>} requires
 * @param {!Object<string, string>} loadFlags
 * @param {boolean} needsTranspile
 * @struct @constructor
 * @extends {goog.Dependency}
 */
function FakeDependency(
    path, relativePath, provides, requires, loadFlags, needsTranspile) {
  FakeDependency.base(
      this, 'constructor', path, relativePath, provides, requires, loadFlags);
  this.loadCalled = false;
  this.needsTranspile = needsTranspile;
}
goog.inherits(FakeDependency, goog.Dependency);


/**
 * @override
 * @suppress {missingProperties,checkTypes}
 */
FakeDependency.prototype.load = function(controller) {
  this.markLoaded = function() {
    delete this.markLoaded;
    controller.loaded();
  };

  this.loadCalled = true;
  if (autoLoadDep) {
    this.markLoaded(controller);
  }
};


/**
 * @struct @constructor
 * @extends {goog.DependencyFactory}
 * @suppress {checkTypes}
 */
function FakeDependencyFactory() {
  this.realFactory = new goog.DependencyFactory();
}
goog.inherits(FakeDependencyFactory, goog.DependencyFactory);


/** @override */
FakeDependencyFactory.prototype.createDependency = function(
    path, relativePath, provides, requires, loadFlags, needsTranspile) {
  var dep = new FakeDependency(
      path, relativePath, provides, requires, loadFlags, needsTranspile);
  testDependencies.push(dep);
  realDependencies.push(this.realFactory.createDependency(
      path, relativePath, provides, requires, loadFlags, needsTranspile));
  return dep;
};


var testDependencies = [];
var realDependencies = [];

// Avoid the compiler checks on goog.addDependency
const addDependency = goog.addDependency;


function setUpPage() {
  goog.global['CLOSURE_NO_DEPS'] = true;
}


/** @suppress {visibility,const} */
function setUp() {
  autoLoadDep = true;
  testDependencies = [];
  realDependencies = [];
  goog.debugLoader_ = new goog.DebugLoader_();
  goog.setDependencyFactory(new FakeDependencyFactory());
}


function tearDown() {
  // Use computed property here to avoid the compiler check that the value
  // is valid.
  goog['setCssNameMapping'](undefined);
  stubs.reset();
  goog.bind = originalGoogBind;
}

/** @suppress {constantProperty,visibility,checkTypes} */
function testInHtmlDocument() {
  var savedGoogGlobal = goog.global;
  try {
    goog.global = {};
    assertFalse(goog.inHtmlDocument_());
    goog.global.document = {};
    assertFalse(goog.inHtmlDocument_());
    goog.global.document.write = function() {};
    assertTrue(goog.inHtmlDocument_());
  } finally {
    // Restore context to respect other tests.
    goog.global = savedGoogGlobal;
  }
}

/** @suppress {constantProperty,missingRequire,visibility,checkTypes} */
function testLoadInNonHtmlNotThrows() {
  var savedGoogGlobal = goog.global;
  try {
    goog.global = {};
    goog.global.document = {};
    assertFalse(goog.inHtmlDocument_());
    // The goog code which is executed at load.
    goog.findBasePath_();
    goog.setDependencyFactory(new goog.DependencyFactory());
    addDependency('loadInNonHtmlNotThrows', ['load.InNonHtmlNotThrows'], []);
    var require = goog.require;
    require('load.InNonHtmlNotThrows');
  } finally {
    // Restore context to respect other tests.
    goog.global = savedGoogGlobal;
  }
}

/** @suppress {constantProperty,visibility,checkTypes} */
function testLoadBaseWithQueryParamOk() {
  var savedGoogGlobal = goog.global;
  try {
    goog.global = {};
    goog.global.document = {
      write: goog.nullFunction,
      getElementsByTagName:
          goog.functions.constant([{src: '/path/to/base.js?zx=5'}])
    };
    assertTrue(goog.inHtmlDocument_());
    goog.findBasePath_();
    assertEquals('/path/to/', goog.basePath);
  } finally {
    // Restore context to respect other tests.
    goog.global = savedGoogGlobal;
  }
}

/** @suppress {constantProperty,visibility,checkTypes} */
function testLoadBaseFromGlobalVariableOk() {
  var savedGoogGlobal = goog.global;
  try {
    goog.global = {};
    goog.global.document = {
      write: goog.nullFunction,
      getElementsByTagName:
          goog.functions.constant([{src: '/path/to/base.js?zx=5'}])
    };
    goog.global.CLOSURE_BASE_PATH = '/from/constant/';
    goog.findBasePath_();
    assertEquals(goog.global.CLOSURE_BASE_PATH, goog.basePath);
  } finally {
    // Restore context to respect other tests.
    goog.global = savedGoogGlobal;
  }
}

/** @suppress {constantProperty,visibility,checkTypes} */
function testLoadBaseFromGlobalVariableDOMClobbered() {
  var savedGoogGlobal = goog.global;
  try {
    goog.global = {};
    goog.global.document = {
      write: goog.nullFunction,
      getElementsByTagName:
          goog.functions.constant([{src: '/path/to/base.js?zx=5'}])
    };
    // Make goog.global.CLOSURE_BASE_PATH an object with a toString, like
    // it would be if it were a DOM clobbered HTMLElement.
    goog.global.CLOSURE_BASE_PATH = {};
    goog.global.CLOSURE_BASE_PATH.toString = function() {
      return '/from/constant/';
    };
    goog.findBasePath_();
    assertEquals('/path/to/', goog.basePath);
  } finally {
    // Restore context to respect other tests.
    goog.global = savedGoogGlobal;
  }
}

/** @suppress {constantProperty,visibility,checkTypes} */
function testLoadBaseFromCurrentScriptIgnoringOthers() {
  var savedGoogGlobal = goog.global;
  try {
    goog.global = {};
    goog.global.document = {
      write: goog.nullFunction,
      currentScript: {src: '/currentScript/base.js?zx=5'},
      getElementsByTagName:
          goog.functions.constant([{src: '/path/to/base.js?zx=5'}])
    };
    goog.findBasePath_();
    assertEquals('/currentScript/', goog.basePath);
  } finally {
    // Restore context to respect other tests.
    goog.global = savedGoogGlobal;
  }
}

/** @suppress {undefinedVars,checkTypes} testDep */
function testAddDependency() {
  addDependency('foo.js', ['testDep.foo'], ['testDep.bar']);

  // alias to avoid the being picked up by the deps scanner.
  var provide = goog.provide;

  provide('testDep.bar');

  // To differentiate this call from the real one.
  var require = goog.require;

  // this used to throw an exception
  require('testDep.foo');

  assertTrue(goog.isObject(testDep.bar));

  // Unset provided namespace so the test can be re-run.
  testDep = undefined;
}


/**
 * Asserts all test deps up to and including the expected index have loaded,
 * and anything past that has not.
 *
 * @param {number} expected
 */
function assertLoaded(expected) {
  for (var i = 0; i < testDependencies.length; i++) {
    assertEquals(expected >= i, testDependencies[i].loadCalled);
  }
}


/**
 * @param {!goog.Dependency} dep
 * @param {string} relPath
 * @param {!Array<string>} provides
 * @param {!Array<string>} requires
 * @param {!Object<string, string>} loadFlags
 */
function assertDepData(dep, relPath, provides, requires, loadFlags) {
  assertEquals(relPath, dep.relativePath);
  assertArrayEquals(provides, dep.provides);
  assertArrayEquals(requires, dep.requires);
  assertTrue(goog.object.equals(loadFlags, dep.loadFlags));
}


function testAddDependencyModule() {
  addDependency('mod.js', ['testDep.mod'], [], true);
  addDependency('empty.js', ['testDep.empty'], [], {});
  addDependency('mod-goog.js', ['testDep.goog'], [], {'module': 'goog'});

  // To differentiate this call from the real one.
  var require = goog.require;

  require('testDep.mod');
  assertLoaded(0);
  assertDepData(
      realDependencies[0], 'mod.js', ['testDep.mod'], [], {'module': 'goog'});
  assertTrue(realDependencies[0] instanceof goog.GoogModuleDependency);

  require('testDep.empty');
  assertLoaded(1);
  assertDepData(realDependencies[1], 'empty.js', ['testDep.empty'], [], {});
  assertTrue(realDependencies[0] instanceof goog.Dependency);

  require('testDep.goog');
  assertLoaded(2);
  assertDepData(
      realDependencies[2], 'mod-goog.js', ['testDep.goog'], [],
      {'module': 'goog'});
  assertTrue(realDependencies[2] instanceof goog.GoogModuleDependency);

  // Unset provided namespace so the test can be re-run.
  testDep = undefined;
}


function testAddDependencyEs6Module() {
  addDependency('mod-es6.js', ['testDep.es6'], [], {'module': 'es6'});

  // To differentiate this call from the real one.
  var require = goog.require;

  require('testDep.es6');
  assertLoaded(0);
  assertDepData(
      realDependencies[0], 'mod-es6.js', ['testDep.es6'], [],
      {'module': 'es6'});
  assertTrue(
      realDependencies[0] instanceof
      ('noModule' in document.createElement('script') ?
           goog.Es6ModuleDependency :
           goog.TranspiledDependency));
}

/** @suppress {visibility} */
function testAddDependencyTranspile() {
  var requireTranspilation = false;
  stubs.set(goog.transpiler_, 'needsTranspile', function() {
    return requireTranspilation;
  });

  addDependency(
      'fancy.js', ['testDep.fancy'], [], {'lang': 'es6', 'module': 'goog'});
  requireTranspilation = true;
  addDependency('super.js', ['testDep.superFancy'], [], {'lang': 'es7'});

  assertFalse(testDependencies[0].needsTranspile);
  assertTrue(testDependencies[1].needsTranspile);

  // Unset provided namespace so the test can be re-run.
  testDep = undefined;
}


async function testBootstrap() {
  addDependency('a.js', ['a'], [], {});
  addDependency('b.js', ['b'], ['a'], {});
  addDependency('c.js', ['c'], [], {});

  assertEquals(3, testDependencies.length);

  await new Promise((resolve, reject) => {
    goog.bootstrap(['b', 'c'], function() {
      resolve();
    });
  });

  for (var i = 0; i < testDependencies.length; i++) {
    assertTrue(testDependencies[i].loadCalled);
  }
}


/** @suppress {missingProperties} */
async function testBootstrapDelayLoadingDep() {
  autoLoadDep = false;

  addDependency('a.js', ['a'], [], {});
  addDependency('b.js', ['b'], ['a'], {});
  addDependency('c.js', ['c'], [], {});

  assertEquals(3, testDependencies.length);

  let bootstrapped = false;
  goog.bootstrap(['b', 'c'], () => {
    bootstrapped = true;
  });

  for (var i = 0; i < testDependencies.length; i++) {
    assertTrue(testDependencies[i].loadCalled);
  }
  assertFalse(bootstrapped);

  // Loading shallow deps should do nothing.
  testDependencies[0].markLoaded();
  // Loading one of the two requested deps shouldn't call the function.
  testDependencies[2].markLoaded();

  await new Promise(resolve => setTimeout(resolve, 50));

  assertFalse(bootstrapped);
  testDependencies[1].markLoaded();
  assertFalse(bootstrapped);

  // Await setTimeout so we enter the macrotask queue (goog.boostrap uses
  // setTimeout, which is a macrotask. Native Promises (non-polyfill) are
  // microtasks and would resolve before macrotasks.
  await new Promise(resolve => setTimeout(resolve, 0));

  assertTrue(bootstrapped);
}


//=== tests for Provide logic ===

/** @suppress {missingProperties} */
function testProvide() {
  // alias to avoid the being picked up by the deps scanner.
  const provide = goog.provide;

  provide('goog.test.name.space');
  assertNotUndefined('provide failed: goog.test', goog.test);
  assertNotUndefined('provide failed: goog.test.name', goog.test.name);
  assertNotUndefined(
      'provide failed: goog.test.name.space', goog.test.name.space);

  // ensure that providing 'goog.test.name' doesn't throw an exception
  provide('goog.test');
  provide('goog.test.name');
  delete goog.test;
}


// "watch" is a native member of Object.prototype on Firefox
// Ensure it can still be added as a namespace
/** @suppress {missingProperties} */
function testProvideWatch() {
  // alias to avoid the being picked up by the deps scanner.
  const provide = goog.provide;

  provide('goog.yoddle.watch');
  assertNotUndefined('provide failed: goog.yoddle.watch', goog.yoddle.watch);
  delete goog.yoddle;
}

/** @suppress {missingProperties} */
function testProvideStrictness() {
  // alias to avoid the being picked up by the deps scanner.
  const provide = goog.provide;

  provide('goog.xy');
  assertProvideFails('goog.xy');

  provide('goog.xy.z');
  assertProvideFails('goog.xy');

  window['goog']['xyz'] = 'Bob';
  assertProvideFails('goog.xyz');

  delete goog.xy;
  delete goog.xyz;
}

/** @param {?} namespace */
function assertProvideFails(namespace) {
  assertThrows(
      'goog.provide(' + namespace + ') should have failed',
      goog.partial(goog.provide, namespace));
}


/** @suppress {visibility} */
function testIsProvided() {
  // alias to avoid the being picked up by the deps scanner.
  const provide = goog.provide;

  provide('goog.explicit');
  assertTrue(goog.isProvided_('goog.explicit'));
  provide('goog.implicit.explicit');
  assertFalse(goog.isProvided_('goog.implicit'));
  assertTrue(goog.isProvided_('goog.implicit.explicit'));
}


//=== tests for Require logic ===

function testRequireClosure() {
  assertNotUndefined('goog.Timer should be available', goog.Timer);
  /** @suppress {missingRequire} */
  assertNotUndefined(
      'goog.events.EventTarget should be available', goog.events.EventTarget);
}

function testRequireWithExternalDuplicate() {
  // alias to avoid the being picked up by the deps scanner.
  const provide = goog.provide;

  // Do a provide without going via goog.require. Then goog.require it
  // indirectly and ensure it doesn't cause a duplicate script.
  addDependency('dup.js', ['dup.base'], []);
  addDependency('dup-child.js', ['dup.base.child'], ['dup.base']);
  provide('dup.base');

  stubs.set(goog, 'isDocumentFinishedLoading_', false);
  stubs.set(goog.global, 'CLOSURE_IMPORT_SCRIPT', function(src) {
    if (src == goog.basePath + 'dup.js') {
      fail('Duplicate script written!');
    } else if (src == goog.basePath + 'dup-child.js') {
      // Allow expected script.
      return true;
    } else {
      // Avoid affecting other state.
      return false;
    }
  });

  // To differentiate this call from the real one.
  const require = goog.require;
  require('dup.base.child');
}

/**
 * @suppress {undefinedVars,visibility,missingProperties} 'far' defined by
 * goog.provide
 */
function testGoogRequireCheck() {
  // alias to avoid the being picked up by the deps scanner.
  const provide = goog.provide;

  stubs.set(goog, 'ENABLE_DEBUG_LOADER', true);
  stubs.set(goog, 'useStrictRequires', true);
  stubs.set(goog, 'implicitNamespaces_', {});

  // Aliased so that build tools do not mistake this for an actual call.
  const require = goog.require;
  // should not throw
  require('far.outnotprovided');

  assertUndefined(goog.global.far);
  assertEvaluatesToFalse(goog.getObjectByName('far.out'));
  assertObjectEquals({}, goog.implicitNamespaces_);
  assertFalse(goog.isProvided_('far.out'));

  provide('far.out');

  assertNotUndefined(far.out);
  assertEvaluatesToTrue(goog.getObjectByName('far.out'));
  assertObjectEquals({'far': true}, goog.implicitNamespaces_);
  assertTrue(goog.isProvided_('far.out'));

  goog.global.far.out = 42;
  assertEquals(42, goog.getObjectByName('far.out'));
  assertTrue(goog.isProvided_('far.out'));

  // Empty string should be allowed.
  goog.global.far.out = '';
  assertEquals('', goog.getObjectByName('far.out'));
  assertTrue(goog.isProvided_('far.out'));

  // Null or undefined are not allowed.
  goog.global.far.out = null;
  assertNull(goog.getObjectByName('far.out'));
  assertFalse(goog.isProvided_('far.out'));

  goog.global.far.out = undefined;
  assertNull(goog.getObjectByName('far.out'));
  assertFalse(goog.isProvided_('far.out'));

  stubs.reset();
  delete goog.global.far;
}


/** @suppress {visibility} */
function testGetScriptNonce() {
  // clear nonce cache for test.
  const origNonce = goog.getScriptNonce_();
  goog.cspNonce_ = null;
  const nonce = origNonce ? origNonce : 'ThisIsANonceThisIsANonceThisIsANonce';
  const script = goog.dom.createElement(goog.dom.TagName.SCRIPT);
  script.setAttribute('nonce', 'invalid nonce');
  document.body.appendChild(script);

  try {
    assertEquals(origNonce, goog.getScriptNonce_());
    // clear nonce cache for test.
    goog.cspNonce_ = null;
    script.nonce = nonce;
    assertEquals(nonce, goog.getScriptNonce_());
  } finally {
    goog.dom.removeNode(script);
  }
}

/** @suppress {checkTypes} */
function testBaseClass() {
  function A(x, y) {
    this.foo = x + y;
  }

  function B(x, y) {
    B.base(this, 'constructor', x, y);
    this.foo += 2;
  }
  goog.inherits(B, A);

  function C(x, y) {
    C.base(this, 'constructor', x, y);
    this.foo += 4;
  }
  goog.inherits(C, B);

  function D(x, y) {
    D.base(this, 'constructor', x, y);
    this.foo += 8;
  }
  goog.inherits(D, C);

  assertEquals(15, (new D(1, 0)).foo);
  assertEquals(16, (new D(1, 1)).foo);
  assertEquals(16, (new D(2, 0)).foo);
  assertEquals(7, (new C(1, 0)).foo);
  assertEquals(3, (new B(1, 0)).foo);
}

/** @suppress {checkTypes} */
function testBaseMethodAndBaseCtor() {
  // This will fail on FF4.0 if the following bug is not fixed:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=586482
  function A(x, y) {
    this.foo(x, y);
  }
  A.prototype.foo = function(x, y) {
    this.bar = x + y;
  };

  function B(x, y) {
    B.base(this, 'constructor', x, y);
  }
  goog.inherits(B, A);
  B.prototype.foo = function(x, y) {
    B.base(this, 'foo', x, y);
    this.bar = this.bar * 2;
  };

  assertEquals(14, new B(3, 4).bar);
}


//=== tests for bind() and friends ===

// Function.prototype.bind and Function.prototype.partial are purposefullly
// not defined in open sourced Closure.  These functions sniff for their
// presence.

var foo = 'global';

/**
 * @param {?} arg1
 * @param {?} arg2
 * @return {?}
 * @suppress {checkTypes}
 */
function getFoo(arg1, arg2) {
  return {foo: this.foo, arg1: arg1, arg2: arg2};
}


/** @suppress {checkTypes} */
function testBindWithoutObj() {
  if (Function.prototype.bind) {
    assertEquals(foo, getFoo.bind()().foo);
  }
}

/** @suppress {checkTypes} */
function testBindWithNullObj() {
  if (Function.prototype.bind) {
    assertEquals(foo, getFoo.bind(null)().foo);
  }
}


// Validate the behavior of goog.module when used from traditional files.
/** @suppress {missingProperties} namespaces */
function testGoogModuleGet() {
  // assert that goog.module doesn't modify the global namespace
  assertUndefined(
      'module failed to protect global namespace: ' +
          'goog.test_module_dep',
      goog.test_module_dep);

  // assert that goog.module with goog.module.declareLegacyNamespace is
  // present.
  assertNotUndefined(
      'module failed to declare global namespace: ' +
          'goog.test_module',
      goog.test_module);

  // assert that a require'd goog.module is available immediately after the
  // goog.require call.
  assertNotUndefined(
      'module failed to protect global namespace: ' +
          'goog.test_module_dep',
      earlyTestModuleGet);

  // assert that an non-existent module request doesn't throw and returns
  // null.
  // NOTE: using computed properties to avoid compiler checks
  assertEquals(null, goog['module'].get('unrequired.module.id'));

  // Validate the module exports
  const testModuleExports = goog.module.get('goog.test_module');
  assertTrue(typeof testModuleExports === 'function');

  // Test that any escaping of </script> in test files is correct. Escape the
  // / in </script> here so that any such code does not affect it here.
  assertEquals('<\/script>', testModuleExports.CLOSING_SCRIPT_TAG);

  // Validate that the module exports object has not changed
  assertEquals(earlyTestModuleGet, testModuleExports);
}
