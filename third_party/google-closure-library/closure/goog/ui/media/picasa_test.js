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

goog.module('goog.ui.media.PicasaTest');
goog.setTestOnly();

const FlashObject = goog.require('goog.ui.media.FlashObject');
const Media = goog.require('goog.ui.media.Media');
const PicasaAlbum = goog.require('goog.ui.media.PicasaAlbum');
const PicasaAlbumModel = goog.require('goog.ui.media.PicasaAlbumModel');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

let picasa;
let control;
const PICASA_USERNAME = 'username';
const PICASA_ALBUM = 'albumname';
const PICASA_URL =
    `http://picasaweb.google.com/${PICASA_USERNAME}/${PICASA_ALBUM}`;
const parent = dom.createElement(TagName.DIV);

function assertExtractsCorrectly(
    expectedUserId, expectedAlbumId, expectedAuthKey, url) {
  const model = PicasaAlbumModel.newInstance(url);
  assertEquals(`User for ${url}`, expectedUserId, model.getUserId());
  assertEquals(`Album for ${url}`, expectedAlbumId, model.getAlbumId());
  assertEquals(`AuthKey for ${url}`, expectedAuthKey, model.getAuthKey());
}
testSuite({
  setUp() {
    picasa = new PicasaAlbum();
    const model = new PicasaAlbumModel(
        PICASA_USERNAME, PICASA_ALBUM, null, 'album title');
    control = new Media(model, picasa);
    control.setSelected(true);
  },

  tearDown() {
    control.dispose();
  },

  testBasicRendering() {
    control.render(parent);
    const el = dom.getElementsByTagNameAndClass(
        TagName.DIV, PicasaAlbum.CSS_CLASS, parent);
    assertEquals(1, el.length);
    assertEquals(PICASA_URL, control.getDataModel().getUrl());
  },

  testParsingUrl() {
    assertExtractsCorrectly(PICASA_USERNAME, PICASA_ALBUM, null, PICASA_URL);
    assertExtractsCorrectly(
        'foo', 'bar', null, 'https://picasaweb.google.com/foo/bar');
    assertExtractsCorrectly(
        'foo', 'bar', null, 'https://www.picasaweb.google.com/foo/bar');
    assertExtractsCorrectly(
        'foo', 'bar', null, 'https://www.picasaweb.com/foo/bar');
    assertExtractsCorrectly(
        'foo', 'bar', '8Hzg1CUUAZM',
        'https://www.picasaweb.com/foo/bar?authkey=8Hzg1CUUAZM#');
    assertExtractsCorrectly(
        'foo', 'bar', '8Hzg1CUUAZM',
        'https://www.picasaweb.com/foo/bar?foo=bar&authkey=8Hzg1CUUAZM#');
    assertExtractsCorrectly(
        'foo', 'bar', '8Hzg1CUUAZM',
        'https://www.picasaweb.com/foo/bar?foo=bar&authkey=8Hzg1CUUAZM&' +
            'hello=world#');

    const invalidUrl = 'http://invalidUrl/watch?v=dMH0bHeiRNg';
    const e = assertThrows('parser expects a well formed URL', () => {
      PicasaAlbumModel.newInstance(invalidUrl);
    });
    assertEquals(
        `failed to parse user and album from picasa url: ${invalidUrl}`,
        e.message);
  },

  testBuildingUrl() {
    assertEquals(
        PICASA_URL, PicasaAlbumModel.buildUrl(PICASA_USERNAME, PICASA_ALBUM));
  },

  testCreatingModel() {
    const model = new PicasaAlbumModel(PICASA_USERNAME, PICASA_ALBUM);
    assertEquals(PICASA_USERNAME, model.getUserId());
    assertEquals(PICASA_ALBUM, model.getAlbumId());
    assertEquals(PICASA_URL, model.getUrl());
    assertUndefined(model.getCaption());
  },

  testCreatingDomOnInitialState() {
    control.render(parent);
    const caption = dom.getElementsByTagNameAndClass(
        TagName.DIV, PicasaAlbum.CSS_CLASS + '-caption', parent);
    assertEquals(1, caption.length);

    const flash = dom.getElementsByTagNameAndClass(
        TagName.DIV, FlashObject.CSS_CLASS, parent);
    assertEquals(1, flash.length);
  },
});
