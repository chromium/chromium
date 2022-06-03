// Copyright 2009 The Closure Library Authors. All Rights Reserved.
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

goog.module('goog.ui.media.MediaTest');
goog.setTestOnly();

const ControlRenderer = goog.require('goog.ui.ControlRenderer');
const Media = goog.require('goog.ui.media.Media');
const MediaModel = goog.require('goog.ui.media.MediaModel');
const MediaRenderer = goog.require('goog.ui.media.MediaRenderer');
const Size = goog.require('goog.math.Size');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');
const testing = goog.require('goog.html.testing');

let control;  // The name 'media' collides with a built-in var in Chrome.
let renderer;
let model;

testSuite({
  setUp() {
    renderer = new MediaRenderer();
    model = new MediaModel('http://url.com', 'a caption', 'a description');
    control = new Media(model, renderer);
  },

  tearDown() {
    control.dispose();
  },

  testBasicElements() {
    const model =
        new MediaModel('http://url.com', 'a caption', 'a description');
    const thumb1 = new MediaModel.Thumbnail(
        'http://thumb.com/small.jpg', new Size(320, 288));
    const thumb2 = new MediaModel.Thumbnail(
        'http://thumb.com/big.jpg', new Size(800, 600));
    model.setThumbnails([thumb1, thumb2]);
    model.setPlayer(new MediaModel.Player(
        testing.newTrustedResourceUrlForTest('http://media/player.swf')));
    const control = new Media(model, renderer);
    control.render();

    const caption = dom.getElementsByTagNameAndClass(
        undefined, ControlRenderer.CSS_CLASS + '-caption');
    const description = dom.getElementsByTagNameAndClass(
        undefined, ControlRenderer.CSS_CLASS + '-description');
    const thumbnail0 = dom.getElementsByTagNameAndClass(
        TagName.IMG, ControlRenderer.CSS_CLASS + '-thumbnail0');
    const thumbnail1 = dom.getElementsByTagNameAndClass(
        TagName.IMG, ControlRenderer.CSS_CLASS + '-thumbnail1');
    const player = dom.getElementsByTagNameAndClass(
        TagName.IFRAME, ControlRenderer.CSS_CLASS + '-player');

    assertNotNull(caption);
    assertEquals(1, caption.length);
    assertNotNull(description);
    assertEquals(1, description.length);
    assertNotNull(thumbnail0);
    assertEquals(1, thumbnail0.length);
    assertEquals('320px', thumbnail0[0].style.width);
    assertEquals('288px', thumbnail0[0].style.height);
    assertEquals('http://thumb.com/small.jpg', thumbnail0[0].src);
    assertNotNull(thumbnail1);
    assertEquals(1, thumbnail1.length);
    assertEquals('800px', thumbnail1[0].style.width);
    assertEquals('600px', thumbnail1[0].style.height);
    assertEquals('http://thumb.com/big.jpg', thumbnail1[0].src);
    // players are only shown when media is selected
    assertNotNull(player);
    assertEquals(0, player.length);

    control.dispose();
  },

  testDoesntCreatesCaptionIfUnavailable() {
    const incompleteModel =
        new MediaModel('http://url.com', undefined, 'a description');
    let incompleteMedia = new Media('', renderer);
    incompleteMedia.setDataModel(incompleteModel);
    incompleteMedia.render();
    const caption = dom.getElementsByTagNameAndClass(
        undefined, ControlRenderer.CSS_CLASS + '-caption');
    const description = dom.getElementsByTagNameAndClass(
        undefined, ControlRenderer.CSS_CLASS + '-description');
    assertEquals(0, caption.length);
    assertNotNull(description);
    incompleteMedia.dispose();
  },

  testSetAriaLabel() {
    const model =
        new MediaModel('http://url.com', 'a caption', 'a description');
    const thumb1 = new MediaModel.Thumbnail(
        'http://thumb.com/small.jpg', new Size(320, 288));
    const thumb2 = new MediaModel.Thumbnail(
        'http://thumb.com/big.jpg', new Size(800, 600));
    model.setThumbnails([thumb1, thumb2]);
    model.setPlayer(new MediaModel.Player(
        testing.newTrustedResourceUrlForTest('http://media/player.swf')));
    const control = new Media(model, renderer);
    assertNull(
        'Media must not have aria label by default', control.getAriaLabel());
    control.setAriaLabel('My media');
    control.render();
    const element = control.getElementStrict();
    assertNotNull('Element must not be null', element);
    assertEquals(
        'Media element must have expected aria-label', 'My media',
        element.getAttribute('aria-label'));
    assertTrue(dom.isFocusableTabIndex(element));
    control.setAriaLabel('My new media');
    assertEquals(
        'Media element must have updated aria-label', 'My new media',
        element.getAttribute('aria-label'));
  },
});
