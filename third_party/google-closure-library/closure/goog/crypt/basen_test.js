/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Tests for arbitrary base conversion library baseconversion.js.
 */

goog.module('goog.crypt.baseNTest');
goog.setTestOnly();

const baseN = goog.require('goog.crypt.baseN');
const testSuite = goog.require('goog.testing.testSuite');

function makeHugeBase() {
  // Number of digits in the base.
  // Tests break if this is set to 200'000.  The reason for that is
  // String.fromCharCode(196609) == String.fromCharCode(1).
  const baseSize = 20000;
  const tab = [];
  for (let i = 0; i < baseSize; i++) {
    tab.push(String.fromCharCode(i));
  }
  return tab.join('');
}

function verifyConversion(inputBase, inputNumber, outputBase, outputNumber) {
  assertEquals(
      outputNumber, baseN.recodeString(inputNumber, inputBase, outputBase));
}
testSuite({
  testDecToHex() {
    verifyConversion(
        baseN.BASE_DECIMAL, '0', baseN.BASE_LOWERCASE_HEXADECIMAL, '0');
    verifyConversion(
        baseN.BASE_DECIMAL, '9', baseN.BASE_UPPERCASE_HEXADECIMAL, '9');
    verifyConversion(
        baseN.BASE_DECIMAL, '13', baseN.BASE_LOWERCASE_HEXADECIMAL, 'd');
    verifyConversion(
        baseN.BASE_DECIMAL, '255', baseN.BASE_UPPERCASE_HEXADECIMAL, 'FF');
    verifyConversion(
        baseN.BASE_DECIMAL, '53425987345897', baseN.BASE_LOWERCASE_HEXADECIMAL,
        '309734ff5de9');
    verifyConversion(
        baseN.BASE_DECIMAL, '987080888', baseN.BASE_UPPERCASE_HEXADECIMAL,
        '3AD5A8B8');
    verifyConversion(
        baseN.BASE_DECIMAL, '009341587237', baseN.BASE_LOWERCASE_HEXADECIMAL,
        '22ccd4f25');
  },

  testBinToDec() {
    verifyConversion(
        baseN.BASE_BINARY,
        '11101010101000100010010000010010010000111101000100110111000000100001' +
            '01100100111110110010000010110100111101000010010100001011111011111100' +
            '00000010000010000101010101000000000101100000000100011111011101111001' +
            '10000001000000000100101110001001001101101001101111010101111100010001' +
            '11011100000110111000000100111011100100010010011001111011001111001011' +
            '10001000101111001010011101101100110110011110010000011100101011110010' +
            '11010001001111110011000000001001011011111011010000110011010000010111' +
            '10111100000001100010111100000100000000110001011101011110100000011010' +
            '0110000100011111',
        baseN.BASE_DECIMAL,
        '34589745906769047354795784390596748934723904739085568907689045723489' +
            '05745789789078907890789023447892365623589745678902348976234598723459' +
            '087523496723486089723459078349087');
  },

  testDecToBin() {
    verifyConversion(
        baseN.BASE_DECIMAL,
        '00342589674590347859734908573490568347534805468907960579056785605496' +
            '83475873465859072390486756098742380573908572390463805745656623475234' +
            '82345670247851902784123897349486238502378940637925807378946358964328' +
            '57906148572346857346409823758034763928401296023947234784623765456367' +
            '764623627623574',
        baseN.BASE_BINARY,
        '10010011011100101010001111100111001100110000110111111110010110101000' +
            '01010110110010000111000001100110100101010000101001100001011000101111' +
            '01011101111100101101010010000111011110011110010101111001110010100100' +
            '10111110000101111011010000000111111011110010011110101011100101000001' +
            '00011000101010011001101000011101001010001101011110101001011011100101' +
            '11100000101000010010101001011001100100101110111101010000011010001010' +
            '01011100100111110001100111100100011001001001100011011100100111011111' +
            '01000100101001000100110001011010010000011010111101111111111111110100' +
            '01100101001111001111100110101000001100100000111111100101110010111011' +
            '10110110001100100011101010110110100001001000101011001001100011010110' +
            '10110100000110000110010111110100000100110110010010010101111001001111' +
            '11100100000010101111110100011010011101011010001101110011100110111111' +
            '11000100001111010000000101011011000010010000000100111111010110111100' +
            '00101111010011011010011010010001000101100001111001110010010110');
  },

  test7To9() {
    verifyConversion(
        '0123456',  // Base 7.
        '60625646056660665666066534602566346056634560665606666656465634265434' +
            '66563465664566346406366534664656650660665623456663456654360665',
        '012345678',  // Base 9.
        '11451222686557606458341381287142358175337801548087003804852781764284' +
            '273762357630423116743334671762638240652740158536');
  },

  testZeros() {
    verifyConversion(
        baseN.BASE_DECIMAL, '0', baseN.BASE_LOWERCASE_HEXADECIMAL, '0');
    verifyConversion(
        baseN.BASE_DECIMAL, '000', baseN.BASE_LOWERCASE_HEXADECIMAL, '0');
    verifyConversion(
        baseN.BASE_DECIMAL, '0000007', baseN.BASE_LOWERCASE_HEXADECIMAL, '7');
  },

  testArbitraryBases() {
    verifyConversion(
        'X9(',  // Base 3.
        '9(XX((9X(XX9(9X9(X9(',
        'a:*o9',  // Base 5.
        ':oa**:9o9**9oo');
  },

  testEmptyBases() {
    let e = assertThrows(() => {
      baseN.recodeString('1230', '', '0123');
    });
    assertEquals(
        'Exception message',
        'Number 1230 contains a character ' +
            'not found in base , which is 0',
        e.message);

    e = assertThrows(() => {
      baseN.recodeString('1230', '0123', '');
    });
    assertEquals('Exception message', 'Empty output base', e.message);
  },

  testInvalidDigits() {
    const e = assertThrows(() => {
      baseN.recodeString('123x456', '01234567', '01234567');
    });
    assertEquals(
        'Exception message',
        'Number 123x456 contains a character ' +
            'not found in base 01234567, which is x',
        e.message);
  },

  testHugeInputBase() {
    verifyConversion(
        makeHugeBase(), String.fromCharCode(12345), baseN.BASE_DECIMAL,
        '12345');
  },

  testHugeOutputBase() {
    verifyConversion(
        baseN.BASE_DECIMAL, '12345', makeHugeBase(),
        String.fromCharCode(12345));
  },
});
