/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.emoji.FastNonProgressiveEmojiPickerTest');
goog.setTestOnly();

const Emoji = goog.require('goog.ui.emoji.Emoji');
const EmojiPicker = goog.require('goog.ui.emoji.EmojiPicker');
const EventType = goog.require('goog.events.EventType');
const GoogPromise = goog.require('goog.Promise');
const NetEventType = goog.require('goog.net.EventType');
const SpriteInfo = goog.require('goog.ui.emoji.SpriteInfo');
const TestCase = goog.require('goog.testing.TestCase');
const classlist = goog.require('goog.dom.classlist');
const events = goog.require('goog.events');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');

let images;
let palette;
let picker;
const base = '../../demos/emoji';
const sprite = base + '/sprite.png';
const sprite2 = base + '/sprite2.png';

/**
 * Creates a SpriteInfo object with the specified properties. If the image is
 * sprited via CSS, then only the first parameter needs a value. If the image
 * is sprited via metadata, then the first parameter should be left null.
 * @param {?string} cssClass CSS class to properly display the sprited image.
 * @param {string=} url Url of the sprite image.
 * @param {number=} width Width of the image being sprited.
 * @param {number=} height Height of the image being sprited.
 * @param {number=} xOffset Positive x offset of the image being sprited within
 *     the sprite.
 * @param {number=} yOffset Positive y offset of the image being sprited within
 *     the sprite.
 * @param {boolean=} animated Whether the sprite info is for an animated emoji.
 */
function si(
    cssClass, url = undefined, width = undefined, height = undefined,
    xOffset = undefined, yOffset = undefined, animated = undefined) {
  return new SpriteInfo(
      cssClass, url, width, height, xOffset, yOffset, animated);
}

// This group contains a mix of sprited emoji via css, sprited emoji via
// metadata, and non-sprited emoji.
/** @suppress {checkTypes} suppression added to enable type checking */
const spritedEmoji2 = [
  'Emoji 1',
  [
    [base + '/200.gif', 'std.200', si('SPRITE_200')],
    [base + '/201.gif', 'std.201', si('SPRITE_201')],
    [base + '/202.gif', 'std.202', si('SPRITE_202')],
    [base + '/203.gif', 'std.203', si('SPRITE_203')],
    [base + '/204.gif', 'std.204', si('SPRITE_204')],
    [base + '/205.gif', 'std.205', si('SPRITE_205')],
    [base + '/206.gif', 'std.206', si('SPRITE_206')],
    [base + '/2BC.gif', 'std.2BC', si('SPRITE_2BC')],
    [base + '/2BD.gif', 'std.2BD', si('SPRITE_2BD')],
    [base + '/2BE.gif', 'std.2BE', si(null, sprite, 18, 18, 36, 54)],
    [base + '/2BF.gif', 'std.2BF', si(null, sprite, 18, 18, 0, 126)],
    [base + '/2C0.gif', 'std.2C0', si(null, sprite, 18, 18, 18, 305)],
    [base + '/2C1.gif', 'std.2C1', si(null, sprite, 18, 18, 0, 287)],
    [base + '/2C2.gif', 'std.2C2', si(null, sprite, 18, 18, 18, 126)],
    [base + '/2C3.gif', 'std.2C3', si(null, sprite, 18, 18, 36, 234)],
    [base + '/2C4.gif', 'std.2C4', si(null, sprite, 18, 18, 36, 72)],
    [base + '/2C5.gif', 'std.2C5', si(null, sprite, 18, 18, 54, 54)],
    [base + '/2C6.gif', 'std.2C6'],
    [base + '/2C7.gif', 'std.2C7'],
    [base + '/2C8.gif', 'std.2C8'],
    [base + '/2C9.gif', 'std.2C9'],
    [base + '/2CA.gif', 'std.2CA', si(null, sprite2, 18, 20, 36, 72, 1)],
    [base + '/2E3.gif', 'std.2E3', si(null, sprite2, 18, 18, 0, 0, 1)],
    [base + '/2EF.gif', 'std.2EF', si(null, sprite2, 18, 20, 0, 300, 1)],
    [base + '/2F1.gif', 'std.2F1', si(null, sprite2, 18, 18, 0, 320, 1)],
  ],
];

/**
 * Returns true if the two paths end with the same file.
 * E.g., ('../../cool.gif', 'file:///home/usr/somewhere/cool.gif') --> true
 * @param {string} path1 First url
 * @param {string} path2 Second url
 */
function checkPathsEndWithSameFile(path1, path2) {
  const pieces1 = path1.split('/');
  const file1 = pieces1[pieces1.length - 1];
  const pieces2 = path2.split('/');
  const file2 = pieces2[pieces2.length - 1];

  return file1 == file2;
}

testSuite({
  /**
   * Checks and verifies the structure of a non-progressive fast-loading picker
   * after the animated emoji have loaded.
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  testStructure() {
    const emoji = spritedEmoji2;

    for (let i = 0; i < emoji[1].length; i++) {
      palette.setSelectedIndex(i);
      const emojiInfo = emoji[1][i];
      const cell = palette.getSelectedItem();
      const id = cell.getAttribute(Emoji.ATTRIBUTE);
      const inner = /** @type {Element} */ (cell.firstChild);

      // Check that the cell is a div wrapped around something else, and that
      // the outer div contains the goomoji attribute
      assertEquals(
          'The palette item should be a div wrapped around something',
          cell.tagName, 'DIV');
      assertNotNull(
          'The outer div is not wrapped around another element', inner);
      assertEquals(
          'The palette item should have the goomoji attribute',
          cell.getAttribute(Emoji.ATTRIBUTE), emojiInfo[1]);
      assertEquals(
          'The palette item should have the data-goomoji attribute',
          cell.getAttribute(Emoji.DATA_ATTRIBUTE), emojiInfo[1]);

      // Now check the contents of the cells
      const url = emojiInfo[0];  // url of the animated emoji
      const spriteInfo = emojiInfo[2];
      if (spriteInfo) {
        assertEquals(inner.tagName, 'DIV');
        if (spriteInfo.isAnimated()) {
          const img = images[id];
          checkPathsEndWithSameFile(
              style.getStyle(inner, 'background-image'), url);
          assertEquals(
              String(img.width),
              style.getStyle(inner, 'width')
                  .replace(/px/g, '')
                  .replace(/pt/g, ''));
          assertEquals(
              String(img.height),
              style.getStyle(inner, 'height')
                  .replace(/px/g, '')
                  .replace(/pt/g, ''));
          assertEquals(
              '0 0',
              style.getStyle(inner, 'background-position')
                  .replace(/px/g, '')
                  .replace(/pt/g, ''));
        } else {
          const cssClass = spriteInfo.getCssClass();
          if (cssClass) {
            assertTrue(
                'Sprite should have its CSS class set',
                classlist.contains(inner, cssClass));
          } else {
            checkPathsEndWithSameFile(
                style.getStyle(inner, 'background-image'), spriteInfo.getUrl());
            assertEquals(
                spriteInfo.getWidthCssValue(), style.getStyle(inner, 'width'));
            assertEquals(
                spriteInfo.getHeightCssValue(),
                style.getStyle(inner, 'height'));
            assertEquals(
                (spriteInfo.getXOffsetCssValue() + ' ' +
                 spriteInfo.getYOffsetCssValue())
                    .replace(/px/g, '')
                    .replace(/pt/g, ''),
                style.getStyle(inner, 'background-position')
                    .replace(/px/g, '')
                    .replace(/pt/g, ''));
          }
        }
      } else {
        // A non-sprited emoji is just an img
        assertEquals(inner.tagName, 'IMG');
        checkPathsEndWithSameFile(inner.src, emojiInfo[0]);
      }
    }
  },

  setUpPage() {
    // This test is insanely slow on Safari for some reason.
    TestCase.getActiveTestCase().promiseTimeout = 10 * 1000;
  },

  setUp() {
    const defaultImg = base + '/none.gif';
    picker = new EmojiPicker(defaultImg);
    picker.setDelayedLoad(false);
    picker.setManualLoadOfAnimatedEmoji(true);
    picker.setProgressiveRender(false);
    picker.addEmojiGroup(spritedEmoji2[0], spritedEmoji2[1]);
    picker.render();

    palette = picker.getPage(0);
    const imageLoader = palette.getImageLoader();
    images = {};

    return new GoogPromise((resolve, reject) => {
      events.listen(imageLoader, NetEventType.COMPLETE, resolve);
      events.listen(imageLoader, EventType.LOAD, (e) => {
        const image = e.target;
        images[image.id] = image;
      });

      // Now we load the animated emoji and check the structure again. The
      // animated emoji will be different.
      picker.manuallyLoadAnimatedEmoji();
    });
  },

  tearDown() {
    picker.dispose();
  },
});
