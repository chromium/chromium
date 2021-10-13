/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.emoji.EmojiPickerTest');
goog.setTestOnly();

const Component = goog.require('goog.ui.Component');
const Emoji = goog.require('goog.ui.emoji.Emoji');
const EmojiPalette = goog.requireType('goog.ui.emoji.EmojiPalette');
const EmojiPicker = goog.require('goog.ui.emoji.EmojiPicker');
const EventHandler = goog.require('goog.events.EventHandler');
const SpriteInfo = goog.require('goog.ui.emoji.SpriteInfo');
const TagName = goog.require('goog.dom.TagName');
const classlist = goog.require('goog.dom.classlist');
const events = goog.require('goog.testing.events');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');

let handler;

// 26 emoji
const emojiGroup1 = [
  'Emoji 1',
  [
    ['../../demos/emoji/200.gif', 'std.200'],
    ['../../demos/emoji/201.gif', 'std.201'],
    ['../../demos/emoji/202.gif', 'std.202'],
    ['../../demos/emoji/203.gif', 'std.203'],
    ['../../demos/emoji/204.gif', 'std.204'],
    ['../../demos/emoji/200.gif', 'std.200'],
    ['../../demos/emoji/201.gif', 'std.201'],
    ['../../demos/emoji/202.gif', 'std.202'],
    ['../../demos/emoji/203.gif', 'std.203'],
    ['../../demos/emoji/204.gif', 'std.204'],
    ['../../demos/emoji/200.gif', 'std.200'],
    ['../../demos/emoji/201.gif', 'std.201'],
    ['../../demos/emoji/202.gif', 'std.202'],
    ['../../demos/emoji/203.gif', 'std.203'],
    ['../../demos/emoji/204.gif', 'std.204'],
    ['../../demos/emoji/200.gif', 'std.200'],
    ['../../demos/emoji/201.gif', 'std.201'],
    ['../../demos/emoji/202.gif', 'std.202'],
    ['../../demos/emoji/203.gif', 'std.203'],
    ['../../demos/emoji/204.gif', 'std.204'],
    ['../../demos/emoji/200.gif', 'std.200'],
    ['../../demos/emoji/201.gif', 'std.201'],
    ['../../demos/emoji/202.gif', 'std.202'],
    ['../../demos/emoji/203.gif', 'std.203'],
    ['../../demos/emoji/204.gif', 'std.204'],
    ['../../demos/emoji/200.gif', 'std.200'],
  ],
];

// 20 emoji
const emojiGroup2 = [
  'Emoji 2',
  [
    ['../../demos/emoji/200.gif', 'std.200'],
    ['../../demos/emoji/201.gif', 'std.201'],
    ['../../demos/emoji/202.gif', 'std.202'],
    ['../../demos/emoji/203.gif', 'std.203'],
    ['../../demos/emoji/204.gif', 'std.204'],
    ['../../demos/emoji/200.gif', 'std.200'],
    ['../../demos/emoji/201.gif', 'std.201'],
    ['../../demos/emoji/202.gif', 'std.202'],
    ['../../demos/emoji/203.gif', 'std.203'],
    ['../../demos/emoji/204.gif', 'std.204'],
    ['../../demos/emoji/200.gif', 'std.200'],
    ['../../demos/emoji/201.gif', 'std.201'],
    ['../../demos/emoji/202.gif', 'std.202'],
    ['../../demos/emoji/203.gif', 'std.203'],
    ['../../demos/emoji/204.gif', 'std.204'],
    ['../../demos/emoji/200.gif', 'std.200'],
    ['../../demos/emoji/201.gif', 'std.201'],
    ['../../demos/emoji/202.gif', 'std.202'],
    ['../../demos/emoji/203.gif', 'std.203'],
    ['../../demos/emoji/204.gif', 'std.204'],
  ],
];

// 20 emoji
const emojiGroup3 = [
  'Emoji 3',
  [
    ['../../demos/emoji/200.gif', 'std.200'],
    ['../../demos/emoji/201.gif', 'std.201'],
    ['../../demos/emoji/202.gif', 'std.202'],
    ['../../demos/emoji/203.gif', 'std.203'],
    ['../../demos/emoji/204.gif', 'std.204'],
    ['../../demos/emoji/200.gif', 'std.200'],
    ['../../demos/emoji/201.gif', 'std.201'],
    ['../../demos/emoji/202.gif', 'std.202'],
    ['../../demos/emoji/203.gif', 'std.203'],
    ['../../demos/emoji/204.gif', 'std.204'],
    ['../../demos/emoji/200.gif', 'std.200'],
    ['../../demos/emoji/201.gif', 'std.201'],
    ['../../demos/emoji/202.gif', 'std.202'],
    ['../../demos/emoji/203.gif', 'std.203'],
    ['../../demos/emoji/204.gif', 'std.204'],
    ['../../demos/emoji/200.gif', 'std.200'],
    ['../../demos/emoji/201.gif', 'std.201'],
    ['../../demos/emoji/202.gif', 'std.202'],
    ['../../demos/emoji/203.gif', 'std.203'],
    ['../../demos/emoji/204.gif', 'std.204'],
  ],
];

const sprite = '../../demos/emoji/sprite.png';
const sprite2 = '../../demos/emoji/sprite2.png';

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

// Contains a mix of sprited emoji via css, sprited emoji via metadata, and
// non-sprited emoji
const spritedEmoji1 = [
  'Emoji 1',
  [
    ['../../demos/emoji/200.gif', 'std.200', si('SPRITE_200')],
    ['../../demos/emoji/201.gif', 'std.201', si('SPRITE_201')],
    ['../../demos/emoji/202.gif', 'std.202', si('SPRITE_202')],
    ['../../demos/emoji/203.gif', 'std.203', si('SPRITE_203')],
    ['../../demos/emoji/204.gif', 'std.204', si('SPRITE_204')],
    ['../../demos/emoji/200.gif', 'std.200', si('SPRITE_200')],
    ['../../demos/emoji/201.gif', 'std.201', si('SPRITE_201')],
    ['../../demos/emoji/202.gif', 'std.202', si('SPRITE_202')],
    ['../../demos/emoji/203.gif', 'std.203', si('SPRITE_203')],
    ['../../demos/emoji/2BE.gif', 'std.2BE', si(null, sprite, 18, 18, 36, 54)],
    ['../../demos/emoji/2BF.gif', 'std.2BF', si(null, sprite, 18, 18, 0, 126)],
    ['../../demos/emoji/2C0.gif', 'std.2C0', si(null, sprite, 18, 18, 18, 305)],
    ['../../demos/emoji/2C1.gif', 'std.2C1', si(null, sprite, 18, 18, 0, 287)],
    ['../../demos/emoji/2C2.gif', 'std.2C2', si(null, sprite, 18, 18, 18, 126)],
    ['../../demos/emoji/2C3.gif', 'std.2C3', si(null, sprite, 18, 18, 36, 234)],
    ['../../demos/emoji/2C4.gif', 'std.2C4', si(null, sprite, 18, 18, 36, 72)],
    ['../../demos/emoji/2C5.gif', 'std.2C5', si(null, sprite, 18, 18, 54, 54)],
    ['../../demos/emoji/200.gif', 'std.200'],
    ['../../demos/emoji/201.gif', 'std.201'],
    ['../../demos/emoji/202.gif', 'std.202'],
    ['../../demos/emoji/203.gif', 'std.203'],
    ['../../demos/emoji/204.gif', 'std.204'],
    ['../../demos/emoji/200.gif', 'std.200'],
    ['../../demos/emoji/201.gif', 'std.201'],
    ['../../demos/emoji/202.gif', 'std.202'],
    ['../../demos/emoji/203.gif', 'std.203'],
  ],
];

// This group contains a mix of sprited emoji via css, sprited emoji via
// metadata, and non-sprited emoji.
/** @suppress {checkTypes} suppression added to enable type checking */
const spritedEmoji2 = [
  'Emoji 1',
  [
    ['../../demos/emoji/200.gif', 'std.200', si('SPRITE_200')],
    ['../../demos/emoji/201.gif', 'std.201', si('SPRITE_201')],
    ['../../demos/emoji/202.gif', 'std.202', si('SPRITE_202')],
    ['../../demos/emoji/203.gif', 'std.203', si('SPRITE_203')],
    ['../../demos/emoji/204.gif', 'std.204', si('SPRITE_204')],
    ['../../demos/emoji/200.gif', 'std.200', si('SPRITE_200')],
    ['../../demos/emoji/201.gif', 'std.201', si('SPRITE_201')],
    ['../../demos/emoji/202.gif', 'std.202', si('SPRITE_202')],
    ['../../demos/emoji/203.gif', 'std.203', si('SPRITE_203')],
    ['../../demos/emoji/2BE.gif', 'std.2BE', si(null, sprite, 18, 18, 36, 54)],
    ['../../demos/emoji/2BF.gif', 'std.2BF', si(null, sprite, 18, 18, 0, 126)],
    ['../../demos/emoji/2C0.gif', 'std.2C0', si(null, sprite, 18, 18, 18, 305)],
    ['../../demos/emoji/2C1.gif', 'std.2C1', si(null, sprite, 18, 18, 0, 287)],
    ['../../demos/emoji/2C2.gif', 'std.2C2', si(null, sprite, 18, 18, 18, 126)],
    ['../../demos/emoji/2C3.gif', 'std.2C3', si(null, sprite, 18, 18, 36, 234)],
    ['../../demos/emoji/2C4.gif', 'std.2C4', si(null, sprite, 18, 18, 36, 72)],
    ['../../demos/emoji/2C5.gif', 'std.2C5', si(null, sprite, 18, 18, 54, 54)],
    ['../../demos/emoji/2C6.gif', 'std.2C6'],
    ['../../demos/emoji/2C7.gif', 'std.2C7'],
    ['../../demos/emoji/2C8.gif', 'std.2C8'],
    ['../../demos/emoji/2C9.gif', 'std.2C9'],
    [
      '../../demos/emoji/2CA.gif',
      'std.2CA',
      si(null, sprite2, 18, 20, 36, 72, 1),
    ],
    [
      '../../demos/emoji/2E3.gif',
      'std.2E3',
      si(null, sprite2, 18, 18, 0, 0, 1),
    ],
    [
      '../../demos/emoji/2EF.gif',
      'std.2EF',
      si(null, sprite2, 18, 20, 0, 300, 1),
    ],
    [
      '../../demos/emoji/2F1.gif',
      'std.2F1',
      si(null, sprite2, 18, 18, 0, 320, 1),
    ],
  ],
];

const emojiGroups = [emojiGroup1, emojiGroup2, emojiGroup3];

/**
 * Helper for testDelayedLoad. Returns true if the two paths end with the same
 * file.
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

/**
 * Gets the emoji URL from a palette element. Palette elements are divs or
 * imgs wrapped in an outer div. The returns the background-image if it's a div,
 * or the src attribute if it's an image.
 * @param {Element} element Element to get the image url for
 * @return {string}
 * @suppress {strictMissingProperties} suppression added to enable type checking
 */
function getImageUrl(element) {
  /** @suppress {checkTypes} suppression added to enable type checking */
  element = element.firstChild;  // get the wrapped element
  if (element.tagName == TagName.IMG) {
    return element.src;
  } else {
    /** @suppress {checkTypes} suppression added to enable type checking */
    let url = style.getStyle(element, 'background-image');
    url = url.replace(/url\(/, '');
    url = url.replace(/\)/, '');
    return url;
  }
}

/**
 * Checks that the content of an emojipicker page is all images pointing to
 * the default img.
 * @param {!EmojiPalette} page The page of the picker to check
 * @param {string} defaultImgUrl The url of the default img
 */
function checkContentIsDefaultImg(page, defaultImgUrl) {
  const content = page.getContent();

  for (let i = 0; i < content.length; i++) {
    const url = getImageUrl(content[i]);
    assertTrue(
        `img src should be ${defaultImgUrl} but is ${url}`,
        checkPathsEndWithSameFile(url, defaultImgUrl));
  }
}

/**
 * Checks that the content of an emojipicker page is the specified emoji and
 * the default img after the emoji are all used.
 * @param {!EmojiPalette} page The page of the picker to check
 * @param {Array<Array<string>>} emojiList List of emoji that should be in the
 *     palette
 * @param {string} defaultImgUrl The url of the default img
 * @suppress {checkTypes} suppression added to enable type checking
 */
function checkContentIsEmojiImages(page, emojiList, defaultImg) {
  const content = page.getContent();

  for (let i = 0; i < content.length; i++) {
    const url = getImageUrl(content[i]);
    if (i < emojiList.length) {
      assertTrue(
          `Paths should end with the same file: ${url}, ` + emojiList[i][0],
          checkPathsEndWithSameFile(url, emojiList[i][0]));
    } else {
      assertTrue(
          `Paths should end with the same file: ${url}, ${defaultImg}`,
          checkPathsEndWithSameFile(url, defaultImg));
    }
  }
}

/**
 * Checks and verifies the structure of a non-progressively-rendered
 * emojipicker.
 * @param {!EmojiPalette} palette Emoji palette to check.
 * @param {Array<Array<string>>} emoji Emoji that should be in the palette.
 * @suppress {strictMissingProperties} suppression added to enable type checking
 */
function checkStructureForNonProgressivePicker(palette, emoji) {
  // We can hackily check the items by selecting an item and then getting the
  // selected item.
  for (let i = 0; i < emoji[1].length; i++) {
    palette.setSelectedIndex(i);
    const emojiInfo = emoji[1][i];
    const cell = palette.getSelectedItem();
    const inner = /** @type {Element} */ (cell.firstChild);

    // Check that the cell is a div wrapped around something else, and that the
    // outer div contains the goomoji attribute
    assertEquals(
        'The palette item should be a div wrapped around something',
        cell.tagName, 'DIV');
    assertNotNull('The outer div is not wrapped around another element', inner);
    assertEquals(
        'The palette item should have the goomoji attribute',
        cell.getAttribute(Emoji.ATTRIBUTE), emojiInfo[1]);
    assertEquals(
        'The palette item should have the data-goomoji attribute',
        cell.getAttribute(Emoji.DATA_ATTRIBUTE), emojiInfo[1]);

    // Now check the contents of the cells
    const spriteInfo = emojiInfo[2];
    if (spriteInfo) {
      assertEquals(inner.tagName, 'DIV');
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
            spriteInfo.getHeightCssValue(), style.getStyle(inner, 'height'));
        assertEquals(
            (spriteInfo.getXOffsetCssValue() + ' ' +
             spriteInfo.getYOffsetCssValue())
                .replace(/px/g, '')
                .replace(/pt/g, ''),
            style.getStyle(inner, 'background-position')
                .replace(/px/g, '')
                .replace(/pt/g, ''));
      }
    } else {
      // A non-sprited emoji is just an img
      assertEquals(inner.tagName, 'IMG');
      checkPathsEndWithSameFile(inner.src, emojiInfo[0]);
    }
  }
}

/**
 * Checks and verifies the structure of a progressively-rendered emojipicker.
 * @param {!EmojiPalette} palette Emoji palette to check.
 * @param {Array<Array<string>>} emoji Emoji that should be in the palette.
 * @suppress {strictMissingProperties,checkTypes} suppression added to enable
 * type checking
 */
function checkStructureForProgressivePicker(palette, emoji) {
  // We can hackily check the items by selecting an item and then getting the
  // selected item.
  for (let i = 0; i < emoji[1].length; i++) {
    palette.setSelectedIndex(i);
    const emojiInfo = emoji[1][i];
    const cell = palette.getSelectedItem();
    const inner = /** @type {Element} */ (cell.firstChild);

    // Check that the cell is a div wrapped around something else, and that the
    // outer div contains the goomoji attribute
    assertEquals(
        'The palette item should be a div wrapped around something',
        cell.tagName, 'DIV');
    assertNotNull('The outer div is not wrapped around another element', inner);
    assertEquals(
        'The palette item should have the goomoji attribute',
        cell.getAttribute(Emoji.ATTRIBUTE), emojiInfo[1]);
    assertEquals(
        'The palette item should have the data-goomoji attribute',
        cell.getAttribute(Emoji.DATA_ATTRIBUTE), emojiInfo[1]);

    // Now check the contents of the cells
    const spriteInfo = emojiInfo[2];
    if (spriteInfo) {
      const cssClass = spriteInfo.getCssClass();
      if (cssClass) {
        assertEquals('DIV', inner.tagName);
        assertTrue(
            'Sprite should have its CSS class set',
            classlist.contains(inner, cssClass));
      } else {
        // There's an inner div wrapping an img tag
        assertEquals('DIV', inner.tagName);
        const img = inner.firstChild;
        assertNotNull('Div should be wrapping something', img);
        assertEquals('IMG', img.tagName);
        checkPathsEndWithSameFile(img.src, spriteInfo.getUrl());
        assertEquals(
            spriteInfo.getWidthCssValue(), style.getStyle(inner, 'width'));
        assertEquals(
            spriteInfo.getHeightCssValue(), style.getStyle(inner, 'height'));
        assertEquals(
            spriteInfo.getXOffsetCssValue().replace(/px/, '').replace(/pt/, ''),
            style.getStyle(img, 'left').replace(/px/, '').replace(/pt/, ''));
        assertEquals(
            spriteInfo.getYOffsetCssValue().replace(/px/, '').replace(/pt/, ''),
            style.getStyle(img, 'top').replace(/px/, '').replace(/pt/, ''));
      }
    } else {
      // A non-sprited emoji is just an img
      assertEquals(inner.tagName, 'IMG');
      checkPathsEndWithSameFile(inner.src, emojiInfo[0]);
    }
  }
}

/**
 * Checks and verifies the structure of a non-progressive fast-loading picker
 * after the animated emoji have loaded.
 * @param {!EmojiPalette} palette Emoji palette to check.
 * @param {Array<Array<string>>} emoji Emoji that should be in the palette.
 * @suppress {strictMissingProperties} suppression added to enable type checking
 */
function checkPostLoadStructureForFastLoadNonProgressivePicker(palette, emoji) {
  for (let i = 0; i < emoji[1].length; i++) {
    palette.setSelectedIndex(i);
    const emojiInfo = emoji[1][i];
    const cell = palette.getSelectedItem();
    const inner = /** @type {Element} */ (cell.firstChild);

    // Check that the cell is a div wrapped around something else, and that the
    // outer div contains the goomoji attribute
    assertEquals(
        'The palette item should be a div wrapped around something',
        cell.tagName, 'DIV');
    assertNotNull('The outer div is not wrapped around another element', inner);
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
        const img = new Image();
        img.src = url;
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
              spriteInfo.getHeightCssValue(), style.getStyle(inner, 'height'));
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
}

/**
 * Checks and verifies the structure of a progressive fast-loading picker
 * after the animated emoji have loaded.
 * @param {!EmojiPalette} palette Emoji palette to check.
 * @param {Array<Array<string>>} emoji Emoji that should be in the palette.
 * @suppress {strictMissingProperties,checkTypes} suppression added to enable
 * type checking
 */
function checkPostLoadStructureForFastLoadProgressivePicker(palette, emoji) {
  for (let i = 0; i < emoji[1].length; i++) {
    palette.setSelectedIndex(i);
    const emojiInfo = emoji[1][i];
    const cell = palette.getSelectedItem();
    const inner = /** @type {Element} */ (cell.firstChild);

    // Check that the cell is a div wrapped around something else, and that the
    // outer div contains the goomoji attribute
    assertEquals(
        'The palette item should be a div wrapped around something',
        cell.tagName, 'DIV');
    assertNotNull('The outer div is not wrapped around another element', inner);
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
      if (spriteInfo.isAnimated()) {
        const testImg = new Image();
        testImg.src = url;
        const img = inner.firstChild;
        checkPathsEndWithSameFile(img.src, url);
        assertEquals(testImg.width, img.width);
        assertEquals(testImg.height, img.height);
        assertEquals(
            '0',
            style.getStyle(img, 'left').replace(/px/g, '').replace(/pt/g, ''));
        assertEquals(
            '0',
            style.getStyle(img, 'top').replace(/px/g, '').replace(/pt/g, ''));
      } else {
        const cssClass = spriteInfo.getCssClass();
        if (cssClass) {
          assertEquals('DIV', inner.tagName);
          assertTrue(
              'Sprite should have its CSS class set',
              classlist.contains(inner, cssClass));
        } else {
          // There's an inner div wrapping an img tag
          assertEquals('DIV', inner.tagName);
          const img = inner.firstChild;
          assertNotNull('Div should be wrapping something', img);
          assertEquals('IMG', img.tagName);
          checkPathsEndWithSameFile(img.src, spriteInfo.getUrl());
          assertEquals(
              spriteInfo.getWidthCssValue(), style.getStyle(inner, 'width'));
          assertEquals(
              spriteInfo.getHeightCssValue(), style.getStyle(inner, 'height'));
          assertEquals(
              spriteInfo.getXOffsetCssValue().replace(/px/, '').replace(
                  /pt/, ''),
              style.getStyle(img, 'left').replace(/px/, '').replace(/pt/, ''));
          assertEquals(
              spriteInfo.getYOffsetCssValue().replace(/px/, '').replace(
                  /pt/, ''),
              style.getStyle(img, 'top').replace(/px/, '').replace(/pt/, ''));
        }
      }
    } else {
      // A non-sprited emoji is just an img
      assertEquals(inner.tagName, 'IMG');
      checkPathsEndWithSameFile(inner.src, emojiInfo[0]);
    }
  }
}

testSuite({
  setUp() {
    handler = new EventHandler();
  },

  tearDown() {
    handler.removeAll();
  },

  testConstructAndRenderOnePageEmojiPicker() {
    const picker = new EmojiPicker('../../demos/emoji/none.gif');
    picker.addEmojiGroup(emojiGroup1[0], emojiGroup1[1]);
    picker.render();
    picker.dispose();
  },

  testConstructAndRenderMultiPageEmojiPicker() {
    const picker = new EmojiPicker('../../demos/emoji/none.gif');
    picker.addEmojiGroup(emojiGroup1[0], emojiGroup1[1]);
    picker.addEmojiGroup(emojiGroup2[0], emojiGroup2[1]);
    picker.addEmojiGroup(emojiGroup3[0], emojiGroup3[1]);
    picker.render();
    picker.dispose();
  },

  testExitDocumentCleansUpProperlyForSinglePageEmojiPicker() {
    const picker = new EmojiPicker('../../demos/emoji/none.gif');
    picker.addEmojiGroup(emojiGroup1[0], emojiGroup1[1]);
    picker.render();
    picker.enterDocument();
    picker.exitDocument();
    picker.dispose();
  },

  testExitDocumentCleansUpProperlyForMultiPageEmojiPicker() {
    const picker = new EmojiPicker('../../demos/emoji/none.gif');
    picker.addEmojiGroup(emojiGroup1[0], emojiGroup1[1]);
    picker.addEmojiGroup(emojiGroup2[0], emojiGroup2[1]);
    picker.render();
    picker.enterDocument();
    picker.exitDocument();
    picker.dispose();
  },

  testNumGroups() {
    const picker = new EmojiPicker('../../demos/emoji/none.gif');

    for (let i = 0; i < emojiGroups.length; i++) {
      picker.addEmojiGroup(emojiGroups[i][0], emojiGroups[i][1]);
    }

    assertTrue(picker.getNumEmojiGroups() == emojiGroups.length);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testAdjustNumRowsIfNecessaryIsCorrect() {
    const picker = new EmojiPicker('../../demos/emoji/none.gif');
    picker.addEmojiGroup(emojiGroup1[0], emojiGroup1[1]);
    picker.setAutoSizeByColumnCount(true);
    picker.setNumColumns(5);
    assertEquals(5, picker.getNumColumns());
    assertEquals(EmojiPicker.DEFAULT_NUM_ROWS, picker.getNumRows());

    picker.adjustNumRowsIfNecessary_();

    // The emojiGroup has 26 emoji. ceil(26/5) should give 6 rows.
    assertEquals(6, picker.getNumRows());

    // Change col count to 10, should give 3 rows.
    picker.setNumColumns(10);
    picker.adjustNumRowsIfNecessary_();
    assertEquals(3, picker.getNumRows());

    // Add another gruop, with 20 emoji. Deliberately set the number of rows too
    // low. It should adjust it to three to accommodate the emoji in the first
    // group.
    picker.addEmojiGroup(emojiGroup2[0], emojiGroup2[1]);
    picker.setNumColumns(10);
    picker.setNumRows(2);
    picker.adjustNumRowsIfNecessary_();
    assertEquals(3, picker.getNumRows());
  },

  testNonDelayedLoadPaletteCreationForSinglePagePicker() {
    const defaultImg = '../../demos/emoji/none.gif';
    const picker = new EmojiPicker(defaultImg);
    picker.setDelayedLoad(false);
    picker.addEmojiGroup(emojiGroup1[0], emojiGroup1[1]);
    picker.render();

    const page = picker.getPage(0);
    assertTrue(
        'Page should be in the document but is not', page.isInDocument());

    // The content should be the actual emoji images now, with the remainder set
    // to the default img
    checkContentIsEmojiImages(page, emojiGroup1[1], defaultImg);

    picker.dispose();
  },

  testNonDelayedLoadPaletteCreationForMultiPagePicker() {
    const defaultImg = '../../demos/emoji/none.gif';
    const picker = new EmojiPicker(defaultImg);
    picker.setDelayedLoad(false);

    for (let i = 0; i < emojiGroups.length; i++) {
      picker.addEmojiGroup(emojiGroups[i][0], emojiGroups[i][1]);
    }

    picker.render();

    for (let i = 0; i < emojiGroups.length; i++) {
      const page = picker.getPage(i);
      assertTrue(
          `Page ${i} should be in the document but is not`,
          page.isInDocument());
      checkContentIsEmojiImages(page, emojiGroups[i][1], defaultImg);
    }

    picker.dispose();
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testDelayedLoadPaletteCreationForSinglePagePicker() {
    const defaultImg = '../../demos/emoji/none.gif';
    const picker = new EmojiPicker(defaultImg);
    picker.setDelayedLoad(true);
    picker.addEmojiGroup(emojiGroup1[0], emojiGroup1[1]);
    picker.render();

    // At this point the picker should have pages filled with the default img
    checkContentIsDefaultImg(picker.getPage(0), defaultImg);

    // Now load the images
    picker.loadImages();

    const page = picker.getPage(0);
    assertTrue(
        'Page should be in the document but is not', page.isInDocument());

    // The content should be the actual emoji images now, with the remainder set
    // to the default img
    checkContentIsEmojiImages(page, emojiGroup1[1], defaultImg);

    picker.dispose();
  },

  testGetSelectedEmoji() {
    const defaultImg = '../../demos/emoji/none.gif';
    const picker = new EmojiPicker(defaultImg);
    picker.setDelayedLoad(false);
    picker.addEmojiGroup(emojiGroup1[0], emojiGroup1[1]);
    picker.render();

    const palette = picker.getPage(0);

    // No emoji should be selected yet
    assertUndefined(palette.getSelectedEmoji());

    // Artificially select the first emoji
    palette.setSelectedIndex(0);

    // Now we should get the first emoji back. See emojiGroup1 above.
    const emoji = palette.getSelectedEmoji();
    assertEquals(emoji.getId(), 'std.200');
    assertEquals(emoji.getUrl(), '../../demos/emoji/200.gif');

    picker.dispose();
  },

  testGetSelectedEmoji_click() {
    const defaultImg = '../../demos/emoji/none.gif';
    const picker = new EmojiPicker(defaultImg);
    picker.setDelayedLoad(false);
    picker.addEmojiGroup(emojiGroup1[0], emojiGroup1[1]);
    picker.render();

    const palette = picker.getPage(0);
    // Artificially select the an emoji
    palette.setSelectedIndex(2);
    const element = palette.getSelectedItem();
    palette.setSelectedIndex(0);  // Select a different emoji.

    let eventSent;
    handler.listen(picker, Component.EventType.ACTION, (e) => {
      eventSent = e;
    });
    events.fireClickSequence(element, undefined, undefined, {shiftKey: false});

    // Now we should get the first emoji back. See emojiGroup1 above.
    const emoji = picker.getSelectedEmoji();
    assertEquals(emoji.getId(), 'std.202');
    assertEquals(emoji.getUrl(), '../../demos/emoji/202.gif');
    assertFalse(eventSent.shiftKey);

    picker.dispose();
  },

  testGetSelectedEmoji_shiftClick() {
    const defaultImg = '../../demos/emoji/none.gif';
    const picker = new EmojiPicker(defaultImg);
    picker.setDelayedLoad(false);
    picker.addEmojiGroup(emojiGroup1[0], emojiGroup1[1]);
    picker.render();

    const palette = picker.getPage(0);
    // Artificially select the an emoji
    palette.setSelectedIndex(3);
    const element = palette.getSelectedItem();
    palette.setSelectedIndex(0);  // Select a different emoji.

    let eventSent;
    handler.listen(picker, Component.EventType.ACTION, (e) => {
      eventSent = e;
    });
    events.fireClickSequence(element, undefined, undefined, {shiftKey: true});

    // Now we should get the first emoji back. See emojiGroup1 above.
    const emoji = picker.getSelectedEmoji();
    assertEquals(emoji.getId(), 'std.203');
    assertEquals(emoji.getUrl(), '../../demos/emoji/203.gif');
    assertTrue(eventSent.shiftKey);

    picker.dispose();
  },

  testGetSelectedEmoji_urlPrefix() {
    const defaultImg = 'a/b/../../../../demos/emoji/none.gif';
    const picker = new EmojiPicker(defaultImg);
    picker.setDelayedLoad(false);
    picker.addEmojiGroup(emojiGroup1[0], emojiGroup1[1]);
    picker.setUrlPrefix('a/b/../../');
    picker.render();

    const palette = picker.getPage(0);
    // Artificially select the an emoji
    palette.setSelectedIndex(0);
    palette.dispatchEvent(Component.EventType.ACTION);

    // Now we should get the first emoji back. See emojiGroup1 above.
    const emoji = picker.getSelectedEmoji();
    assertEquals('std.200', emoji.getId());
    assertEquals('a/b/../../../../demos/emoji/200.gif', emoji.getUrl());

    picker.dispose();
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testPreLoadCellConstructionForFastLoadingNonProgressive() {
    const defaultImg = '../../demos/emoji/none.gif';
    const picker = new EmojiPicker(defaultImg);
    picker.setDelayedLoad(false);
    picker.setManualLoadOfAnimatedEmoji(true);
    picker.setProgressiveRender(false);
    picker.addEmojiGroup(spritedEmoji2[0], spritedEmoji2[1]);
    picker.render();

    const palette = picker.getPage(0);

    checkStructureForNonProgressivePicker(palette, spritedEmoji2);

    picker.dispose();
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testPreLoadCellConstructionForFastLoadingProgressive() {
    const defaultImg = '../../demos/emoji/none.gif';
    const picker = new EmojiPicker(defaultImg);
    picker.setDelayedLoad(false);
    picker.setManualLoadOfAnimatedEmoji(true);
    picker.setProgressiveRender(true);
    picker.addEmojiGroup(spritedEmoji2[0], spritedEmoji2[1]);
    picker.render();

    const palette = picker.getPage(0);

    checkStructureForProgressivePicker(palette, spritedEmoji2);

    picker.dispose();
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testCellConstructionForNonProgressiveRenderingSpriting() {
    const defaultImg = '../../demos/emoji/none.gif';
    const picker = new EmojiPicker(defaultImg);
    picker.setDelayedLoad(false);
    picker.addEmojiGroup(spritedEmoji1[0], spritedEmoji1[1]);
    picker.render();

    const palette = picker.getPage(0);

    checkStructureForNonProgressivePicker(palette, spritedEmoji1);
    picker.dispose();
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testCellConstructionForProgressiveRenderingSpriting() {
    const defaultImg = '../../demos/emoji/none.gif';
    const picker = new EmojiPicker(defaultImg);
    picker.setDelayedLoad(false);
    picker.setProgressiveRender(true);
    picker.addEmojiGroup(spritedEmoji1[0], spritedEmoji1[1]);
    picker.render();

    const palette = picker.getPage(0);

    checkStructureForProgressivePicker(palette, spritedEmoji1);

    picker.dispose();
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testDelayedLoadPaletteCreationForMultiPagePicker() {
    const defaultImg = '../../demos/emoji/none.gif';
    const picker = new EmojiPicker(defaultImg);
    picker.setDelayedLoad(true);

    for (let i = 0; i < emojiGroups.length; i++) {
      picker.addEmojiGroup(emojiGroups[i][0], emojiGroups[i][1]);
    }

    picker.render();

    // At this point the picker should have pages filled with the default img
    for (let i = 0; i < emojiGroups.length; i++) {
      checkContentIsDefaultImg(picker.getPage(i), defaultImg);
    }

    // Now load the images
    picker.loadImages();

    // The first page should be loaded
    let page = picker.getPage(0);
    assertTrue(
        'Page should be in the document but is not', page.isInDocument());
    checkContentIsEmojiImages(page, emojiGroups[0][1], defaultImg);

    // The other pages should all be filled with the default img since they are
    // lazily loaded
    for (let i = 1; i < 3; i++) {
      checkContentIsDefaultImg(picker.getPage(i), defaultImg);
    }

    // Activate the other two pages so that their images get loaded, and check
    // that they're now loaded correctly
    const tabPane = picker.getTabPane();

    for (let i = 1; i < 3; i++) {
      tabPane.setSelectedIndex(i);
      page = picker.getPage(i);
      assertTrue(page.isInDocument());
      checkContentIsEmojiImages(page, emojiGroups[i][1], defaultImg);
    }

    picker.dispose();
  },
});
