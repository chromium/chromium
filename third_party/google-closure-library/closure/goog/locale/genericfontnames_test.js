// Copyright 2008 The Closure Library Authors. All Rights Reserved.
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

goog.module('goog.locale.genericFontNamesTest');
goog.setTestOnly();

const genericFontNames = goog.require('goog.locale.genericFontNames');
const testSuite = goog.require('goog.testing.testSuite');

genericFontNames.data_['zh_TW'] = [
  {
    'caption': '\u5fae\u8edf\u6b63\u9ed1\u9ad4',
    'value': 'Microsoft JhengHei,\u5fae\u8edf\u6b63\u9ed1\u9ad4,SimHei,' +
        '\u9ed1\u4f53,MS Hei,STHeiti,\u534e\u6587\u9ed1\u4f53,Apple ' +
        'LiGothic Medium,\u860b\u679c\u5137\u4e2d\u9ed1,LiHei Pro Medium,' +
        '\u5137\u9ed1 Pro,STHeiti Light,\u534e\u6587\u7ec6\u9ed1,AR PL ' +
        'ZenKai Uni,\u6587\u9f0ePL\u4e2d\u6977Uni,FreeSans,sans-serif',
  },
  {
    'caption': '\u5fae\u8f6f\u96c5\u9ed1\u5b57\u4f53',
    'value': 'Microsoft YaHei,\u5fae\u8f6f\u96c5\u9ed1\u5b57\u4f53,FreeSans,' +
        'sans-serif',
  },
  {
    'caption': '\u65b0\u7d30\u660e\u9ad4',
    'value': 'SimSun,\u5b8b\u4f53,MS Song,STSong,\u534e\u6587\u5b8b\u4f53,' +
        'Apple LiSung Light,\u860b\u679c\u5137\u7d30\u5b8b,LiSong Pro Light,' +
        '\u5137\u5b8b Pro,STFangSong,\u534e\u6587\u4eff\u5b8b,AR PL ' +
        'ShanHeiSun Uni,\u6587\u9f0eP' +
        'L\u7ec6\u4e0a\u6d77\u5b8bUni,AR PL New Sung,\u6587\u9f0e PL \u65b0' +
        '\u5b8b,FreeSerif,serif',
  },
  {
    'caption': '\u7d30\u660e\u9ad4',
    'value': 'NSimsun,\u65b0\u5b8b\u4f53,FreeMono,monospace',
  },
];

testSuite({
  testNormalize() {
    let result = genericFontNames.normalize_('zh');
    assertEquals('zh', result);
    result = genericFontNames.normalize_('zh-hant');
    assertEquals('zh_Hant', result);
    result = genericFontNames.normalize_('zh-hant-tw');
    assertEquals('zh_Hant_TW', result);
  },

  testInvalid() {
    const result = genericFontNames.getList('invalid');
    assertArrayEquals([], result);
  },

  testZhHant() {
    const result = genericFontNames.getList('zh-tw');
    assertObjectEquals(
        [
          {
            'caption': '\u5fae\u8edf\u6b63\u9ed1\u9ad4',
            'value':
                'Microsoft JhengHei,\u5fae\u8edf\u6b63\u9ed1\u9ad4,SimHei,' +
                '\u9ed1\u4f53,MS Hei,STHeiti,\u534e\u6587\u9ed1\u4f53,Apple ' +
                'LiGothic Medium,\u860b\u679c\u5137\u4e2d\u9ed1,LiHei Pro Medium,' +
                '\u5137\u9ed1 Pro,STHeiti Light,\u534e\u6587\u7ec6\u9ed1,AR PL ' +
                'ZenKai Uni,\u6587\u9f0ePL\u4e2d\u6977Uni,FreeSans,sans-serif',
          },
          {
            'caption': '\u5fae\u8f6f\u96c5\u9ed1\u5b57\u4f53',
            'value': 'Microsoft YaHei,\u5fae\u8f6f\u96c5\u9ed1\u5b57\u4f53,' +
                'FreeSans,sans-serif',
          },
          {
            'caption': '\u65b0\u7d30\u660e\u9ad4',
            'value':
                'SimSun,\u5b8b\u4f53,MS Song,STSong,\u534e\u6587\u5b8b\u4f53,' +
                'Apple LiSung Light,\u860b\u679c\u5137\u7d30\u5b8b,LiSong Pro ' +
                'Light,\u5137\u5b8b Pro,STFangSong,\u534e\u6587\u4eff\u5b8b,AR PL ' +
                'ShanHeiSun Uni,\u6587\u9f0ePL\u7ec6\u4e0a\u6d77\u5b8bUni,AR PL New' +
                ' Sung,\u6587\u9f0e PL \u65b0\u5b8b,FreeSerif,serif',
          },
          {
            'caption': '\u7d30\u660e\u9ad4',
            'value': 'NSimsun,\u65b0\u5b8b\u4f53,FreeMono,monospace',
          },
        ],
        result);
  },
});
