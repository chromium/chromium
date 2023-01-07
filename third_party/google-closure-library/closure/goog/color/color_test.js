/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */
goog.module('goog.colorTest');
goog.setTestOnly();

const googColor = goog.require('goog.color');
const names = goog.require('goog.color.names');
const testSuite = goog.require('goog.testing.testSuite');

// Tests accuracy of HSL to RGB conversion

// Tests HSV to RGB conversion

// Tests that HSV space is (0-360) for hue

// Tests conversion between HSL and Hex

// Tests conversion between HSV and Hex

/**
 * This helper method compares two RGB colors, checking that each color
 * component is the same.
 * @param {Array<number>} rgb1 Color represented by a 3-element array with red,
 *     green, and blue values respectively, in the range [0, 255].
 * @param {Array<number>} rgb2 Color represented by a 3-element array with red,
 *     green, and blue values respectively, in the range [0, 255].
 * @return {boolean} True if the colors are the same, false otherwise.
 */
function rgbColorsAreEqual(rgb1, rgb2) {
  return (rgb1[0] == rgb2[0] && rgb1[1] == rgb2[1] && rgb1[2] == rgb2[2]);
}

/**
 * Helper function for color conversion functions between two colorspaces.
 * @param {Function} funcOne Function that converts from 1st colorspace to 2nd
 * @param {Function} funcTwo Function that converts from 2nd colorspace to 2nd
 * @param {Array<number>} color The color array passed to funcOne
 * @param {number} DELTA Margin of error for each element in color
 * @suppress {visibility} accessing private properties
 */
function colorConversionTestHelper(funcOne, funcTwo, color, DELTA) {
  const temp = funcOne(color);

  if (!googColor.isValidHexColor_(temp)) {
    assertTrue(`First conversion had a NaN: ${temp}`, !isNaN(temp[0]));
    assertTrue(`First conversion had a NaN: ${temp}`, !isNaN(temp[1]));
    assertTrue(`First conversion had a NaN: ${temp}`, !isNaN(temp[2]));
  }

  const back = funcTwo(temp);

  if (!googColor.isValidHexColor_(temp)) {
    assertTrue(`Second conversion had a NaN: ${back}`, !isNaN(back[0]));
    assertTrue(`Second conversion had a NaN: ${back}`, !isNaN(back[1]));
    assertTrue(`Second conversion had a NaN: ${back}`, !isNaN(back[2]));
  }

  assertColorFuzzyEquals('Color was off', color, back, DELTA);
}

/**
 * Checks equivalence between two colors' respective values.  Accepts +- delta
 * for each pair of values
 * @param {string} str
 * @param {Array<number>} expected
 * @param {Array<number>} actual
 * @param {number} delta Margin of error for each element in color array
 */
function assertColorFuzzyEquals(str, expected, actual, delta) {
  assertTrue(
      `${str} Expected: ${expected}  and got: ${actual} w/ delta: ` + delta,
      (Math.abs(expected[0] - actual[0]) <= delta) &&
          (Math.abs(expected[1] - actual[1]) <= delta) &&
          (Math.abs(expected[2] - actual[2]) <= delta));
}
testSuite({
  testIsValidColor() {
    const goodColors = [
      '#ffffff',
      '#ff7812',
      '#012345',
      '#Ff003D',
      '#3CA',
      '(255, 26, 75)',
      'RGB(2, 3, 4)',
      '(0,0,0)',
      'white',
      'blue',
    ];
    const badColors = [
      '#xxxxxx',
      '8899000',
      'not_color',
      '#1234567',
      'fffffg',
      '(2555,0,0)',
      '(1,2,3,4)',
      'rgb(1,20,)',
      'RGB(20,20,20,)',
      'omgwtfbbq',
    ];
    for (let i = 0; i < goodColors.length; i++) {
      assertTrue(goodColors[i], googColor.isValidColor(goodColors[i]));
    }
    for (let i = 0; i < badColors.length; i++) {
      assertFalse(badColors[i], googColor.isValidColor(badColors[i]));
    }
  },

  /**
   * @suppress {visibility} accessing private properties
   */
  testIsValidHexColor() {
    const goodHexColors = ['#ffffff', '#ff7812', '#012345', '#Ff003D', '#3CA'];
    const badHexColors =
        ['#xxxxxx', '889900', 'not_color', '#1234567', 'fffffg'];
    for (let i = 0; i < goodHexColors.length; i++) {
      assertTrue(
          goodHexColors[i], googColor.isValidHexColor_(goodHexColors[i]));
    }
    for (let i = 0; i < badHexColors.length; i++) {
      assertFalse(badHexColors[i], googColor.isValidHexColor_(badHexColors[i]));
    }
  },

  /**
   * @suppress {visibility} accessing private properties
   */
  testIsValidRgbColor() {
    const goodRgbColors =
        ['(255, 26, 75)', 'RGB(2, 3, 4)', '(0,0,0)', 'rgb(255,255,255)'];
    const badRgbColors =
        ['(2555,0,0)', '(1,2,3,4)', 'rgb(1,20,)', 'RGB(20,20,20,)'];
    for (let i = 0; i < goodRgbColors.length; i++) {
      assertEquals(
          goodRgbColors[i], googColor.isValidRgbColor_(goodRgbColors[i]).length,
          3);
    }
    for (let i = 0; i < badRgbColors.length; i++) {
      assertEquals(
          badRgbColors[i], googColor.isValidRgbColor_(badRgbColors[i]).length,
          0);
    }
  },

  testParse() {
    const colors =
        ['rgb(15, 250, 77)', '(127, 127, 127)', '#ffeedd', '123456', 'magenta'];
    const parsed = colors.map(googColor.parse);
    assertEquals('rgb', parsed[0].type);
    assertEquals(googColor.rgbToHex(15, 250, 77), parsed[0].hex);
    assertEquals('rgb', parsed[1].type);
    assertEquals(googColor.rgbToHex(127, 127, 127), parsed[1].hex);
    assertEquals('hex', parsed[2].type);
    assertEquals('#ffeedd', parsed[2].hex);
    assertEquals('hex', parsed[3].type);
    assertEquals('#123456', parsed[3].hex);
    assertEquals('named', parsed[4].type);
    assertEquals('#ff00ff', parsed[4].hex);

    const badColors = ['rgb(01, 1, 23)', '(256, 256, 256)', '#ffeeddaa'];
    for (let i = 0; i < badColors.length; i++) {
      const e = assertThrows(goog.partial(googColor.parse, badColors[i]));
      assertContains('is not a valid color string', e.message);
    }
  },

  testHexToRgb() {
    const testColors = [
      ['#B0FF2D', [176, 255, 45]],
      ['#b26e5f', [178, 110, 95]],
      ['#66f', [102, 102, 255]],
    ];

    for (let i = 0; i < testColors.length; i++) {
      const r = googColor.hexToRgb(testColors[i][0]);
      const t = testColors[i][1];

      assertEquals('Red channel should match.', t[0], r[0]);
      assertEquals('Green channel should match.', t[1], r[1]);
      assertEquals('Blue channel should match.', t[2], r[2]);
    }

    const badColors = ['', '#g00', 'some words'];
    for (let i = 0; i < badColors.length; i++) {
      const e = assertThrows(goog.partial(googColor.hexToRgb, badColors[i]));
      assertEquals(
          '\'' + badColors[i] + '\' is not a valid hex color', e.message);
    }
  },

  testHexToRgbStyle() {
    assertEquals('rgb(255,0,0)', googColor.hexToRgbStyle(names['red']));
    assertEquals('rgb(206,206,206)', googColor.hexToRgbStyle('#cecece'));
    assertEquals('rgb(51,204,170)', googColor.hexToRgbStyle('#3CA'));
    const badHexColors = ['#1234', null, undefined, '#.1234567890'];
    for (let i = 0; i < badHexColors.length; ++i) {
      const badHexColor = badHexColors[i];
      const e =
          assertThrows(goog.partial(googColor.hexToRgbStyle, badHexColor));
      assertEquals(`'${badHexColor}' is not a valid hex color`, e.message);
    }
  },

  testRgbToHex() {
    assertEquals(names['red'], googColor.rgbToHex(255, 0, 0));
    assertEquals('#af13ff', googColor.rgbToHex(175, 19, 255));
    const badRgb = [
      [-1, -1, -1],
      [256, 0, 0],
      ['a', 'b', 'c'],
      [undefined, 5, 5],
      [1.2, 3, 4],
    ];
    for (let i = 0; i < badRgb.length; ++i) {
      const e = assertThrows(goog.partial(googColor.rgbArrayToHex, badRgb[i]));
      assertContains('is not a valid RGB color', e.message);
    }
  },

  testRgbToHsl() {
    const rgb = [255, 171, 32];
    const hsl = googColor.rgbArrayToHsl(rgb);
    assertEquals(37, hsl[0]);
    assertTrue(1.0 - hsl[1] < 0.01);
    assertTrue(hsl[2] - .5625 < 0.01);
  },

  testHslToRgb() {
    const hsl = [60, 0.5, 0.1];
    const rgb = googColor.hslArrayToRgb(hsl);
    assertEquals(38, rgb[0]);
    assertEquals(38, rgb[1]);
    assertEquals(13, rgb[2]);
  },

  testHSLBidiToRGB() {
    const DELTA = 1;

    const color = [
      [100, 56, 200],
      [255, 0, 0],
      [0, 0, 255],
      [0, 255, 0],
      [255, 255, 255],
      [0, 0, 0],
    ];

    for (let i = 0; i < color.length; i++) {
      colorConversionTestHelper(
          (color) => googColor.rgbToHsl(color[0], color[1], color[2]),
          (color) => googColor.hslToRgb(color[0], color[1], color[2]), color[i],
          DELTA);

      colorConversionTestHelper(
          (color) => googColor.rgbArrayToHsl(color),
          (color) => googColor.hslArrayToRgb(color), color[i], DELTA);
    }
  },

  testHSVToRGB() {
    const DELTA = 1;

    const color = [
      [100, 56, 200],
      [255, 0, 0],
      [0, 0, 255],
      [0, 255, 0],
      [255, 255, 255],
      [0, 0, 0],
    ];

    for (let i = 0; i < color.length; i++) {
      colorConversionTestHelper(
          (color) => googColor.rgbToHsv(color[0], color[1], color[2]),
          (color) => googColor.hsvToRgb(color[0], color[1], color[2]), color[i],
          DELTA);

      colorConversionTestHelper(
          (color) => googColor.rgbArrayToHsv(color),
          (color) => googColor.hsvArrayToRgb(color), color[i], DELTA);
    }
  },

  testHSVSpecRangeIsCorrect() {
    const color = [0, 0, 255];  // Blue is in the middle of hue range

    const hsv = googColor.rgbToHsv(color[0], color[1], color[2]);

    assertTrue('H in HSV space looks like it\'s not 0-360', hsv[0] > 1);
  },

  testHslToHex() {
    const DELTA = 1;

    const color = [[0, 0, 0], [20, 0.5, 0.5], [0, 0, 1], [255, .45, .76]];

    for (let i = 0; i < color.length; i++) {
      colorConversionTestHelper(
          (hsl) => googColor.hslToHex(hsl[0], hsl[1], hsl[2]),
          (hex) => googColor.hexToHsl(hex), color[i], DELTA);

      colorConversionTestHelper(
          (hsl) => googColor.hslArrayToHex(hsl),
          (hex) => googColor.hexToHsl(hex), color[i], DELTA);
    }
  },

  testHsvToHex() {
    const DELTA = 1;

    const color = [[0, 0, 0], [.5, 0.5, 155], [0, 0, 255], [.7, .45, 21]];

    for (let i = 0; i < color.length; i++) {
      colorConversionTestHelper(
          (hsl) => googColor.hsvToHex(hsl[0], hsl[1], hsl[2]),
          (hex) => googColor.hexToHsv(hex), color[i], DELTA);

      colorConversionTestHelper(
          (hsl) => googColor.hsvArrayToHex(hsl),
          (hex) => googColor.hexToHsv(hex), color[i], DELTA);
    }
  },

  /**
   * This method runs unit tests against googColor.blend().  Test cases include:
   * blending arbitrary colors with factors of 0 and 1, blending the same colors
   * using arbitrary factors, blending different colors of varying factors,
   * and blending colors using factors outside the expected range.
   */
  testColorBlend() {
    // Define some RGB colors for our tests.
    const black = [0, 0, 0];
    const blue = [0, 0, 255];
    const gray = [128, 128, 128];
    const green = [0, 255, 0];
    const purple = [128, 0, 128];
    const red = [255, 0, 0];
    const yellow = [255, 255, 0];
    const white = [255, 255, 255];

    // Blend arbitrary colors, using 0 and 1 for factors. Using 0 should return
    // the first color, while using 1 should return the second color.
    const redWithNoGreen = googColor.blend(red, green, 1);
    assertTrue('red + 0 * green = red', rgbColorsAreEqual(red, redWithNoGreen));
    const whiteWithAllBlue = googColor.blend(white, blue, 0);
    assertTrue(
        'white + 1 * blue = blue', rgbColorsAreEqual(blue, whiteWithAllBlue));

    // Blend the same colors using arbitrary factors. This should return the
    // same colors.
    const greenWithGreen = googColor.blend(green, green, .25);
    assertTrue(
        'green + .25 * green = green',
        rgbColorsAreEqual(green, greenWithGreen));

    // Blend different colors using varying factors.
    const blackWithWhite = googColor.blend(black, white, .5);
    assertTrue(
        'black + .5 * white = gray', rgbColorsAreEqual(gray, blackWithWhite));
    const redAndBlue = googColor.blend(red, blue, .5);
    assertTrue(
        'red + .5 * blue = purple', rgbColorsAreEqual(purple, redAndBlue));
    const lightGreen = googColor.blend(green, white, .75);
    assertTrue(
        'green + .25 * white = a lighter shade of white',
        lightGreen[0] > 0 && lightGreen[1] == 255 && lightGreen[2] > 0);

    // Blend arbitrary colors using factors outside the expected range.
    const noGreenAllPurple = googColor.blend(green, purple, -0.5);
    assertTrue(
        'green * -0.5 + purple = purple.',
        rgbColorsAreEqual(purple, noGreenAllPurple));
    const allBlueNoYellow = googColor.blend(blue, yellow, 1.37);
    assertTrue(
        'blue * 1.37 + yellow = blue.',
        rgbColorsAreEqual(blue, allBlueNoYellow));
  },

  /**
   * This method runs unit tests against googColor.darken(). Test cases
   * include darkening black with arbitrary factors, edge cases (using 0 and 1),
   * darkening colors using various factors, and darkening colors using factors
   * outside the expected range.
   */
  testColorDarken() {
    // Define some RGB colors
    const black = [0, 0, 0];
    const green = [0, 255, 0];
    const darkGray = [68, 68, 68];
    const olive = [128, 128, 0];
    const purple = [128, 0, 128];
    const white = [255, 255, 255];

    // Darken black by an arbitrary factor, which should still return black.
    const darkBlack = googColor.darken(black, .63);
    assertTrue(
        'black darkened by .63 is still black.',
        rgbColorsAreEqual(black, darkBlack));

    // Call darken() with edge-case factors (0 and 1).
    const greenNotDarkened = googColor.darken(green, 0);
    assertTrue(
        'green darkened by 0 is still green.',
        rgbColorsAreEqual(green, greenNotDarkened));
    const whiteFullyDarkened = googColor.darken(white, 1);
    assertTrue(
        'white darkened by 1 is black.',
        rgbColorsAreEqual(black, whiteFullyDarkened));

    // Call darken() with various colors and factors. The result should be
    // a color with less luminance.
    const whiteHsl = googColor.rgbToHsl(white[0], white[1], white[2]);
    const whiteDarkened = googColor.darken(white, .43);
    const whiteDarkenedHsl = googColor.rgbToHsl(
        whiteDarkened[0], whiteDarkened[1], whiteDarkened[2]);
    assertTrue(
        'White that\'s darkened has less luminance than white.',
        whiteDarkenedHsl[2] < whiteHsl[2]);
    const purpleHsl = googColor.rgbToHsl(purple[0], purple[1], purple[2]);
    const purpleDarkened = googColor.darken(purple, .1);
    const purpleDarkenedHsl = googColor.rgbToHsl(
        purpleDarkened[0], purpleDarkened[1], purpleDarkened[2]);
    assertTrue(
        'Purple that\'s darkened has less luminance than purple.',
        purpleDarkenedHsl[2] < purpleHsl[2]);

    // Call darken() with factors outside the expected range.
    const darkGrayTurnedBlack = googColor.darken(darkGray, 2.1);
    assertTrue(
        'Darkening dark gray by 2.1 returns black.',
        rgbColorsAreEqual(black, darkGrayTurnedBlack));
    const whiteNotDarkened = googColor.darken(white, -0.62);
    assertTrue(
        'Darkening white by -0.62 returns white.',
        rgbColorsAreEqual(white, whiteNotDarkened));
  },

  /**
   * This method runs unit tests against googColor.lighten(). Test cases
   * include lightening white with arbitrary factors, edge cases (using 0 and
   * 1), lightening colors using various factors, and lightening colors using
   * factors outside the expected range.
   */
  testColorLighten() {
    // Define some RGB colors
    const black = [0, 0, 0];
    const brown = [165, 42, 42];
    const navy = [0, 0, 128];
    const orange = [255, 165, 0];
    const white = [255, 255, 255];

    // Lighten white by an arbitrary factor, which should still return white.
    const lightWhite = googColor.lighten(white, .41);
    assertTrue(
        'white lightened by .41 is still white.',
        rgbColorsAreEqual(white, lightWhite));

    // Call lighten() with edge-case factors(0 and 1).
    const navyNotLightened = googColor.lighten(navy, 0);
    assertTrue(
        'navy lightened by 0 is still navy.',
        rgbColorsAreEqual(navy, navyNotLightened));
    const orangeFullyLightened = googColor.lighten(orange, 1);
    assertTrue(
        'orange lightened by 1 is white.',
        rgbColorsAreEqual(white, orangeFullyLightened));

    // Call lighten() with various colors and factors. The result should be
    // a color with greater luminance.
    const blackHsl = googColor.rgbToHsl(black[0], black[1], black[2]);
    const blackLightened = googColor.lighten(black, .33);
    const blackLightenedHsl = googColor.rgbToHsl(
        blackLightened[0], blackLightened[1], blackLightened[2]);
    assertTrue(
        'Black that\'s lightened has more luminance than black.',
        blackLightenedHsl[2] >= blackHsl[2]);
    const orangeHsl = googColor.rgbToHsl(orange[0], orange[1], orange[2]);
    const orangeLightened = googColor.lighten(orange, .91);
    const orangeLightenedHsl = googColor.rgbToHsl(
        orangeLightened[0], orangeLightened[1], orangeLightened[2]);
    assertTrue(
        'Orange that\'s lightened has more luminance than orange.',
        orangeLightenedHsl[2] >= orangeHsl[2]);

    // Call lighten() with factors outside the expected range.
    const navyTurnedWhite = googColor.lighten(navy, 1.01);
    assertTrue(
        'Lightening navy by 1.01 returns white.',
        rgbColorsAreEqual(white, navyTurnedWhite));
    const brownNotLightened = googColor.lighten(brown, -0.0000001);
    assertTrue(
        'Lightening brown by -0.0000001 returns brown.',
        rgbColorsAreEqual(brown, brownNotLightened));
  },

  /** This method runs unit tests against googColor.hslDistance(). */
  testHslDistance() {
    // Define some HSL colors
    const aliceBlueHsl = googColor.rgbToHsl(240, 248, 255);
    const blackHsl = googColor.rgbToHsl(0, 0, 0);
    const ghostWhiteHsl = googColor.rgbToHsl(248, 248, 255);
    const navyHsl = googColor.rgbToHsl(0, 0, 128);
    const redHsl = googColor.rgbToHsl(255, 0, 0);
    const whiteHsl = googColor.rgbToHsl(255, 255, 255);

    // The distance between the same colors should be 0.
    assertTrue(
        'There is no HSL distance between white and white.',
        googColor.hslDistance(whiteHsl, whiteHsl) == 0);
    assertTrue(
        'There is no HSL distance between red and red.',
        googColor.hslDistance(redHsl, redHsl) == 0);

    // The distance between various colors should be within certain thresholds.
    let hslDistance = googColor.hslDistance(whiteHsl, ghostWhiteHsl);
    assertTrue(
        'The HSL distance between white and ghost white is > 0.',
        hslDistance > 0);
    assertTrue(
        'The HSL distance between white and ghost white is <= 0.02.',
        hslDistance <= 0.02);
    hslDistance = googColor.hslDistance(whiteHsl, redHsl);
    assertTrue(
        'The HSL distance between white and red is > 0.02.',
        hslDistance > 0.02);
    hslDistance = googColor.hslDistance(navyHsl, aliceBlueHsl);
    assertTrue(
        'The HSL distance between navy and alice blue is > 0.02.',
        hslDistance > 0.02);
    hslDistance = googColor.hslDistance(blackHsl, whiteHsl);
    assertTrue(
        'The HSL distance between white and black is 1.', hslDistance == 1);
  },

  /**
   * This method runs unit tests against googColor.yiqBrightness_().
   * @suppress {visibility} accessing private properties
   */
  testYiqBrightness() {
    const white = [255, 255, 255];
    const black = [0, 0, 0];
    const coral = [255, 127, 80];
    const lightgreen = [144, 238, 144];

    const whiteBrightness = googColor.yiqBrightness_(white);
    const blackBrightness = googColor.yiqBrightness_(black);
    const coralBrightness = googColor.yiqBrightness_(coral);
    const lightgreenBrightness = googColor.yiqBrightness_(lightgreen);

    // brightness should be a number
    assertTrue(
        'White brightness is a number.', typeof whiteBrightness == 'number');
    assertTrue(
        'Coral brightness is a number.', typeof coralBrightness == 'number');

    // brightness for known colors should match known values
    assertEquals('White brightness is 255', whiteBrightness, 255);
    assertEquals('Black brightness is 0', blackBrightness, 0);
    assertEquals('Coral brightness is 160', coralBrightness, 160);
    assertEquals('Lightgreen brightness is 199', lightgreenBrightness, 199);
  },

  /**
   * This method runs unit tests against googColor.yiqBrightnessDiff_().
   * @suppress {visibility} accessing private properties
   */
  testYiqBrightnessDiff() {
    const colors = {
      'deeppink': [255, 20, 147],
      'indigo': [75, 0, 130],
      'saddlebrown': [139, 69, 19],
    };

    const diffs = new Object();
    for (let name1 in colors) {
      for (let name2 in colors) {
        diffs[`${name1}-${name2}`] =
            googColor.yiqBrightnessDiff_(colors[name1], colors[name2]);
      }
    }

    for (let pair in diffs) {
      // each brightness diff should be a number
      assertTrue(`${pair} diff is a number.`, typeof diffs[pair] == 'number');
      // each brightness diff should be greater than or equal to 0
      assertTrue(
          `${pair} diff is greater than or equal to 0.`, diffs[pair] >= 0);
    }

    // brightness diff for same-color pairs should be 0
    assertEquals('deeppink-deeppink is 0.', diffs['deeppink-deeppink'], 0);
    assertEquals('indigo-indigo is 0.', diffs['indigo-indigo'], 0);

    // brightness diff for known pairs should match known values
    assertEquals('deeppink-indigo is 68.', diffs['deeppink-indigo'], 68);
    assertEquals(
        'saddlebrown-deeppink is 21.', diffs['saddlebrown-deeppink'], 21);

    // reversed pairs should have equal values
    assertEquals('indigo-saddlebrown is 47.', diffs['indigo-saddlebrown'], 47);
    assertEquals(
        'saddlebrown-indigo is also 47.', diffs['saddlebrown-indigo'], 47);
  },

  /**
   * This method runs unit tests against googColor.colorDiff_().
   * @suppress {visibility} accessing private properties
   */
  testColorDiff() {
    const colors = {
      'mediumblue': [0, 0, 205],
      'oldlace': [253, 245, 230],
      'orchid': [218, 112, 214],
    };

    const diffs = new Object();
    for (let name1 in colors) {
      for (let name2 in colors) {
        diffs[`${name1}-${name2}`] =
            googColor.colorDiff_(colors[name1], colors[name2]);
      }
    }

    for (let pair in diffs) {
      // each color diff should be a number
      assertTrue(`${pair} diff is a number.`, typeof diffs[pair] == 'number');
      // each color diff should be greater than or equal to 0
      assertTrue(
          `${pair} diff is greater than or equal to 0.`, diffs[pair] >= 0);
    }

    // color diff for same-color pairs should be 0
    assertEquals(
        'mediumblue-mediumblue is 0.', diffs['mediumblue-mediumblue'], 0);
    assertEquals('oldlace-oldlace is 0.', diffs['oldlace-oldlace'], 0);

    // color diff for known pairs should match known values
    assertEquals(
        'mediumblue-oldlace is 523.', diffs['mediumblue-oldlace'], 523);
    assertEquals('oldlace-orchid is 184.', diffs['oldlace-orchid'], 184);

    // reversed pairs should have equal values
    assertEquals('orchid-mediumblue is 339.', diffs['orchid-mediumblue'], 339);
    assertEquals(
        'mediumblue-orchid is also 339.', diffs['mediumblue-orchid'], 339);
  },

  /** This method runs unit tests against googColor.highContrast(). */
  testHighContrast() {
    const white = [255, 255, 255];
    const black = [0, 0, 0];
    const lemonchiffron = [255, 250, 205];
    const sienna = [160, 82, 45];

    const suggestion =
        googColor.highContrast(black, [white, black, sienna, lemonchiffron]);

    // should return an array of three numbers
    assertTrue('Return value is an array.', typeof suggestion == 'object');
    assertTrue('Return value is 3 long.', suggestion.length == 3);

    // known color combos should return a known (i.e. human-verified) suggestion
    assertArrayEquals(
        'White is best on sienna.',
        googColor.highContrast(sienna, [white, black, sienna, lemonchiffron]),
        white);
    assertArrayEquals(
        'Black is best on lemonchiffron.',
        googColor.highContrast(white, [white, black, sienna, lemonchiffron]),
        black);
  },
});
