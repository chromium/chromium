/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.crypt.pbkdf2Test');
goog.setTestOnly();

const crypt = goog.require('goog.crypt');
const pbkdf2 = goog.require('goog.crypt.pbkdf2');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

testSuite({
  testPBKDF2() {
    // PBKDF2 test vectors from:
    // http://tools.ietf.org/html/rfc6070

    if (userAgent.IE) {
      return;
    }

    let testPassword = crypt.stringToByteArray('password');
    let testSalt = crypt.stringToByteArray('salt');

    assertElementsEquals(
        crypt.hexToByteArray('0c60c80f961f0e71f3a9b524af6012062fe037a6'),
        pbkdf2.deriveKeySha1(testPassword, testSalt, 1, 160));

    assertElementsEquals(
        crypt.hexToByteArray('ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957'),
        pbkdf2.deriveKeySha1(testPassword, testSalt, 2, 160));

    assertElementsEquals(
        crypt.hexToByteArray('4b007901b765489abead49d926f721d065a429c1'),
        pbkdf2.deriveKeySha1(testPassword, testSalt, 4096, 160));

    testPassword = crypt.stringToByteArray('passwordPASSWORDpassword');
    testSalt = crypt.stringToByteArray('saltSALTsaltSALTsaltSALTsaltSALTsalt');

    assertElementsEquals(
        crypt.hexToByteArray(
            '3d2eec4fe41c849b80c8d83662c0e44a8b291a964cf2f07038'),
        pbkdf2.deriveKeySha1(testPassword, testSalt, 4096, 200));

    testPassword = crypt.stringToByteArray('pass\0word');
    testSalt = crypt.stringToByteArray('sa\0lt');

    assertElementsEquals(
        crypt.hexToByteArray('56fa6aa75548099dcc37d7f03425e0c3'),
        pbkdf2.deriveKeySha1(testPassword, testSalt, 4096, 128));
  },
});
