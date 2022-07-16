/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.FormPostTest');
goog.setTestOnly();

const FormPost = goog.require('goog.ui.FormPost');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const googArray = goog.require('goog.array');
const googObject = goog.require('goog.object');
const isVersion = goog.require('goog.userAgent.product.isVersion');
const product = goog.require('goog.userAgent.product');
const testSuite = goog.require('goog.testing.testSuite');

const TARGET = 'target';
const ACTION_URL = 'http://url/';
let formPost;
let parameters;
let submits;
const originalCreateDom = FormPost.prototype.createDom;

function isChrome7or8() {
  // Temporarily disabled in Chrome 7 & 8. See b/3176768
  if (product.CHROME && isVersion('7.0') && !isVersion('8.0')) {
    return false;
  }

  return true;
}

function expectUrlAndParameters(url, target, parameters) {
  const form = formPost.getElement();
  assertEquals('element must be a form', String(TagName.FORM), form.tagName);
  assertEquals('form must be hidden', 'none', form.style.display);
  assertEquals('form method must be POST', 'POST', form.method.toUpperCase());
  assertEquals('submits', 1, submits);
  assertEquals('action attribute', url, form.action);
  assertEquals('target attribute', target, form.target);
  const inputs = dom.getElementsByTagNameAndClass(TagName.INPUT, null, form);
  const formValues = {};
  for (let i = 0, input = inputs[i]; input = inputs[i]; i++) {
    if (Array.isArray(formValues[input.name])) {
      formValues[input.name].push(input.value);
    } else if (input.name in formValues) {
      formValues[input.name] = [formValues[input.name], input.value];
    } else {
      formValues[input.name] = input.value;
    }
  }
  const expected = googObject.map(parameters, valueToString);
  assertObjectEquals('form values must match', expected, formValues);
}

function valueToString(value) {
  if (Array.isArray(value)) {
    return googArray.map(value, valueToString);
  }
  return String(value);
}
testSuite({
  setUp() {
    formPost = new FormPost();
    submits = 0;

    // Replace the form's submit method with a fake.
    FormPost.prototype.createDom = function() {
      originalCreateDom.apply(this, arguments);

      /**
       * @suppress {strictMissingProperties} suppression added to enable type
       * checking
       */
      this.getElement().submit = () => {
        submits++;
      };
    };
    parameters = {'foo': 'bar', 'baz': 1, 'array': [0, 'yes']};
  },

  tearDown() {
    formPost.dispose();
    FormPost.prototype.createDom = originalCreateDom;
  },

  testPost() {
    formPost.post(parameters, ACTION_URL, TARGET);
    expectUrlAndParameters(ACTION_URL, TARGET, parameters);
  },

  testPostWithDefaults() {
    // Temporarily disabled in Chrome 7. See See b/3176768
    if (isChrome7or8) {
      return;
    }
    formPost = new FormPost();
    formPost.post(parameters);
    expectUrlAndParameters('', '', parameters);
  },
});
