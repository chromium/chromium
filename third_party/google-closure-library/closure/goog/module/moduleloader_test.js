/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Tests for ModuleLoader.
 * @suppress {missingRequire} swapping XmlHttp
 */

goog.module('goog.module.ModuleLoaderTest');
goog.setTestOnly();

const BulkLoader = goog.require('goog.net.BulkLoader');
const Const = goog.require('goog.string.Const');
const EventObserver = goog.require('goog.testing.events.EventObserver');
const GoogPromise = goog.require('goog.Promise');
const ModuleLoader = goog.require('goog.module.ModuleLoader');
const ModuleManager = goog.require('goog.module.ModuleManager');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TagName = goog.require('goog.dom.TagName');
const TestCase = goog.require('goog.testing.TestCase');
const TrustedResourceUrl = goog.require('goog.html.TrustedResourceUrl');
const XmlHttp = goog.require('goog.net.XmlHttp');
const activeModuleManager = goog.require('goog.loader.activeModuleManager');
const dispose = goog.require('goog.dispose');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const functions = goog.require('goog.functions');
const googArray = goog.require('goog.array');
const googObject = goog.require('goog.object');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

/**
 * @suppress {strictMissingProperties} suppression added to enable type
 * checking
 */
window.modA1Loaded = false;
/**
 * @suppress {strictMissingProperties} suppression added to enable type
 * checking
 */
window.modA2Loaded = false;
/**
 * @suppress {strictMissingProperties} suppression added to enable type
 * checking
 */
window.modB1Loaded = false;

let moduleLoader = null;
let moduleManager = null;
const stubs = new PropertyReplacer();
const modA1 = TrustedResourceUrl.fromConstant(Const.from('testdata/modA_1.js'));
const modA2 = TrustedResourceUrl.fromConstant(Const.from('testdata/modA_2.js'));
const modB1 = TrustedResourceUrl.fromConstant(Const.from('testdata/modB_1.js'));

const EventType = ModuleLoader.EventType;
let observer;

function assertSourceInjection() {
  return new GoogPromise((resolve, reject) => {
           moduleManager.execOnLoad('modB', resolve);
         })
      .then(/**
               @suppress {undefinedVars} suppression added to enable type
               checking
             */
            () => {
              assertTrue(!!throwErrorInModuleB);

              const ex = assertThrows(() => {
                throwErrorInModuleB();
              });

              if (!ex.stack) {
                return;
              }

              const stackTrace = ex.stack.toString();
              const expectedString = 'testdata/modB_1.js';

              if (ModuleLoader.supportsSourceUrlStackTraces()) {
                // Source URL should be added in eval or in jsloader.
                assertContains(expectedString, stackTrace);
              } else if (moduleLoader.getDebugMode()) {
                // Browsers used jsloader, thus URLs are present.
                assertContains(expectedString, stackTrace);
              } else {
                // Browser used eval, does not support source URL.
                assertNotContains(expectedString, stackTrace);
              }
            });
}

function assertLoaded(id) {
  assertTrue(moduleManager.getModuleInfo(id).isLoaded());
}

function assertNotLoaded(id) {
  assertFalse(moduleManager.getModuleInfo(id).isLoaded());
}
testSuite({
  setUpPage() {
    TestCase.getActiveTestCase().promiseTimeout = 10000;  // 10s
  },

  setUp() {
    /** @suppress {undefinedVars} suppression added to enable type checking */
    modA1Loaded = false;
    /** @suppress {undefinedVars} suppression added to enable type checking */
    modA2Loaded = false;
    /** @suppress {undefinedVars} suppression added to enable type checking */
    modB1Loaded = false;

    goog.provide = goog.nullFunction;
    moduleManager = ModuleManager.getInstance();
    stubs.replace(moduleManager, 'getBackOff_', functions.constant(0));

    moduleLoader = new ModuleLoader();
    observer = new EventObserver();

    events.listen(moduleLoader, googObject.getValues(EventType), observer);

    moduleManager.setLoader(moduleLoader);
    moduleManager.setAllModuleInfo({'modA': [], 'modB': ['modA']});
    moduleManager.setModuleTrustedUris(
        {'modA': [modA1, modA2], 'modB': [modB1]});

    assertNotLoaded('modA');
    assertNotLoaded('modB');
    assertFalse(modA1Loaded);
  },

  tearDown() {
    stubs.reset();
    dispose(moduleLoader);

    // Ensure that the module manager was created.
    assertNotNull(ModuleManager.getInstance());
    activeModuleManager.reset();

    // tear down the module loaded flag.
    modA1Loaded = false;

    // Remove all the fake scripts.
    const scripts = googArray.clone(dom.getElementsByTagName(TagName.SCRIPT));
    for (let i = 0; i < scripts.length; i++) {
      if (scripts[i].src.indexOf('testdata') != -1) {
        dom.removeNode(scripts[i]);
      }
    }
  },

  testLoadModuleA() {
    return new GoogPromise((resolve, reject) => {
             moduleManager.execOnLoad('modA', () => {
               assertLoaded('modA');
               assertNotLoaded('modB');
               assertTrue(modA1Loaded);

               // The code is not evaluated immediately, but only after a
               // browser yield.
               assertEquals(
                   'EVALUATE_CODE', 0,
                   observer.getEvents(EventType.EVALUATE_CODE).length);
               assertEquals(
                   'REQUEST_SUCCESS', 1,
                   observer.getEvents(EventType.REQUEST_SUCCESS).length);
               assertArrayEquals(
                   ['modA'],
                   observer.getEvents(EventType.REQUEST_SUCCESS)[0].moduleIds);
               assertEquals(
                   'REQUEST_ERROR', 0,
                   observer.getEvents(EventType.REQUEST_ERROR).length);
               resolve();
             });
           })
        .then(() => {
          assertEquals(
              'EVALUATE_CODE after tick', 1,
              observer.getEvents(EventType.EVALUATE_CODE).length);
          assertEquals(
              'REQUEST_SUCCESS after tick', 1,
              observer.getEvents(EventType.REQUEST_SUCCESS).length);
          assertEquals(
              'REQUEST_ERROR after tick', 0,
              observer.getEvents(EventType.REQUEST_ERROR).length);
        });
  },

  testLoadModuleB() {
    return new GoogPromise((resolve, reject) => {
             moduleManager.execOnLoad('modB', resolve);
           })
        .then(() => {
          assertLoaded('modA');
          assertLoaded('modB');
          assertTrue(modA1Loaded);
        });
  },

  testLoadDebugModuleA() {
    moduleLoader.setDebugMode(true);
    return new GoogPromise((resolve, reject) => {
             moduleManager.execOnLoad('modA', resolve);
           })
        .then(() => {
          assertLoaded('modA');
          assertNotLoaded('modB');
          assertTrue(modA1Loaded);
        });
  },

  testLoadDebugModuleB() {
    moduleLoader.setDebugMode(true);
    return new GoogPromise((resolve, reject) => {
             moduleManager.execOnLoad('modB', resolve);
           })
        .then(() => {
          assertLoaded('modA');
          assertLoaded('modB');
          assertTrue(modA1Loaded);
        });
  },

  testLoadDebugModuleAThenB() {
    // Swap the script tags of module A, to introduce a race condition.
    // See the comments on this in ModuleLoader's debug loader.
    moduleManager.setModuleTrustedUris(
        {'modA': [modA2, modA1], 'modB': [modB1]});
    moduleLoader.setDebugMode(true);
    return new GoogPromise((resolve, reject) => {
             moduleManager.execOnLoad('modB', resolve);
           })
        .then(() => {
          assertLoaded('modA');
          assertLoaded('modB');

          const scripts =
              googArray.clone(dom.getElementsByTagName(TagName.SCRIPT));
          let seenLastScriptOfModuleA = false;
          for (let i = 0; i < scripts.length; i++) {
            const uri = scripts[i].src;
            if (uri.indexOf('modA_1.js') >= 0) {
              seenLastScriptOfModuleA = true;
            } else if (uri.indexOf('modB') >= 0) {
              assertTrue(seenLastScriptOfModuleA);
            }
          }
        });
  },

  testLoadScriptTagModuleA() {
    moduleLoader.setUseScriptTags(true);
    return new GoogPromise((resolve, reject) => {
             moduleManager.execOnLoad('modA', resolve);
           })
        .then(() => {
          assertLoaded('modA');
          assertNotLoaded('modB');
          assertTrue(modA1Loaded);
        });
  },

  testLoadScriptTagModuleB() {
    moduleLoader.setUseScriptTags(true);
    return new GoogPromise((resolve, reject) => {
             moduleManager.execOnLoad('modB', resolve);
           })
        .then(() => {
          assertLoaded('modA');
          assertLoaded('modB');
          assertTrue(modA1Loaded);
        });
  },

  testSourceInjection() {
    moduleLoader.setSourceUrlInjection(true);
    return assertSourceInjection();
  },

  testSourceInjectionViaDebugMode() {
    moduleLoader.setDebugMode(true);
    return assertSourceInjection();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testModuleLoaderRecursesTooDeep(opt_numModules) {
    // There was a bug in the module loader where it would retry recursively
    // whenever there was a synchronous failure in the module load. When you
    // asked for modB, it would try to load its dependency modA. When modA
    // failed, it would move onto modB, and then start over, repeating until it
    // ran out of stack.
    const numModules = opt_numModules || 1;
    const uris = {};
    const deps = {};
    const mods = [];
    for (let num = 0; num < numModules; num++) {
      const modName = `mod${num}`;
      mods.unshift(modName);
      uris[modName] = [];
      deps[modName] = num ? ['mod' + (num - 1)] : [];
      for (let i = 0; i < 5; i++) {
        uris[modName].push(TrustedResourceUrl.format(
            Const.from('https://www.google.com/crossdomain%{num}x%{i}.js'),
            {'num': num, 'i': i}));
      }
    }

    moduleManager.setAllModuleInfo(deps);
    moduleManager.setModuleTrustedUris(uris);

    // Make all XHRs throw an error, so that we test the error-handling
    // functionality.
    const oldXmlHttp = goog.net.XmlHttp;
    stubs.set(goog.net, 'XmlHttp', function() {
      return {open: functions.error('mock error'), abort: goog.nullFunction};
    });
    googObject.extend(goog.net.XmlHttp, oldXmlHttp);

    let errorCount = 0;
    const errorIds = [];
    const errorHandler = (ignored, modId) => {
      errorCount++;
      errorIds.push(modId);
    };
    moduleManager.registerCallback(
        ModuleManager.CallbackType.ERROR, errorHandler);

    moduleManager.execOnLoad(mods[0], () => {
      fail('modB should not load successfully');
    });

    assertEquals(mods.length, errorCount);

    googArray.sort(mods);
    googArray.sort(errorIds);
    assertArrayEquals(mods, errorIds);

    assertArrayEquals([], moduleManager.requestedModuleIdsQueue_);
    assertArrayEquals([], moduleManager.userInitiatedLoadingModuleIds_);
  },

  testModuleLoaderRecursesTooDeep2modules() {
    this.testModuleLoaderRecursesTooDeep(2);
  },

  testModuleLoaderRecursesTooDeep3modules() {
    this.testModuleLoaderRecursesTooDeep(3);
  },

  testModuleLoaderRecursesTooDeep4modules() {
    this.testModuleLoaderRecursesTooDeep(3);
  },

  testErrback() {
    // Don't run this test on IE, because the way the test runner catches
    // errors on IE plays badly with the simulated errors in the test.
    if (userAgent.IE) return;

    // Modules will throw an exception if this boolean is set to true.
    modA1Loaded = true;

    return new GoogPromise((resolve, reject) => {
      const errorHandler = () => {
        try {
          assertNotLoaded('modA');
        } catch (e) {
          reject(e);
        }
        resolve();
      };
      moduleManager.registerCallback(
          ModuleManager.CallbackType.ERROR, errorHandler);

      moduleManager.execOnLoad('modA', () => {
        fail('modA should not load successfully');
      });
    });
  },

  testEventError() {
    // Don't run this test on older IE, because the way the test runner catches
    // errors on IE plays badly with the simulated errors in the test.
    if (userAgent.IE && !userAgent.isVersionOrHigher(11)) {
      return;
    }

    // Modules will throw an exception if this boolean is set to true.
    modA1Loaded = true;

    return new GoogPromise((resolve, reject) => {
             const errorHandler = () => {
               try {
                 assertNotLoaded('modA');
               } catch (e) {
                 reject(e);
               }
               resolve();
             };
             moduleManager.registerCallback(
                 ModuleManager.CallbackType.ERROR, errorHandler);

             moduleManager.execOnLoad('modA', () => {
               fail('modA should not load successfully');
             });
           })
        .then(() => {
          assertEquals(
              'EVALUATE_CODE', 3,
              observer.getEvents(EventType.EVALUATE_CODE).length);
          assertUndefined(observer.getEvents(EventType.EVALUATE_CODE)[0].error);

          assertEquals(
              'REQUEST_SUCCESS', 3,
              observer.getEvents(EventType.REQUEST_SUCCESS).length);
          assertUndefined(
              observer.getEvents(EventType.REQUEST_SUCCESS)[0].error);

          const requestErrors = observer.getEvents(EventType.REQUEST_ERROR);
          assertEquals('REQUEST_ERROR', 3, requestErrors.length);
          const requestError = requestErrors[0];
          assertNotNull(requestError.error);
          const expectedString = 'loaded twice';
          const messageAndStack =
              requestErrors[0].error.message + requestErrors[0].error.stack;
          assertContains(expectedString, messageAndStack);
          assertNull(requestError.status);
        });
  },

  testPrefetchThenLoadModuleA() {
    moduleManager.prefetchModule('modA');
    stubs.set(BulkLoader.prototype, 'load', () => {
      fail('modA should not be reloaded');
    });

    return new GoogPromise((resolve, reject) => {
             moduleManager.execOnLoad('modA', resolve);
           })
        .then(() => {
          assertLoaded('modA');
          assertEquals(
              'REQUEST_SUCCESS', 1,
              observer.getEvents(EventType.REQUEST_SUCCESS).length);
          assertArrayEquals(
              ['modA'],
              observer.getEvents(EventType.REQUEST_SUCCESS)[0].moduleIds);
          assertEquals(
              'REQUEST_ERROR', 0,
              observer.getEvents(EventType.REQUEST_ERROR).length);
        });
  },

  testPrefetchThenLoadModuleB() {
    moduleManager.prefetchModule('modB');
    stubs.set(BulkLoader.prototype, 'load', () => {
      fail('modA and modB should not be reloaded');
    });

    return new GoogPromise((resolve, reject) => {
             moduleManager.execOnLoad('modB', resolve);
           })
        .then(() => {
          assertLoaded('modA');
          assertLoaded('modB');
          assertEquals(
              'REQUEST_SUCCESS', 2,
              observer.getEvents(EventType.REQUEST_SUCCESS).length);
          assertArrayEquals(
              ['modA'],
              observer.getEvents(EventType.REQUEST_SUCCESS)[0].moduleIds);
          assertArrayEquals(
              ['modB'],
              observer.getEvents(EventType.REQUEST_SUCCESS)[1].moduleIds);
          assertEquals(
              'REQUEST_ERROR', 0,
              observer.getEvents(EventType.REQUEST_ERROR).length);
        });
  },

  testPrefetchModuleAThenLoadModuleB() {
    moduleManager.prefetchModule('modA');

    return new GoogPromise((resolve, reject) => {
             moduleManager.execOnLoad('modB', resolve);
           })
        .then(() => {
          assertLoaded('modA');
          assertLoaded('modB');
          assertEquals(
              'REQUEST_SUCCESS', 2,
              observer.getEvents(EventType.REQUEST_SUCCESS).length);
          assertArrayEquals(
              ['modA'],
              observer.getEvents(EventType.REQUEST_SUCCESS)[0].moduleIds);
          assertArrayEquals(
              ['modB'],
              observer.getEvents(EventType.REQUEST_SUCCESS)[1].moduleIds);
          assertEquals(
              'REQUEST_ERROR', 0,
              observer.getEvents(EventType.REQUEST_ERROR).length);
        });
  },

  testLoadModuleBThenPrefetchModuleA() {
    return new GoogPromise((resolve, reject) => {
             moduleManager.execOnLoad('modB', resolve);
           })
        .then(() => {
          assertLoaded('modA');
          assertLoaded('modB');
          assertEquals(
              'REQUEST_SUCCESS', 2,
              observer.getEvents(EventType.REQUEST_SUCCESS).length);
          assertArrayEquals(
              ['modA'],
              observer.getEvents(EventType.REQUEST_SUCCESS)[0].moduleIds);
          assertArrayEquals(
              ['modB'],
              observer.getEvents(EventType.REQUEST_SUCCESS)[1].moduleIds);
          assertEquals(
              'REQUEST_ERROR', 0,
              observer.getEvents(EventType.REQUEST_ERROR).length);
          assertThrows('Module load already requested: modB', () => {
            moduleManager.prefetchModule('modA');
          });
        });
  },

  testPrefetchModuleWithBatchModeEnabled() {
    moduleManager.setBatchModeEnabled(true);
    assertThrows('Modules prefetching is not supported in batch mode', () => {
      moduleManager.prefetchModule('modA');
    });
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testLoadErrorCallbackExecutedWhenPrefetchFails() {
    // Make all XHRs throw an error, so that we test the error-handling
    // functionality.
    const oldXmlHttp = goog.net.XmlHttp;
    stubs.set(goog.net, 'XmlHttp', function() {
      return {open: functions.error('mock error'), abort: goog.nullFunction};
    });
    googObject.extend(goog.net.XmlHttp, oldXmlHttp);

    let errorCount = 0;
    const errorHandler = () => {
      errorCount++;
    };
    moduleManager.registerCallback(
        ModuleManager.CallbackType.ERROR, errorHandler);

    moduleLoader.prefetchModule('modA', moduleManager.moduleInfoMap['modA']);
    moduleLoader.loadModules(['modA'], moduleManager.moduleInfoMap, {
      onSuccess: () => {
        fail('modA should not load successfully');
      },
      onError: errorHandler,
    });

    assertEquals(1, errorCount);
  },
});
