/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.module.ModuleManagerTest');
goog.setTestOnly();

const BaseModule = goog.require('goog.module.BaseModule');
const MockClock = goog.require('goog.testing.MockClock');
const ModuleManager = goog.require('goog.module.ModuleManager');
const functions = goog.require('goog.functions');
const googArray = goog.require('goog.array');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');
const testing = goog.require('goog.testing');

let clock;
let requestCount = 0;

function getModuleManager(infoMap) {
  const mm = new ModuleManager();
  mm.setAllModuleInfo(infoMap);

  /**
   * @suppress {globalThis,checkTypes} suppression added to enable type
   * checking
   */
  mm.isModuleLoaded = function(id) {
    return this.getModuleInfo(id).isLoaded();
  };
  return mm;
}

function createSuccessfulBatchLoader(moduleMgr) {
  return {
    loadModules: /**
                    @suppress {globalThis} suppression added to enable type
                    checking
                  */
        function(ids, moduleInfoMap, {onError, onSuccess, onTimeout}) {
          requestCount++;
          setTimeout(goog.bind(this.onLoad, this, ids.concat(), 0), 5);
        },
    onLoad: /**
               @suppress {globalThis} suppression added to enable type checking
             */
        function(ids, idxLoaded) {
          moduleMgr.beforeLoadModuleCode(ids[idxLoaded]);
          moduleMgr.setLoaded();
          const idx = idxLoaded + 1;
          if (idx < ids.length) {
            setTimeout(goog.bind(this.onLoad, this, ids, idx), 2);
          }
        },
  };
}

function createSuccessfulNonBatchLoader(moduleMgr) {
  return {
    loadModules: function(ids, moduleInfoMap, {onError, onSuccess, onTimeout}) {
      requestCount++;
      setTimeout(() => {
        moduleMgr.beforeLoadModuleCode(ids[0]);
        moduleMgr.setLoaded();
        if (onSuccess) {
          onSuccess();
        }
      }, 5);
    },
  };
}

function createUnsuccessfulLoader(moduleMgr, status) {
  return {
    loadModules: function(ids, moduleInfoMap, {onError, onSuccess, onTimeout}) {
      moduleMgr.beforeLoadModuleCode(ids[0]);
      setTimeout(() => {
        onError(status);
      }, 5);
    },
  };
}

function createUnsuccessfulBatchLoader(moduleMgr, status) {
  return {
    loadModules: function(ids, moduleInfoMap, {onError, onSuccess, onTimeout}) {
      setTimeout(() => {
        onError(status);
      }, 5);
    },
  };
}

function createTimeoutLoader(moduleMgr, status) {
  return {
    loadModules: function(ids, moduleInfoMap, {onError, onSuccess, onTimeout}) {
      setTimeout(() => {
        onTimeout(status);
      }, 5);
    },
  };
}

/**
 * Tests execOnLoad with the specified module manager.
 * @param {ModuleManager} mm The module manager.
 * @suppress {missingProperties,checkTypes} suppression added to enable type
 * checking
 */
function execOnLoad(mm) {
  // When module is unloaded, execOnLoad is async.
  let execCalled1 = false;
  mm.execOnLoad('a', () => {
    execCalled1 = true;
  });
  assertFalse('module "a" should not be loaded', mm.isModuleLoaded('a'));
  assertTrue('module "a" should be loading', mm.isModuleLoading('a'));
  assertFalse('execCalled1 should not be set yet', execCalled1);
  assertTrue('ModuleManager should be active', mm.isActive());
  assertFalse('ModuleManager should not be user active', mm.isUserActive());
  clock.tick(5);
  assertTrue('module "a" should be loaded', mm.isModuleLoaded('a'));
  assertFalse('module "a" should not be loading', mm.isModuleLoading('a'));
  assertTrue('execCalled1 should be set', execCalled1);
  assertFalse('ModuleManager should not be active', mm.isActive());
  assertFalse('ModuleManager should not be user active', mm.isUserActive());

  // When module is already loaded, execOnLoad is still async unless
  // specified otherwise.
  let execCalled2 = false;
  mm.execOnLoad('a', () => {
    execCalled2 = true;
  });
  assertTrue('module "a" should be loaded', mm.isModuleLoaded('a'));
  assertFalse('module "a" should not be loading', mm.isModuleLoading('a'));
  assertFalse('execCalled2 should not be set yet', execCalled2);
  clock.tick(5);
  assertTrue('execCalled2 should be set', execCalled2);

  // When module is unloaded, execOnLoad is async (user active).
  let execCalled5 = false;
  mm.execOnLoad('c', () => {
    execCalled5 = true;
  }, null, null, true);
  assertFalse('module "c" should not be loaded', mm.isModuleLoaded('c'));
  assertTrue('module "c" should be loading', mm.isModuleLoading('c'));
  assertFalse('execCalled1 should not be set yet', execCalled5);
  assertTrue('ModuleManager should be active', mm.isActive());
  assertTrue('ModuleManager should be user active', mm.isUserActive());
  clock.tick(5);
  assertTrue('module "c" should be loaded', mm.isModuleLoaded('c'));
  assertFalse('module "c" should not be loading', mm.isModuleLoading('c'));
  assertTrue('execCalled1 should be set', execCalled5);
  assertFalse('ModuleManager should not be active', mm.isActive());
  assertFalse('ModuleManager should not be user active', mm.isUserActive());

  // When module is already loaded, execOnLoad is still synchronous when
  // so specified
  let execCalled6 = false;
  mm.execOnLoad('c', () => {
    execCalled6 = true;
  }, undefined, undefined, undefined, true);
  assertTrue('module "c" should be loaded', mm.isModuleLoaded('c'));
  assertFalse('module "c" should not be loading', mm.isModuleLoading('c'));
  assertTrue('execCalled6 should be set', execCalled6);
  clock.tick(5);
  assertTrue('execCalled6 should still be set', execCalled6);
}

/**
 * Perform tests with the specified module manager.
 * @param {ModuleManager} mm The module manager.
 * @suppress {missingProperties} suppression added to enable type checking
 */
function execOnLoadWhilePreloadingAndViceVersa(mm) {
  mm = getModuleManager({'c': [], 'd': []});
  mm.setLoader(createSuccessfulNonBatchLoader(mm));

  const origBeforeLoadModuleCode = mm.beforeLoadModuleCode;
  const origSetLoaded = mm.setLoaded;
  const calls = [0, 0];
  mm.beforeLoadModuleCode = (id) => {
    calls[0]++;
    origBeforeLoadModuleCode.call(mm, id);
  };
  mm.setLoaded = () => {
    calls[1]++;
    origSetLoaded.call(mm);
  };

  mm.preloadModule('c', 2);
  assertFalse('module "c" should not be loading yet', mm.isModuleLoading('c'));
  clock.tick(2);
  assertTrue('module "c" should now be loading', mm.isModuleLoading('c'));
  mm.execOnLoad('c', () => {});
  assertTrue('module "c" should still be loading', mm.isModuleLoading('c'));
  clock.tick(5);
  assertFalse('module "c" should be done loading', mm.isModuleLoading('c'));
  assertEquals('beforeLoad should only be called once for "c"', 1, calls[0]);
  assertEquals('setLoaded should only be called once for "c"', 1, calls[1]);

  mm.execOnLoad('d', () => {});
  assertTrue('module "d" should now be loading', mm.isModuleLoading('d'));
  mm.preloadModule('d', 2);
  clock.tick(5);
  assertFalse('module "d" should be done loading', mm.isModuleLoading('d'));
  assertTrue('module "d" should now be loaded', mm.isModuleLoaded('d'));
  assertEquals('beforeLoad should only be called once for "d"', 2, calls[0]);
  assertEquals('setLoaded should only be called once for "d"', 2, calls[1]);
}

function assertDependencyOrder(list, mm) {
  const seen = {};
  for (let i = 0; i < list.length; i++) {
    const id = list[i];
    seen[id] = true;
    const deps = mm.getModuleInfo(id).getDependencies();
    for (let j = 0; j < deps.length; j++) {
      const dep = deps[j];
      assertTrue(
          `Unresolved dependency [${dep}] for [${id}].`,
          seen[dep] || mm.getModuleInfo(dep).isLoaded());
    }
  }
}

function createSuccessfulNonBatchLoaderWithRegisterInitCallback(moduleMgr, fn) {
  return {
    loadModules: function(ids, moduleInfoMap, {onError, onSuccess, onTimeout}) {
      moduleMgr.beforeLoadModuleCode(ids[0]);
      moduleMgr.registerInitializationCallback(fn);
      setTimeout(() => {
        moduleMgr.setLoaded();
        if (onSuccess) {
          onSuccess();
        }
      }, 5);
    },
  };
}

function createModulesFor(var_args) {
  const result = {};
  for (let i = 0; i < arguments.length; i++) {
    const key = arguments[i];
    result[key] = {ctor: BaseModule};
  }
  return result;
}

function createSuccessfulNonBatchLoaderWithConstructor(moduleMgr, info) {
  return {
    loadModules: function(ids, moduleInfoMap, {onError, onSuccess, onTimeout}) {
      setTimeout(() => {
        moduleMgr.beforeLoadModuleCode(ids[0]);
        moduleMgr.setModuleConstructor(info[ids[0]].ctor);
        moduleMgr.setLoaded();
        if (onSuccess) {
          onSuccess();
        }
      }, 5);
    },
  };
}

/**
 * Creates an AbstractModuleLoader implementation with extra edges support
 * @param {!Array} loaderCalls array to which the arguments of loadModules will
 *     be appended
 * @return {!Object<function(), boolean>}
 * @suppress {checkTypes} suppression added to enable type checking
 */
function createModuleLoaderWithExtraEdgesSupport(loaderCalls) {
  return {
    loadModules(ids, moduleInfoMap, loadOptions) {
      loaderCalls.push({
        ids: ids,
        moduleInfoMap: moduleInfoMap,
        ...loadOptions,
      });
    },
    supportsExtraEdges: true,
  };
}

/**
 * Creates an AbstractModuleLoader implementation that registers one
 * initialization callback for a synthetic module, then simulates loading the
 * given modules.
 * @param {!ModuleManager} moduleMgr
 * @param {!Array} modulesToMarkAsLoaded
 * @return {{loadModules: function(), syntheticModuleCallbackCalled: boolean}}
 * @suppress {checkTypes} suppression added to enable type checking
 */
function createExcludingSyntheticModuleOverheadLoader(
    moduleMgr, modulesToMarkAsLoaded) {
  return {
    syntheticModuleCallbackCalled: false,
    loadModules: function(ids, moduleInfoMap, {onError, onSuccess, onTimeout}) {
      const cb = () => {
        this.syntheticModuleCallbackCalled = true;
      };
      requestCount++;
      setTimeout(() => {
        // Simulate a synthetic module loading first, and registering a cb.
        moduleMgr.registerInitializationCallback(cb);
        for (const id of modulesToMarkAsLoaded) {
          moduleMgr.beforeLoadModuleCode(id);
          moduleMgr.setLoaded();
        }
        if (onSuccess) {
          onSuccess();
        }
      }, 5);
    },
  };
}

testSuite({
  tearDown() {
    clock.dispose();
  },

  setUp() {
    clock = new MockClock(true);
    requestCount = 0;
  },

  /**
   * Tests loading a module under different conditions i.e. unloaded
   * module, already loaded module, module loaded through user initiated
   * actions, synchronous callback for a module that has been already
   * loaded. Test both batch and non-batch loaders.
   */
  testExecOnLoad() {
    let mm = getModuleManager({'a': [], 'b': [], 'c': []});
    mm.setLoader(createSuccessfulNonBatchLoader(mm));
    execOnLoad(mm);

    mm = getModuleManager({'a': [], 'b': [], 'c': []});
    mm.setLoader(createSuccessfulBatchLoader(mm));
    mm.setBatchModeEnabled(true);
    execOnLoad(mm);
  },

  /**
     Test aborting the callback called on module load.
     @suppress {missingProperties} suppression added to enable type checking
   */
  testExecOnLoadAbort() {
    const mm = getModuleManager({'a': [], 'b': [], 'c': []});
    mm.setLoader(createSuccessfulNonBatchLoader(mm));

    // When module is unloaded and abort is called, module still gets
    // loaded, but callback is cancelled.
    let execCalled1 = false;
    const callback1 = mm.execOnLoad('b', () => {
      execCalled1 = true;
    });
    callback1.abort();
    clock.tick(5);
    assertTrue('module "b" should be loaded', mm.isModuleLoaded('b'));
    assertFalse('execCalled3 should not be set', execCalled1);

    // When module is already loaded, execOnLoad is still async, so calling
    // abort should still cancel the callback.
    let execCalled2 = false;
    const callback2 = mm.execOnLoad('a', () => {
      execCalled2 = true;
    });
    callback2.abort();
    clock.tick(5);
    assertFalse('execCalled2 should not be set', execCalled2);
  },

  /**
   * Test preloading modules and ensure that the before load, after load
   * and set load called are called only once per module.
   */
  testExecOnLoadWhilePreloadingAndViceVersa() {
    let mm = getModuleManager({'c': [], 'd': []});
    mm.setLoader(createSuccessfulNonBatchLoader(mm));
    execOnLoadWhilePreloadingAndViceVersa(mm);

    mm = getModuleManager({'c': [], 'd': []});
    mm.setLoader(createSuccessfulBatchLoader(mm));
    mm.setBatchModeEnabled(true);
    execOnLoadWhilePreloadingAndViceVersa(mm);
  },

  /**
   * Tests that multiple callbacks on the same module don't cause
   * confusion about the active state after the module is finally loaded.
   */
  testUserInitiatedExecOnLoadEventuallyLeavesManagerIdle() {
    const mm = getModuleManager({'c': [], 'd': []});
    mm.setLoader(createSuccessfulNonBatchLoader(mm));

    let calledBack1 = false;
    let calledBack2 = false;

    mm.execOnLoad('c', () => {
      calledBack1 = true;
    }, undefined, undefined, true);
    mm.execOnLoad('c', () => {
      calledBack2 = true;
    }, undefined, undefined, true);
    mm.load('c');

    assertTrue(
        'Manager should be active while waiting for load', mm.isUserActive());

    clock.tick(5);

    assertTrue('First callback should be called', calledBack1);
    assertTrue('Second callback should be called', calledBack2);
    assertFalse(
        'Manager should be inactive after loading is complete',
        mm.isUserActive());
  },

  /** Tests loading a module by requesting a Deferred object. */
  testLoad() {
    const mm = getModuleManager({'a': [], 'b': [], 'c': []});
    mm.setLoader(createSuccessfulNonBatchLoader(mm));

    let calledBack = false;
    let error = null;

    const d = mm.load('a');
    d.then(
        (ctx) => {
          calledBack = true;
        },
        (err) => {
          error = err;
        });

    assertFalse(calledBack);
    assertNull(error);
    assertFalse(mm.isUserActive());

    clock.tick(5);

    assertTrue(calledBack);
    assertNull(error);
  },

  /**
   * Tests loading 2 modules asserting that the loads happen in parallel
   * in one unit of time.
   */
  testLoad_concurrent() {
    const mm = getModuleManager({'a': [], 'b': [], 'c': []});
    mm.setConcurrentLoadingEnabled(true);
    mm.setLoader(createSuccessfulNonBatchLoader(mm));

    const calledBack = false;
    const error = null;

    mm.load('a');
    mm.load('b');
    assertEquals(2, requestCount);
    // Only time for one serialized download.
    clock.tick(5);

    assertTrue(mm.getModuleInfo('a').isLoaded());
    assertTrue(mm.getModuleInfo('b').isLoaded());
  },

  testLoad_concurrentSecondIsDepOfFist() {
    const mm = getModuleManager({'a': [], 'b': [], 'c': []});
    mm.setBatchModeEnabled(true);
    mm.setConcurrentLoadingEnabled(true);
    mm.setLoader(createSuccessfulBatchLoader(mm));

    const calledBack = false;
    const error = null;

    mm.loadMultiple(['a', 'b']);
    mm.load('b');
    assertEquals('No 2nd request expected', 1, requestCount);
    // Only time for one serialized download.
    clock.tick(5);
    clock.tick(2);  // Makes second module come in from batch requst.

    assertTrue(mm.getModuleInfo('a').isLoaded());
    assertTrue(mm.getModuleInfo('b').isLoaded());
  },

  testLoad_nonConcurrent() {
    const mm = getModuleManager({'a': [], 'b': [], 'c': []});
    mm.setLoader(createSuccessfulNonBatchLoader(mm));

    const calledBack = false;
    const error = null;

    mm.load('a');
    mm.load('b');
    assertEquals(1, requestCount);
    // Only time for one serialized download.
    clock.tick(5);

    assertTrue(mm.getModuleInfo('a').isLoaded());
    assertFalse(mm.getModuleInfo('b').isLoaded());
  },

  testLoadUnknown() {
    const mm = getModuleManager({'a': [], 'b': [], 'c': []});
    mm.setLoader(createSuccessfulNonBatchLoader(mm));
    const e = assertThrows(() => {
      mm.load('DoesNotExist');
    });
    assertEquals('Unknown module: DoesNotExist', e.message);
  },

  /**
     Tests loading multiple modules by requesting a Deferred object.
     @suppress {missingProperties} suppression added to enable type checking
   */
  testLoadMultiple() {
    const mm = getModuleManager({'a': [], 'b': [], 'c': []});
    mm.setBatchModeEnabled(true);
    mm.setLoader(createSuccessfulBatchLoader(mm));

    let calledBack = false;
    let error = null;
    let calledBack2 = false;
    let error2 = null;

    const dMap = mm.loadMultiple(['a', 'b']);
    dMap['a'].then(
        (ctx) => {
          calledBack = true;
        },
        (err) => {
          error = err;
        });
    dMap['b'].then(
        (ctx) => {
          calledBack2 = true;
        },
        (err) => {
          error2 = err;
        });

    assertFalse(calledBack);
    assertFalse(calledBack2);

    clock.tick(5);
    assertTrue(calledBack);
    assertFalse(calledBack2);
    assertTrue('module "a" should be loaded', mm.isModuleLoaded('a'));
    assertFalse('module "b" should not be loaded', mm.isModuleLoaded('b'));
    assertFalse('module "c" should not be loaded', mm.isModuleLoaded('c'));

    clock.tick(2);

    assertTrue(calledBack);
    assertTrue(calledBack2);
    assertTrue('module "a" should be loaded', mm.isModuleLoaded('a'));
    assertTrue('module "b" should be loaded', mm.isModuleLoaded('b'));
    assertFalse('module "c" should not be loaded', mm.isModuleLoaded('c'));
    assertNull(error);
    assertNull(error2);
  },

  /**
   * Tests loading multiple modules with deps by requesting a Deferred
   *      object.
   * @suppress {missingProperties} suppression added to enable type checking
   */
  testLoadMultipleWithDeps() {
    const mm = getModuleManager({'a': [], 'b': ['c'], 'c': []});
    mm.setBatchModeEnabled(true);
    mm.setLoader(createSuccessfulBatchLoader(mm));

    let calledBack = false;
    let error = null;
    let calledBack2 = false;
    let error2 = null;

    const dMap = mm.loadMultiple(['a', 'b']);
    dMap['a'].then(
        (ctx) => {
          calledBack = true;
        },
        (err) => {
          error = err;
        });
    dMap['b'].then(
        (ctx) => {
          calledBack2 = true;
        },
        (err) => {
          error2 = err;
        });

    assertFalse(calledBack);
    assertFalse(calledBack2);

    clock.tick(5);
    assertTrue(calledBack);
    assertFalse(calledBack2);
    assertTrue('module "a" should be loaded', mm.isModuleLoaded('a'));
    assertFalse('module "b" should not be loaded', mm.isModuleLoaded('b'));
    assertFalse('module "c" should not be loaded', mm.isModuleLoaded('c'));

    clock.tick(2);

    assertFalse(calledBack2);
    assertTrue('module "a" should be loaded', mm.isModuleLoaded('a'));
    assertFalse('module "b" should not be loaded', mm.isModuleLoaded('b'));
    assertTrue('module "c" should be loaded', mm.isModuleLoaded('c'));

    clock.tick(2);

    assertTrue(calledBack2);
    assertTrue('module "a" should be loaded', mm.isModuleLoaded('a'));
    assertTrue('module "b" should be loaded', mm.isModuleLoaded('b'));
    assertTrue('module "c" should be loaded', mm.isModuleLoaded('c'));
    assertNull(error);
    assertNull(error2);
  },

  /**
   * Tests loading multiple modules by requesting a Deferred object when
   * a server error occurs.
   * @suppress {missingProperties} suppression added to enable type checking
   */
  testLoadMultipleWithErrors() {
    const mm = getModuleManager({'a': [], 'b': [], 'c': []});
    mm.setBatchModeEnabled(true);
    mm.setLoader(createUnsuccessfulLoader(mm, 500));

    let calledBack = false;
    let error = null;
    let calledBack2 = false;
    let error2 = null;
    let calledBack3 = false;
    let error3 = null;

    const dMap = mm.loadMultiple(['a', 'b', 'c']);
    dMap['a'].then(
        (ctx) => {
          calledBack = true;
        },
        (err) => {
          error = err;
        });
    dMap['b'].then(
        (ctx) => {
          calledBack2 = true;
        },
        (err) => {
          error2 = err;
        });
    dMap['c'].then(
        (ctx) => {
          calledBack3 = true;
        },
        (err) => {
          error3 = err;
        });

    assertFalse(calledBack);
    assertFalse(calledBack2);
    assertFalse(calledBack3);

    clock.tick(4);

    // A module request is now underway using the unsuccessful loader.
    // We substitute a successful loader for future module load requests.
    mm.setLoader(createSuccessfulBatchLoader(mm));

    clock.tick(1);

    assertFalse(calledBack);
    assertFalse(calledBack2);
    assertFalse(calledBack3);
    assertFalse('module "a" should not be loaded', mm.isModuleLoaded('a'));
    assertFalse('module "b" should not be loaded', mm.isModuleLoaded('b'));
    assertFalse('module "c" should not be loaded', mm.isModuleLoaded('c'));

    // Retry should happen after a backoff
    clock.tick(5 + mm.getBackOff_());

    assertTrue(calledBack);
    assertFalse(calledBack2);
    assertFalse(calledBack3);
    assertTrue('module "a" should be loaded', mm.isModuleLoaded('a'));
    assertFalse('module "b" should not be loaded', mm.isModuleLoaded('b'));
    assertFalse('module "c" should not be loaded', mm.isModuleLoaded('c'));

    clock.tick(2);
    assertTrue(calledBack2);
    assertFalse(calledBack3);
    assertTrue('module "b" should be loaded', mm.isModuleLoaded('b'));
    assertFalse('module "c" should not be loaded', mm.isModuleLoaded('c'));

    clock.tick(2);
    assertTrue(calledBack3);
    assertTrue('module "c" should be loaded', mm.isModuleLoaded('c'));

    assertNull(error);
    assertNull(error2);
    assertNull(error3);
  },

  /**
   * Tests loading multiple modules by requesting a Deferred object when
   * consecutive server error occur and the loader falls back to serial
   * loads.
   * @suppress {missingProperties} suppression added to enable type checking
   */
  testLoadMultipleWithErrorsFallbackOnSerial() {
    const mm = getModuleManager({'a': [], 'b': [], 'c': []});
    mm.setBatchModeEnabled(true);
    mm.setLoader(createUnsuccessfulLoader(mm, 500));

    let calledBack = false;
    let error = null;
    let calledBack2 = false;
    let error2 = null;
    let calledBack3 = false;
    let error3 = null;

    const dMap = mm.loadMultiple(['a', 'b', 'c']);
    dMap['a'].then(
        (ctx) => {
          calledBack = true;
        },
        (err) => {
          error = err;
        });
    dMap['b'].then(
        (ctx) => {
          calledBack2 = true;
        },
        (err) => {
          error2 = err;
        });
    dMap['c'].then(
        (ctx) => {
          calledBack3 = true;
        },
        (err) => {
          error3 = err;
        });

    assertFalse(calledBack);
    assertFalse(calledBack2);
    assertFalse(calledBack3);

    clock.tick(5);

    assertFalse(calledBack);
    assertFalse(calledBack2);
    assertFalse(calledBack3);
    assertFalse('module "a" should not be loaded', mm.isModuleLoaded('a'));
    assertFalse('module "b" should not be loaded', mm.isModuleLoaded('b'));
    assertFalse('module "c" should not be loaded', mm.isModuleLoaded('c'));

    // Retry should happen and fail after a backoff
    clock.tick(5 + mm.getBackOff_());
    assertFalse(calledBack);
    assertFalse(calledBack2);
    assertFalse(calledBack3);
    assertFalse('module "a" should not be loaded', mm.isModuleLoaded('a'));
    assertFalse('module "b" should not be loaded', mm.isModuleLoaded('b'));
    assertFalse('module "c" should not be loaded', mm.isModuleLoaded('c'));

    // A second retry should happen after a backoff
    clock.tick(4 + mm.getBackOff_());
    // The second retry is now underway using the unsuccessful loader.
    // We substitute a successful loader for future module load requests.
    mm.setLoader(createSuccessfulBatchLoader(mm));

    clock.tick(1);

    // A second retry should fail now
    assertFalse(calledBack);
    assertFalse(calledBack2);
    assertFalse(calledBack3);
    assertFalse('module "a" should not be loaded', mm.isModuleLoaded('a'));
    assertFalse('module "b" should not be loaded', mm.isModuleLoaded('b'));
    assertFalse('module "c" should not be loaded', mm.isModuleLoaded('c'));

    // Each module should be loaded individually now, each taking 5 ticks

    clock.tick(5);
    assertTrue(calledBack);
    assertFalse(calledBack2);
    assertFalse(calledBack3);
    assertTrue('module "a" should be loaded', mm.isModuleLoaded('a'));
    assertFalse('module "b" should not be loaded', mm.isModuleLoaded('b'));
    assertFalse('module "c" should not be loaded', mm.isModuleLoaded('c'));

    clock.tick(5);
    assertTrue(calledBack2);
    assertFalse(calledBack3);
    assertTrue('module "b" should be loaded', mm.isModuleLoaded('b'));
    assertFalse('module "c" should not be loaded', mm.isModuleLoaded('c'));

    clock.tick(5);
    assertTrue(calledBack3);
    assertTrue('module "c" should be loaded', mm.isModuleLoaded('c'));

    assertNull(error);
    assertNull(error2);
    assertNull(error3);
  },

  /**
     Tests loading a module by user action by requesting a Deferred object.
   */
  testLoadForUser() {
    const mm = getModuleManager({'a': [], 'b': [], 'c': []});
    mm.setLoader(createSuccessfulNonBatchLoader(mm));

    let calledBack = false;
    let error = null;

    const d = mm.load('a', true);
    d.then(
        (ctx) => {
          calledBack = true;
        },
        (err) => {
          error = err;
        });

    assertFalse(calledBack);
    assertNull(error);
    assertTrue(mm.isUserActive());

    clock.tick(5);

    assertTrue(calledBack);
    assertNull(error);
  },

  /**
   * Test loading modules that include synthetic modules that omit their
   * calls to beforeLoadModuleCode() and setLoaded().
   */
  testLoadWithoutSyntheticModuleOverhead() {
    const mm = getModuleManager({'a': []});
    const loader = createExcludingSyntheticModuleOverheadLoader(
        mm, /* modulesToMarkAsLoaded= */['a']);
    mm.setLoader(loader);

    let calledBack = false;
    let error = null;

    const d = mm.load('a');
    d.then(
        (ctx) => {
          calledBack = true;
        },
        (err) => {
          error = err;
        });

    assertFalse(calledBack);
    assertNull(error);
    assertFalse(mm.isUserActive());
    assertFalse(loader.syntheticModuleCallbackCalled);
    assertFalse(mm.getModuleInfo('a').isLoaded());

    clock.tick(5);

    assertTrue(calledBack);
    assertNull(error);
    assertTrue(loader.syntheticModuleCallbackCalled);
    assertTrue(mm.getModuleInfo('a').isLoaded());
  },

  /**
   * Same as testLoadWithoutSyntheticModuleOverhead, but this time we load
   * module info to simulate positive module loading, where the manager is aware
   * of synthetic modules.
   */
  testLoadWithoutSyntheticModuleOverhead_MarksSyntheticModulesAsLoaded() {
    const mm = getModuleManager({'sy0': [], 'a': [], 'b': ['sy0', 'a']});
    const loader = createExcludingSyntheticModuleOverheadLoader(
        mm, /* modulesToMarkAsLoaded= */['a', 'b']);
    mm.setLoader(loader);

    let calledBack = false;
    let error = null;

    const d = mm.load('a');
    d.then(
        (ctx) => {
          calledBack = true;
        },
        (err) => {
          error = err;
        });

    assertFalse(calledBack);
    assertNull(error);
    assertFalse(mm.isUserActive());
    assertFalse(loader.syntheticModuleCallbackCalled);
    assertFalse(mm.getModuleInfo('sy0').isLoaded());
    assertFalse(mm.getModuleInfo('a').isLoaded());
    assertFalse(mm.getModuleInfo('b').isLoaded());

    clock.tick(5);

    assertTrue(calledBack);
    assertNull(error);
    assertTrue(loader.syntheticModuleCallbackCalled);
    assertTrue(mm.getModuleInfo('sy0').isLoaded());
    assertTrue(mm.getModuleInfo('a').isLoaded());
    assertTrue(mm.getModuleInfo('b').isLoaded());
  },

  testExtraEdges() {
    const mm =
        getModuleManager({'modA': [], 'modB': [], 'modC': [], 'modD': []});
    const loaderCalls = [];
    mm.setLoader(createModuleLoaderWithExtraEdgesSupport(loaderCalls));
    mm.addExtraEdge('modA', 'modB');
    mm.addExtraEdge('modA', 'modC');
    mm.addExtraEdge('modC', 'modD');

    const expectedExtraEdges = {
      'modA': {'modB': true, 'modC': true},
      'modC': {'modD': true},
    };

    mm.load('modA');
    assertEquals(1, loaderCalls.length);
    assertObjectEquals(expectedExtraEdges, loaderCalls[0].extraEdges);
  },

  testAddExtraEdge_managerDoesNotSupportExtraEdges() {
    const mm =
        getModuleManager({'modA': [], 'modB': [], 'modC': [], 'modD': []});
    mm.setLoader({
      loadModules(ids, moduleInfoMap, loadOptions) {},
    });
    assertThrows(() => mm.addExtraEdge('modA', 'modB'));
  },

  testRemoveExtraEdge() {
    const mm =
        getModuleManager({'modA': [], 'modB': [], 'modC': [], 'modD': []});
    const loaderCalls = [];
    mm.setLoader(createModuleLoaderWithExtraEdgesSupport(loaderCalls));
    mm.addExtraEdge('modA', 'modB');
    mm.addExtraEdge('modA', 'modC');
    mm.addExtraEdge('modC', 'modD');
    mm.removeExtraEdge('modA', 'modB');

    const expectedExtraEdges = {
      'modA': {'modC': true},
      'modC': {'modD': true},
    };

    mm.load('modA');
    assertEquals(1, loaderCalls.length);
    assertObjectEquals(expectedExtraEdges, loaderCalls[0].extraEdges);
  },

  testRemoveEdge_nonexistentEdge() {
    const mm =
        getModuleManager({'modA': [], 'modB': [], 'modC': [], 'modD': []});
    const loaderCalls = [];
    mm.setLoader(createModuleLoaderWithExtraEdgesSupport(loaderCalls));
    mm.addExtraEdge('modA', 'modC');
    mm.addExtraEdge('modC', 'modD');
    mm.removeExtraEdge('modA', 'modB');

    const expectedExtraEdges = {
      'modA': {'modC': true},
      'modC': {'modD': true},
    };

    mm.load('modA');
    assertEquals(1, loaderCalls.length);
    assertObjectEquals(expectedExtraEdges, loaderCalls[0].extraEdges);
  },

  /** Tests that preloading a module calls back the deferred object. */
  testPreloadDeferredWhenNotLoaded() {
    const mm = getModuleManager({'a': []});
    mm.setLoader(createSuccessfulNonBatchLoader(mm));

    let calledBack = false;

    const d = mm.preloadModule('a');
    d.addCallback((ctx) => {
      calledBack = true;
    });

    // First load should take five ticks.
    assertFalse('module "a" should not be loaded yet', calledBack);
    clock.tick(5);
    assertTrue('module "a" should be loaded', calledBack);
  },

  /** Tests preloading an already loaded module. */
  testPreloadDeferredWhenLoaded() {
    const mm = getModuleManager({'a': []});
    mm.setLoader(createSuccessfulNonBatchLoader(mm));

    let calledBack = false;

    mm.preloadModule('a');
    clock.tick(5);

    const d = mm.preloadModule('a');
    d.addCallback((ctx) => {
      calledBack = true;
    });

    // Module is already loaded, should be called back after the setTimeout
    // in preloadModule.
    assertFalse('deferred for module "a" should not be called yet', calledBack);
    clock.tick(1);
    assertTrue('module "a" should be loaded', calledBack);
  },

  /** Tests preloading a module that is currently loading. */
  testPreloadDeferredWhenLoading() {
    const mm = getModuleManager({'a': []});
    mm.setLoader(createSuccessfulNonBatchLoader(mm));

    mm.preloadModule('a');
    clock.tick(1);

    // 'b' is in the middle of loading, should get called back when it's
    // done.
    let calledBack = false;
    const d = mm.preloadModule('a');
    d.addCallback((ctx) => {
      calledBack = true;
    });

    assertFalse('module "a" should not be loaded yet', calledBack);
    clock.tick(4);
    assertTrue('module "a" should be loaded', calledBack);
  },

  /**
   * Tests that load doesn't trigger another load if a module is already
   * preloading.
   */
  testLoadWhenPreloading() {
    const mm = getModuleManager({'a': [], 'b': [], 'c': []});
    mm.setLoader(createSuccessfulNonBatchLoader(mm));

    const origBeforeLoadModuleCode = mm.beforeLoadModuleCode;
    const origSetLoaded = mm.setLoaded;
    const calls = [0, 0];
    mm.beforeLoadModuleCode = (id) => {
      calls[0]++;
      origBeforeLoadModuleCode.call(mm, id);
    };
    mm.setLoaded = () => {
      calls[1]++;
      origSetLoaded.call(mm);
    };

    let calledBack = false;
    let error = null;

    mm.preloadModule('c', 2);
    assertFalse(
        'module "c" should not be loading yet', mm.isModuleLoading('c'));
    clock.tick(2);
    assertTrue('module "c" should now be loading', mm.isModuleLoading('c'));

    const d = mm.load('c');
    d.then(
        (ctx) => {
          calledBack = true;
        },
        (err) => {
          error = err;
        });

    assertTrue('module "c" should still be loading', mm.isModuleLoading('c'));
    clock.tick(5);
    assertFalse('module "c" should be done loading', mm.isModuleLoading('c'));
    assertEquals('beforeLoad should only be called once for "c"', 1, calls[0]);
    assertEquals('setLoaded should only be called once for "c"', 1, calls[1]);

    assertTrue(calledBack);
    assertNull(error);
  },

  /**
   * Tests that load doesn't trigger another load if a module is already
   * preloading.
   */
  testLoadMultipleWhenPreloading() {
    const mm = getModuleManager({'a': [], 'b': ['d'], 'c': [], 'd': []});
    mm.setLoader(createSuccessfulBatchLoader(mm));
    mm.setBatchModeEnabled(true);

    const origBeforeLoadModuleCode = mm.beforeLoadModuleCode;
    const origSetLoaded = mm.setLoaded;
    const calls = {'a': 0, 'b': 0, 'c': 0, 'd': 0};
    mm.beforeLoadModuleCode = (id) => {
      calls[id]++;
      origBeforeLoadModuleCode.call(mm, id);
    };
    let setLoadedCalls = 0;
    mm.setLoaded = () => {
      setLoadedCalls++;
      origSetLoaded.call(mm);
    };

    let calledBack = false;
    let error = null;
    let calledBack2 = false;
    let error2 = null;
    let calledBack3 = false;
    let error3 = null;

    mm.preloadModule('c', 2);
    mm.preloadModule('d', 3);
    assertFalse(
        'module "c" should not be loading yet', mm.isModuleLoading('c'));
    assertFalse(
        'module "d" should not be loading yet', mm.isModuleLoading('d'));
    clock.tick(2);
    assertTrue('module "c" should now be loading', mm.isModuleLoading('c'));
    clock.tick(1);
    assertTrue('module "d" should now be loading', mm.isModuleLoading('d'));

    const dMap = mm.loadMultiple(['a', 'b', 'c']);
    dMap['a'].then(
        (ctx) => {
          calledBack = true;
        },
        (err) => {
          error = err;
        });
    dMap['b'].then(
        (ctx) => {
          calledBack2 = true;
        },
        (err) => {
          error2 = err;
        });
    dMap['c'].then(
        (ctx) => {
          calledBack3 = true;
        },
        (err) => {
          error3 = err;
        });

    assertTrue('module "a" should be loading', mm.isModuleLoading('a'));
    assertTrue('module "b" should be loading', mm.isModuleLoading('b'));
    assertTrue('module "c" should still be loading', mm.isModuleLoading('c'));
    clock.tick(4);
    assertTrue(calledBack3);

    assertFalse('module "c" should be done loading', mm.isModuleLoading('c'));
    assertTrue('module "d" should still be loading', mm.isModuleLoading('d'));
    clock.tick(5);
    assertFalse('module "d" should be done loading', mm.isModuleLoading('d'));

    assertFalse(calledBack);
    assertFalse(calledBack2);
    assertTrue('module "a" should still be loading', mm.isModuleLoading('a'));
    assertTrue('module "b" should still be loading', mm.isModuleLoading('b'));
    clock.tick(7);

    assertTrue(calledBack);
    assertTrue(calledBack2);
    assertFalse('module "a" should be done loading', mm.isModuleLoading('a'));
    assertFalse('module "b" should be done loading', mm.isModuleLoading('b'));

    assertEquals(
        'beforeLoad should only be called once for "a"', 1, calls['a']);
    assertEquals(
        'beforeLoad should only be called once for "b"', 1, calls['b']);
    assertEquals(
        'beforeLoad should only be called once for "c"', 1, calls['c']);
    assertEquals(
        'beforeLoad should only be called once for "d"', 1, calls['d']);
    assertEquals(
        'setLoaded should have been called 4 times', 4, setLoadedCalls);

    assertNull(error);
    assertNull(error2);
    assertNull(error3);
  },

  /**
   * Tests that the deferred is still called when loadMultiple loads modules
   * that are already preloading.
   */
  testLoadMultipleWhenPreloadingSameModules() {
    const mm = getModuleManager({'a': [], 'b': ['d'], 'c': [], 'd': []});
    mm.setLoader(createSuccessfulBatchLoader(mm));
    mm.setBatchModeEnabled(true);

    const origBeforeLoadModuleCode = mm.beforeLoadModuleCode;
    const origSetLoaded = mm.setLoaded;
    const calls = {'c': 0, 'd': 0};
    mm.beforeLoadModuleCode = (id) => {
      calls[id]++;
      origBeforeLoadModuleCode.call(mm, id);
    };
    let setLoadedCalls = 0;
    mm.setLoaded = () => {
      setLoadedCalls++;
      origSetLoaded.call(mm);
    };

    let calledBack = false;
    let error = null;
    let calledBack2 = false;
    let error2 = null;

    mm.preloadModule('c', 2);
    mm.preloadModule('d', 3);
    assertFalse(
        'module "c" should not be loading yet', mm.isModuleLoading('c'));
    assertFalse(
        'module "d" should not be loading yet', mm.isModuleLoading('d'));
    clock.tick(2);
    assertTrue('module "c" should now be loading', mm.isModuleLoading('c'));
    clock.tick(1);
    assertTrue('module "d" should now be loading', mm.isModuleLoading('d'));

    const dMap = mm.loadMultiple(['c', 'd']);
    dMap['c'].then(
        (ctx) => {
          calledBack = true;
        },
        (err) => {
          error = err;
        });
    dMap['d'].then(
        (ctx) => {
          calledBack2 = true;
        },
        (err) => {
          error2 = err;
        });

    assertTrue('module "c" should still be loading', mm.isModuleLoading('c'));
    clock.tick(4);
    assertFalse('module "c" should be done loading', mm.isModuleLoading('c'));
    assertTrue('module "d" should still be loading', mm.isModuleLoading('d'));
    clock.tick(5);
    assertFalse('module "d" should be done loading', mm.isModuleLoading('d'));

    assertTrue(calledBack);
    assertTrue(calledBack2);

    assertEquals(
        'beforeLoad should only be called once for "c"', 1, calls['c']);
    assertEquals(
        'beforeLoad should only be called once for "d"', 1, calls['d']);
    assertEquals('setLoaded should have been called twice', 2, setLoadedCalls);

    assertNull(error);
    assertNull(error2);
  },

  /**
   * Tests loading a module via load when the module is already
   * loaded.  The deferred's callback should be called immediately.
   */
  testLoadWhenLoaded() {
    const mm = getModuleManager({'a': [], 'b': [], 'c': []});
    mm.setLoader(createSuccessfulNonBatchLoader(mm));

    let calledBack = false;
    let error = null;

    mm.preloadModule('b', 2);
    clock.tick(10);

    assertFalse('module "b" should be done loading', mm.isModuleLoading('b'));

    const d = mm.load('b');
    d.then(
        (ctx) => {
          calledBack = true;
        },
        (err) => {
          error = err;
        });

    clock.tick(1);
    assertTrue(calledBack);
    assertNull(error);
  },

  /**
     Tests that the deferred's errbacks are called if the module fails to
     load.
   */
  testLoadWithFailingModule() {
    const mm = getModuleManager({'a': [], 'b': [], 'c': []});
    mm.setLoader(createUnsuccessfulLoader(mm, 401));
    mm.registerCallback(
        ModuleManager.CallbackType.ERROR, (callbackType, id, cause) => {
          assertEquals(
              'Failure cause was not as expected',
              ModuleManager.FailureType.UNAUTHORIZED, cause);
        });
    let calledBack = false;
    let error = null;

    const d = mm.load('a');
    d.then(
        (ctx) => {
          calledBack = true;
        },
        (err) => {
          error = err;
        });

    assertFalse(calledBack);
    assertNull(error);

    clock.tick(500);

    assertFalse(calledBack);

    // NOTE: Deferred always calls errbacks with an Error object.  The
    // failure type enum is present as error.failureType, while the error
    // message is human readable and contains the module id.
    assertEquals(
        'Failure cause was not as expected',
        ModuleManager.FailureType.UNAUTHORIZED, error.failureType);
    assertEquals(
        'Error message was not as expected', 'Error loading a: Unauthorized',
        error.message);
  },

  /**
     Tests that the deferred's errbacks are called if a module fails to
     load.
   */
  testLoadMultipleWithFailingModule() {
    const mm = getModuleManager({'a': [], 'b': [], 'c': []});
    mm.setLoader(createUnsuccessfulLoader(mm, 401));
    mm.setBatchModeEnabled(true);
    mm.registerCallback(
        ModuleManager.CallbackType.ERROR, (callbackType, id, cause) => {
          assertEquals(
              'Failure cause was not as expected',
              ModuleManager.FailureType.UNAUTHORIZED, cause);
        });
    let calledBack11 = false;
    let error11 = null;
    let calledBack12 = false;
    let error12 = null;
    let calledBack21 = false;
    let error21 = null;
    let calledBack22 = false;
    let error22 = null;

    const dMap = mm.loadMultiple(['a', 'b']);
    dMap['a'].then(
        (ctx) => {
          calledBack11 = true;
        },
        (err) => {
          error11 = err;
        });
    dMap['b'].then(
        (ctx) => {
          calledBack12 = true;
        },
        (err) => {
          error12 = err;
        });

    const dMap2 = mm.loadMultiple(['b', 'c']);
    dMap2['b'].then(
        (ctx) => {
          calledBack21 = true;
        },
        (err) => {
          error21 = err;
        });
    dMap2['c'].then(
        (ctx) => {
          calledBack22 = true;
        },
        (err) => {
          error22 = err;
        });

    assertFalse(calledBack11);
    assertFalse(calledBack12);
    assertFalse(calledBack21);
    assertFalse(calledBack22);
    assertNull(error11);
    assertNull(error12);
    assertNull(error21);
    assertNull(error22);

    clock.tick(5);

    assertFalse(calledBack11);
    assertFalse(calledBack12);
    assertFalse(calledBack21);
    assertFalse(calledBack22);

    // NOTE: Deferred always calls errbacks with an Error object.  The
    // failure type enum is present as error.failureType, while the error
    // message is human readable and contains the module id.
    assertEquals(
        'Failure cause was not as expected',
        ModuleManager.FailureType.UNAUTHORIZED, error11.failureType);
    assertEquals(
        'Error message was not as expected', 'Error loading a: Unauthorized',
        error11.message);
    assertEquals(
        'Failure cause was not as expected',
        ModuleManager.FailureType.UNAUTHORIZED, error12.failureType);
    assertEquals(
        'Error message was not as expected', 'Error loading b: Unauthorized',
        error12.message);

    // The first deferred of the second load should be called since it asks
    // for one of the failed modules.
    assertEquals(
        'Failure cause was not as expected',
        ModuleManager.FailureType.UNAUTHORIZED, Number(error21.failureType));
    assertEquals(
        'Error message was not as expected', 'Error loading b: Unauthorized',
        error21.message);

    // The last deferred should be dropped so it is neither called back nor
    // an error.
    assertFalse(calledBack22);
    assertNull(error22);
  },

  /**
     Tests that the right dependencies are cancelled on a loadMultiple
     failure.
   */
  testLoadMultipleWithFailingModuleDependencies() {
    const mm =
        getModuleManager({'a': [], 'b': [], 'c': ['b'], 'd': ['c'], 'e': []});
    mm.setLoader(createUnsuccessfulLoader(mm, 401));
    mm.setBatchModeEnabled(true);
    const cancelledIds = [];

    mm.registerCallback(
        ModuleManager.CallbackType.ERROR, (callbackType, id, cause) => {
          assertEquals(
              'Failure cause was not as expected',
              ModuleManager.FailureType.UNAUTHORIZED, cause);
          cancelledIds.push(id);
        });
    let calledBack11 = false;
    let error11 = null;
    let calledBack12 = false;
    let error12 = null;
    let calledBack21 = false;
    let error21 = null;
    let calledBack22 = false;
    let error22 = null;
    let calledBack23 = false;
    let error23 = null;

    const dMap = mm.loadMultiple(['a', 'b']);
    dMap['a'].then(
        (ctx) => {
          calledBack11 = true;
        },
        (err) => {
          error11 = err;
        });
    dMap['b'].then(
        (ctx) => {
          calledBack12 = true;
        },
        (err) => {
          error12 = err;
        });

    const dMap2 = mm.loadMultiple(['c', 'd', 'e']);
    dMap2['c'].then(
        (ctx) => {
          calledBack21 = true;
        },
        (err) => {
          error21 = err;
        });
    dMap2['d'].then(
        (ctx) => {
          calledBack22 = true;
        },
        (err) => {
          error22 = err;
        });
    dMap2['e'].then(
        (ctx) => {
          calledBack23 = true;
        },
        (err) => {
          error23 = err;
        });

    assertFalse(calledBack11);
    assertFalse(calledBack12);
    assertFalse(calledBack21);
    assertFalse(calledBack22);
    assertFalse(calledBack23);
    assertNull(error11);
    assertNull(error12);
    assertNull(error21);
    assertNull(error22);
    assertNull(error23);

    clock.tick(5);

    assertFalse(calledBack11);
    assertFalse(calledBack12);
    assertFalse(calledBack21);
    assertFalse(calledBack22);
    assertFalse(calledBack23);

    // NOTE: Deferred always calls errbacks with an Error object.  The
    // failure type enum is present as error.failureType, while the error
    // message is human readable and contains the module id.
    assertEquals(
        'Failure cause was not as expected',
        ModuleManager.FailureType.UNAUTHORIZED, error11.failureType);
    assertEquals(
        'Error message was not as expected', 'Error loading a: Unauthorized',
        error11.message);
    assertEquals(
        'Failure cause was not as expected',
        ModuleManager.FailureType.UNAUTHORIZED, error12.failureType);
    assertEquals(
        'Error message was not as expected', 'Error loading b: Unauthorized',
        error12.message);

    // Check that among the failed modules, 'c' and 'd' are also cancelled
    // due to dependencies.
    assertTrue(googArray.equals(['a', 'b', 'c', 'd'], cancelledIds.sort()));
  },

  /**
   * Tests that when loading multiple modules, the input array is not
   * modified when it has duplicates.
   */
  testLoadMultipleWithDuplicates() {
    const mm = getModuleManager({'a': [], 'b': []});
    mm.setBatchModeEnabled(true);
    mm.setLoader(createSuccessfulBatchLoader(mm));

    const listWithDuplicates = ['a', 'a', 'b'];
    mm.loadMultiple(listWithDuplicates);
    assertArrayEquals(
        'loadMultiple should not modify its input', ['a', 'a', 'b'],
        listWithDuplicates);
  },

  /**
   * Test loading dependencies transitively.
   * @suppress {missingProperties} suppression added to enable type checking
   */
  testLoadingDepsInNonBatchMode1() {
    const mm =
        getModuleManager({'i': [], 'j': [], 'k': ['j'], 'l': ['i', 'j', 'k']});
    mm.setLoader(createSuccessfulNonBatchLoader(mm));

    mm.preloadModule('j');
    clock.tick(5);
    assertTrue('module "j" should be loaded', mm.isModuleLoaded('j'));
    assertFalse('module "i" should not be loaded (1)', mm.isModuleLoaded('i'));
    assertFalse('module "k" should not be loaded (1)', mm.isModuleLoaded('k'));
    assertFalse('module "l" should not be loaded (1)', mm.isModuleLoaded('l'));

    // When loading a module in non-batch mode, its dependencies should be
    // requested independently, and in dependency order.
    mm.preloadModule('l');
    clock.tick(5);
    assertTrue('module "i" should be loaded', mm.isModuleLoaded('i'));
    assertFalse('module "k" should not be loaded (2)', mm.isModuleLoaded('k'));
    assertFalse('module "l" should not be loaded (2)', mm.isModuleLoaded('l'));
    clock.tick(5);
    assertTrue('module "k" should be loaded', mm.isModuleLoaded('k'));
    assertFalse('module "l" should not be loaded (3)', mm.isModuleLoaded('l'));
    clock.tick(5);
    assertTrue('module "l" should be loaded', mm.isModuleLoaded('l'));
  },

  /**
     Test loading dependencies transitively and in dependency order.
     @suppress {missingProperties} suppression added to enable type checking
   */
  testLoadingDepsInNonBatchMode2() {
    const mm = getModuleManager({
      'h': [],
      'i': ['h'],
      'j': ['i'],
      'k': ['j'],
      'l': ['i', 'j', 'k'],
      'm': ['l'],
    });
    mm.setLoader(createSuccessfulNonBatchLoader(mm));

    // When loading a module in non-batch mode, its dependencies should be
    // requested independently, and in dependency order. The order in this
    // case should be h,i,j,k,l,m.
    mm.preloadModule('m');
    clock.tick(5);
    assertTrue('module "h" should be loaded', mm.isModuleLoaded('h'));
    assertFalse('module "i" should not be loaded (1)', mm.isModuleLoaded('i'));
    assertFalse('module "j" should not be loaded (1)', mm.isModuleLoaded('j'));
    assertFalse('module "k" should not be loaded (1)', mm.isModuleLoaded('k'));
    assertFalse('module "l" should not be loaded (1)', mm.isModuleLoaded('l'));
    assertFalse('module "m" should not be loaded (1)', mm.isModuleLoaded('m'));

    clock.tick(5);
    assertTrue('module "i" should be loaded', mm.isModuleLoaded('i'));
    assertFalse('module "j" should not be loaded (2)', mm.isModuleLoaded('j'));
    assertFalse('module "k" should not be loaded (2)', mm.isModuleLoaded('k'));
    assertFalse('module "l" should not be loaded (2)', mm.isModuleLoaded('l'));
    assertFalse('module "m" should not be loaded (2)', mm.isModuleLoaded('m'));

    clock.tick(5);
    assertTrue('module "j" should be loaded', mm.isModuleLoaded('j'));
    assertFalse('module "k" should not be loaded (3)', mm.isModuleLoaded('k'));
    assertFalse('module "l" should not be loaded (3)', mm.isModuleLoaded('l'));
    assertFalse('module "m" should not be loaded (3)', mm.isModuleLoaded('m'));

    clock.tick(5);
    assertTrue('module "k" should be loaded', mm.isModuleLoaded('k'));
    assertFalse('module "l" should not be loaded (4)', mm.isModuleLoaded('l'));
    assertFalse('module "m" should not be loaded (4)', mm.isModuleLoaded('m'));

    clock.tick(5);
    assertTrue('module "l" should be loaded', mm.isModuleLoaded('l'));
    assertFalse('module "m" should not be loaded (5)', mm.isModuleLoaded('m'));

    clock.tick(5);
    assertTrue('module "m" should be loaded', mm.isModuleLoaded('m'));
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testLoadingDepsInBatchMode() {
    const mm =
        getModuleManager({'e': [], 'f': [], 'g': ['f'], 'h': ['e', 'f', 'g']});
    mm.setLoader(createSuccessfulBatchLoader(mm));
    mm.setBatchModeEnabled(true);

    mm.preloadModule('f');
    clock.tick(5);
    assertTrue('module "f" should be loaded', mm.isModuleLoaded('f'));
    assertFalse('module "e" should not be loaded (1)', mm.isModuleLoaded('e'));
    assertFalse('module "g" should not be loaded (1)', mm.isModuleLoaded('g'));
    assertFalse('module "h" should not be loaded (1)', mm.isModuleLoaded('h'));

    // When loading a module in batch mode, its not-yet-loaded dependencies
    // should be requested at the same time, and in dependency order.
    mm.preloadModule('h');
    clock.tick(5);
    assertTrue('module "e" should be loaded', mm.isModuleLoaded('e'));
    assertFalse('module "g" should not be loaded (2)', mm.isModuleLoaded('g'));
    assertFalse('module "h" should not be loaded (2)', mm.isModuleLoaded('h'));
    clock.tick(2);
    assertTrue('module "g" should be loaded', mm.isModuleLoaded('g'));
    assertFalse('module "h" should not be loaded (3)', mm.isModuleLoaded('h'));
    clock.tick(2);
    assertTrue('module "h" should be loaded', mm.isModuleLoaded('h'));
  },

  /**
     Test unauthorized errors while loading modules.
     @suppress {missingProperties} suppression added to enable type checking
   */
  testUnauthorizedLoading() {
    const mm = getModuleManager({'m': [], 'n': [], 'o': ['n']});
    mm.setLoader(createUnsuccessfulLoader(mm, 401));

    // Callback checks for an unauthorized error
    let firedLoadFailed = false;
    mm.registerCallback(
        ModuleManager.CallbackType.ERROR, (callbackType, id, cause) => {
          assertEquals(
              'Failure cause was not as expected',
              ModuleManager.FailureType.UNAUTHORIZED, cause);
          firedLoadFailed = true;
        });
    mm.execOnLoad('o', () => {});
    assertTrue('module "o" should be loading', mm.isModuleLoading('o'));
    assertTrue('module "n" should be loading', mm.isModuleLoading('n'));
    clock.tick(5);
    assertTrue(
        'should have called unauthorized module callback', firedLoadFailed);
    assertFalse('module "o" should not be loaded', mm.isModuleLoaded('o'));
    assertFalse('module "o" should not be loading', mm.isModuleLoading('o'));
    assertFalse('module "n" should not be loaded', mm.isModuleLoaded('n'));
    assertFalse('module "n" should not be loading', mm.isModuleLoading('n'));
  },

  /**
     Test error loading modules which are retried.
     @suppress {missingProperties} suppression added to enable type checking
   */
  testErrorLoadingModule() {
    const mm = getModuleManager({'p': ['q'], 'q': [], 'r': ['q', 'p']});
    mm.setLoader(createUnsuccessfulLoader(mm, 500));

    mm.preloadModule('r');
    clock.tick(4);

    // A module request is now underway using the unsuccessful loader.
    // We substitute a successful loader for future module load requests.
    mm.setLoader(createSuccessfulNonBatchLoader(mm));
    clock.tick(1);
    assertFalse('module "q" should not be loaded (1)', mm.isModuleLoaded('q'));
    assertFalse('module "p" should not be loaded (1)', mm.isModuleLoaded('p'));
    assertFalse('module "r" should not be loaded (1)', mm.isModuleLoaded('r'));

    // Failed loads are automatically retried after a backOff.
    clock.tick(5 + mm.getBackOff_());
    assertTrue('module "q" should be loaded', mm.isModuleLoaded('q'));
    assertFalse('module "p" should not be loaded (2)', mm.isModuleLoaded('p'));
    assertFalse('module "r" should not be loaded (2)', mm.isModuleLoaded('r'));

    // A successful load decrements the backOff.
    clock.tick(5);
    assertTrue('module "p" should be loaded', mm.isModuleLoaded('p'));
    assertFalse('module "r" should not be loaded (3)', mm.isModuleLoaded('r'));
    clock.tick(5);
    assertTrue('module "r" should be loaded', mm.isModuleLoaded('r'));
  },

  /**
     Tests error loading modules which are retried.
     @suppress {missingProperties} suppression added to enable type checking
   */
  testErrorLoadingModule_batchMode() {
    const mm = getModuleManager({'p': ['q'], 'q': [], 'r': ['q', 'p']});
    mm.setLoader(createUnsuccessfulBatchLoader(mm, 500));
    mm.setBatchModeEnabled(true);

    mm.preloadModule('r');
    clock.tick(4);

    // A module request is now underway using the unsuccessful loader.
    // We substitute a successful loader for future module load requests.
    mm.setLoader(createSuccessfulBatchLoader(mm));
    clock.tick(1);
    assertFalse('module "q" should not be loaded (1)', mm.isModuleLoaded('q'));
    assertFalse('module "p" should not be loaded (1)', mm.isModuleLoaded('p'));
    assertFalse('module "r" should not be loaded (1)', mm.isModuleLoaded('r'));

    // Failed loads are automatically retried after a backOff.
    clock.tick(5 + mm.getBackOff_());
    assertTrue('module "q" should be loaded', mm.isModuleLoaded('q'));
    clock.tick(2);
    assertTrue('module "p" should not be loaded (2)', mm.isModuleLoaded('p'));
    clock.tick(2);
    assertTrue('module "r" should not be loaded (2)', mm.isModuleLoaded('r'));
  },

  /**
     Test consecutive errors in loading modules.
     @suppress {missingProperties} suppression added to enable type checking
   */
  testConsecutiveErrors() {
    const mm = getModuleManager({'s': []});
    mm.setLoader(createUnsuccessfulLoader(mm, 500));

    // Register an error callback for consecutive failures.
    let firedLoadFailed = false;
    mm.registerCallback(
        ModuleManager.CallbackType.ERROR, (callbackType, id, cause) => {
          assertEquals(
              'Failure cause was not as expected',
              ModuleManager.FailureType.CONSECUTIVE_FAILURES, cause);
          firedLoadFailed = true;
        });

    mm.preloadModule('s');
    assertFalse('module "s" should not be loaded (0)', mm.isModuleLoaded('s'));

    // Fail twice.
    for (let i = 0; i < 2; i++) {
      clock.tick(5 + mm.getBackOff_());
      assertFalse(
          'module "s" should not be loaded (1)', mm.isModuleLoaded('s'));
      assertFalse('should not fire failed callback (1)', firedLoadFailed);
    }

    // Fail a third time and check that the callback is fired.
    clock.tick(5 + mm.getBackOff_());
    assertFalse('module "s" should not be loaded (2)', mm.isModuleLoaded('s'));
    assertTrue('should have fired failed callback', firedLoadFailed);

    // Check that it doesn't attempt to load the module anymore after it has
    // failed.
    let triedLoad = false;
    mm.setLoader({
      loadModules: function(ids, moduleInfoMap, {onError, onSuccess}) {
        triedLoad = true;
      },
    });

    // Also reset the failed callback flag and make sure it isn't called
    // again.
    firedLoadFailed = false;
    clock.tick(10 + mm.getBackOff_());
    assertFalse('module "s" should not be loaded (3)', mm.isModuleLoaded('s'));
    assertFalse('No more loads should have been tried', triedLoad);
    assertFalse(
        'The load failed callback should be fired only once', firedLoadFailed);
  },

  /**
   * Test loading errors due to old code.
   * @suppress {missingProperties} suppression added to enable type checking
   */
  testOldCodeGoneError() {
    const mm = getModuleManager({'s': []});
    mm.setLoader(createUnsuccessfulLoader(mm, 410));

    // Callback checks for an old code failure
    let firedLoadFailed = false;
    mm.registerCallback(
        ModuleManager.CallbackType.ERROR, (callbackType, id, cause) => {
          assertEquals(
              'Failure cause was not as expected',
              ModuleManager.FailureType.OLD_CODE_GONE, cause);
          firedLoadFailed = true;
        });

    mm.preloadModule('s', 0);
    assertFalse('module "s" should not be loaded (0)', mm.isModuleLoaded('s'));
    clock.tick(5);
    assertFalse('module "s" should not be loaded (1)', mm.isModuleLoaded('s'));
    assertTrue('should have called old code gone callback', firedLoadFailed);
  },

  /**
   * Test timeout.
   * @suppress {missingProperties,checkTypes} suppression
   *      added to enable type checking
   */
  testTimeout() {
    const mm = getModuleManager({'s': []});
    mm.setLoader(createTimeoutLoader(mm, undefined));

    // Callback checks for timeout
    let firedTimeout = false;
    mm.registerCallback(
        ModuleManager.CallbackType.ERROR, (callbackType, id, cause) => {
          assertEquals(
              'Failure cause was not as expected',
              ModuleManager.FailureType.TIMEOUT, cause);
          firedTimeout = true;
        });

    mm.preloadModule('s', 0);
    assertFalse('module "s" should not be loaded (0)', mm.isModuleLoaded('s'));
    clock.tick(5);
    assertFalse('module "s" should not be loaded (1)', mm.isModuleLoaded('s'));
    assertTrue('should have called timeout callback', firedTimeout);
  },

  /**
   * Tests that an error during execOnLoad will trigger the error callback.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testExecOnLoadError() {
    // Expect two callbacks, each of which will be called with callback type
    // ERROR, the right module id and failure type INIT_ERROR.
    const errorCallback1 = testing.createFunctionMock('callback1');
    errorCallback1(
        ModuleManager.CallbackType.ERROR, 'b',
        ModuleManager.FailureType.INIT_ERROR);

    const errorCallback2 = testing.createFunctionMock('callback2');
    errorCallback2(
        ModuleManager.CallbackType.ERROR, 'b',
        ModuleManager.FailureType.INIT_ERROR);

    errorCallback1.$replay();
    errorCallback2.$replay();

    const mm = new ModuleManager();
    mm.setLoader(createSuccessfulNonBatchLoader(mm));

    // Register the first callback before setting the module info map.
    mm.registerCallback(ModuleManager.CallbackType.ERROR, errorCallback1);

    mm.setAllModuleInfo({'a': [], 'b': [], 'c': []});

    // Register the second callback after setting the module info map.
    mm.registerCallback(ModuleManager.CallbackType.ERROR, errorCallback2);

    let execOnLoadBCalled = false;
    mm.execOnLoad('b', () => {
      execOnLoadBCalled = true;
      throw new Error();
    });

    assertThrows(() => {
      clock.tick(5);
    });

    assertTrue(
        'execOnLoad should have been called on module b.', execOnLoadBCalled);
    errorCallback1.$verify();
    errorCallback2.$verify();
  },

  /**
   * Tests that an error during execOnLoad will trigger the error callback.
   * Uses setAllModuleInfoString rather than setAllModuleInfo.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testExecOnLoadErrorModuleInfoString() {
    // Expect a callback to be called with callback type ERROR, the right
    // module id and failure type INIT_ERROR.
    const errorCallback = testing.createFunctionMock('callback');
    errorCallback(
        ModuleManager.CallbackType.ERROR, 'b',
        ModuleManager.FailureType.INIT_ERROR);

    errorCallback.$replay();

    const mm = new ModuleManager();
    mm.setLoader(createSuccessfulNonBatchLoader(mm));

    // Register the first callback before setting the module info map.
    mm.registerCallback(ModuleManager.CallbackType.ERROR, errorCallback);

    mm.setAllModuleInfoString('a/b/c');

    let execOnLoadBCalled = false;
    mm.execOnLoad('b', () => {
      execOnLoadBCalled = true;
      throw new Error();
    });

    assertThrows(() => {
      clock.tick(5);
    });

    assertTrue(
        'execOnLoad should have been called on module b.', execOnLoadBCalled);
    errorCallback.$verify();
  },

  /** Make sure ModuleInfo objects in moduleInfoMap_ get disposed. */
  testDispose() {
    const mm = getModuleManager({'a': [], 'b': [], 'c': []});

    const moduleInfoA = mm.getModuleInfo('a');
    assertNotNull(moduleInfoA);
    const moduleInfoB = mm.getModuleInfo('b');
    assertNotNull(moduleInfoB);
    const moduleInfoC = mm.getModuleInfo('c');
    assertNotNull(moduleInfoC);

    mm.dispose();
    assertTrue(moduleInfoA.isDisposed());
    assertTrue(moduleInfoB.isDisposed());
    assertTrue(moduleInfoC.isDisposed());
  },

  testDependencyOrderingWithSimpleDeps() {
    const mm = getModuleManager({
      'a': ['b', 'c'],
      'b': ['d'],
      'c': ['e', 'f'],
      'd': [],
      'e': [],
      'f': [],
    });
    const ids = mm.getNotYetLoadedTransitiveDepIds_('a');
    assertDependencyOrder(ids, mm);
    assertArrayEquals(['d', 'e', 'f', 'b', 'c', 'a'], ids);
  },

  testDependencyOrderingWithRequestedDep() {
    const mm = getModuleManager({
      'a': ['b', 'c'],
      'b': ['d'],
      'c': ['e', 'f'],
      'd': [],
      'e': [],
      'f': [],
    });
    mm.requestedModuleIds_ = ['a', 'b'];
    const ids = mm.getNotYetLoadedTransitiveDepIds_('a');
    assertDependencyOrder(ids, mm);
    assertArrayEquals(['e', 'f', 'c'], ids);
  },

  testDependencyOrderingWithCommonDepsInDeps() {
    // Tests to make sure that if dependencies of the root are loaded before
    // their common dependencies.
    const mm =
        getModuleManager({'a': ['b', 'c'], 'b': ['d'], 'c': ['d'], 'd': []});
    const ids = mm.getNotYetLoadedTransitiveDepIds_('a');
    assertDependencyOrder(ids, mm);
    assertArrayEquals(['d', 'b', 'c', 'a'], ids);
  },

  testDependencyOrderingWithCommonDepsInRoot1() {
    // Tests the case where a dependency of the root depends on another
    // dependency of the root.  Regardless of ordering in the root's
    // deps.
    const mm = getModuleManager({'a': ['b', 'c'], 'b': ['c'], 'c': []});
    const ids = mm.getNotYetLoadedTransitiveDepIds_('a');
    assertDependencyOrder(ids, mm);
    assertArrayEquals(['c', 'b', 'a'], ids);
  },

  testDependencyOrderingWithCommonDepsInRoot2() {
    // Tests the case where a dependency of the root depends on another
    // dependency of the root.  Regardless of ordering in the root's
    // deps.
    const mm = getModuleManager({'a': ['b', 'c'], 'b': [], 'c': ['b']});
    const ids = mm.getNotYetLoadedTransitiveDepIds_('a');
    assertDependencyOrder(ids, mm);
    assertArrayEquals(['b', 'c', 'a'], ids);
  },

  testDependencyOrderingWithGmailExample() {
    // Real dependency graph taken from gmail.
    const mm = getModuleManager({
      's': ['dp', 'ml', 'md'],
      'dp': ['a'],
      'ml': ['ld', 'm'],
      'ld': ['a'],
      'm': ['ad', 'mh', 'n'],
      'md': ['mh', 'ld'],
      'a': [],
      'mh': [],
      'ad': [],
      'n': [],
    });

    mm.beforeLoadModuleCode('a');
    mm.setLoaded();
    mm.beforeLoadModuleCode('m');
    mm.setLoaded();
    mm.beforeLoadModuleCode('n');
    mm.setLoaded();
    mm.beforeLoadModuleCode('ad');
    mm.setLoaded();
    mm.beforeLoadModuleCode('mh');
    mm.setLoaded();

    const ids = mm.getNotYetLoadedTransitiveDepIds_('s');
    assertDependencyOrder(ids, mm);
    assertArrayEquals(['ld', 'dp', 'ml', 'md', 's'], ids);
  },

  testRegisterInitializationCallback() {
    let initCalled = 0;
    const mm = getModuleManager({'a': [], 'b': [], 'c': []});
    mm.setLoader(
        createSuccessfulNonBatchLoaderWithRegisterInitCallback(mm, () => {
          ++initCalled;
        }));
    execOnLoad(mm);
    // execOnLoad_ loads modules a and c
    assertTrue(initCalled == 2);
  },

  testSetModuleConstructor() {
    const initCalled = 0;
    const mm = getModuleManager({'a': [], 'b': [], 'c': []});
    const info = {
      'a': {ctor: AModule, count: 0},
      'b': {ctor: BModule, count: 0},
      'c': {ctor: CModule, count: 0},
    };
    function AModule() {
      ++info['a'].count;
      BaseModule.call(this);
    }
    goog.inherits(AModule, BaseModule);
    function BModule() {
      ++info['b'].count;
      BaseModule.call(this);
    }
    goog.inherits(BModule, BaseModule);
    function CModule() {
      ++info['c'].count;
      BaseModule.call(this);
    }
    goog.inherits(CModule, BaseModule);

    mm.setLoader(createSuccessfulNonBatchLoaderWithConstructor(mm, info));
    execOnLoad(mm);
    assertTrue(info['a'].count == 1);
    assertTrue(info['b'].count == 0);
    assertTrue(info['c'].count == 1);
    assertTrue(mm.getModuleInfo('a').getModule() instanceof AModule);
    assertTrue(mm.getModuleInfo('c').getModule() instanceof CModule);
  },

  /**
   * Tests that a call to load the loading module during module
   * initialization doesn't trigger a second load.
   */
  testLoadWhenInitializing() {
    const mm = getModuleManager({'a': []});
    mm.setLoader(createSuccessfulNonBatchLoader(mm));

    const info = {'a': {ctor: AModule, count: 0}};
    function AModule() {
      ++info['a'].count;
      BaseModule.call(this);
    }
    goog.inherits(AModule, BaseModule);
    AModule.prototype.initialize = () => {
      mm.load('a');
    };
    mm.setLoader(createSuccessfulNonBatchLoaderWithConstructor(mm, info));
    mm.preloadModule('a');
    clock.tick(5);
    assertEquals(info['a'].count, 1);
  },

  testErrorInEarlyCallback() {
    const errback = recordFunction();
    const callback = recordFunction();
    const mm = getModuleManager({'a': [], 'b': ['a']});
    mm.getModuleInfo('a').registerEarlyCallback(functions.error('error'));
    mm.getModuleInfo('a').registerCallback(callback);
    mm.getModuleInfo('a').registerErrback(errback);

    mm.setLoader(createSuccessfulNonBatchLoaderWithConstructor(
        mm, createModulesFor('a', 'b')));
    mm.preloadModule('b');
    const e = assertThrows(() => {
      clock.tick(5);
    });

    assertEquals('error', e.message);
    assertEquals(0, callback.getCallCount());
    assertEquals(1, errback.getCallCount());
    assertEquals(
        ModuleManager.FailureType.INIT_ERROR,
        errback.getLastCall().getArguments()[0]);
    assertTrue(mm.getModuleInfo('a').isLoaded());
    assertFalse(mm.getModuleInfo('b').isLoaded());

    clock.tick(5);
    assertTrue(mm.getModuleInfo('b').isLoaded());
  },

  testErrorInNormalCallback() {
    const earlyCallback = recordFunction();
    const errback = recordFunction();
    const mm = getModuleManager({'a': [], 'b': ['a']});
    mm.getModuleInfo('a').registerEarlyCallback(earlyCallback);
    mm.getModuleInfo('a').registerCallback(functions.error('error'));
    mm.getModuleInfo('a').registerErrback(errback);

    mm.setLoader(createSuccessfulNonBatchLoaderWithConstructor(
        mm, createModulesFor('a', 'b')));
    mm.preloadModule('b');
    const e = assertThrows(() => {
      clock.tick(10);
    });
    clock.tick(10);

    assertEquals('error', e.message);
    assertEquals(1, errback.getCallCount());
    assertEquals(
        ModuleManager.FailureType.INIT_ERROR,
        errback.getLastCall().getArguments()[0]);
    assertTrue(mm.getModuleInfo('a').isLoaded());
    assertTrue(mm.getModuleInfo('b').isLoaded());
  },

  testErrorInErrback() {
    const mm = getModuleManager({'a': [], 'b': ['a']});
    mm.getModuleInfo('a').registerCallback(functions.error('error1'));
    mm.getModuleInfo('a').registerErrback(functions.error('error2'));

    mm.setLoader(createSuccessfulNonBatchLoaderWithConstructor(
        mm, createModulesFor('a', 'b')));
    mm.preloadModule('a');
    let e = assertThrows(() => {
      clock.tick(10);
    });
    assertEquals('error1', e.message);
    e = assertThrows(() => {
      clock.tick(10);
    });
    assertEquals('error2', e.message);
    assertTrue(mm.getModuleInfo('a').isLoaded());
  },

  testInitCallbackInBaseModule() {
    let mm = new ModuleManager();
    let called = false;
    let context;
    mm.registerInitializationCallback((mcontext) => {
      called = true;
      context = mcontext;
    });
    mm.setAllModuleInfo({'a': [], 'b': ['a']});
    assertTrue('Base initialization not called', called);
    assertNull('Context should still be null', context);

    mm = new ModuleManager();
    called = false;
    mm.registerInitializationCallback((mcontext) => {
      called = true;
      context = mcontext;
    });
    const appContext = {};
    mm.setModuleContext(appContext);
    assertTrue('Base initialization not called after setModuleContext', called);
    assertEquals('Did not receive module context', appContext, context);
  },

  testSetAllModuleInfo() {
    const callback = recordFunction();
    const errback = recordFunction();
    const moduleInfo = {'base': [], 'one': ['base'], 'two': ['one']};
    const mm = getModuleManager(moduleInfo);
    mm.getModuleInfo('one').registerEarlyCallback(callback);
    mm.getModuleInfo('one').registerCallback(functions.error('error'));
    mm.getModuleInfo('one').registerErrback(errback);
    mm.setLoader(createSuccessfulNonBatchLoaderWithConstructor(
        mm, createModulesFor('base', 'one', 'two')));
    mm.preloadModule('base');
    clock.tick(10);
    // Module 'base' is now loaded.
    assertTrue(mm.getModuleInfo('base').isLoaded());
    // Re-init all modules using same instance.
    mm.setAllModuleInfo(moduleInfo);
    // Re-init all modules using new instance.
    mm.setAllModuleInfo({'base': [], 'one': ['base'], 'two': ['one']});
    // Module 'base' is still loaded.
    assertTrue(mm.getModuleInfo('base').isLoaded());

    // Callbacks are still registered.
    mm.preloadModule('two');
    assertThrows(() => {
      clock.tick(10);
    });
    clock.tick(10);

    assertEquals(1, callback.getCallCount());
    assertEquals(1, errback.getCallCount());
  },

  testSetAllModuleInfoString() {
    const callback = recordFunction();
    const errback = recordFunction();
    const moduleInfo = {'base': [], 'one': ['base'], 'two': ['one']};
    const mm = getModuleManager(moduleInfo);
    mm.getModuleInfo('one').registerEarlyCallback(callback);
    mm.getModuleInfo('one').registerCallback(functions.error('error'));
    mm.getModuleInfo('one').registerErrback(errback);
    mm.setLoader(createSuccessfulNonBatchLoaderWithConstructor(
        mm, createModulesFor('base', 'one', 'two')));
    mm.preloadModule('base');
    clock.tick(10);
    // Module 'base' is now loaded.
    assertTrue(mm.getModuleInfo('base').isLoaded());
    // Re-init all modules using same instance.
    mm.setAllModuleInfoString('base/one:0/two:1/three:0,1,2/four:0,3/five:');
    // Module 'base' is still loaded.
    assertTrue(mm.getModuleInfo('base').isLoaded());

    assertNotNull('Base should exist', mm.getModuleInfo('base'));
    assertNotNull('One should exist', mm.getModuleInfo('one'));
    assertNotNull('Two should exist', mm.getModuleInfo('two'));
    assertNotNull('Three should exist', mm.getModuleInfo('three'));
    assertNotNull('Four should exist', mm.getModuleInfo('four'));
    assertNotNull('Five should exist', mm.getModuleInfo('five'));

    assertArrayEquals(
        ['base', 'one', 'two'], mm.getModuleInfo('three').getDependencies());
    assertArrayEquals(
        ['base', 'three'], mm.getModuleInfo('four').getDependencies());
    assertArrayEquals([], mm.getModuleInfo('five').getDependencies());

    // Callbacks are still registered.
    mm.preloadModule('two');
    assertThrows(() => {
      clock.tick(10);
    });
    clock.tick(10);

    assertEquals(1, callback.getCallCount());
    assertEquals(1, errback.getCallCount());
  },

  testSetAllModuleInfoStringWithEmptyString() {
    const mm = new ModuleManager();
    let called = false;
    let context;
    mm.registerInitializationCallback((mcontext) => {
      called = true;
      context = mcontext;
    });
    mm.setAllModuleInfoString('');
    assertTrue('Initialization not called', called);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testBackOffAmounts() {
    const mm = new ModuleManager();
    assertEquals(0, mm.getBackOff_());

    mm.consecutiveFailures_++;
    assertEquals(5000, mm.getBackOff_());

    mm.consecutiveFailures_++;
    assertEquals(20000, mm.getBackOff_());
  },

  /**
   * Tests that the IDLE callbacks are executed for active->idle transitions
   * after setAllModuleInfoString with currently loading modules.
   */
  testIdleCallbackWithInitialModules() {
    const callback = recordFunction();

    const mm = new ModuleManager();
    mm.setAllModuleInfoString('a', ['a']);
    mm.registerCallback(ModuleManager.CallbackType.IDLE, callback);

    assertTrue(mm.isActive());

    mm.beforeLoadModuleCode('a');

    assertEquals(0, callback.getCallCount());

    mm.setLoaded();

    assertFalse(mm.isActive());

    assertEquals(1, callback.getCallCount());
  },
});
