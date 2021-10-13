/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.datasetTest');
goog.setTestOnly();

const dataset = goog.require('goog.dom.dataset');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

const $ = dom.getElement;

testSuite({
  setUp() {
    const el = $('el2');
    el.setAttribute('data-dynamic-key', 'dynamic');
  },

  testHas() {
    const el = $('el1');

    assertTrue(
        'Dataset should have an existing key', dataset.has(el, 'basicKey'));
    assertTrue(
        'Dataset should have an existing (unusual) key',
        dataset.has(el, 'UnusualKey1'));
    assertTrue(
        'Dataset should have an existing (unusual) key',
        dataset.has(el, 'unusual-Key2'));
    assertTrue(
        'Dataset should have an existing (bizarre) key',
        dataset.has(el, '-Bizarre--Key'));
    assertTrue(
        'Dataset should have an existing but empty-value attribute key',
        dataset.has(el, 'emptyString'));
    assertTrue(
        'Dataset should have a boolean attribute key',
        dataset.has(el, 'boolean'));
    assertFalse(
        'Dataset should not have a non-existent key',
        dataset.has(el, 'bogusKey'));
    assertFalse(
        'Dataset should not have invalid key', dataset.has(el, 'basic-key'));
  },

  testGet() {
    let el = $('el1');

    assertEquals(
        'Dataset should return the proper value for an existing key',
        dataset.get(el, 'basicKey'), 'basic');
    assertEquals(
        'Dataset should have an existing (unusual) key',
        dataset.get(el, 'UnusualKey1'), 'unusual1');
    assertEquals(
        'Dataset should have an existing (unusual) key',
        dataset.get(el, 'unusual-Key2'), 'unusual2');
    assertEquals(
        'Dataset should have an existing (bizarre) key',
        dataset.get(el, '-Bizarre--Key'), 'bizarre');
    assertEquals(
        'Dataset should have an existing but empty-value attribute key',
        dataset.get(el, 'emptyString'), '');
    assertEquals(
        'Dataset should have a boolean attribute key',
        dataset.get(el, 'boolean'), '');
    assertNull(
        'Dataset should return null for a non-existent key',
        dataset.get(el, 'bogusKey'));
    assertNull(
        'Dataset should return null for an invalid key',
        dataset.get(el, 'basic-key'));

    el = $('el2');
    assertEquals(
        'Dataset should return the proper value for an existing key',
        dataset.get(el, 'dynamicKey'), 'dynamic');
  },

  testSet() {
    const el = $('el2');

    dataset.set(el, 'newKey', 'newValue');
    assertTrue(
        'Dataset should have a newly created key', dataset.has(el, 'newKey'));
    assertEquals(
        'Dataset should return the proper value for a newly created key',
        dataset.get(el, 'newKey'), 'newValue');

    dataset.set(el, 'dynamicKey', 'customValue');
    assertTrue(
        'Dataset should have a modified, existing key',
        dataset.has(el, 'dynamicKey'));
    assertEquals(
        'Dataset should return the proper value for a modified key',
        dataset.get(el, 'dynamicKey'), 'customValue');

    assertThrows('Invalid key should fail noticeably', () => {
      dataset.set(el, 'basic-key', '');
    });
  },

  testRemove() {
    const el = $('el2');

    assertTrue('Dataset starts with key', dataset.has(el, 'dynamicKey'));
    dataset.remove(el, 'dynamic-key');
    assertTrue('Dataset should still have key', dataset.has(el, 'dynamicKey'));

    dataset.remove(el, 'dynamicKey');
    assertFalse(
        'Dataset should not have a removed key', dataset.has(el, 'dynamicKey'));
    assertNull(
        'Dataset should return null for removed key',
        dataset.get(el, 'dynamicKey'));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetAll() {
    const el = $('el1');
    const expectedDataset = {
      'basicKey': 'basic',
      'UnusualKey1': 'unusual1',
      'unusual-Key2': 'unusual2',
      '-Bizarre--Key': 'bizarre',
      'emptyString': '',
      'boolean': '',
    };
    assertHashEquals(
        'Dataset should have basicKey, UnusualKey1, ' +
            'unusual-Key2, and -Bizarre--Key',
        expectedDataset, dataset.getAll(el));
  },
});
