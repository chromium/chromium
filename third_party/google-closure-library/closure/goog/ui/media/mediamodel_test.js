/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.media.MediaModelTest');
goog.setTestOnly();

const MediaModel = goog.require('goog.ui.media.MediaModel');
const testSuite = goog.require('goog.testing.testSuite');

/** A simple model used in many tests. */
let model;

testSuite({
  setUp() {
    model = new MediaModel('http://url.com', 'a caption', 'a description');
  },

  testMediaModel() {
    assertEquals('http://url.com', model.getUrl());
    assertEquals('a caption', model.getCaption());
    assertEquals('a description', model.getDescription());

    const incompleteModel = new MediaModel(
        'http://foo.bar', undefined,
        'This media has no caption but has a description and a URL');
    assertEquals('http://foo.bar', incompleteModel.getUrl());
    assertUndefined(incompleteModel.getCaption());
    assertEquals(
        'This media has no caption but has a description and a URL',
        incompleteModel.getDescription());
    assertArrayEquals([], incompleteModel.getThumbnails());
  },

  testMediaModelFindCategoryWithScheme() {
    assertNull(model.findCategoryWithScheme('no such scheme'));

    model.setCategories([
      new MediaModel.Category('scheme-a', 'value-a'),
      new MediaModel.Category('scheme-b', 'value-b'),
    ]);
    assertNull(model.findCategoryWithScheme('no such scheme'));
    assertEquals(
        'value-a', model.findCategoryWithScheme('scheme-a').getValue());
    assertEquals(
        'value-b', model.findCategoryWithScheme('scheme-b').getValue());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testMediaModelFindCreditsWithRole() {
    assertEquals(0, model.findCreditsWithRole('no such role').length);

    model.setCredits([
      new MediaModel.Credit('value-a', 'role-a'),
      new MediaModel.Credit('value-a2', 'role-a'),
      new MediaModel.Credit('value-b', 'role-b'),
    ]);

    assertEquals(0, model.findCreditsWithRole('no such role').length);
    assertEquals(2, model.findCreditsWithRole('role-a').length);
    assertEquals('value-a', model.findCreditsWithRole('role-a')[0].getValue());
    assertEquals('value-a2', model.findCreditsWithRole('role-a')[1].getValue());
    assertEquals('value-b', model.findCreditsWithRole('role-b')[0].getValue());
  },

  testMediaModelSubtitles() {
    model.setSubTitles(
        [new MediaModel.SubTitle('uri', '*', 'application/tts+xml')]);
    assertEquals(1, model.getSubTitles().length);
    assertEquals('uri', model.getSubTitles()[0].getHref());
    assertEquals('*', model.getSubTitles()[0].getLang());
    assertEquals('application/tts+xml', model.getSubTitles()[0].getType());
  },

  testMediaModelNoSubtitles() {
    assertEquals(0, model.getSubTitles().length);
  },
});
