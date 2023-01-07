/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.i18n.mime.encodeTest');
goog.setTestOnly();

const encode = goog.require('goog.i18n.mime.encode');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testEncodeAllAscii() {
    // A string holding all the characters that should be encoded unchanged.
    // Double-quote is doubled to avoid annoying syntax highlighting in emacs,
    // which doesn't recognize the double-quote as being in a string constant.
    const identity =
        '!""#$%&\'()*+,-./0123456789:;<>@ABCDEFGHIJKLMNOPQRSTUVWXYZ' +
        '[\\]^`abcdefghijklmnopqrstuvwxyz{|}~';
    assertEquals(identity, encode(identity));
  },

  testEncodeSpecials() {
    assertEquals('=?UTF-8?Q?=f0=9f=92=a9?=', encode('💩'));
    assertEquals('=?UTF-8?Q?=3f=5f=3d_?=', encode('?_= '));
    assertEquals('=?UTF-8?Q?=3f=5f=3d_=22=22?=', encode('?_= ""', true));
  },

  testEncodeUnicode() {
    // Two-byte UTF-8, plus a special
    assertEquals('=?UTF-8?Q?=c2=82=de=a0_dude?=', encode('\u0082\u07a0 dude'));
    // Three-byte UTF-8, plus a special
    assertEquals('=?UTF-8?Q?=e0=a0=80=ef=bf=bf=3d?=', encode('\u0800\uffff='));
  },
});
