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

goog.module('goog.ui.media.FlickrSetTest');
goog.setTestOnly();

const FlashObject = goog.require('goog.ui.media.FlashObject');
const FlickrSet = goog.require('goog.ui.media.FlickrSet');
const FlickrSetModel = goog.require('goog.ui.media.FlickrSetModel');
const Media = goog.require('goog.ui.media.Media');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');
const testing = goog.require('goog.html.testing');

let flickr;
let control;
const FLICKR_USER = 'ingawalker';
const FLICKR_SET = '72057594102831547';
const FLICKRSET_URL =
    `http://flickr.com/photos/${FLICKR_USER}/sets/${FLICKR_SET}`;
const parent = dom.createElement(TagName.DIV);

function assertExtractsCorrectly(expectedUserId, expectedSetId, url) {
  const flickr = FlickrSetModel.newInstance(url);
  assertEquals(`userId for ${url}`, expectedUserId, flickr.getUserId());
  assertEquals(`setId for ${url}`, expectedSetId, flickr.getSetId());
}
testSuite({
  setUp() {
    flickr = new FlickrSet();
    const set = new FlickrSetModel(FLICKR_USER, FLICKR_SET, 'caption');
    control = new Media(set, flickr);
  },

  tearDown() {
    control.dispose();
  },

  testBasicRendering() {
    control.render(parent);
    const el = dom.getElementsByTagNameAndClass(
        TagName.DIV, FlickrSet.CSS_CLASS, parent);
    assertEquals(1, el.length);
    assertEquals(FLICKRSET_URL, control.getDataModel().getUrl());
  },

  testParsingUrl() {
    assertExtractsCorrectly(FLICKR_USER, FLICKR_SET, FLICKRSET_URL);
    // user id with @ sign
    assertExtractsCorrectly(
        '30441750@N06', '7215760789302468',
        'http://flickr.com/photos/30441750@N06/sets/7215760789302468/');
    // user id with - sign
    assertExtractsCorrectly(
        '30441750-N06', '7215760789302468',
        'http://flickr.com/photos/30441750-N06/sets/7215760789302468/');

    const invalidUrl = 'http://invalidUrl/filename.doc';
    const e = assertThrows('', () => {
      FlickrSetModel.newInstance(invalidUrl);
    });
    assertEquals(`failed to parse flickr url: ${invalidUrl}`, e.message);
  },

  testBuildingUrl() {
    assertEquals(
        FLICKRSET_URL,
        FlickrSetModel.buildUrl(FLICKR_USER, FLICKR_SET, FLICKRSET_URL));
  },

  testCreatingModel() {
    const model = new FlickrSetModel(FLICKR_USER, FLICKR_SET);
    assertEquals(FLICKR_USER, model.getUserId());
    assertEquals(FLICKR_SET, model.getSetId());
    assertEquals(FLICKRSET_URL, model.getUrl());
    assertUndefined(model.getCaption());
  },

  testSettingWhichFlashUrlToUse() {
    FlickrSet.setFlashUrl(testing.newTrustedResourceUrlForTest('http://foo'));
    assertEquals('http://foo', FlickrSet.flashUrl_.getTypedStringValue());
  },

  testCreatingDomOnInitialState() {
    control.render(parent);
    const caption = dom.getElementsByTagNameAndClass(
        TagName.DIV, FlickrSet.CSS_CLASS + '-caption', parent);
    assertEquals(1, caption.length);

    const flash = dom.getElementsByTagNameAndClass(
        TagName.DIV, FlashObject.CSS_CLASS, parent);
    assertEquals(1, flash.length);
  },
});
