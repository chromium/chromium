/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */
goog.module('goog.color.alphaTest');
goog.setTestOnly();

const alpha = goog.require('goog.color.alpha');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  /**
   * @suppress {visibility} accessing private properties
   */
  testIsValidAlphaHexColor() {
    const goodAlphaHexColors = [
      '#ffffffff',
      '#ff781259',
      '#01234567',
      '#Ff003DaB',
      '#3CAF',
      '#abcdefab',
      '#3CAB',
    ];
    const badAlphaHexColors =
        ['#xxxxxxxx', '88990077', 'not_color', '#123456789', 'fffffgfg'];
    for (let i = 0; i < goodAlphaHexColors.length; i++) {
      assertTrue(
          goodAlphaHexColors[i],
          alpha.isValidAlphaHexColor_(goodAlphaHexColors[i]));
    }
    for (let i = 0; i < badAlphaHexColors.length; i++) {
      assertFalse(
          badAlphaHexColors[i],
          alpha.isValidAlphaHexColor_(badAlphaHexColors[i]));
    }
  },

  /**
   * @suppress {visibility} accessing private properties
   */
  testIsValidRgbaColor() {
    const goodRgbaColors = [
      'rgba(1, 20, 234, 1)',
      'rgba(255,127, 0,1)',
      'rgba(0,0,255,0.5)',
      '(255, 26, 75, 0.2)',
      'RGBA(0, 55, 0, 0.6)',
      'rGbA(0, 200, 0, 0.123456789)',
      'rgba(255, 0, 0, 1.0)',
      '  rgba(1,\t2,\n3,\r0.2) ',
    ];
    const badRgbaColors = [
      '(255, 0, 0)',
      '(2555,0,0, 0)',
      '(1,2,3,4,5)',
      'rgba(1,20,)',
      'RGBA(20,20,20,)',
      'RGBA',
      'rgba(255, 0, 0, 1.1)',
      'rgba(255, 0, 0, 1.00001)',
      'rgba(255, 0, 0, 1.)',
      'rgba(01, 0, 0, 1)',
    ];
    for (let i = 0; i < goodRgbaColors.length; i++) {
      assertEquals(
          goodRgbaColors[i], 4,
          alpha.isValidRgbaColor_(goodRgbaColors[i]).length);
    }
    for (let i = 0; i < badRgbaColors.length; i++) {
      assertEquals(
          badRgbaColors[i], 0,
          alpha.isValidRgbaColor_(badRgbaColors[i]).length);
    }
  },

  /**
   * @suppress {visibility} accessing private properties
   */
  testIsValidHslaColor() {
    const goodHslaColors = [
      'hsla(120, 0%, 0%, 1)',
      'hsla(360,20%,0%,1)',
      'hsla(0,0%,50%,0.5)',
      'HSLA(0, 55%, 0%, 0.6)',
      'hsla(0, 85%, 0%, 0.123456789)',
      'hsla(120, 0%, 0%, 1.0)',
      '  hsla(120,\t0%,\n0%,\r0.2) ',
    ];
    const badHslaColors = [
      '(255, 0, 0, 0)',
      'hsla(2555,0,0, 0)',
      'hsla(1,2,3,4,5)',
      'hsla(1,20,)',
      'HSLA(20,20,20,)',
      'hsla(255, 0, 0, 1.1)',
      'hsla(255, 0, 0, 1.00001)',
      'HSLA',
      'hsla(255, 0, 0, 1.)',
    ];
    for (let i = 0; i < goodHslaColors.length; i++) {
      assertEquals(
          goodHslaColors[i], 4,
          alpha.isValidHslaColor_(goodHslaColors[i]).length);
    }
    for (let i = 0; i < badHslaColors.length; i++) {
      assertEquals(
          badHslaColors[i], 0,
          alpha.isValidHslaColor_(badHslaColors[i]).length);
    }
  },

  testParse() {
    const colors = [
      'rgba(15, 250, 77, 0.5)',
      '(127, 127, 127, 0.8)',
      '#ffeeddaa',
      '12345678',
      'hsla(160, 50%, 90%, 0.2)',
    ];
    const parsed = colors.map(alpha.parse);
    assertEquals('rgba', parsed[0].type);
    assertEquals(alpha.rgbaToHex(15, 250, 77, 0.5), parsed[0].hex);
    assertEquals('rgba', parsed[1].type);
    assertEquals(alpha.rgbaToHex(127, 127, 127, 0.8), parsed[1].hex);
    assertEquals('hex', parsed[2].type);
    assertEquals('#ffeeddaa', parsed[2].hex);
    assertEquals('hex', parsed[3].type);
    assertEquals('#12345678', parsed[3].hex);
    assertEquals('hsla', parsed[4].type);
    assertEquals('#d9f2ea33', parsed[4].hex);

    const e = assertThrows(
        'not_color is not a valid color string',
        goog.partial(alpha.parse, 'not_color'));
    assertContains(
        'Error processing not_color', 'is not a valid color string', e.message);
  },

  testHexToRgba() {
    const testColors = [
      ['#B0FF2D66', [176, 255, 45, 0.4]],
      ['#b26e5fcc', [178, 110, 95, 0.8]],
      ['#66f3', [102, 102, 255, 0.2]],
    ];

    for (let i = 0; i < testColors.length; i++) {
      const r = alpha.hexToRgba(testColors[i][0]);
      const t = testColors[i][1];

      assertEquals('Red channel should match.', t[0], r[0]);
      assertEquals('Green channel should match.', t[1], r[1]);
      assertEquals('Blue channel should match.', t[2], r[2]);
      assertEquals('Alpha channel should match.', t[3], r[3]);
    }

    const badColors = ['', '#g00', 'some words'];
    for (let i = 0; i < badColors.length; i++) {
      const e = assertThrows(goog.partial(alpha.hexToRgba, badColors[i]));
      assertEquals(
          '\'' + badColors[i] + '\' is not a valid alpha hex color', e.message);
    }
  },

  testHexToRgbaStyle() {
    assertEquals('rgba(255,0,0,1)', alpha.hexToRgbaStyle('#ff0000ff'));
    assertEquals('rgba(206,206,206,0.8)', alpha.hexToRgbaStyle('#cecececc'));
    assertEquals('rgba(51,204,170,0.2)', alpha.hexToRgbaStyle('#3CA3'));
    assertEquals('rgba(1,2,3,0.016)', alpha.hexToRgbaStyle('#01020304'));
    assertEquals('rgba(255,255,0,0.333)', alpha.hexToRgbaStyle('#FFFF0055'));

    const badHexColors = ['#12345', null, undefined, '#.1234567890'];
    for (let i = 0; i < badHexColors.length; ++i) {
      const e = assertThrows(
          badHexColors[i] + ' is an invalid hex color',
          goog.partial(alpha.hexToRgbaStyle, badHexColors[i]));
      assertEquals(
          '\'' + badHexColors[i] + '\' is not a valid alpha hex color',
          e.message);
    }
  },

  testRgbaToHex() {
    assertEquals('#af13ffff', alpha.rgbaToHex(175, 19, 255, 1));
    assertEquals('#357cf099', alpha.rgbaToHex(53, 124, 240, 0.6));
    const badRgba = [
      [-1, -1, -1, -1],
      [0, 0, 0, 2],
      ['a', 'b', 'c', 'd'],
      [undefined, 5, 5, 5],
    ];
    for (let i = 0; i < badRgba.length; ++i) {
      const e = assertThrows(
          badRgba[i] + ' is not a valid rgba color',
          goog.partial(alpha.rgbaArrayToHex, badRgba[i]));
      assertContains('is not a valid RGBA color', e.message);
    }
  },

  testRgbaToRgbaStyle() {
    const testColors = [
      [[175, 19, 255, 1], 'rgba(175,19,255,1)'],
      [[53, 124, 240, .6], 'rgba(53,124,240,0.6)'],
      [[10, 20, 30, .1234567], 'rgba(10,20,30,0.123)'],
      [[20, 30, 40, 1 / 3], 'rgba(20,30,40,0.333)'],
    ];

    for (let i = 0; i < testColors.length; ++i) {
      const r = alpha.rgbaToRgbaStyle(
          testColors[i][0][0], testColors[i][0][1], testColors[i][0][2],
          testColors[i][0][3]);
      assertEquals(testColors[i][1], r);
    }

    const badColors = [[0, 0, 0, 2]];
    for (let i = 0; i < badColors.length; ++i) {
      const e = assertThrows(goog.partial(
          alpha.rgbaToRgbaStyle, badColors[i][0], badColors[i][1],
          badColors[i][2], badColors[i][3]));

      assertContains('is not a valid RGBA color', e.message);
    }

    // Loop through all bad color values and ensure they fail in each channel.
    const badValues = [-1, 300, 'a', undefined, null, NaN];
    const color = [0, 0, 0, 0];
    for (let i = 0; i < badValues.length; ++i) {
      for (let channel = 0; channel < color.length; ++channel) {
        color[channel] = badValues[i];
        const e = assertThrows(
            `${color} is not a valid rgba color`,
            goog.partial(alpha.rgbaToRgbaStyle, color));
        assertContains('is not a valid RGBA color', e.message);

        color[channel] = 0;
      }
    }
  },

  testRgbaArrayToRgbaStyle() {
    const testColors = [
      [[175, 19, 255, 1], 'rgba(175,19,255,1)'],
      [[53, 124, 240, .6], 'rgba(53,124,240,0.6)'],
    ];

    for (let i = 0; i < testColors.length; ++i) {
      const r = alpha.rgbaArrayToRgbaStyle(testColors[i][0]);
      assertEquals(testColors[i][1], r);
    }

    const badColors = [[0, 0, 0, 2]];
    for (let i = 0; i < badColors.length; ++i) {
      const e =
          assertThrows(goog.partial(alpha.rgbaArrayToRgbaStyle, badColors[i]));

      assertContains('is not a valid RGBA color', e.message);
    }

    // Loop through all bad color values and ensure they fail in each channel.
    const badValues = [-1, 300, 'a', undefined, null, NaN];
    const color = [0, 0, 0, 0];
    for (let i = 0; i < badValues.length; ++i) {
      for (let channel = 0; channel < color.length; ++channel) {
        color[channel] = badValues[i];
        const e = assertThrows(
            `${color} is not a valid rgba color`,
            goog.partial(alpha.rgbaToRgbaStyle, color));
        assertContains('is not a valid RGBA color', e.message);

        color[channel] = 0;
      }
    }
  },

  testRgbaArrayToHsla() {
    const opaqueBlueRgb = [0, 0, 255, 1];
    const opaqueBlueHsl = alpha.rgbaArrayToHsla(opaqueBlueRgb);
    assertArrayEquals(
        'Conversion from RGBA to HSLA should be as expected', [240, 1, 0.5, 1],
        opaqueBlueHsl);

    const nearlyOpaqueYellowRgb = [255, 190, 0, 0.7];
    const nearlyOpaqueYellowHsl = alpha.rgbaArrayToHsla(nearlyOpaqueYellowRgb);
    assertArrayEquals(
        'Conversion from RGBA to HSLA should be as expected', [45, 1, 0.5, 0.7],
        nearlyOpaqueYellowHsl);

    const transparentPurpleRgb = [180, 0, 255, 0];
    const transparentPurpleHsl = alpha.rgbaArrayToHsla(transparentPurpleRgb);
    assertArrayEquals(
        'Conversion from RGBA to HSLA should be as expected', [282, 1, 0.5, 0],
        transparentPurpleHsl);
  },

  /**
   * @suppress {visibility} accessing private properties
   */
  testNormalizeAlphaHex() {
    const compactColor = '#abcd';
    const normalizedCompactColor = alpha.normalizeAlphaHex_(compactColor);
    assertEquals(
        'The color should have been normalized to the right length',
        '#aabbccdd', normalizedCompactColor);

    const uppercaseColor = '#ABCDEF01';
    const normalizedUppercaseColor = alpha.normalizeAlphaHex_(uppercaseColor);
    assertEquals(
        'The color should have been normalized to lowercase', '#abcdef01',
        normalizedUppercaseColor);
  },

  testHsvaArrayToHex() {
    const opaqueSkyBlueHsv = [190, 1, 255, 1];
    const opaqueSkyBlueHex = alpha.hsvaArrayToHex(opaqueSkyBlueHsv);
    assertEquals(
        'The HSVA array should have been properly converted to hex',
        '#00d5ffff', opaqueSkyBlueHex);

    const halfTransparentPinkHsv = [300, 1, 255, 0.5];
    const halfTransparentPinkHex = alpha.hsvaArrayToHex(halfTransparentPinkHsv);
    assertEquals(
        'The HSVA array should have been properly converted to hex',
        '#ff00ff7f', halfTransparentPinkHex);

    const transparentDarkTurquoiseHsv = [175, 1, 127, 0.5];
    const transparentDarkTurquoiseHex =
        alpha.hsvaArrayToHex(transparentDarkTurquoiseHsv);
    assertEquals(
        'The HSVA array should have been properly converted to hex',
        '#007f747f', transparentDarkTurquoiseHex);
  },

  testExtractHexColor() {
    const opaqueRed = '#ff0000ff';
    const red = alpha.extractHexColor(opaqueRed);
    assertEquals(
        'The hex part of the color should have been extracted correctly',
        '#ff0000', red);

    const halfOpaqueDarkGreenCompact = '#0507';
    const darkGreen = alpha.extractHexColor(halfOpaqueDarkGreenCompact);
    assertEquals(
        'The hex part of the color should have been extracted correctly',
        '#005500', darkGreen);
  },

  testExtractAlpha() {
    const colors = ['#ff0000ff', '#0507', '#ff000005'];
    const expectedOpacities = ['ff', '77', '05'];

    for (let i = 0; i < colors.length; i++) {
      const opacity = alpha.extractAlpha(colors[i]);
      assertEquals(
          'The alpha transparency should have been extracted correctly',
          expectedOpacities[i], opacity);
    }
  },

  testHslaArrayToRgbaStyle() {
    assertEquals(
        'rgba(102,255,102,0.5)',
        alpha.hslaArrayToRgbaStyle([120, 100, 70, 0.5]));
    assertEquals(
        'rgba(28,23,23,0.9)', alpha.hslaArrayToRgbaStyle([0, 10, 10, 0.9]));
  },

  /**
   * @suppress {visibility} accessing private properties
   */
  testRgbaStyleParsableResult() {
    const testColors = [
      [175, 19, 255, 1],
      [53, 124, 240, .6],
      [20, 30, 40, 0.3333333],
      [255, 255, 255, 0.7071067811865476],
    ];

    for (let i = 0, testColor; testColor = testColors[i]; i++) {
      const rgbaStyle = alpha.rgbaStyle_(testColor);
      const parsedColor = alpha.hexToRgba(alpha.parse(rgbaStyle).hex);
      assertEquals(testColor[0], parsedColor[0]);
      assertEquals(testColor[1], parsedColor[1]);
      assertEquals(testColor[2], parsedColor[2]);
      // Parsing keeps a 1/255 accuracy on the alpha channel.
      assertRoughlyEquals(testColor[3], parsedColor[3], 0.005);
    }
  },
});
