/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.jsloaderTest');
goog.setTestOnly();

const Const = goog.require('goog.string.Const');
const ErrorCode = goog.require('goog.net.jsloader.ErrorCode');
const TagName = goog.require('goog.dom.TagName');
const TrustedResourceUrl = goog.require('goog.html.TrustedResourceUrl');
const dom = goog.require('goog.dom');
const googArray = goog.require('goog.array');
const jsloader = goog.require('goog.net.jsloader');
const testSuite = goog.require('goog.testing.testSuite');

// Sunny day scenario for load function.

// Sunny day scenario for safeLoadAndVerify function.

// What happens when the verification object is not set by the loaded script?

// Tests that callers can cancel the deferred without error.

// Test the safeLoadMany function.

// Test the load function with additional options.

testSuite({
  setUp() {
    goog.provide = goog.nullFunction;
  },

  tearDown() {
    // Remove all the fake scripts.
    const scripts = googArray.clone(dom.getElementsByTagName(TagName.SCRIPT));
    for (let i = 0; i < scripts.length; i++) {
      if (scripts[i].src.indexOf('testdata') != -1) {
        dom.removeNode(scripts[i]);
      }
    }
  },

  testSafeLoad() {
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    window.test1 = null;
    const testUrl = TrustedResourceUrl.fromConstant(
        Const.from('testdata/jsloader_test1.js'));
    const testUrlValue = TrustedResourceUrl.unwrap(testUrl);
    const result = jsloader.safeLoad(testUrl);

    return result
        .then(/**
                 @suppress {strictMissingProperties} suppression added to
                 enable type checking
               */
              () => {
                /**
                 * @suppress {visibility,strictMissingProperties} suppression
                 * added to enable type checking
                 */
                const script = result.defaultScope_.script_;

                assertNotNull('script created', script);
                assertEquals('encoding is utf-8', 'UTF-8', script.charset);

                // Check that the URI matches ours.
                assertTrue('server URI', script.src.indexOf(testUrlValue) >= 0);

                // Check that the script was really loaded.
                assertEquals(
                    'verification object', 'Test #1 loaded', window.test1);
              });
  },

  testSafeLoadAndVerify() {
    const testUrl = TrustedResourceUrl.fromConstant(
        Const.from('testdata/jsloader_test2.js'));
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = jsloader.safeLoadAndVerify(testUrl, 'test2');

    return result.then((verifyObj) => {
      // Check that the verification object has passed ok.
      assertEquals('verification object', 'Test #2 loaded', verifyObj);
    });
  },

  testSafeLoadAndVerifyError() {
    const testUrl = TrustedResourceUrl.fromConstant(
        Const.from('testdata/jsloader_test2.js'));
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = jsloader.safeLoadAndVerify(testUrl, 'fake');

    return result.then(
        fail, /**
                 @suppress {strictMissingProperties} suppression added to
                 enable type checking
               */
        (error) => {
          // Check that the error code is right.
          assertEquals(
              'verification error', ErrorCode.VERIFY_ERROR, error.code);
        });
  },

  testSafeLoadAndVerifyCanceled() {
    const testUrl = TrustedResourceUrl.fromConstant(
        Const.from('testdata/jsloader_test2.js'));
    /** @suppress {checkTypes} suppression added to enable type checking */
    const result = jsloader.safeLoadAndVerify(testUrl, 'test2');
    result.cancel();
  },

  testSafeLoadMany() {
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    window.test1 = null;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    window.test4 = null;

    // Load test #3 and then #1.
    const testUrls1 = [
      TrustedResourceUrl.fromConstant(Const.from('testdata/jsloader_test3.js')),
      TrustedResourceUrl.fromConstant(Const.from('testdata/jsloader_test1.js')),
    ];
    const result = jsloader.safeLoadMany(testUrls1);

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    window.test3Callback = (msg) => {
      // Check that the 1st test was not loaded yet.
      assertEquals('verification object', null, window.test1);

      // Load test #4, which is supposed to wait for #1 to load.
      const testUrls2 = [TrustedResourceUrl.fromConstant(
          Const.from('testdata/jsloader_test4.js'))];
      jsloader.safeLoadMany(testUrls2);
    };

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    window.test4Callback = (msg) => {
      // Check that the 1st test was already loaded.
      assertEquals('verification object', 'Test #1 loaded', window.test1);

      // on last script loaded, set variable
      /**
       * @suppress {strictMissingProperties} suppression added to enable type
       * checking
       */
      window.test4 = msg;
    };

    return result.then(/**
                          @suppress {strictMissingProperties} suppression added
                          to enable type checking
                        */
                       () => {
                         // verify that the last loaded script callback has
                         // executed
                         assertEquals(
                             'verification object', 'Test #4 loaded',
                             window.test4);
                       });
  },

  testLoadWithOptions() {
    const testUrl = TrustedResourceUrl.fromConstant(
        Const.from('testdata/jsloader_test1.js'));
    const testUrlValue = TrustedResourceUrl.unwrap(testUrl);
    const options = {
      attributes: {'data-attr1': 'enabled', 'data-attr2': 'disabled'},
      timeout: undefined,          // Use default
      cleanupWhenDone: undefined,  // Use default
      document: undefined,         // Use default
    };
    const result = jsloader.safeLoad(testUrl, options);

    return result.then(() => {
      /**
       * @suppress {visibility,strictMissingProperties} suppression added to
       * enable type checking
       */
      const script = result.defaultScope_.script_;

      // Check that the URI matches ours.
      assertTrue('server URI', script.src.indexOf(testUrlValue) >= 0);

      // Check that the attributes specified are set on the script tag.
      assertEquals(
          'attribute option not applied for attr1', 'enabled',
          script.getAttribute('data-attr1'));
      assertEquals(
          'attribute option not applied for attr2', 'disabled',
          script.getAttribute('data-attr2'));
    });
  },
});
