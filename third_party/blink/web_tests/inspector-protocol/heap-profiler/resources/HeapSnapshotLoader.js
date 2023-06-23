const HeapSnapshotLoader = (function (exports) {
  'use strict';

  // Copyright 2020 The Chromium Authors
  // Use of this source code is governed by a BSD-style license that can be
  // found in the LICENSE file.

  /**
   * @param {string} inputString
   * @param {string} charsToEscape
   * @return {string} the string with any matching chars escaped
   */
  const escapeCharacters = (inputString, charsToEscape) => {
    let foundChar = false;
    for (let i = 0; i < charsToEscape.length; ++i) {
      if (inputString.indexOf(charsToEscape.charAt(i)) !== -1) {
        foundChar = true;
        break;
      }
    }

    if (!foundChar) {
      return String(inputString);
    }

    let result = '';
    for (let i = 0; i < inputString.length; ++i) {
      if (charsToEscape.indexOf(inputString.charAt(i)) !== -1) {
        result += '\\';
      }
      result += inputString.charAt(i);
    }

    return result;
  };

  /**
   * @enum {string}
   */
  const FORMATTER_TYPES = {
    STRING: 'string',
    SPECIFIER: 'specifier',
  };

  /**
   * @param {string} formatString
   * @param {!Object.<string, function(string, ...*):*>} formatters
   * @return {!Array.<!FORMATTER_TOKEN>}
   */
  const tokenizeFormatString = function(formatString, formatters) {
    /** @type {!Array<!FORMATTER_TOKEN>} */
    const tokens = [];

    /**
     * @param {string} str
     */
    function addStringToken(str) {
      if (!str) {
        return;
      }
      if (tokens.length && tokens[tokens.length - 1].type === FORMATTER_TYPES.STRING) {
        tokens[tokens.length - 1].value += str;
      } else {
        tokens.push({
          type: FORMATTER_TYPES.STRING,
          value: str,
          specifier: undefined,
          precision: undefined,
          substitutionIndex: undefined
        });
      }
    }

    /**
     * @param {string} specifier
     * @param {number} precision
     * @param {number} substitutionIndex
     */
    function addSpecifierToken(specifier, precision, substitutionIndex) {
      tokens.push({type: FORMATTER_TYPES.SPECIFIER, specifier, precision, substitutionIndex, value: undefined});
    }

    /**
     * @param {number} code
     */
    function addAnsiColor(code) {
      /**
       * @type {!Object<number, string>}
       */
      const types = {3: 'color', 9: 'colorLight', 4: 'bgColor', 10: 'bgColorLight'};
      const colorCodes = ['black', 'red', 'green', 'yellow', 'blue', 'magenta', 'cyan', 'lightGray', '', 'default'];
      const colorCodesLight =
          ['darkGray', 'lightRed', 'lightGreen', 'lightYellow', 'lightBlue', 'lightMagenta', 'lightCyan', 'white', ''];
      /** @type {!Object<string, !Array<string>>} */
      const colors = {color: colorCodes, colorLight: colorCodesLight, bgColor: colorCodes, bgColorLight: colorCodesLight};
      const type = types[Math.floor(code / 10)];
      if (!type) {
        return;
      }
      const color = colors[type][code % 10];
      if (!color) {
        return;
      }
      tokens.push({
        type: FORMATTER_TYPES.SPECIFIER,
        specifier: 'c',
        value: {description: (type.startsWith('bg') ? 'background : ' : 'color: ') + color},
        precision: undefined,
        substitutionIndex: undefined,
      });
    }

    let textStart = 0;
    let substitutionIndex = 0;
    const re =
        new RegExp(`%%|%(?:(\\d+)\\$)?(?:\\.(\\d*))?([${Object.keys(formatters).join('')}])|\\u001b\\[(\\d+)m`, 'g');
    for (let match = re.exec(formatString); !!match; match = re.exec(formatString)) {
      const matchStart = match.index;
      if (matchStart > textStart) {
        addStringToken(formatString.substring(textStart, matchStart));
      }

      if (match[0] === '%%') {
        addStringToken('%');
      } else if (match[0].startsWith('%')) {
        // eslint-disable-next-line no-unused-vars
        const [_, substitionString, precisionString, specifierString] = match;
        if (substitionString && Number(substitionString) > 0) {
          substitutionIndex = Number(substitionString) - 1;
        }
        const precision = precisionString ? Number(precisionString) : -1;
        addSpecifierToken(specifierString, precision, substitutionIndex);
        ++substitutionIndex;
      } else {
        const code = Number(match[4]);
        addAnsiColor(code);
      }
      textStart = matchStart + match[0].length;
    }
    addStringToken(formatString.substring(textStart));
    return tokens;
  };

  /**
   * @param {string} formatString
   * @param {?ArrayLike<*>} substitutions
   * @param {!Object.<string, function(string, ...*):*>} formatters
   * @param {!T} initialValue
   * @param {function(T, *): T} append
   * @param {!Array.<!FORMATTER_TOKEN>=} tokenizedFormat
   * @return {!{formattedResult: T, unusedSubstitutions: ?ArrayLike<*>}};
   * @template T
   */
  const format = function(formatString, substitutions, formatters, initialValue, append, tokenizedFormat) {
    if (!formatString || ((!substitutions || !substitutions.length) && formatString.search(/\u001b\[(\d+)m/) === -1)) {
      return {formattedResult: append(initialValue, formatString), unusedSubstitutions: substitutions};
    }

    function prettyFunctionName() {
      return 'String.format("' + formatString + '", "' + Array.prototype.join.call(substitutions, '", "') + '")';
    }

    /**
     * @param {string} msg
     */
    function warn(msg) {
      console.warn(prettyFunctionName() + ': ' + msg);
    }

    /**
     * @param {string} msg
     */
    function error(msg) {
      console.error(prettyFunctionName() + ': ' + msg);
    }

    let result = initialValue;
    const tokens = tokenizedFormat || tokenizeFormatString(formatString, formatters);
    /** @type {!Object<number, boolean>} */
    const usedSubstitutionIndexes = {};
    /** @type {!ArrayLike<*>} */
    const actualSubstitutions = substitutions || [];

    for (let i = 0; i < tokens.length; ++i) {
      const token = tokens[i];

      if (token.type === FORMATTER_TYPES.STRING) {
        result = append(result, token.value);
        continue;
      }

      if (token.type !== FORMATTER_TYPES.SPECIFIER) {
        error('Unknown token type "' + token.type + '" found.');
        continue;
      }

      if (!token.value && token.substitutionIndex !== undefined &&
          token.substitutionIndex >= actualSubstitutions.length) {
        // If there are not enough substitutions for the current substitutionIndex
        // just output the format specifier literally and move on.
        error(
            'not enough substitution arguments. Had ' + actualSubstitutions.length + ' but needed ' +
            (token.substitutionIndex + 1) + ', so substitution was skipped.');
        result = append(
            result,
            '%' + ((token.precision !== undefined && token.precision > -1) ? token.precision : '') + token.specifier);
        continue;
      }

      if (!token.value && token.substitutionIndex !== undefined) {
        usedSubstitutionIndexes[token.substitutionIndex] = true;
      }

      if (token.specifier === undefined || !(token.specifier in formatters)) {
        // Encountered an unsupported format character, treat as a string.
        warn('unsupported format character \u201C' + token.specifier + '\u201D. Treating as a string.');
        result = append(
            result,
            (token.value || token.substitutionIndex === undefined) ? '' : actualSubstitutions[token.substitutionIndex]);
        continue;
      }

      result = append(
          result,
          formatters[token.specifier](
              token.value || (token.substitutionIndex !== undefined && actualSubstitutions[token.substitutionIndex]),
              token));
    }

    const unusedSubstitutions = [];
    for (let i = 0; i < actualSubstitutions.length; ++i) {
      if (i in usedSubstitutionIndexes) {
        continue;
      }
      unusedSubstitutions.push(actualSubstitutions[i]);
    }

    return {formattedResult: result, unusedSubstitutions: unusedSubstitutions};
  };

  const standardFormatters = {
    /**
     * @param {*} substitution
     * @return {number}
     */
    d: function(substitution) {
      return /** @type {number} */ (!isNaN(substitution) ? substitution : 0);
    },

    /**
     * @param {*} substitution
     * @param {!FORMATTER_TOKEN} token
     * @return {number}
     */
    f: function(substitution, token) {
      if (substitution && token.precision !== undefined && token.precision > -1) {
        substitution = substitution.toFixed(token.precision);
      }
      const precision = (token.precision !== undefined && token.precision > -1) ? Number(0).toFixed(token.precision) : 0;
      return /** @type number} */ (!isNaN(substitution) ? substitution : precision);
    },

    /**
     * @param {*} substitution
     * @return {string}
     */
    s: function(substitution) {
      return /** @type {string} */ (substitution);
    }
  };

  /**
   * @param {string} formatString
   * @param {!Array.<*>} substitutions
   * @return {string}
   */
  const vsprintf = function(formatString, substitutions) {
    // @ts-ignore
    return format(formatString, substitutions, standardFormatters, '', function(a, b) {
             return a + b;
           }).formattedResult;
  };

  /**
   * @param {string} format
   * @param {...*} var_arg
   * @return {string}
   */
  const sprintf = function(format, var_arg) {
    return vsprintf(format, Array.prototype.slice.call(arguments, 1));
  };

  /*
   * Copyright (C) 2007 Apple Inc.  All rights reserved.
   * Copyright (C) 2012 Google Inc. All rights reserved.
   *
   * Redistribution and use in source and binary forms, with or without
   * modification, are permitted provided that the following conditions
   * are met:
   *
   * 1.  Redistributions of source code must retain the above copyright
   *     notice, this list of conditions and the following disclaimer.
   * 2.  Redistributions in binary form must reproduce the above copyright
   *     notice, this list of conditions and the following disclaimer in the
   *     documentation and/or other materials provided with the distribution.
   * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
   *     its contributors may be used to endorse or promote products derived
   *     from this software without specific prior written permission.
   *
   * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
   * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
   * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
   * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   */

  // Still used in the test runners that can't use ES modules :(
  String.sprintf = sprintf;

  /**
   * @param {string} chars
   * @return {string}
   */
  /**
   * @return {string}
   */
  String.regexSpecialCharacters = function() {
    return '^[]{}()\\.^$*+?|-,';
  };

  /**
   * @this {string}
   * @return {string}
   */
  String.prototype.escapeForRegExp = function() {
    return escapeCharacters(this, String.regexSpecialCharacters());
  };

  /**
   * @param {string} query
   * @return {!RegExp}
   */
  String.filterRegex = function(query) {
    const toEscape = String.regexSpecialCharacters();
    let regexString = '';
    for (let i = 0; i < query.length; ++i) {
      let c = query.charAt(i);
      if (toEscape.indexOf(c) !== -1) {
        c = '\\' + c;
      }
      if (i) {
        regexString += '[^\\0' + c + ']*';
      }
      regexString += c;
    }
    return new RegExp(regexString, 'i');
  };

  /**
   * @param {number} maxLength
   * @return {string}
   */
  String.prototype.trimMiddle = function(maxLength) {
    if (this.length <= maxLength) {
      return String(this);
    }
    let leftHalf = maxLength >> 1;
    let rightHalf = maxLength - leftHalf - 1;
    if (this.codePointAt(this.length - rightHalf - 1) >= 0x10000) {
      --rightHalf;
      ++leftHalf;
    }
    if (leftHalf > 0 && this.codePointAt(leftHalf - 1) >= 0x10000) {
      --leftHalf;
    }
    return this.substr(0, leftHalf) + '…' + this.substr(this.length - rightHalf, rightHalf);
  };

  /**
   * @param {number} maxLength
   * @return {string}
   */
  String.prototype.trimEndWithMaxLength = function(maxLength) {
    if (this.length <= maxLength) {
      return String(this);
    }
    return this.substr(0, maxLength - 1) + '…';
  };

  /**
   * @param {string|undefined} string
   * @return {number}
   */
  String.hashCode = function(string) {
    if (!string) {
      return 0;
    }
    // Hash algorithm for substrings is described in "Über die Komplexität der Multiplikation in
    // eingeschränkten Branchingprogrammmodellen" by Woelfe.
    // http://opendatastructures.org/versions/edition-0.1d/ods-java/node33.html#SECTION00832000000000000000
    const p = ((1 << 30) * 4 - 5);  // prime: 2^32 - 5
    const z = 0x5033d967;           // 32 bits from random.org
    const z2 = 0x59d2f15d;          // random odd 32 bit number
    let s = 0;
    let zi = 1;
    for (let i = 0; i < string.length; i++) {
      const xi = string.charCodeAt(i) * z2;
      s = (s + zi * xi) % p;
      zi = (zi * z) % p;
    }
    s = (s + zi * (p - 1)) % p;
    return Math.abs(s | 0);
  };

  /**
   * @param {string} a
   * @param {string} b
   * @return {number}
   */
  String.naturalOrderComparator = function(a, b) {
    const chunk = /^\d+|^\D+/;
    let chunka, chunkb, anum, bnum;
    while (1) {
      if (a) {
        if (!b) {
          return 1;
        }
      } else {
        if (b) {
          return -1;
        }
        return 0;
      }
      chunka = a.match(chunk)[0];
      chunkb = b.match(chunk)[0];
      anum = !isNaN(chunka);
      bnum = !isNaN(chunkb);
      if (anum && !bnum) {
        return -1;
      }
      if (bnum && !anum) {
        return 1;
      }
      if (anum && bnum) {
        const diff = chunka - chunkb;
        if (diff) {
          return diff;
        }
        if (chunka.length !== chunkb.length) {
          if (!+chunka && !+chunkb) {  // chunks are strings of all 0s (special case)
            return chunka.length - chunkb.length;
          }
          return chunkb.length - chunka.length;
        }
      } else if (chunka !== chunkb) {
        return (chunka < chunkb) ? -1 : 1;
      }
      a = a.substring(chunka.length);
      b = b.substring(chunkb.length);
    }
  };

  /**
   * @param {string} a
   * @param {string} b
   * @return {number}
   */
  String.caseInsensetiveComparator = function(a, b) {
    a = a.toUpperCase();
    b = b.toUpperCase();
    if (a === b) {
      return 0;
    }
    return a > b ? 1 : -1;
  };

  /**
   * @param {string} value
   * @return {string}
   */
  Number.toFixedIfFloating = function(value) {
    if (!value || isNaN(value)) {
      return value;
    }
    const number = Number(value);
    return number % 1 ? number.toFixed(3) : String(number);
  };

  (function() {
  const partition = {
    /**
       * @this {Array.<number>}
       * @param {function(number, number): number} comparator
       * @param {number} left
       * @param {number} right
       * @param {number} pivotIndex
       */
    value: function(comparator, left, right, pivotIndex) {
      function swap(array, i1, i2) {
        const temp = array[i1];
        array[i1] = array[i2];
        array[i2] = temp;
      }

      const pivotValue = this[pivotIndex];
      swap(this, right, pivotIndex);
      let storeIndex = left;
      for (let i = left; i < right; ++i) {
        if (comparator(this[i], pivotValue) < 0) {
          swap(this, storeIndex, i);
          ++storeIndex;
        }
      }
      swap(this, right, storeIndex);
      return storeIndex;
    },
    configurable: true
  };
  Object.defineProperty(Array.prototype, 'partition', partition);
  Object.defineProperty(Uint32Array.prototype, 'partition', partition);

  const sortRange = {
    /**
       * @param {function(number, number): number} comparator
       * @param {number} leftBound
       * @param {number} rightBound
       * @param {number} sortWindowLeft
       * @param {number} sortWindowRight
       * @return {!Array.<number>}
       * @this {Array.<number>}
       */
    value: function(comparator, leftBound, rightBound, sortWindowLeft, sortWindowRight) {
      function quickSortRange(array, comparator, left, right, sortWindowLeft, sortWindowRight) {
        if (right <= left) {
          return;
        }
        const pivotIndex = Math.floor(Math.random() * (right - left)) + left;
        const pivotNewIndex = array.partition(comparator, left, right, pivotIndex);
        if (sortWindowLeft < pivotNewIndex) {
          quickSortRange(array, comparator, left, pivotNewIndex - 1, sortWindowLeft, sortWindowRight);
        }
        if (pivotNewIndex < sortWindowRight) {
          quickSortRange(array, comparator, pivotNewIndex + 1, right, sortWindowLeft, sortWindowRight);
        }
      }
      if (leftBound === 0 && rightBound === (this.length - 1) && sortWindowLeft === 0 && sortWindowRight >= rightBound) {
        this.sort(comparator);
      } else {
        quickSortRange(this, comparator, leftBound, rightBound, sortWindowLeft, sortWindowRight);
      }
      return this;
    },
    configurable: true
  };
  Object.defineProperty(Array.prototype, 'sortRange', sortRange);
  Object.defineProperty(Uint32Array.prototype, 'sortRange', sortRange);
  })();

  Object.defineProperty(Array.prototype, 'lowerBound', {
    /**
     * Return index of the leftmost element that is equal or greater
     * than the specimen object. If there's no such element (i.e. all
     * elements are smaller than the specimen) returns right bound.
     * The function works for sorted array.
     * When specified, |left| (inclusive) and |right| (exclusive) indices
     * define the search window.
     *
     * @param {!T} object
     * @param {function(!T,!S):number=} comparator
     * @param {number=} left
     * @param {number=} right
     * @return {number}
     * @this {Array.<!S>}
     * @template T,S
     */
    value: function(object, comparator, left, right) {
      function defaultComparator(a, b) {
        return a < b ? -1 : (a > b ? 1 : 0);
      }
      comparator = comparator || defaultComparator;
      let l = left || 0;
      let r = right !== undefined ? right : this.length;
      while (l < r) {
        const m = (l + r) >> 1;
        if (comparator(object, this[m]) > 0) {
          l = m + 1;
        } else {
          r = m;
        }
      }
      return r;
    },
    configurable: true
  });

  Object.defineProperty(Array.prototype, 'upperBound', {
    /**
     * Return index of the leftmost element that is greater
     * than the specimen object. If there's no such element (i.e. all
     * elements are smaller or equal to the specimen) returns right bound.
     * The function works for sorted array.
     * When specified, |left| (inclusive) and |right| (exclusive) indices
     * define the search window.
     *
     * @param {!T} object
     * @param {function(!T,!S):number=} comparator
     * @param {number=} left
     * @param {number=} right
     * @return {number}
     * @this {Array.<!S>}
     * @template T,S
     */
    value: function(object, comparator, left, right) {
      function defaultComparator(a, b) {
        return a < b ? -1 : (a > b ? 1 : 0);
      }
      comparator = comparator || defaultComparator;
      let l = left || 0;
      let r = right !== undefined ? right : this.length;
      while (l < r) {
        const m = (l + r) >> 1;
        if (comparator(object, this[m]) >= 0) {
          l = m + 1;
        } else {
          r = m;
        }
      }
      return r;
    },
    configurable: true
  });

  Object.defineProperty(Uint32Array.prototype, 'lowerBound', {value: Array.prototype.lowerBound, configurable: true});

  Object.defineProperty(Uint32Array.prototype, 'upperBound', {value: Array.prototype.upperBound, configurable: true});

  Object.defineProperty(Int32Array.prototype, 'lowerBound', {value: Array.prototype.lowerBound, configurable: true});

  Object.defineProperty(Int32Array.prototype, 'upperBound', {value: Array.prototype.upperBound, configurable: true});

  Object.defineProperty(Float64Array.prototype, 'lowerBound', {value: Array.prototype.lowerBound, configurable: true});

  Object.defineProperty(Array.prototype, 'binaryIndexOf', {
    /**
     * @param {!T} value
     * @param {function(!T,!S):number} comparator
     * @return {number}
     * @this {Array.<!S>}
     * @template T,S
     */
    value: function(value, comparator) {
      const index = this.lowerBound(value, comparator);
      return index < this.length && comparator(value, this[index]) === 0 ? index : -1;
    },
    configurable: true
  });

  Object.defineProperty(Array.prototype, 'peekLast', {
    /**
     * @return {!T|undefined}
     * @this {Array.<!T>}
     * @template T
     */
    value: function() {
      return this[this.length - 1];
    },
    configurable: true
  });

  (function() {
    /**
     * @param {!Array.<T>} array1
     * @param {!Array.<T>} array2
     * @param {function(T,T):number} comparator
     * @param {boolean} mergeNotIntersect
     * @return {!Array.<T>}
     * @template T
     */
    function mergeOrIntersect(array1, array2, comparator, mergeNotIntersect) {
      const result = [];
      let i = 0;
      let j = 0;
      while (i < array1.length && j < array2.length) {
        const compareValue = comparator(array1[i], array2[j]);
        if (mergeNotIntersect || !compareValue) {
          result.push(compareValue <= 0 ? array1[i] : array2[j]);
        }
        if (compareValue <= 0) {
          i++;
        }
        if (compareValue >= 0) {
          j++;
        }
      }
      if (mergeNotIntersect) {
        while (i < array1.length) {
          result.push(array1[i++]);
        }
        while (j < array2.length) {
          result.push(array2[j++]);
        }
      }
      return result;
    }

    Object.defineProperty(Array.prototype, 'intersectOrdered', {
      /**
       * @param {!Array.<T>} array
       * @param {function(T,T):number} comparator
       * @return {!Array.<T>}
       * @this {!Array.<T>}
       * @template T
       */
      value: function(array, comparator) {
        return mergeOrIntersect(this, array, comparator, false);
      },
      configurable: true
    });

    Object.defineProperty(Array.prototype, 'mergeOrdered', {
      /**
       * @param {!Array.<T>} array
       * @param {function(T,T):number} comparator
       * @return {!Array.<T>}
       * @this {!Array.<T>}
       * @template T
       */
      value: function(array, comparator) {
        return mergeOrIntersect(this, array, comparator, true);
      },
      configurable: true
    });
  })();

  /**
   * @param {string} query
   * @param {boolean} caseSensitive
   * @param {boolean} isRegex
   * @return {!RegExp}
   */
  self.createSearchRegex = function(query, caseSensitive, isRegex) {
    const regexFlags = caseSensitive ? 'g' : 'gi';
    let regexObject;

    if (isRegex) {
      try {
        regexObject = new RegExp(query, regexFlags);
      } catch (e) {
        // Silent catch.
      }
    }

    if (!regexObject) {
      regexObject = self.createPlainTextSearchRegex(query, regexFlags);
    }

    return regexObject;
  };

  /**
   * @param {string} query
   * @param {string=} flags
   * @return {!RegExp}
   */
  self.createPlainTextSearchRegex = function(query, flags) {
    // This should be kept the same as the one in StringUtil.cpp.
    const regexSpecialCharacters = String.regexSpecialCharacters();
    let regex = '';
    for (let i = 0; i < query.length; ++i) {
      const c = query.charAt(i);
      if (regexSpecialCharacters.indexOf(c) !== -1) {
        regex += '\\';
      }
      regex += c;
    }
    return new RegExp(regex, flags || '');
  };

  /**
   * @param {number} spacesCount
   * @return {string}
   */
  self.spacesPadding = function(spacesCount) {
    return '\xA0'.repeat(spacesCount);
  };

  /**
   * @param {number} value
   * @param {number} symbolsCount
   * @return {string}
   */
  self.numberToStringWithSpacesPadding = function(value, symbolsCount) {
    const numberString = value.toString();
    const paddingLength = Math.max(0, symbolsCount - numberString.length);
    return self.spacesPadding(paddingLength) + numberString;
  };

  /**
   * @return {?T}
   * @template T
   */
  Set.prototype.firstValue = function() {
    if (!this.size) {
      return null;
    }
    return this.values().next().value;
  };

  /**
   * @return {!Platform.Multimap<!KEY, !VALUE>}
   */
  Map.prototype.inverse = function() {
    const result = new Platform.Multimap();
    for (const key of this.keys()) {
      const value = this.get(key);
      result.set(value, key);
    }
    return result;
  };

  /**
   * @template K, V
   */
  class Multimap {
    constructor() {
      /** @type {!Map.<K, !Set.<!V>>} */
      this._map = new Map();
    }

    /**
     * @param {K} key
     * @param {V} value
     */
    set(key, value) {
      let set = this._map.get(key);
      if (!set) {
        set = new Set();
        this._map.set(key, set);
      }
      set.add(value);
    }

    /**
     * @param {K} key
     * @return {!Set<!V>}
     */
    get(key) {
      return this._map.get(key) || new Set();
    }

    /**
     * @param {K} key
     * @return {boolean}
     */
    has(key) {
      return this._map.has(key);
    }

    /**
     * @param {K} key
     * @param {V} value
     * @return {boolean}
     */
    hasValue(key, value) {
      const set = this._map.get(key);
      if (!set) {
        return false;
      }
      return set.has(value);
    }

    /**
     * @return {number}
     */
    get size() {
      return this._map.size;
    }

    /**
     * @param {K} key
     * @param {V} value
     * @return {boolean}
     */
    delete(key, value) {
      const values = this.get(key);
      if (!values) {
        return false;
      }
      const result = values.delete(value);
      if (!values.size) {
        this._map.delete(key);
      }
      return result;
    }

    /**
     * @param {K} key
     */
    deleteAll(key) {
      this._map.delete(key);
    }

    /**
     * @return {!Array.<K>}
     */
    keysArray() {
      return [...this._map.keys()];
    }

    /**
     * @return {!Array.<!V>}
     */
    valuesArray() {
      const result = [];
      for (const set of this._map.values()) {
        result.push(...set.values());
      }
      return result;
    }

    clear() {
      this._map.clear();
    }
  }

  /**
   * @param {*} value
   */
  self.suppressUnused = function(value) {};

  /**
   * @param {function()} callback
   * @return {number}
   */
  self.setImmediate = function(callback) {
    const args = [...arguments].slice(1);
    Promise.resolve().then(() => callback(...args));
    return 0;
  };

  /**
   * TODO: move into its own module
   * @param {function()} callback
   * @suppressGlobalPropertiesCheck
   */
  self.runOnWindowLoad = function(callback) {
    /**
     * @suppressGlobalPropertiesCheck
     */
    function windowLoaded() {
      self.removeEventListener('DOMContentLoaded', windowLoaded, false);
      callback();
    }

    if (document.readyState === 'complete' || document.readyState === 'interactive') {
      callback();
    } else {
      self.addEventListener('DOMContentLoaded', windowLoaded, false);
    }
  };

  const _singletonSymbol = Symbol('singleton');

  /**
   * @template T
   * @param {function(new:T, ...)} constructorFunction
   * @return {!T}
   */
  self.singleton = function(constructorFunction) {
    if (_singletonSymbol in constructorFunction) {
      return constructorFunction[_singletonSymbol];
    }
    const instance = new constructorFunction();
    constructorFunction[_singletonSymbol] = instance;
    return instance;
  };

  /**
   * @param {?string} content
   * @return {number}
   */
  self.base64ToSize = function(content) {
    if (!content) {
      return 0;
    }
    let size = content.length * 3 / 4;
    if (content[content.length - 1] === '=') {
      size--;
    }
    if (content.length > 1 && content[content.length - 2] === '=') {
      size--;
    }
    return size;
  };

  /**
   * @param {?string} input
   * @return {string}
   */
  self.unescapeCssString = function(input) {
    // https://drafts.csswg.org/css-syntax/#consume-escaped-code-point
    const reCssEscapeSequence = /(?<!\\)\\(?:([a-fA-F0-9]{1,6})|(.))[\n\t\x20]?/gs;
    return input.replace(reCssEscapeSequence, (_, $1, $2) => {
      if ($2) {  // Handle the single-character escape sequence.
        return $2;
      }
      // Otherwise, handle the code point escape sequence.
      const codePoint = parseInt($1, 16);
      const isSurrogate = 0xD800 <= codePoint && codePoint <= 0xDFFF;
      if (isSurrogate || codePoint === 0x0000 || codePoint > 0x10FFFF) {
        return '\uFFFD';
      }
      return String.fromCodePoint(codePoint);
    });
  };

  self.Platform = self.Platform || {};
  Platform = Platform || {};

  /** @constructor */
  Platform.Multimap = Multimap;

  /*
   * Copyright (C) 2011 Google Inc.  All rights reserved.
   * Copyright (C) 2006, 2007, 2008 Apple Inc.  All rights reserved.
   * Copyright (C) 2007 Matt Lilek (pewtermoose@gmail.com).
   * Copyright (C) 2009 Joseph Pecoraro
   *
   * Redistribution and use in source and binary forms, with or without
   * modification, are permitted provided that the following conditions
   * are met:
   *
   * 1.  Redistributions of source code must retain the above copyright
   *     notice, this list of conditions and the following disclaimer.
   * 2.  Redistributions in binary form must reproduce the above copyright
   *     notice, this list of conditions and the following disclaimer in the
   *     documentation and/or other materials provided with the distribution.
   * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
   *     its contributors may be used to endorse or promote products derived
   *     from this software without specific prior written permission.
   *
   * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
   * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
   * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
   * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   */

  /**
   * @param {string} string
   * @param {...*} vararg
   * @return {string}
   */
  function UIString(string, ...vararg) {
    return vsprintf(localize(string), Array.prototype.slice.call(arguments, 1));
  }

  /**
   * @param {string} string
   * @param {?ArrayLike<*>} values
   * @return {string}
   */
  function serializeUIString(string, values = []) {
    const messageParts = [string];
    const serializedMessage = {messageParts, values};
    return JSON.stringify(serializedMessage);
  }

  /**
   * @param {string=} serializedMessage
   * @return {*}
   */
  function deserializeUIString(serializedMessage) {
    if (!serializedMessage) {
      return {};
    }

    return JSON.parse(serializedMessage);
  }

  /**
   * @param {string} string
   * @return {string}
   */
  function localize(string) {
    return string;
  }

  /**
   * @unrestricted
   */
  class UIStringFormat {
    /**
     * @param {string} format
     */
    constructor(format) {
      /** @type {string} */
      this._localizedFormat = localize(format);
      /** @type {!Array.<!StringUtilities.FORMATTER_TOKEN>} */
      this._tokenizedFormat =
          tokenizeFormatString(this._localizedFormat, standardFormatters);
    }

    /**
     * @param {string} a
     * @param {*} b
     * @return {string}
     */
    static _append(a, b) {
      return a + b;
    }

    /**
     * @param {...*} vararg
     * @return {string}
     */
    format(vararg) {
      // the code here uses odd generics that Closure likes but TS doesn't
      // so rather than fight to typecheck this in a dodgy way we just let TS ignore it
      // @ts-ignore
      return format(
              this._localizedFormat, arguments, standardFormatters, '', UIStringFormat._append,
              this._tokenizedFormat)
          .formattedResult;
    }
  }

  const _substitutionStrings = new WeakMap();

  /**
   * @param {!ITemplateArray|string} strings
   * @param {...*} vararg
   * @return {string}
   */
  function ls$1(strings, ...vararg) {
    if (typeof strings === 'string') {
      return strings;
    }
    let substitutionString = _substitutionStrings.get(strings);
    if (!substitutionString) {
      substitutionString = strings.join('%s');
      _substitutionStrings.set(strings, substitutionString);
    }
    // @ts-ignore TS gets confused with the arguments slicing
    return UIString(substitutionString, ...vararg);
  }

  var UIString$1 = /*#__PURE__*/Object.freeze({
    __proto__: null,
    UIString: UIString,
    serializeUIString: serializeUIString,
    deserializeUIString: deserializeUIString,
    localize: localize,
    UIStringFormat: UIStringFormat,
    ls: ls$1
  });

  /*
   * Copyright (C) 2019 Google Inc. All rights reserved.
   *
   * Redistribution and use in source and binary forms, with or without
   * modification, are permitted provided that the following conditions are
   * met:
   *
   *     * Redistributions of source code must retain the above copyright
   * notice, this list of conditions and the following disclaimer.
   *     * Redistributions in binary form must reproduce the above
   * copyright notice, this list of conditions and the following disclaimer
   * in the documentation and/or other materials provided with the
   * distribution.
   *     * Neither the name of Google Inc. nor the names of its
   * contributors may be used to endorse or promote products derived from
   * this software without specific prior written permission.
   *
   * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   */

  const ls = x => x;

  // Copyright 2020 The Chromium Authors
  // Use of this source code is governed by a BSD-style license that can be
  // found in the LICENSE file.

  /**
   * Combine the two given colors according to alpha blending.
   * @param {!Array<number>} fgRGBA
   * @param {!Array<number>} bgRGBA
   * @return {!Array<number>}
   */
  function blendColors(fgRGBA, bgRGBA) {
    const alpha = fgRGBA[3];
    return [
      ((1 - alpha) * bgRGBA[0]) + (alpha * fgRGBA[0]),
      ((1 - alpha) * bgRGBA[1]) + (alpha * fgRGBA[1]),
      ((1 - alpha) * bgRGBA[2]) + (alpha * fgRGBA[2]),
      alpha + (bgRGBA[3] * (1 - alpha)),
    ];
  }

  /**
   * @param {!Array<number>} rgba
   * @return {!Array<number>}
   */
  function rgbaToHsla([r, g, b, a]) {
    const max = Math.max(r, g, b);
    const min = Math.min(r, g, b);
    const diff = max - min;
    const sum = max + min;

    let h;
    if (min === max) {
      h = 0;
    } else if (r === max) {
      h = ((1 / 6 * (g - b) / diff) + 1) % 1;
    } else if (g === max) {
      h = (1 / 6 * (b - r) / diff) + 1 / 3;
    } else {
      h = (1 / 6 * (r - g) / diff) + 2 / 3;
    }

    const l = 0.5 * sum;

    let s;
    if (l === 0) {
      s = 0;
    } else if (l === 1) {
      s = 0;
    } else if (l <= 0.5) {
      s = diff / sum;
    } else {
      s = diff / (2 - sum);
    }

    return [h, s, l, a];
  }

  /**
  * Calculate the luminance of this color using the WCAG algorithm.
  * See http://www.w3.org/TR/2008/REC-WCAG20-20081211/#relativeluminancedef
  * @param {!Array<number>} rgba
  * @return {number}
  */
  function luminance([rSRGB, gSRGB, bSRGB]) {
    const r = rSRGB <= 0.03928 ? rSRGB / 12.92 : Math.pow(((rSRGB + 0.055) / 1.055), 2.4);
    const g = gSRGB <= 0.03928 ? gSRGB / 12.92 : Math.pow(((gSRGB + 0.055) / 1.055), 2.4);
    const b = bSRGB <= 0.03928 ? bSRGB / 12.92 : Math.pow(((bSRGB + 0.055) / 1.055), 2.4);

    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
  }

  /*
   * Copyright (C) 2009 Apple Inc.  All rights reserved.
   * Copyright (C) 2009 Joseph Pecoraro
   *
   * Redistribution and use in source and binary forms, with or without
   * modification, are permitted provided that the following conditions
   * are met:
   *
   * 1.  Redistributions of source code must retain the above copyright
   *     notice, this list of conditions and the following disclaimer.
   * 2.  Redistributions in binary form must reproduce the above copyright
   *     notice, this list of conditions and the following disclaimer in the
   *     documentation and/or other materials provided with the distribution.
   * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
   *     its contributors may be used to endorse or promote products derived
   *     from this software without specific prior written permission.
   *
   * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
   * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
   * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
   * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   */

  /** @type {?Map<string, string>} */
  let _rgbaToNickname;

  /**
   * @unrestricted
   */
  class Color {
    /**
     * @param {!Array.<number>} rgba
     * @param {!Format} format
     * @param {string=} originalText
     */
    constructor(rgba, format, originalText) {
      this._hsla = undefined;
      this._rgba = rgba;
      this._originalText = originalText || null;
      this._originalTextIsValid = !!this._originalText;
      this._format = format;
      if (typeof this._rgba[3] === 'undefined') {
        this._rgba[3] = 1;
      }

      for (let i = 0; i < 4; ++i) {
        if (this._rgba[i] < 0) {
          this._rgba[i] = 0;
          this._originalTextIsValid = false;
        }
        if (this._rgba[i] > 1) {
          this._rgba[i] = 1;
          this._originalTextIsValid = false;
        }
      }
    }

    /**
     * @param {string} text
     * @return {?Color}
     */
    static parse(text) {
      // Simple - #hex, nickname
      const value = text.toLowerCase().replace(/\s+/g, '');
      const simple = /^(?:#([0-9a-f]{3,4}|[0-9a-f]{6}|[0-9a-f]{8})|(\w+))$/i;
      let match = value.match(simple);
      if (match) {
        if (match[1]) {  // hex
          let hex = match[1].toLowerCase();
          let format;
          if (hex.length === 3) {
            format = Format.ShortHEX;
            hex = hex.charAt(0) + hex.charAt(0) + hex.charAt(1) + hex.charAt(1) + hex.charAt(2) + hex.charAt(2);
          } else if (hex.length === 4) {
            format = Format.ShortHEXA;
            hex = hex.charAt(0) + hex.charAt(0) + hex.charAt(1) + hex.charAt(1) + hex.charAt(2) + hex.charAt(2) +
                hex.charAt(3) + hex.charAt(3);
          } else if (hex.length === 6) {
            format = Format.HEX;
          } else {
            format = Format.HEXA;
          }
          const r = parseInt(hex.substring(0, 2), 16);
          const g = parseInt(hex.substring(2, 4), 16);
          const b = parseInt(hex.substring(4, 6), 16);
          let a = 1;
          if (hex.length === 8) {
            a = parseInt(hex.substring(6, 8), 16) / 255;
          }
          return new Color([r / 255, g / 255, b / 255, a], format, text);
        }

        if (match[2]) {  // nickname
          const nickname = match[2].toLowerCase();
          if (nickname in Nicknames) {
            const rgba = Nicknames[nickname];
            const color = Color.fromRGBA(rgba);
            color._format = Format.Nickname;
            color._originalText = text;
            return color;
          }
          return null;
        }

        return null;
      }

      // rgb/rgba(), hsl/hsla()
      match = text.toLowerCase().match(/^\s*(?:(rgba?)|(hsla?))\((.*)\)\s*$/);

      if (match) {
        const components = match[3].trim();
        let values = components.split(/\s*,\s*/);
        if (values.length === 1) {
          values = components.split(/\s+/);
          if (values[3] === '/') {
            values.splice(3, 1);
            if (values.length !== 4) {
              return null;
            }
          } else if ((values.length > 2 && values[2].indexOf('/') !== -1) || (values.length > 3 && values[3].indexOf('/') !== -1)) {
            const alpha = values.slice(2, 4).join('');
            values = values.slice(0, 2).concat(alpha.split(/\//)).concat(values.slice(4));
          } else if (values.length >= 4) {
            return null;
          }
        }
        if (values.length !== 3 && values.length !== 4 || values.indexOf('') > -1) {
          return null;
        }
        const hasAlpha = (values[3] !== undefined);

        if (match[1]) {  // rgb/rgba
          const rgba = [
            Color._parseRgbNumeric(values[0]), Color._parseRgbNumeric(values[1]), Color._parseRgbNumeric(values[2]),
            hasAlpha ? Color._parseAlphaNumeric(values[3]) : 1
          ];
          if (rgba.indexOf(null) > -1) {
            return null;
          }
          return new Color(/** @type {!Array.<number>} */ (rgba), hasAlpha ? Format.RGBA : Format.RGB, text);
        }

        if (match[2]) {  // hsl/hsla
          const hsla = [
            Color._parseHueNumeric(values[0]), Color._parseSatLightNumeric(values[1]),
            Color._parseSatLightNumeric(values[2]), hasAlpha ? Color._parseAlphaNumeric(values[3]) : 1
          ];
          if (hsla.indexOf(null) > -1) {
            return null;
          }
          /** @type {!Array.<number>} */
          const rgba = [];
          Color.hsl2rgb(/** @type {!Array.<number>} */ (hsla), rgba);
          return new Color(rgba, hasAlpha ? Format.HSLA : Format.HSL, text);
        }
      }

      return null;
    }

    /**
     * @param {!Array.<number>} rgba
     * @return {!Color}
     */
    static fromRGBA(rgba) {
      return new Color([rgba[0] / 255, rgba[1] / 255, rgba[2] / 255, rgba[3]], Format.RGBA);
    }

    /**
     * @param {!Array.<number>} hsva
     * @return {!Color}
     */
    static fromHSVA(hsva) {
      /** @type {!Array.<number>} */
      const rgba = [];
      Color.hsva2rgba(hsva, rgba);
      return new Color(rgba, Format.HSLA);
    }

    /**
     * @param {string} value
     * @return {number|null}
     */
    static _parsePercentOrNumber(value) {
      // @ts-ignore: isNaN can accept strings
      if (isNaN(value.replace('%', ''))) {
        return null;
      }
      const parsed = parseFloat(value);

      if (value.indexOf('%') !== -1) {
        if (value.indexOf('%') !== value.length - 1) {
          return null;
        }
        return parsed / 100;
      }
      return parsed;
    }

    /**
     * @param {string} value
     * @return {number|null}
     */
    static _parseRgbNumeric(value) {
      const parsed = Color._parsePercentOrNumber(value);
      if (parsed === null) {
        return null;
      }

      if (value.indexOf('%') !== -1) {
        return parsed;
      }
      return parsed / 255;
    }

    /**
     * @param {string} value
     * @return {number|null}
     */
    static _parseHueNumeric(value) {
      const angle = value.replace(/(deg|g?rad|turn)$/, '');
      // @ts-ignore: isNaN can accept strings
      if (isNaN(angle) || value.match(/\s+(deg|g?rad|turn)/)) {
        return null;
      }
      const number = parseFloat(angle);

      if (value.indexOf('turn') !== -1) {
        return number % 1;
      }
      if (value.indexOf('grad') !== -1) {
        return (number / 400) % 1;
      }
      if (value.indexOf('rad') !== -1) {
        return (number / (2 * Math.PI)) % 1;
      }
      return (number / 360) % 1;
    }

    /**
     * @param {string} value
     * @return {number|null}
     */
    static _parseSatLightNumeric(value) {
      // @ts-ignore: isNaN can accept strings
      if (value.indexOf('%') !== value.length - 1 || isNaN(value.replace('%', ''))) {
        return null;
      }
      const parsed = parseFloat(value);
      return Math.min(1, parsed / 100);
    }

    /**
     * @param {string} value
     * @return {number|null}
     */
    static _parseAlphaNumeric(value) {
      return Color._parsePercentOrNumber(value);
    }

    /**
     * @param {!Array.<number>} hsva
     * @param {!Array.<number>} out_hsla
     */
    static _hsva2hsla(hsva, out_hsla) {
      const h = hsva[0];
      let s = hsva[1];
      const v = hsva[2];

      const t = (2 - s) * v;
      if (v === 0 || s === 0) {
        s = 0;
      } else {
        s *= v / (t < 1 ? t : 2 - t);
      }

      out_hsla[0] = h;
      out_hsla[1] = s;
      out_hsla[2] = t / 2;
      out_hsla[3] = hsva[3];
    }

    /**
     * @param {!Array.<number>} hsl
     * @param {!Array.<number>} out_rgb
     */
    static hsl2rgb(hsl, out_rgb) {
      const h = hsl[0];
      let s = hsl[1];
      const l = hsl[2];

      /**
       * @param {number} p
       * @param {number} q
       * @param {number} h
       */
      function hue2rgb(p, q, h) {
        if (h < 0) {
          h += 1;
        } else if (h > 1) {
          h -= 1;
        }

        if ((h * 6) < 1) {
          return p + (q - p) * h * 6;
        }
        if ((h * 2) < 1) {
          return q;
        }
        if ((h * 3) < 2) {
          return p + (q - p) * ((2 / 3) - h) * 6;
        }
        return p;
      }

      if (s < 0) {
        s = 0;
      }

      let q;
      if (l <= 0.5) {
        q = l * (1 + s);
      } else {
        q = l + s - (l * s);
      }

      const p = 2 * l - q;

      const tr = h + (1 / 3);
      const tg = h;
      const tb = h - (1 / 3);

      out_rgb[0] = hue2rgb(p, q, tr);
      out_rgb[1] = hue2rgb(p, q, tg);
      out_rgb[2] = hue2rgb(p, q, tb);
      out_rgb[3] = hsl[3];
    }

    /**
     * @param {!Array<number>} hsva
     * @param {!Array<number>} out_rgba
     */
    static hsva2rgba(hsva, out_rgba) {
      Color._hsva2hsla(hsva, _tmpHSLA);
      Color.hsl2rgb(_tmpHSLA, out_rgba);

      for (let i = 0; i < _tmpHSLA.length; i++) {
        _tmpHSLA[i] = 0;
      }
    }

    /**
     * Compute a desired luminance given a given luminance and a desired contrast
     * ratio.
     * @param {number} luminance The given luminance.
     * @param {number} contrast The desired contrast ratio.
     * @param {boolean} lighter Whether the desired luminance is lighter or darker
     * than the given luminance. If no luminance can be found which meets this
     * requirement, a luminance which meets the inverse requirement will be
     * returned.
     * @return {number} The desired luminance.
     */
    static desiredLuminance(luminance, contrast, lighter) {
      function computeLuminance() {
        if (lighter) {
          return (luminance + 0.05) * contrast - 0.05;
        }
        return (luminance + 0.05) / contrast - 0.05;
      }
      let desiredLuminance = computeLuminance();
      if (desiredLuminance < 0 || desiredLuminance > 1) {
        lighter = !lighter;
        desiredLuminance = computeLuminance();
      }
      return desiredLuminance;
    }

    /**
     * Approach a value of the given component of `candidateHSVA` such that the
     * calculated luminance of `candidateHSVA` approximates `desiredLuminance`.
     * @param {!Array<number>} candidateHSVA
     * @param {!Array<number>} bgRGBA
     * @param {number} index - the index of the color component
     * @param {number} desiredLuminance
     * @return {?number} The new value for the modified component, or `null` if
     *     no suitable value exists.
     */
    static approachColorValue(candidateHSVA, bgRGBA, index, desiredLuminance) {
      const candidateLuminance = () => {
        return luminance(blendColors(Color.fromHSVA(candidateHSVA).rgba(), bgRGBA));
      };

      const epsilon = 0.0002;

      let x = candidateHSVA[index];
      let multiplier = 1;
      let dLuminance = candidateLuminance() - desiredLuminance;
      let previousSign = Math.sign(dLuminance);

      for (let guard = 100; guard; guard--) {
        if (Math.abs(dLuminance) < epsilon) {
          candidateHSVA[index] = x;
          return x;
        }

        const sign = Math.sign(dLuminance);
        if (sign !== previousSign) {
          // If `x` overshoots the correct value, halve the step size.
          multiplier /= 2;
          previousSign = sign;
        } else if (x < 0 || x > 1) {
          // If there is no overshoot and `x` is out of bounds, there is no
          // acceptable value for `x`.
          return null;
        }

        // Adjust `x` by a multiple of `dLuminance` to decrease step size as
        // the computed luminance converges on `desiredLuminance`.
        x += multiplier * (index === 2 ? -dLuminance : dLuminance);

        candidateHSVA[index] = x;

        dLuminance = candidateLuminance() - desiredLuminance;
      }

      // The loop should always converge or go out of bounds on its own.
      console.error('Loop exited unexpectedly');
      return null;
    }

    /**
     *
     * @param {!Color} fgColor
     * @param {!Color} bgColor
     * @param {number} requiredContrast
     * @return {?Color}
     */
    static findFgColorForContrast(fgColor, bgColor, requiredContrast) {
      const candidateHSVA = fgColor.hsva();
      const bgRGBA = bgColor.rgba();

      const candidateLuminance = () => {
        return luminance(blendColors(Color.fromHSVA(candidateHSVA).rgba(), bgRGBA));
      };

      const bgLuminance = luminance(bgColor.rgba());
      const fgLuminance = candidateLuminance();
      const fgIsLighter = fgLuminance > bgLuminance;

      const desiredLuminance = Color.desiredLuminance(bgLuminance, requiredContrast, fgIsLighter);

      const saturationComponentIndex = 1;
      const valueComponentIndex = 2;

      if (Color.approachColorValue(candidateHSVA, bgRGBA, valueComponentIndex, desiredLuminance)) {
        return Color.fromHSVA(candidateHSVA);
      }

      candidateHSVA[valueComponentIndex] = 1;
      if (Color.approachColorValue(candidateHSVA, bgRGBA, saturationComponentIndex, desiredLuminance)) {
        return Color.fromHSVA(candidateHSVA);
      }

      return null;
    }

    /**
     * @return {!Format}
     */
    format() {
      return this._format;
    }

    /**
     * @return {!Array.<number>} HSLA with components within [0..1]
     */
    hsla() {
      if (this._hsla) {
        return this._hsla;
      }
      this._hsla = rgbaToHsla(this._rgba);
      return this._hsla;
    }

    /**
     * @return {!Array.<number>}
     */
    canonicalHSLA() {
      const hsla = this.hsla();
      return [Math.round(hsla[0] * 360), Math.round(hsla[1] * 100), Math.round(hsla[2] * 100), hsla[3]];
    }

    /**
     * @return {!Array.<number>} HSVA with components within [0..1]
     */
    hsva() {
      const hsla = this.hsla();
      const h = hsla[0];
      let s = hsla[1];
      const l = hsla[2];

      s *= l < 0.5 ? l : 1 - l;
      return [h, s !== 0 ? 2 * s / (l + s) : 0, (l + s), hsla[3]];
    }

    /**
     * @return {boolean}
     */
    hasAlpha() {
      return this._rgba[3] !== 1;
    }

    /**
     * @return {!Format}
     */
    detectHEXFormat() {
      let canBeShort = true;
      for (let i = 0; i < 4; ++i) {
        const c = Math.round(this._rgba[i] * 255);
        if (c % 17) {
          canBeShort = false;
          break;
        }
      }

      const hasAlpha = this.hasAlpha();
      const cf = Format;
      if (canBeShort) {
        return hasAlpha ? cf.ShortHEXA : cf.ShortHEX;
      }
      return hasAlpha ? cf.HEXA : cf.HEX;
    }

    /**
     * @param {?string=} format
     * @return {?string}
     */
    asString(format) {
      if (format === this._format && this._originalTextIsValid) {
        return this._originalText;
      }

      if (!format) {
        format = this._format;
      }

      /**
       * @param {number} value
       * @return {number}
       */
      function toRgbValue(value) {
        return Math.round(value * 255);
      }

      /**
       * @param {number} value
       * @return {string}
       */
      function toHexValue(value) {
        const hex = Math.round(value * 255).toString(16);
        return hex.length === 1 ? '0' + hex : hex;
      }

      /**
       * @param {number} value
       * @return {string}
       */
      function toShortHexValue(value) {
        return (Math.round(value * 255) / 17).toString(16);
      }

      switch (format) {
        case Format.Original: {
          return this._originalText;
        }
        case Format.RGB:
        case Format.RGBA: {
          const start = sprintf(
              'rgb(%d %d %d', toRgbValue(this._rgba[0]), toRgbValue(this._rgba[1]), toRgbValue(this._rgba[2]));
          if (this.hasAlpha()) {
            return start + sprintf(' / %d%)', Math.round(this._rgba[3] * 100));
          }
          return start + ')';
        }
        case Format.HSL:
        case Format.HSLA: {
          const hsla = this.hsla();
          const start = sprintf(
              'hsl(%ddeg %d% %d%', Math.round(hsla[0] * 360), Math.round(hsla[1] * 100), Math.round(hsla[2] * 100));
          if (this.hasAlpha()) {
            return start + sprintf(' / %d%)', Math.round(hsla[3] * 100));
          }
          return start + ')';
        }
        case Format.HEXA: {
          return sprintf(
                  '#%s%s%s%s', toHexValue(this._rgba[0]), toHexValue(this._rgba[1]), toHexValue(this._rgba[2]),
                  toHexValue(this._rgba[3]))
              .toLowerCase();
        }
        case Format.HEX: {
          if (this.hasAlpha()) {
            return null;
          }
          return sprintf('#%s%s%s', toHexValue(this._rgba[0]), toHexValue(this._rgba[1]), toHexValue(this._rgba[2]))
              .toLowerCase();
        }
        case Format.ShortHEXA: {
          const hexFormat = this.detectHEXFormat();
          if (hexFormat !== Format.ShortHEXA && hexFormat !== Format.ShortHEX) {
            return null;
          }
          return sprintf(
                  '#%s%s%s%s', toShortHexValue(this._rgba[0]), toShortHexValue(this._rgba[1]),
                  toShortHexValue(this._rgba[2]), toShortHexValue(this._rgba[3]))
              .toLowerCase();
        }
        case Format.ShortHEX: {
          if (this.hasAlpha()) {
            return null;
          }
          if (this.detectHEXFormat() !== Format.ShortHEX) {
            return null;
          }
          return sprintf(
                  '#%s%s%s', toShortHexValue(this._rgba[0]), toShortHexValue(this._rgba[1]),
                  toShortHexValue(this._rgba[2]))
              .toLowerCase();
        }
        case Format.Nickname: {
          return this.nickname();
        }
      }

      return this._originalText;
    }

    /**
     * @return {!Array<number>}
     */
    rgba() {
      return this._rgba.slice();
    }

    /**
     * @return {!Array.<number>}
     */
    canonicalRGBA() {
      const rgba = new Array(4);
      for (let i = 0; i < 3; ++i) {
        rgba[i] = Math.round(this._rgba[i] * 255);
      }
      rgba[3] = this._rgba[3];
      return rgba;
    }

    /**
     * @return {?string} nickname
     */
    nickname() {
      if (!_rgbaToNickname) {
        _rgbaToNickname = new Map();
        for (const nickname in Nicknames) {
          let rgba = Nicknames[nickname];
          if (rgba.length !== 4) {
            rgba = rgba.concat(1);
          }
          _rgbaToNickname.set(String(rgba), nickname);
        }
      }

      return _rgbaToNickname.get(String(this.canonicalRGBA())) || null;
    }

    /**
     * @return {!{r: number, g: number, b: number, a: (number|undefined)}}
     */
    toProtocolRGBA() {
      const rgba = this.canonicalRGBA();
      /** @type {!{r: number, g: number, b: number, a: (number|undefined)}} */
      const result = {r: rgba[0], g: rgba[1], b: rgba[2], a: undefined};
      if (rgba[3] !== 1) {
        result.a = rgba[3];
      }
      return result;
    }

    /**
     * @return {!Color}
     */
    invert() {
      const rgba = [];
      rgba[0] = 1 - this._rgba[0];
      rgba[1] = 1 - this._rgba[1];
      rgba[2] = 1 - this._rgba[2];
      rgba[3] = this._rgba[3];
      return new Color(rgba, Format.RGBA);
    }

    /**
     * @param {number} alpha
     * @return {!Color}
     */
    setAlpha(alpha) {
      const rgba = this._rgba.slice();
      rgba[3] = alpha;
      return new Color(rgba, Format.RGBA);
    }

    /**
     * @param {!Color} fgColor
     * @return {!Color}
     */
    blendWith(fgColor) {
      /** @type {!Array.<number>} */
      const rgba = blendColors(fgColor._rgba, this._rgba);
      return new Color(rgba, Format.RGBA);
    }

    /**
     * @param {!Format} format
     */
    setFormat(format) {
      this._format = format;
    }
  }

  /**
   * @enum {string}
   */
  const Format = {
    Original: 'original',
    Nickname: 'nickname',
    HEX: 'hex',
    ShortHEX: 'shorthex',
    HEXA: 'hexa',
    ShortHEXA: 'shorthexa',
    RGB: 'rgb',
    RGBA: 'rgba',
    HSL: 'hsl',
    HSLA: 'hsla'
  };

  /** @type {!Object<string, !Array.<number>>} */
  const Nicknames = {
    'aliceblue': [240, 248, 255],
    'antiquewhite': [250, 235, 215],
    'aqua': [0, 255, 255],
    'aquamarine': [127, 255, 212],
    'azure': [240, 255, 255],
    'beige': [245, 245, 220],
    'bisque': [255, 228, 196],
    'black': [0, 0, 0],
    'blanchedalmond': [255, 235, 205],
    'blue': [0, 0, 255],
    'blueviolet': [138, 43, 226],
    'brown': [165, 42, 42],
    'burlywood': [222, 184, 135],
    'cadetblue': [95, 158, 160],
    'chartreuse': [127, 255, 0],
    'chocolate': [210, 105, 30],
    'coral': [255, 127, 80],
    'cornflowerblue': [100, 149, 237],
    'cornsilk': [255, 248, 220],
    'crimson': [237, 20, 61],
    'cyan': [0, 255, 255],
    'darkblue': [0, 0, 139],
    'darkcyan': [0, 139, 139],
    'darkgoldenrod': [184, 134, 11],
    'darkgray': [169, 169, 169],
    'darkgrey': [169, 169, 169],
    'darkgreen': [0, 100, 0],
    'darkkhaki': [189, 183, 107],
    'darkmagenta': [139, 0, 139],
    'darkolivegreen': [85, 107, 47],
    'darkorange': [255, 140, 0],
    'darkorchid': [153, 50, 204],
    'darkred': [139, 0, 0],
    'darksalmon': [233, 150, 122],
    'darkseagreen': [143, 188, 143],
    'darkslateblue': [72, 61, 139],
    'darkslategray': [47, 79, 79],
    'darkslategrey': [47, 79, 79],
    'darkturquoise': [0, 206, 209],
    'darkviolet': [148, 0, 211],
    'deeppink': [255, 20, 147],
    'deepskyblue': [0, 191, 255],
    'dimgray': [105, 105, 105],
    'dimgrey': [105, 105, 105],
    'dodgerblue': [30, 144, 255],
    'firebrick': [178, 34, 34],
    'floralwhite': [255, 250, 240],
    'forestgreen': [34, 139, 34],
    'fuchsia': [255, 0, 255],
    'gainsboro': [220, 220, 220],
    'ghostwhite': [248, 248, 255],
    'gold': [255, 215, 0],
    'goldenrod': [218, 165, 32],
    'gray': [128, 128, 128],
    'grey': [128, 128, 128],
    'green': [0, 128, 0],
    'greenyellow': [173, 255, 47],
    'honeydew': [240, 255, 240],
    'hotpink': [255, 105, 180],
    'indianred': [205, 92, 92],
    'indigo': [75, 0, 130],
    'ivory': [255, 255, 240],
    'khaki': [240, 230, 140],
    'lavender': [230, 230, 250],
    'lavenderblush': [255, 240, 245],
    'lawngreen': [124, 252, 0],
    'lemonchiffon': [255, 250, 205],
    'lightblue': [173, 216, 230],
    'lightcoral': [240, 128, 128],
    'lightcyan': [224, 255, 255],
    'lightgoldenrodyellow': [250, 250, 210],
    'lightgreen': [144, 238, 144],
    'lightgray': [211, 211, 211],
    'lightgrey': [211, 211, 211],
    'lightpink': [255, 182, 193],
    'lightsalmon': [255, 160, 122],
    'lightseagreen': [32, 178, 170],
    'lightskyblue': [135, 206, 250],
    'lightslategray': [119, 136, 153],
    'lightslategrey': [119, 136, 153],
    'lightsteelblue': [176, 196, 222],
    'lightyellow': [255, 255, 224],
    'lime': [0, 255, 0],
    'limegreen': [50, 205, 50],
    'linen': [250, 240, 230],
    'magenta': [255, 0, 255],
    'maroon': [128, 0, 0],
    'mediumaquamarine': [102, 205, 170],
    'mediumblue': [0, 0, 205],
    'mediumorchid': [186, 85, 211],
    'mediumpurple': [147, 112, 219],
    'mediumseagreen': [60, 179, 113],
    'mediumslateblue': [123, 104, 238],
    'mediumspringgreen': [0, 250, 154],
    'mediumturquoise': [72, 209, 204],
    'mediumvioletred': [199, 21, 133],
    'midnightblue': [25, 25, 112],
    'mintcream': [245, 255, 250],
    'mistyrose': [255, 228, 225],
    'moccasin': [255, 228, 181],
    'navajowhite': [255, 222, 173],
    'navy': [0, 0, 128],
    'oldlace': [253, 245, 230],
    'olive': [128, 128, 0],
    'olivedrab': [107, 142, 35],
    'orange': [255, 165, 0],
    'orangered': [255, 69, 0],
    'orchid': [218, 112, 214],
    'palegoldenrod': [238, 232, 170],
    'palegreen': [152, 251, 152],
    'paleturquoise': [175, 238, 238],
    'palevioletred': [219, 112, 147],
    'papayawhip': [255, 239, 213],
    'peachpuff': [255, 218, 185],
    'peru': [205, 133, 63],
    'pink': [255, 192, 203],
    'plum': [221, 160, 221],
    'powderblue': [176, 224, 230],
    'purple': [128, 0, 128],
    'rebeccapurple': [102, 51, 153],
    'red': [255, 0, 0],
    'rosybrown': [188, 143, 143],
    'royalblue': [65, 105, 225],
    'saddlebrown': [139, 69, 19],
    'salmon': [250, 128, 114],
    'sandybrown': [244, 164, 96],
    'seagreen': [46, 139, 87],
    'seashell': [255, 245, 238],
    'sienna': [160, 82, 45],
    'silver': [192, 192, 192],
    'skyblue': [135, 206, 235],
    'slateblue': [106, 90, 205],
    'slategray': [112, 128, 144],
    'slategrey': [112, 128, 144],
    'snow': [255, 250, 250],
    'springgreen': [0, 255, 127],
    'steelblue': [70, 130, 180],
    'tan': [210, 180, 140],
    'teal': [0, 128, 128],
    'thistle': [216, 191, 216],
    'tomato': [255, 99, 71],
    'turquoise': [64, 224, 208],
    'violet': [238, 130, 238],
    'wheat': [245, 222, 179],
    'white': [255, 255, 255],
    'whitesmoke': [245, 245, 245],
    'yellow': [255, 255, 0],
    'yellowgreen': [154, 205, 50],
    'transparent': [0, 0, 0, 0],
  };

  const PageHighlight = {
    Content: Color.fromRGBA([111, 168, 220, .66]),
    ContentLight: Color.fromRGBA([111, 168, 220, .5]),
    ContentOutline: Color.fromRGBA([9, 83, 148]),
    Padding: Color.fromRGBA([147, 196, 125, .55]),
    PaddingLight: Color.fromRGBA([147, 196, 125, .4]),
    Border: Color.fromRGBA([255, 229, 153, .66]),
    BorderLight: Color.fromRGBA([255, 229, 153, .5]),
    Margin: Color.fromRGBA([246, 178, 107, .66]),
    MarginLight: Color.fromRGBA([246, 178, 107, .5]),
    EventTarget: Color.fromRGBA([255, 196, 196, .66]),
    Shape: Color.fromRGBA([96, 82, 177, 0.8]),
    ShapeMargin: Color.fromRGBA([96, 82, 127, .6]),
    CssGrid: Color.fromRGBA([0x4b, 0, 0x82, 1]),
    GridRowLine: Color.fromRGBA([127, 32, 210, 1]),
    GridColumnLine: Color.fromRGBA([127, 32, 210, 1]),
    GridBorder: Color.fromRGBA([127, 32, 210, 1]),
    GridRowGapBackground: Color.fromRGBA([127, 32, 210, .3]),
    GridColumnGapBackground: Color.fromRGBA([127, 32, 210, .3]),
    GridRowGapHatch: Color.fromRGBA([127, 32, 210, .8]),
    GridColumnGapHatch: Color.fromRGBA([127, 32, 210, .8]),
    GridAreaBorder: Color.fromRGBA([26, 115, 232, 1]),
  };

  const SourceOrderHighlight = {
    ParentOutline: Color.fromRGBA([224, 90, 183, 1]),
    ChildOutline: Color.fromRGBA([0, 120, 212, 1]),
  };

  const _tmpHSLA = [0, 0, 0, 0];

  /*
   * Copyright (C) 2012 Google Inc. All rights reserved.
   *
   * Redistribution and use in source and binary forms, with or without
   * modification, are permitted provided that the following conditions are
   * met:
   *
   *     * Redistributions of source code must retain the above copyright
   * notice, this list of conditions and the following disclaimer.
   *     * Redistributions in binary form must reproduce the above
   * copyright notice, this list of conditions and the following disclaimer
   * in the documentation and/or other materials provided with the
   * distribution.
   *     * Neither the name of Google Inc. nor the names of its
   * contributors may be used to endorse or promote products derived from
   * this software without specific prior written permission.
   *
   * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   */

  /**
   * @unrestricted
   */
  class ParsedURL {
    /**
     * @param {string} url
     */
    constructor(url) {
      this.isValid = false;
      this.url = url;
      this.scheme = '';
      this.user = '';
      this.host = '';
      this.port = '';
      this.path = '';
      this.queryParams = '';
      this.fragment = '';
      this.folderPathComponents = '';
      this.lastPathComponent = '';

      const isBlobUrl = this.url.startsWith('blob:');
      const urlToMatch = isBlobUrl ? url.substring(5) : url;
      const match = urlToMatch.match(ParsedURL._urlRegex());
      if (match) {
        this.isValid = true;
        if (isBlobUrl) {
          this._blobInnerScheme = match[2].toLowerCase();
          this.scheme = 'blob';
        } else {
          this.scheme = match[2].toLowerCase();
        }
        this.user = match[3];
        this.host = match[4];
        this.port = match[5];
        this.path = match[6] || '/';
        this.queryParams = match[7] || '';
        this.fragment = match[8];
      } else {
        if (this.url.startsWith('data:')) {
          this.scheme = 'data';
          return;
        }
        if (this.url.startsWith('blob:')) {
          this.scheme = 'blob';
          return;
        }
        if (this.url === 'about:blank') {
          this.scheme = 'about';
          return;
        }
        this.path = this.url;
      }

      const lastSlashIndex = this.path.lastIndexOf('/');
      if (lastSlashIndex !== -1) {
        this.folderPathComponents = this.path.substring(0, lastSlashIndex);
        this.lastPathComponent = this.path.substring(lastSlashIndex + 1);
      } else {
        this.lastPathComponent = this.path;
      }
    }

    /**
     * @param {string} string
     * @return {?ParsedURL}
     */
    static fromString(string) {
      const parsedURL = new ParsedURL(string.toString());
      if (parsedURL.isValid) {
        return parsedURL;
      }
      return null;
    }

    /**
     * @param {string} fileSystemPath
     * @return {string}
     */
    static platformPathToURL(fileSystemPath) {
      fileSystemPath = fileSystemPath.replace(/\\/g, '/');
      if (!fileSystemPath.startsWith('file://')) {
        if (fileSystemPath.startsWith('/')) {
          fileSystemPath = 'file://' + fileSystemPath;
        } else {
          fileSystemPath = 'file:///' + fileSystemPath;
        }
      }
      return fileSystemPath;
    }

    /**
     * @param {string} fileURL
     * @param {boolean=} isWindows
     * @return {string}
     */
    static urlToPlatformPath(fileURL, isWindows) {
      console.assert(fileURL.startsWith('file://'), 'This must be a file URL.');
      if (isWindows) {
        return fileURL.substr('file:///'.length).replace(/\//g, '\\');
      }
      return fileURL.substr('file://'.length);
    }

    /**
     * @param {string} url
     * @return {string}
     */
    static urlWithoutHash(url) {
      const hashIndex = url.indexOf('#');
      if (hashIndex !== -1) {
        return url.substr(0, hashIndex);
      }
      return url;
    }

    /**
     * @return {!RegExp}
     */
    static _urlRegex() {
      if (ParsedURL._urlRegexInstance) {
        return ParsedURL._urlRegexInstance;
      }
      // RegExp groups:
      // 1 - scheme, hostname, ?port
      // 2 - scheme (using the RFC3986 grammar)
      // 3 - ?user:password
      // 4 - hostname
      // 5 - ?port
      // 6 - ?path
      // 7 - ?query
      // 8 - ?fragment
      const schemeRegex = /([A-Za-z][A-Za-z0-9+.-]*):\/\//;
      const userRegex = /(?:([A-Za-z0-9\-._~%!$&'()*+,;=:]*)@)?/;
      const hostRegex = /((?:\[::\d?\])|(?:[^\s\/:]*))/;
      const portRegex = /(?::([\d]+))?/;
      const pathRegex = /(\/[^#?]*)?/;
      const queryRegex = /(?:\?([^#]*))?/;
      const fragmentRegex = /(?:#(.*))?/;

      ParsedURL._urlRegexInstance = new RegExp(
          '^(' + schemeRegex.source + userRegex.source + hostRegex.source + portRegex.source + ')' + pathRegex.source +
          queryRegex.source + fragmentRegex.source + '$');
      return ParsedURL._urlRegexInstance;
    }

    /**
     * @param {string} url
     * @return {string}
     */
    static extractPath(url) {
      const parsedURL = this.fromString(url);
      return parsedURL ? parsedURL.path : '';
    }

    /**
     * @param {string} url
     * @return {string}
     */
    static extractOrigin(url) {
      const parsedURL = this.fromString(url);
      return parsedURL ? parsedURL.securityOrigin() : '';
    }

    /**
     * @param {string} url
     * @return {string}
     */
    static extractExtension(url) {
      url = ParsedURL.urlWithoutHash(url);
      const indexOfQuestionMark = url.indexOf('?');
      if (indexOfQuestionMark !== -1) {
        url = url.substr(0, indexOfQuestionMark);
      }
      const lastIndexOfSlash = url.lastIndexOf('/');
      if (lastIndexOfSlash !== -1) {
        url = url.substr(lastIndexOfSlash + 1);
      }
      const lastIndexOfDot = url.lastIndexOf('.');
      if (lastIndexOfDot !== -1) {
        url = url.substr(lastIndexOfDot + 1);
        const lastIndexOfPercent = url.indexOf('%');
        if (lastIndexOfPercent !== -1) {
          return url.substr(0, lastIndexOfPercent);
        }
        return url;
      }
      return '';
    }

    /**
     * @param {string} url
     * @return {string}
     */
    static extractName(url) {
      let index = url.lastIndexOf('/');
      const pathAndQuery = index !== -1 ? url.substr(index + 1) : url;
      index = pathAndQuery.indexOf('?');
      return index < 0 ? pathAndQuery : pathAndQuery.substr(0, index);
    }

    /**
     * @param {string} baseURL
     * @param {string} href
     * @return {?string}
     */
    static completeURL(baseURL, href) {
      // Return special URLs as-is.
      const trimmedHref = href.trim();
      if (trimmedHref.startsWith('data:') || trimmedHref.startsWith('blob:') || trimmedHref.startsWith('javascript:') ||
          trimmedHref.startsWith('mailto:')) {
        return href;
      }

      // Return absolute URLs as-is.
      const parsedHref = this.fromString(trimmedHref);
      if (parsedHref && parsedHref.scheme) {
        return trimmedHref;
      }

      const parsedURL = this.fromString(baseURL);
      if (!parsedURL) {
        return null;
      }

      if (parsedURL.isDataURL()) {
        return href;
      }

      if (href.length > 1 && href.charAt(0) === '/' && href.charAt(1) === '/') {
        // href starts with "//" which is a full URL with the protocol dropped (use the baseURL protocol).
        return parsedURL.scheme + ':' + href;
      }

      const securityOrigin = parsedURL.securityOrigin();
      const pathText = parsedURL.path;
      const queryText = parsedURL.queryParams ? '?' + parsedURL.queryParams : '';

      // Empty href resolves to a URL without fragment.
      if (!href.length) {
        return securityOrigin + pathText + queryText;
      }

      if (href.charAt(0) === '#') {
        return securityOrigin + pathText + queryText + href;
      }

      if (href.charAt(0) === '?') {
        return securityOrigin + pathText + href;
      }

      const hrefMatches = href.match(/^[^#?]*/);
      if (!hrefMatches || !href.length) {
        throw new Error('Invalid href');
      }
      let hrefPath = hrefMatches[0];
      const hrefSuffix = href.substring(hrefPath.length);
      if (hrefPath.charAt(0) !== '/') {
        hrefPath = parsedURL.folderPathComponents + '/' + hrefPath;
      }
      // @ts-ignore Runtime needs to be properly exported
      return securityOrigin + Root.Runtime.normalizePath(hrefPath) + hrefSuffix;
    }

    /**
     * @param {string} string
     * @return {!{url: string, lineNumber: (number|undefined), columnNumber: (number|undefined)}}
     */
    static splitLineAndColumn(string) {
      // Only look for line and column numbers in the path to avoid matching port numbers.
      const beforePathMatch = string.match(ParsedURL._urlRegex());
      let beforePath = '';
      let pathAndAfter = string;
      if (beforePathMatch) {
        beforePath = beforePathMatch[1];
        pathAndAfter = string.substring(beforePathMatch[1].length);
      }

      const lineColumnRegEx = /(?::(\d+))?(?::(\d+))?$/;
      const lineColumnMatch = lineColumnRegEx.exec(pathAndAfter);
      let lineNumber;
      let columnNumber;
      console.assert(!!lineColumnMatch);
      if (!lineColumnMatch) {
        return { url: string, lineNumber: 0, columnNumber: 0 };
      }

      if (typeof(lineColumnMatch[1]) === 'string') {
        lineNumber = parseInt(lineColumnMatch[1], 10);
        // Immediately convert line and column to 0-based numbers.
        lineNumber = isNaN(lineNumber) ? undefined : lineNumber - 1;
      }
      if (typeof(lineColumnMatch[2]) === 'string') {
        columnNumber = parseInt(lineColumnMatch[2], 10);
        columnNumber = isNaN(columnNumber) ? undefined : columnNumber - 1;
      }

      return {
        url: beforePath + pathAndAfter.substring(0, pathAndAfter.length - lineColumnMatch[0].length),
        lineNumber: lineNumber,
        columnNumber: columnNumber
      };
    }

    /**
     * @param {string} url
     * @return {string}
     */
    static removeWasmFunctionInfoFromURL(url) {
      const wasmFunctionRegEx = /:wasm-function\[\d+\]/;
      const wasmFunctionIndex = url.search(wasmFunctionRegEx);
      if (wasmFunctionIndex === -1) {
        return url;
      }
      return url.substring(0, wasmFunctionIndex);
    }

    /**
     * @param {string} url
     * @return {boolean}
     */
    static isRelativeURL(url) {
      return !(/^[A-Za-z][A-Za-z0-9+.-]*:/.test(url));
    }

    get displayName() {
      if (this._displayName) {
        return this._displayName;
      }

      if (this.isDataURL()) {
        return this.dataURLDisplayName();
      }
      if (this.isBlobURL()) {
        return this.url;
      }
      if (this.isAboutBlank()) {
        return this.url;
      }

      this._displayName = this.lastPathComponent;
      if (!this._displayName) {
        this._displayName = (this.host || '') + '/';
      }
      if (this._displayName === '/') {
        this._displayName = this.url;
      }
      return this._displayName;
    }

    /**
     * @return {string}
     */
    dataURLDisplayName() {
      if (this._dataURLDisplayName) {
        return this._dataURLDisplayName;
      }
      if (!this.isDataURL()) {
        return '';
      }
      this._dataURLDisplayName = this.url.trimEndWithMaxLength(20);
      return this._dataURLDisplayName;
    }

    /**
     * @return {boolean}
     */
    isAboutBlank() {
      return this.url === 'about:blank';
    }

    /**
     * @return {boolean}
     */
    isDataURL() {
      return this.scheme === 'data';
    }

    /**
     * @return {boolean}
     */
    isBlobURL() {
      return this.url.startsWith('blob:');
    }

    /**
     * @return {string}
     */
    lastPathComponentWithFragment() {
      return this.lastPathComponent + (this.fragment ? '#' + this.fragment : '');
    }

    /**
     * @return {string}
     */
    domain() {
      if (this.isDataURL()) {
        return 'data:';
      }
      return this.host + (this.port ? ':' + this.port : '');
    }

    /**
     * @return {string}
     */
    securityOrigin() {
      if (this.isDataURL()) {
        return 'data:';
      }
      const scheme = this.isBlobURL() ? this._blobInnerScheme : this.scheme;
      return scheme + '://' + this.domain();
    }

    /**
     * @return {string}
     */
    urlWithoutScheme() {
      if (this.scheme && this.url.startsWith(this.scheme + '://')) {
        return this.url.substring(this.scheme.length + 3);
      }
      return this.url;
    }
  }

  /** @type {?RegExp} */
  ParsedURL._urlRegexInstance = null;

  /*
   * Copyright (C) 2012 Google Inc.  All rights reserved.
   * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
   *
   * Redistribution and use in source and binary forms, with or without
   * modification, are permitted provided that the following conditions
   * are met:
   *
   * 1.  Redistributions of source code must retain the above copyright
   *     notice, this list of conditions and the following disclaimer.
   * 2.  Redistributions in binary form must reproduce the above copyright
   *     notice, this list of conditions and the following disclaimer in the
   *     documentation and/or other materials provided with the distribution.
   * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
   *     its contributors may be used to endorse or promote products derived
   *     from this software without specific prior written permission.
   *
   * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
   * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
   * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
   * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   */

  /**
   * @unrestricted
   */
  class ResourceType {
    /**
     * @param {string} name
     * @param {string} title
     * @param {!ResourceCategory} category
     * @param {boolean} isTextType
     */
    constructor(name, title, category, isTextType) {
      this._name = name;
      this._title = title;
      this._category = category;
      this._isTextType = isTextType;
    }

    /**
     * @param {?string} mimeType
     * @return {!ResourceType}
     */
    static fromMimeType(mimeType) {
      if (!mimeType) {
        return resourceTypes.Other;
      }
      if (mimeType.startsWith('text/html')) {
        return resourceTypes.Document;
      }
      if (mimeType.startsWith('text/css')) {
        return resourceTypes.Stylesheet;
      }
      if (mimeType.startsWith('image/')) {
        return resourceTypes.Image;
      }
      if (mimeType.startsWith('text/')) {
        return resourceTypes.Script;
      }

      if (mimeType.includes('font')) {
        return resourceTypes.Font;
      }
      if (mimeType.includes('script')) {
        return resourceTypes.Script;
      }
      if (mimeType.includes('octet')) {
        return resourceTypes.Other;
      }
      if (mimeType.includes('application')) {
        return resourceTypes.Script;
      }

      return resourceTypes.Other;
    }

    /**
     * @param {string} url
     * @return {?ResourceType}
     */
    static fromURL(url) {
      return _resourceTypeByExtension.get(ParsedURL.extractExtension(url)) || null;
    }

    /**
     * @param {string} name
     * @return {?ResourceType}
     */
    static fromName(name) {
      for (const resourceTypeId in resourceTypes) {
        const resourceType = /** @type {!Object<string, !ResourceType>} */(resourceTypes)[resourceTypeId];
        if (resourceType.name() === name) {
          return resourceType;
        }
      }
      return null;
    }

    /**
     * @param {string} url
     * @return {string|undefined}
     */
    static mimeFromURL(url) {
      const name = ParsedURL.extractName(url);
      if (_mimeTypeByName.has(name)) {
        return _mimeTypeByName.get(name);
      }

      const ext = ParsedURL.extractExtension(url).toLowerCase();
      return _mimeTypeByExtension.get(ext);
    }

    /**
     * @param {string} ext
     * @return {string|undefined}
     */
    static mimeFromExtension(ext) {
      return _mimeTypeByExtension.get(ext);
    }

    /**
     * @return {string}
     */
    name() {
      return this._name;
    }

    /**
     * @return {string}
     */
    title() {
      return this._title;
    }

    /**
     * @return {!ResourceCategory}
     */
    category() {
      return this._category;
    }

    /**
     * @return {boolean}
     */
    isTextType() {
      return this._isTextType;
    }

    /**
     * @return {boolean}
     */
    isScript() {
      return this._name === 'script' || this._name === 'sm-script';
    }

    /**
     * @return {boolean}
     */
    hasScripts() {
      return this.isScript() || this.isDocument();
    }

    /**
     * @return {boolean}
     */
    isStyleSheet() {
      return this._name === 'stylesheet' || this._name === 'sm-stylesheet';
    }

    /**
     * @return {boolean}
     */
    isDocument() {
      return this._name === 'document';
    }

    /**
     * @return {boolean}
     */
    isDocumentOrScriptOrStyleSheet() {
      return this.isDocument() || this.isScript() || this.isStyleSheet();
    }

    /**
     * @return {boolean}
     */
    isFromSourceMap() {
      return this._name.startsWith('sm-');
    }

    /**
     * @override
     * @return {string}
     */
    toString() {
      return this._name;
    }

    /**
     * @return {string}
     */
    canonicalMimeType() {
      if (this.isDocument()) {
        return 'text/html';
      }
      if (this.isScript()) {
        return 'text/javascript';
      }
      if (this.isStyleSheet()) {
        return 'text/css';
      }
      return '';
    }
  }

  /**
   * @unrestricted
   */
  class ResourceCategory {
    /**
     * @param {string} title
     * @param {string} shortTitle
     */
    constructor(title, shortTitle) {
      this.title = title;
      this.shortTitle = shortTitle;
    }
  }

  /**
   * @enum {!ResourceCategory}
   */
  const resourceCategories = {
    XHR: new ResourceCategory(ls`XHR and Fetch`, ls`XHR`),
    Script: new ResourceCategory(ls`Scripts`, ls`JS`),
    Stylesheet: new ResourceCategory(ls`Stylesheets`, ls`CSS`),
    Image: new ResourceCategory(ls`Images`, ls`Img`),
    Media: new ResourceCategory(ls`Media`, ls`Media`),
    Font: new ResourceCategory(ls`Fonts`, ls`Font`),
    Document: new ResourceCategory(ls`Documents`, ls`Doc`),
    WebSocket: new ResourceCategory(ls`WebSockets`, ls`WS`),
    Manifest: new ResourceCategory(ls`Manifest`, ls`Manifest`),
    Other: new ResourceCategory(ls`Other`, ls`Other`),
  };

  /**
   * Keep these in sync with WebCore::InspectorPageAgent::resourceTypeJson
   * @enum {!ResourceType}
   */
  const resourceTypes = {
    Document: new ResourceType('document', ls`Document`, resourceCategories.Document, true),
    Stylesheet: new ResourceType('stylesheet', ls`Stylesheet`, resourceCategories.Stylesheet, true),
    Image: new ResourceType('image', ls`Image`, resourceCategories.Image, false),
    Media: new ResourceType('media', ls`Media`, resourceCategories.Media, false),
    Font: new ResourceType('font', ls`Font`, resourceCategories.Font, false),
    Script: new ResourceType('script', ls`Script`, resourceCategories.Script, true),
    TextTrack: new ResourceType('texttrack', ls`TextTrack`, resourceCategories.Other, true),
    XHR: new ResourceType('xhr', ls`XHR`, resourceCategories.XHR, true),
    Fetch: new ResourceType('fetch', ls`Fetch`, resourceCategories.XHR, true),
    EventSource: new ResourceType('eventsource', ls`EventSource`, resourceCategories.XHR, true),
    WebSocket: new ResourceType('websocket', ls`WebSocket`, resourceCategories.WebSocket, false),
    Manifest: new ResourceType('manifest', ls`Manifest`, resourceCategories.Manifest, true),
    SignedExchange: new ResourceType('signed-exchange', ls`SignedExchange`, resourceCategories.Other, false),
    Ping: new ResourceType('ping', ls`Ping`, resourceCategories.Other, false),
    CSPViolationReport: new ResourceType('csp-violation-report', ls`CSPViolationReport`, resourceCategories.Other, false),
    Other: new ResourceType('other', ls`Other`, resourceCategories.Other, false),
    SourceMapScript: new ResourceType('sm-script', ls`Script`, resourceCategories.Script, true),
    SourceMapStyleSheet: new ResourceType('sm-stylesheet', ls`Stylesheet`, resourceCategories.Stylesheet, true),
  };


  const _mimeTypeByName = new Map([
    // CoffeeScript
    ['Cakefile', 'text/x-coffeescript']
  ]);

  const _resourceTypeByExtension = new Map([
    ['js', resourceTypes.Script], ['mjs', resourceTypes.Script],

    ['css', resourceTypes.Stylesheet], ['xsl', resourceTypes.Stylesheet],

    ['jpeg', resourceTypes.Image], ['jpg', resourceTypes.Image], ['svg', resourceTypes.Image],
    ['gif', resourceTypes.Image], ['png', resourceTypes.Image], ['ico', resourceTypes.Image],
    ['tiff', resourceTypes.Image], ['tif', resourceTypes.Image], ['bmp', resourceTypes.Image],

    ['webp', resourceTypes.Media],

    ['ttf', resourceTypes.Font], ['otf', resourceTypes.Font], ['ttc', resourceTypes.Font], ['woff', resourceTypes.Font]
  ]);

  const _mimeTypeByExtension = new Map([
    // Web extensions
    ['js', 'text/javascript'], ['mjs', 'text/javascript'], ['css', 'text/css'], ['html', 'text/html'],
    ['htm', 'text/html'], ['xml', 'application/xml'], ['xsl', 'application/xml'],

    // HTML Embedded Scripts, ASP], JSP
    ['asp', 'application/x-aspx'], ['aspx', 'application/x-aspx'], ['jsp', 'application/x-jsp'],

    // C/C++
    ['c', 'text/x-c++src'], ['cc', 'text/x-c++src'], ['cpp', 'text/x-c++src'], ['h', 'text/x-c++src'],
    ['m', 'text/x-c++src'], ['mm', 'text/x-c++src'],

    // CoffeeScript
    ['coffee', 'text/x-coffeescript'],

    // Dart
    ['dart', 'text/javascript'],

    // TypeScript
    ['ts', 'text/typescript'], ['tsx', 'text/typescript-jsx'],

    // JSON
    ['json', 'application/json'], ['gyp', 'application/json'], ['gypi', 'application/json'],

    // C#
    ['cs', 'text/x-csharp'],

    // Java
    ['java', 'text/x-java'],

    // Less
    ['less', 'text/x-less'],

    // PHP
    ['php', 'text/x-php'], ['phtml', 'application/x-httpd-php'],

    // Python
    ['py', 'text/x-python'],

    // Shell
    ['sh', 'text/x-sh'],

    // SCSS
    ['scss', 'text/x-scss'],

    // Video Text Tracks.
    ['vtt', 'text/vtt'],

    // LiveScript
    ['ls', 'text/x-livescript'],

    // Markdown
    ['md', 'text/markdown'],

    // ClojureScript
    ['cljs', 'text/x-clojure'], ['cljc', 'text/x-clojure'], ['cljx', 'text/x-clojure'],

    // Stylus
    ['styl', 'text/x-styl'],

    // JSX
    ['jsx', 'text/jsx'],

    // Image
    ['jpeg', 'image/jpeg'], ['jpg', 'image/jpeg'], ['svg', 'image/svg+xml'], ['gif', 'image/gif'], ['webp', 'image/webp'],
    ['png', 'image/png'], ['ico', 'image/ico'], ['tiff', 'image/tiff'], ['tif', 'image/tif'], ['bmp', 'image/bmp'],

    // Font
    ['ttf', 'font/opentype'], ['otf', 'font/opentype'], ['ttc', 'font/opentype'], ['woff', 'application/font-woff']
  ]);

  // Copyright 2020 The Chromium Authors
  // Use of this source code is governed by a BSD-style license that can be
  // found in the LICENSE file.

  const REMOTE_MODULE_FALLBACK_REVISION = '@9c7912d3335c02d62f63be2749d84b2d0b788982';
  const instanceSymbol = Symbol('instance');

  const originalConsole = console;
  const originalAssert = console.assert;

  /** @type {!URLSearchParams} */
  const queryParamsObject = new URLSearchParams(location.search);

  // The following two variables are initialized all the way at the bottom of this file
  /** @type {?string} */
  let remoteBase;
  /** @type {string} */
  let importScriptPathPrefix;

  let runtimePlatform = '';

  /** @type {function(string):string} */
  let l10nCallback;

  /** @type {!Runtime} */
  let runtimeInstance;

  /**
   * @unrestricted
   */
  class Runtime {
    /**
     * @private
     * @param {!Array.<!ModuleDescriptor>} descriptors
     */
    constructor(descriptors) {
      /** @type {!Array<!Module>} */
      this._modules = [];
      /** @type {!Object<string, !Module>} */
      this._modulesMap = {};
      /** @type {!Array<!Extension>} */
      this._extensions = [];
      /** @type {!Object<string, function(new:Object):void>} */
      this._cachedTypeClasses = {};
      /** @type {!Object<string, !ModuleDescriptor>} */
      this._descriptorsMap = {};

      for (let i = 0; i < descriptors.length; ++i) {
        this._registerModule(descriptors[i]);
      }
    }

    /**
     * @param {{forceNew: ?boolean, moduleDescriptors: ?Array.<!ModuleDescriptor>}=} opts
     * @return {!Runtime}
     */
    static instance(opts = {forceNew: null, moduleDescriptors: null}) {
      const {forceNew, moduleDescriptors} = opts;
      if (!moduleDescriptors || forceNew) {
        if (!moduleDescriptors) {
          throw new Error(
              `Unable to create settings: targetManager and workspace must be provided: ${new Error().stack}`);
        }

        runtimeInstance = new Runtime(moduleDescriptors);
      }

      return runtimeInstance;
    }

    /**
     * @param {string} url
     * @return {!Promise.<!ArrayBuffer>}
     */
    loadBinaryResourcePromise(url) {
      return internalLoadResourcePromise(url, true);
    }

    /**
     * http://tools.ietf.org/html/rfc3986#section-5.2.4
     * @param {string} path
     * @return {string}
     */
    static normalizePath(path) {
      if (path.indexOf('..') === -1 && path.indexOf('.') === -1) {
        return path;
      }

      const normalizedSegments = [];
      const segments = path.split('/');
      for (let i = 0; i < segments.length; i++) {
        const segment = segments[i];
        if (segment === '.') {
          continue;
        } else if (segment === '..') {
          normalizedSegments.pop();
        } else if (segment) {
          normalizedSegments.push(segment);
        }
      }
      let normalizedPath = normalizedSegments.join('/');
      if (normalizedPath[normalizedPath.length - 1] === '/') {
        return normalizedPath;
      }
      if (path[0] === '/' && normalizedPath) {
        normalizedPath = '/' + normalizedPath;
      }
      if ((path[path.length - 1] === '/') || (segments[segments.length - 1] === '.') ||
          (segments[segments.length - 1] === '..')) {
        normalizedPath = normalizedPath + '/';
      }

      return normalizedPath;
    }

    /**
     * @param {string} name
     * @return {?string}
     */
    static queryParam(name) {
      return queryParamsObject.get(name);
    }

    /**
     * @return {!Object<string,boolean>}
     */
    static _experimentsSetting() {
      try {
        return /** @type {!Object<string,boolean>} */ (
            JSON.parse(self.localStorage && self.localStorage['experiments'] ? self.localStorage['experiments'] : '{}'));
      } catch (e) {
        console.error('Failed to parse localStorage[\'experiments\']');
        return {};
      }
    }

    /**
     * @param {*} value
     * @param {string} message
     */
    static _assert(value, message) {
      if (value) {
        return;
      }
      originalAssert.call(originalConsole, value, message + ' ' + new Error().stack);
    }

    /**
     * @param {string} platform
     */
    static setPlatform(platform) {
      runtimePlatform = platform;
    }

    /**
     * @param {!ModuleDescriptor|!RuntimeExtensionDescriptor} descriptor
     * @return {boolean}
     */
    static _isDescriptorEnabled(descriptor) {
      const activatorExperiment = descriptor['experiment'];
      if (activatorExperiment === '*') {
        return true;
      }
      if (activatorExperiment && activatorExperiment.startsWith('!') &&
          experiments.isEnabled(activatorExperiment.substring(1))) {
        return false;
      }
      if (activatorExperiment && !activatorExperiment.startsWith('!') && !experiments.isEnabled(activatorExperiment)) {
        return false;
      }
      const condition = descriptor['condition'];
      if (condition && !condition.startsWith('!') && !Runtime.queryParam(condition)) {
        return false;
      }
      if (condition && condition.startsWith('!') && Runtime.queryParam(condition.substring(1))) {
        return false;
      }
      return true;
    }

    /**
     * @param {string} path
     * @return {string}
     */
    static resolveSourceURL(path) {
      let sourceURL = self.location.href;
      if (self.location.search) {
        sourceURL = sourceURL.replace(self.location.search, '');
      }
      sourceURL = sourceURL.substring(0, sourceURL.lastIndexOf('/') + 1) + path;
      return '\n/*# sourceURL=' + sourceURL + ' */';
    }

    /**
     * @param {function(string):string} localizationFunction
     */
    static setL10nCallback(localizationFunction) {
      l10nCallback = localizationFunction;
    }

    useTestBase() {
      remoteBase = 'http://localhost:8000/inspector-sources/';
      if (Runtime.queryParam('debugFrontend')) {
        remoteBase += 'debug/';
      }
    }

    /**
     * @param {string} moduleName
     * @return {!Module}
     */
    module(moduleName) {
      return this._modulesMap[moduleName];
    }

    /**
     * @param {!ModuleDescriptor} descriptor
     */
    _registerModule(descriptor) {
      const module = new Module(this, descriptor);
      this._modules.push(module);
      this._modulesMap[descriptor['name']] = module;
    }

    /**
     * @param {string} moduleName
     * @return {!Promise.<boolean>}
     */
    loadModulePromise(moduleName) {
      return this._modulesMap[moduleName]._loadPromise();
    }

    /**
     * @param {!Array.<string>} moduleNames
     * @return {!Promise.<!Array.<*>>}
     */
    loadAutoStartModules(moduleNames) {
      const promises = [];
      for (let i = 0; i < moduleNames.length; ++i) {
        promises.push(this.loadModulePromise(moduleNames[i]));
      }
      return Promise.all(promises);
    }

    /**
     * @param {!Extension} extension
     * @param {?function(function(new:Object)):boolean} predicate
     * @return {boolean}
     */
    _checkExtensionApplicability(extension, predicate) {
      if (!predicate) {
        return false;
      }
      const contextTypes = extension.descriptor().contextTypes;
      if (!contextTypes) {
        return true;
      }
      for (let i = 0; i < contextTypes.length; ++i) {
        const contextType = this._resolve(contextTypes[i]);
        const isMatching = !!contextType && predicate(contextType);
        if (isMatching) {
          return true;
        }
      }
      return false;
    }

    /**
     * @param {!Extension} extension
     * @param {?Object} context
     * @return {boolean}
     */
    isExtensionApplicableToContext(extension, context) {
      if (!context) {
        return true;
      }
      return this._checkExtensionApplicability(extension, isInstanceOf);

      /**
       * @param {!Function} targetType
       * @return {boolean}
       */
      function isInstanceOf(targetType) {
        return context instanceof targetType;
      }
    }

    /**
     * @param {!Extension} extension
     * @param {!Set.<function(new:Object, ...?):void>} currentContextTypes
     * @return {boolean}
     */
    isExtensionApplicableToContextTypes(extension, currentContextTypes) {
      if (!extension.descriptor().contextTypes) {
        return true;
      }

      let callback = null;

      if (currentContextTypes) {
        /**
         * @param {function(new:Object, ...?):void} targetType
         * @return {boolean}
         */
        callback = targetType => {
          return currentContextTypes.has(targetType);
        };
      }

      return this._checkExtensionApplicability(extension, callback);
    }

    /**
     * @param {*} type
     * @param {?Object=} context
     * @param {boolean=} sortByTitle
     * @return {!Array.<!Extension>}
     */
    extensions(type, context, sortByTitle) {
      return this._extensions.filter(filter).sort(sortByTitle ? titleComparator : orderComparator);

      /**
       * @param {!Extension} extension
       * @return {boolean}
       */
      function filter(extension) {
        if (extension._type !== type && extension._typeClass() !== type) {
          return false;
        }
        if (!extension.enabled()) {
          return false;
        }
        return !context || extension.isApplicable(context);
      }

      /**
       * @param {!Extension} extension1
       * @param {!Extension} extension2
       * @return {number}
       */
      function orderComparator(extension1, extension2) {
        const order1 = extension1.descriptor()['order'] || 0;
        const order2 = extension2.descriptor()['order'] || 0;
        return order1 - order2;
      }

      /**
       * @param {!Extension} extension1
       * @param {!Extension} extension2
       * @return {number}
       */
      function titleComparator(extension1, extension2) {
        const title1 = extension1.title() || '';
        const title2 = extension2.title() || '';
        return title1.localeCompare(title2);
      }
    }

    /**
     * @param {*} type
     * @param {?Object=} context
     * @return {?Extension}
     */
    extension(type, context) {
      return this.extensions(type, context)[0] || null;
    }

    /**
     * @param {*} type
     * @param {?Object=} context
     * @return {!Promise.<!Array.<!Object>>}
     */
    allInstances(type, context) {
      return Promise.all(this.extensions(type, context).map(extension => extension.instance()));
    }

    /**
     * @param {string} typeName
     * @return {?function(new:Object)}
     */
    _resolve(typeName) {
      if (!this._cachedTypeClasses[typeName]) {
        /** @type {!Array<string>} */
        const path = typeName.split('.');
        /** @type {*} */
        let object = self;
        for (let i = 0; object && (i < path.length); ++i) {
          object = object[path[i]];
        }
        if (object) {
          this._cachedTypeClasses[typeName] = /** @type {function(new:Object):void} */ (object);
        }
      }
      return this._cachedTypeClasses[typeName] || null;
    }

    /**
     * @param {function(new:T)} constructorFunction
     * @return {!T}
     * @template T
     */
    sharedInstance(constructorFunction) {
      if (instanceSymbol in constructorFunction &&
          Object.getOwnPropertySymbols(constructorFunction).includes(instanceSymbol)) {
        // @ts-ignore Usage of symbols
        return constructorFunction[instanceSymbol];
      }

      const instance = new constructorFunction();
      // @ts-ignore Usage of symbols
      constructorFunction[instanceSymbol] = instance;
      return instance;
    }
  }

  // Module namespaces.
  // NOTE: Update scripts/build/special_case_namespaces.json if you add a special cased namespace.
  /** @type {!Object<string,string>} */
  const specialCases = {
    'sdk': 'SDK',
    'js_sdk': 'JSSDK',
    'browser_sdk': 'BrowserSDK',
    'ui': 'UI',
    'object_ui': 'ObjectUI',
    'javascript_metadata': 'JavaScriptMetadata',
    'perf_ui': 'PerfUI',
    'har_importer': 'HARImporter',
    'sdk_test_runner': 'SDKTestRunner',
    'cpu_profiler_test_runner': 'CPUProfilerTestRunner'
  };

  /**
   * @unrestricted
   */
  class Module {
    /**
     * @param {!Runtime} manager
     * @param {!ModuleDescriptor} descriptor
     */
    constructor(manager, descriptor) {
      this._manager = manager;
      this._descriptor = descriptor;
      this._name = descriptor.name;
      /** @type {!Array<!Extension>} */
      this._extensions = [];

      /** @type {!Map<string, !Array<!Extension>>} */
      this._extensionsByClassName = new Map();
      const extensions = /** @type {?Array.<!RuntimeExtensionDescriptor>} */ (descriptor.extensions);
      for (let i = 0; extensions && i < extensions.length; ++i) {
        const extension = new Extension(this, extensions[i]);
        this._manager._extensions.push(extension);
        this._extensions.push(extension);
      }
      this._loadedForTest = false;
    }

    /**
     * @return {string}
     */
    name() {
      return this._name;
    }

    /**
     * @return {boolean}
     */
    enabled() {
      return Runtime._isDescriptorEnabled(this._descriptor);
    }

    /**
     * @param {string} name
     * @return {string}
     */
    resource(name) {
      const fullName = this._name + '/' + name;
      const content = self.Runtime.cachedResources[fullName];
      if (!content) {
        throw new Error(fullName + ' not preloaded. Check module.json');
      }
      return content;
    }

    /**
     * @return {!Promise.<boolean>}
     */
    _loadPromise() {
      if (!this.enabled()) {
        return Promise.reject(new Error('Module ' + this._name + ' is not enabled'));
      }

      if (this._pendingLoadPromise) {
        return this._pendingLoadPromise;
      }

      const dependencies = this._descriptor.dependencies;
      const dependencyPromises = [];
      for (let i = 0; dependencies && i < dependencies.length; ++i) {
        dependencyPromises.push(this._manager._modulesMap[dependencies[i]]._loadPromise());
      }

      this._pendingLoadPromise = Promise.all(dependencyPromises)
                                     .then(this._loadResources.bind(this))
                                     .then(this._loadModules.bind(this))
                                     .then(this._loadScripts.bind(this))
                                     .then(() => {
                                       this._loadedForTest = true;
                                       return this._loadedForTest;
                                     });

      return this._pendingLoadPromise;
    }

    /**
     * @return {!Promise.<void>}
     * @this {Module}
     */
    _loadResources() {
      const resources = this._descriptor['resources'];
      if (!resources || !resources.length) {
        return Promise.resolve();
      }
      const promises = [];
      for (const resource of resources) {
        const url = this._modularizeURL(resource);
        const shouldAppendSourceURL = !(url.endsWith('.html') || url.endsWith('.md'));
        promises.push(loadResourceIntoCache(url, shouldAppendSourceURL));
      }
      return Promise.all(promises).then(undefined);
    }

    _loadModules() {
      if (!this._descriptor.modules || !this._descriptor.modules.length) {
        return Promise.resolve();
      }

      const namespace = this._computeNamespace();
      // @ts-ignore Legacy global namespace instantation
      self[namespace] = self[namespace] || {};

      const legacyFileName = `${this._name}-legacy.js`;
      const fileName = this._descriptor.modules.includes(legacyFileName) ? legacyFileName : `${this._name}.js`;

      // TODO(crbug.com/1011811): Remove eval when we use TypeScript which does support dynamic imports
      return eval(`import('../${this._name}/${fileName}')`);
    }

    /**
     * @return {!Promise.<void>}
     */
    _loadScripts() {
      if (!this._descriptor.scripts || !this._descriptor.scripts.length) {
        return Promise.resolve();
      }

      const namespace = this._computeNamespace();
      // @ts-ignore Legacy global namespace instantation
      self[namespace] = self[namespace] || {};
      return loadScriptsPromise(this._descriptor.scripts.map(this._modularizeURL, this), this._remoteBase());
    }

    /**
     * @return {string}
     */
    _computeNamespace() {
      return specialCases[this._name] ||
          this._name.split('_').map(a => a.substring(0, 1).toUpperCase() + a.substring(1)).join('');
    }

    /**
     * @param {string} resourceName
     */
    _modularizeURL(resourceName) {
      return Runtime.normalizePath(this._name + '/' + resourceName);
    }

    /**
     * @return {string|undefined}
     */
    _remoteBase() {
      return !Runtime.queryParam('debugFrontend') && this._descriptor.remote && remoteBase || undefined;
    }

    /**
     * @param {string} resourceName
     * @return {!Promise.<string>}
     */
    fetchResource(resourceName) {
      const base = this._remoteBase();
      const sourceURL = getResourceURL(this._modularizeURL(resourceName), base);
      return base ? loadResourcePromiseWithFallback(sourceURL) : loadResourcePromise(sourceURL);
    }

    /**
     * @param {string} value
     * @return {string}
     */
    substituteURL(value) {
      const base = this._remoteBase() || '';
      return value.replace(/@url\(([^\)]*?)\)/g, convertURL.bind(this));

      /**
       * @param {string} match
       * @param {string} url
       * @this {Module}
       */
      function convertURL(match, url) {
        return base + this._modularizeURL(url);
      }
    }
  }

  /**
   * @unrestricted
   */
  class Extension {
    /**
    * @param {!Module} moduleParam
    * @param {!RuntimeExtensionDescriptor} descriptor
    */
    constructor(moduleParam, descriptor) {
      this._module = moduleParam;
      this._descriptor = descriptor;

      this._type = descriptor.type;
      this._hasTypeClass = this._type.charAt(0) === '@';

      /**
      * @type {?string}
      */
      this._className = descriptor.className || null;
      this._factoryName = descriptor.factoryName || null;
    }

    /**
    * @return {!RuntimeExtensionDescriptor}
    */
    descriptor() {
      return this._descriptor;
    }

    /**
    * @return {!Module}
    */
    module() {
      return this._module;
    }

    /**
    * @return {boolean}
    */
    enabled() {
      return this._module.enabled() && Runtime._isDescriptorEnabled(this.descriptor());
    }

    /**
    * @return {?function(new:Object)}
    */
    _typeClass() {
      if (!this._hasTypeClass) {
        return null;
      }
      return this._module._manager._resolve(this._type.substring(1));
    }

    /**
    * @param {?Object} context
    * @return {boolean}
    */
    isApplicable(context) {
      return this._module._manager.isExtensionApplicableToContext(this, context);
    }

    /**
    * @return {!Promise.<!Object>}
    */
    instance() {
      return this._module._loadPromise().then(this._createInstance.bind(this));
    }

    /**
    * @return {boolean}
    */
    canInstantiate() {
      return !!(this._className || this._factoryName);
    }

    /**
    * @return {!Object}
    */
    _createInstance() {
      const className = this._className || this._factoryName;
      if (!className) {
        throw new Error('Could not instantiate extension with no class');
      }
      const constructorFunction = self.eval(/** @type {string} */ (className));
      if (!(constructorFunction instanceof Function)) {
        throw new Error('Could not instantiate: ' + className);
      }
      if (this._className) {
        return this._module._manager.sharedInstance(constructorFunction);
      }
      return new constructorFunction(this);
    }

    /**
    * @return {string}
    */
    title() {
      // @ts-ignore Magic lookup for objects
      const title = this._descriptor['title-' + runtimePlatform] || this._descriptor['title'];
      if (title && l10nCallback) {
        return l10nCallback(title);
      }
      return title;
    }

    /**
    * @param {function(new:Object, ...?):void} contextType
    * @return {boolean}
    */
    hasContextType(contextType) {
      const contextTypes = this.descriptor().contextTypes;
      if (!contextTypes) {
        return false;
      }
      for (let i = 0; i < contextTypes.length; ++i) {
        if (contextType === this._module._manager._resolve(contextTypes[i])) {
          return true;
        }
      }
      return false;
    }
  }

  /**
  * @unrestricted
  */
  class ExperimentsSupport {
    constructor() {
      /** @type {!Array<!Experiment>} */
      this._experiments = [];
      /** @type {!Object<string,boolean>} */
      this._experimentNames = {};
      /** @type {!Object<string,boolean>} */
      this._enabledTransiently = {};
      /** @type {!Set<string>} */
      this._serverEnabled = new Set();
    }

    /**
    * @return {!Array.<!Experiment>}
    */
    allConfigurableExperiments() {
      const result = [];
      for (let i = 0; i < this._experiments.length; i++) {
        const experiment = this._experiments[i];
        if (!this._enabledTransiently[experiment.name]) {
          result.push(experiment);
        }
      }
      return result;
    }

    /**
    * @return {!Array.<!Experiment>}
    */
    enabledExperiments() {
      return this._experiments.filter(experiment => experiment.isEnabled());
    }

    /**
    * @param {!Object} value
    */
    _setExperimentsSetting(value) {
      if (!self.localStorage) {
        return;
      }
      self.localStorage['experiments'] = JSON.stringify(value);
    }

    /**
    * @param {string} experimentName
    * @param {string} experimentTitle
    * @param {boolean=} unstable
    */
    register(experimentName, experimentTitle, unstable) {
      Runtime._assert(!this._experimentNames[experimentName], 'Duplicate registration of experiment ' + experimentName);
      this._experimentNames[experimentName] = true;
      this._experiments.push(new Experiment(this, experimentName, experimentTitle, !!unstable));
    }

    /**
    * @param {string} experimentName
    * @return {boolean}
    */
    isEnabled(experimentName) {
      this._checkExperiment(experimentName);
      // Check for explicitly disabled experiments first - the code could call setEnable(false) on the experiment enabled
      // by default and we should respect that.
      if (Runtime._experimentsSetting()[experimentName] === false) {
        return false;
      }
      if (this._enabledTransiently[experimentName]) {
        return true;
      }
      if (this._serverEnabled.has(experimentName)) {
        return true;
      }

      return !!Runtime._experimentsSetting()[experimentName];
    }

    /**
    * @param {string} experimentName
    * @param {boolean} enabled
    */
    setEnabled(experimentName, enabled) {
      this._checkExperiment(experimentName);
      const experimentsSetting = Runtime._experimentsSetting();
      experimentsSetting[experimentName] = enabled;
      this._setExperimentsSetting(experimentsSetting);
    }

    /**
    * @param {!Array.<string>} experimentNames
    */
    setDefaultExperiments(experimentNames) {
      for (let i = 0; i < experimentNames.length; ++i) {
        this._checkExperiment(experimentNames[i]);
        this._enabledTransiently[experimentNames[i]] = true;
      }
    }

    /**
    * @param {!Array.<string>} experimentNames
    */
    setServerEnabledExperiments(experimentNames) {
      for (const experiment of experimentNames) {
        this._checkExperiment(experiment);
        this._serverEnabled.add(experiment);
      }
    }

    /**
    * @param {string} experimentName
    */
    enableForTest(experimentName) {
      this._checkExperiment(experimentName);
      this._enabledTransiently[experimentName] = true;
    }

    clearForTest() {
      this._experiments = [];
      this._experimentNames = {};
      this._enabledTransiently = {};
      this._serverEnabled.clear();
    }

    cleanUpStaleExperiments() {
      const experimentsSetting = Runtime._experimentsSetting();
      /** @type {!Object<string,boolean>} */
      const cleanedUpExperimentSetting = {};
      for (let i = 0; i < this._experiments.length; ++i) {
        const experimentName = this._experiments[i].name;
        if (experimentsSetting[experimentName]) {
          cleanedUpExperimentSetting[experimentName] = true;
        }
      }
      this._setExperimentsSetting(cleanedUpExperimentSetting);
    }

    /**
    * @param {string} experimentName
    */
    _checkExperiment(experimentName) {
      Runtime._assert(this._experimentNames[experimentName], 'Unknown experiment ' + experimentName);
    }
  }

  /**
  * @unrestricted
  */
  class Experiment {
    /**
    * @param {!ExperimentsSupport} experiments
    * @param {string} name
    * @param {string} title
    * @param {boolean} unstable
    */
    constructor(experiments, name, title, unstable) {
      this.name = name;
      this.title = title;
      this.unstable = unstable;
      this._experiments = experiments;
    }

    /**
    * @return {boolean}
    */
    isEnabled() {
      return this._experiments.isEnabled(this.name);
    }

    /**
    * @param {boolean} enabled
    */
    setEnabled(enabled) {
      this._experiments.setEnabled(this.name, enabled);
    }
  }

  /**
   * @private
   * @param {string} url
   * @param {boolean} asBinary
   * @template T
   * @return {!Promise.<T>}
   */
  function internalLoadResourcePromise(url, asBinary) {
    return new Promise(load);

    /**
     * @param {function(?):void} fulfill
     * @param {function(*):void} reject
     */
    function load(fulfill, reject) {
      const xhr = new XMLHttpRequest();
      xhr.open('GET', url, true);
      if (asBinary) {
        xhr.responseType = 'arraybuffer';
      }
      xhr.onreadystatechange = onreadystatechange;

      /**
       * @param {!Event} e
       */
      function onreadystatechange(e) {
        if (xhr.readyState !== XMLHttpRequest.DONE) {
          return;
        }

        const {response} = /** @type {*} */ (e.target);

        const text = asBinary ? new TextDecoder().decode(response) : response;

        // DevTools Proxy server can mask 404s as 200s, check the body to be sure
        const status = /^HTTP\/1.1 404/.test(text) ? 404 : xhr.status;

        if ([0, 200, 304].indexOf(status) === -1)  // Testing harness file:/// results in 0.
        {
          reject(new Error('While loading from url ' + url + ' server responded with a status of ' + status));
        } else {
          fulfill(response);
        }
      }
      xhr.send(null);
    }
  }

  /**
   * @type {!Object<string,boolean>}
   */
  const loadedScripts = {};

  /**
   * @param {!Array.<string>} scriptNames
   * @param {string=} base
   * @return {!Promise.<void>}
   */
  function loadScriptsPromise(scriptNames, base) {
    /** @type {!Array<!Promise<void>>} */
    const promises = [];
    /** @type {!Array<string>} */
    const urls = [];
    const sources = new Array(scriptNames.length);
    let scriptToEval = 0;
    for (let i = 0; i < scriptNames.length; ++i) {
      const scriptName = scriptNames[i];
      const sourceURL = getResourceURL(scriptName, base);

      if (loadedScripts[sourceURL]) {
        continue;
      }
      urls.push(sourceURL);
      const promise = base ? loadResourcePromiseWithFallback(sourceURL) : loadResourcePromise(sourceURL);
      promises.push(promise.then(scriptSourceLoaded.bind(null, i), scriptSourceLoaded.bind(null, i, undefined)));
    }
    return Promise.all(promises).then(undefined);

    /**
     * @param {number} scriptNumber
     * @param {string=} scriptSource
     */
    function scriptSourceLoaded(scriptNumber, scriptSource) {
      sources[scriptNumber] = scriptSource || '';
      // Eval scripts as fast as possible.
      while (typeof sources[scriptToEval] !== 'undefined') {
        evaluateScript(urls[scriptToEval], sources[scriptToEval]);
        ++scriptToEval;
      }
    }

    /**
     * @param {string} sourceURL
     * @param {string=} scriptSource
     */
    function evaluateScript(sourceURL, scriptSource) {
      loadedScripts[sourceURL] = true;
      if (!scriptSource) {
        // Do not reject, as this is normal in the hosted mode.
        console.error('Empty response arrived for script \'' + sourceURL + '\'');
        return;
      }
      self.eval(scriptSource + '\n//# sourceURL=' + sourceURL);
    }
  }

  /**
   * @param {string} url
   * @return {!Promise.<string>}
   */
  function loadResourcePromiseWithFallback(url) {
    return loadResourcePromise(url).catch(err => {
      const urlWithFallbackVersion = url.replace(/@[0-9a-f]{40}/, REMOTE_MODULE_FALLBACK_REVISION);
      // TODO(phulce): mark fallbacks in module.json and modify build script instead
      if (urlWithFallbackVersion === url || !url.includes('lighthouse_worker_module')) {
        throw err;
      }
      return loadResourcePromise(urlWithFallbackVersion);
    });
  }

  /**
   * @param {string} url
   * @param {boolean} appendSourceURL
   * @return {!Promise<void>}
   */
  function loadResourceIntoCache(url, appendSourceURL) {
    return loadResourcePromise(url).then(cacheResource.bind(null, url), cacheResource.bind(null, url, undefined));

    /**
     * @param {string} path
     * @param {string=} content
     */
    function cacheResource(path, content) {
      if (!content) {
        console.error('Failed to load resource: ' + path);
        return;
      }
      const sourceURL = appendSourceURL ? Runtime.resolveSourceURL(path) : '';
      self.Runtime.cachedResources[path] = content + sourceURL;
    }
  }

  /**
   * @param {string} url
   * @return {!Promise.<string>}
   */
  function loadResourcePromise(url) {
    return internalLoadResourcePromise(url, false);
  }

  /**
   * @param {string} scriptName
   * @param {string=} base
   * @return {string}
   */
  function getResourceURL(scriptName, base) {
    const sourceURL = (base || importScriptPathPrefix) + scriptName;
    const schemaIndex = sourceURL.indexOf('://') + 3;
    let pathIndex = sourceURL.indexOf('/', schemaIndex);
    if (pathIndex === -1) {
      pathIndex = sourceURL.length;
    }
    return sourceURL.substring(0, pathIndex) + Runtime.normalizePath(sourceURL.substring(pathIndex));
  }

  (function validateRemoteBase() {
    if (location.href.startsWith('devtools://devtools/bundled/')) {
      const queryParam = Runtime.queryParam('remoteBase');
      if (queryParam) {
        const versionMatch = /\/serve_file\/(@[0-9a-zA-Z]+)\/?$/.exec(queryParam);
        if (versionMatch) {
          remoteBase = `${location.origin}/remote/serve_file/${versionMatch[1]}/`;
        }
      }
    }
  })();

  (function() {
  const baseUrl = self.location ? self.location.origin + self.location.pathname : '';
  importScriptPathPrefix = baseUrl.substring(0, baseUrl.lastIndexOf('/') + 1);
  })();

  // This must be constructed after the query parameters have been parsed.
  const experiments = new ExperimentsSupport();

  /*
   * Copyright (C) 2013 Google Inc. All rights reserved.
   *
   * Redistribution and use in source and binary forms, with or without
   * modification, are permitted provided that the following conditions are
   * met:
   *
   *     * Redistributions of source code must retain the above copyright
   * notice, this list of conditions and the following disclaimer.
   *     * Redistributions in binary form must reproduce the above
   * copyright notice, this list of conditions and the following disclaimer
   * in the documentation and/or other materials provided with the
   * distribution.
   *     * Neither the name of Google Inc. nor the names of its
   * contributors may be used to endorse or promote products derived from
   * this software without specific prior written permission.
   *
   * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   */

  /**
   * @unrestricted
   */
  class BalancedJSONTokenizer {
    /**
     * @param {function(string):void} callback
     * @param {boolean=} findMultiple
     */
    constructor(callback, findMultiple) {
      this._callback = callback;
      /** @type {number} */
      this._index = 0;
      this._balance = 0;
      /** @type {string} */
      this._buffer = '';
      this._findMultiple = findMultiple || false;
      this._closingDoubleQuoteRegex = /[^\\](?:\\\\)*"/g;
    }

    /**
     * @param {string} chunk
     * @return {boolean}
     */
    write(chunk) {
      this._buffer += chunk;
      const lastIndex = this._buffer.length;
      const buffer = this._buffer;
      let index;
      for (index = this._index; index < lastIndex; ++index) {
        const character = buffer[index];
        if (character === '"') {
          this._closingDoubleQuoteRegex.lastIndex = index;
          if (!this._closingDoubleQuoteRegex.test(buffer)) {
            break;
          }
          index = this._closingDoubleQuoteRegex.lastIndex - 1;
        } else if (character === '{') {
          ++this._balance;
        } else if (character === '}') {
          --this._balance;
          if (this._balance < 0) {
            this._reportBalanced();
            return false;
          }
          if (!this._balance) {
            this._lastBalancedIndex = index + 1;
            if (!this._findMultiple) {
              break;
            }
          }
        } else if (character === ']' && !this._balance) {
          this._reportBalanced();
          return false;
        }
      }
      this._index = index;
      this._reportBalanced();
      return true;
    }

    _reportBalanced() {
      if (!this._lastBalancedIndex) {
        return;
      }
      this._callback(this._buffer.slice(0, this._lastBalancedIndex));
      this._buffer = this._buffer.slice(this._lastBalancedIndex);
      this._index -= this._lastBalancedIndex;
      this._lastBalancedIndex = 0;
    }

    /**
     * @return {string}
     */
    remainder() {
      return this._buffer;
    }
  }

  /*
   * Copyright (C) 2014 Google Inc. All rights reserved.
   *
   * Redistribution and use in source and binary forms, with or without
   * modification, are permitted provided that the following conditions are
   * met:
   *
   *     * Redistributions of source code must retain the above copyright
   * notice, this list of conditions and the following disclaimer.
   *     * Redistributions in binary form must reproduce the above
   * copyright notice, this list of conditions and the following disclaimer
   * in the documentation and/or other materials provided with the
   * distribution.
   *     * Neither the name of Google Inc. nor the names of its
   * contributors may be used to endorse or promote products derived from
   * this software without specific prior written permission.
   *
   * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   */

  // @ts-nocheck
  // TODO(crbug.com/1011811): Enable TypeScript compiler checks

  const HeapSnapshotProgressEvent = {
    Update: 'ProgressUpdate',
    BrokenSnapshot: 'BrokenSnapshot'
  };

  const baseSystemDistance = 100000000;

  /**
   * @unrestricted
   */
  class AllocationNodeCallers {
    /**
     * @param {!Array.<!SerializedAllocationNode>} nodesWithSingleCaller
     * @param {!Array.<!SerializedAllocationNode>} branchingCallers
     */
    constructor(nodesWithSingleCaller, branchingCallers) {
      /** @type {!Array.<!SerializedAllocationNode>} */
      this.nodesWithSingleCaller = nodesWithSingleCaller;
      /** @type {!Array.<!SerializedAllocationNode>} */
      this.branchingCallers = branchingCallers;
    }
  }

  /**
   * @unrestricted
   */
  class SerializedAllocationNode {
    /**
     * @param {number} nodeId
     * @param {string} functionName
     * @param {string} scriptName
     * @param {number} scriptId
     * @param {number} line
     * @param {number} column
     * @param {number} count
     * @param {number} size
     * @param {number} liveCount
     * @param {number} liveSize
     * @param {boolean} hasChildren
     */
    constructor(nodeId, functionName, scriptName, scriptId, line, column, count, size, liveCount, liveSize, hasChildren) {
      /** @type {number} */
      this.id = nodeId;
      /** @type {string} */
      this.name = functionName;
      /** @type {string} */
      this.scriptName = scriptName;
      /** @type {number} */
      this.scriptId = scriptId;
      /** @type {number} */
      this.line = line;
      /** @type {number} */
      this.column = column;
      /** @type {number} */
      this.count = count;
      /** @type {number} */
      this.size = size;
      /** @type {number} */
      this.liveCount = liveCount;
      /** @type {number} */
      this.liveSize = liveSize;
      /** @type {boolean} */
      this.hasChildren = hasChildren;
    }
  }

  /**
   * @unrestricted
   */
  class AllocationStackFrame {
    /**
     * @param {string} functionName
     * @param {string} scriptName
     * @param {number} scriptId
     * @param {number} line
     * @param {number} column
     */
    constructor(functionName, scriptName, scriptId, line, column) {
      /** @type {string} */
      this.functionName = functionName;
      /** @type {string} */
      this.scriptName = scriptName;
      /** @type {number} */
      this.scriptId = scriptId;
      /** @type {number} */
      this.line = line;
      /** @type {number} */
      this.column = column;
    }
  }

  /**
   * @unrestricted
   */
  class Node {
    /**
     * @param {number} id
     * @param {string} name
     * @param {number} distance
     * @param {number} nodeIndex
     * @param {number} retainedSize
     * @param {number} selfSize
     * @param {string} type
     */
    constructor(id, name, distance, nodeIndex, retainedSize, selfSize, type) {
      this.id = id;
      this.name = name;
      this.distance = distance;
      this.nodeIndex = nodeIndex;
      this.retainedSize = retainedSize;
      this.selfSize = selfSize;
      this.type = type;

      this.canBeQueried = false;
      this.detachedDOMTreeNode = false;
    }
  }

  /**
   * @unrestricted
   */
  class Edge {
    /**
     * @param {string} name
     * @param {!Node} node
     * @param {string} type
     * @param {number} edgeIndex
     */
    constructor(name, node, type, edgeIndex) {
      this.name = name;
      this.node = node;
      this.type = type;
      this.edgeIndex = edgeIndex;
    }
  }

  /**
   * @unrestricted
   */
  class AggregateForDiff {
    constructor() {
      /** @type {!Array.<number>} */
      this.indexes = [];
      /** @type {!Array.<string>} */
      this.ids = [];
      /** @type {!Array.<number>} */
      this.selfSizes = [];
    }
  }

  /**
   * @unrestricted
   */
  class Diff {
    constructor() {
      /** @type {number} */
      this.addedCount = 0;
      /** @type {number} */
      this.removedCount = 0;
      /** @type {number} */
      this.addedSize = 0;
      /** @type {number} */
      this.removedSize = 0;
      /** @type {!Array.<number>} */
      this.deletedIndexes = [];
      /** @type {!Array.<number>} */
      this.addedIndexes = [];
    }
  }

  /**
   * @unrestricted
   */
  class ItemsRange {
    /**
     * @param {number} startPosition
     * @param {number} endPosition
     * @param {number} totalLength
     * @param {!Array.<*>} items
     */
    constructor(startPosition, endPosition, totalLength, items) {
      /** @type {number} */
      this.startPosition = startPosition;
      /** @type {number} */
      this.endPosition = endPosition;
      /** @type {number} */
      this.totalLength = totalLength;
      /** @type {!Array.<*>} */
      this.items = items;
    }
  }

  /**
   * @unrestricted
   */
  class StaticData {
    /**
     * @param {number} nodeCount
     * @param {number} rootNodeIndex
     * @param {number} totalSize
     * @param {number} maxJSObjectId
     */
    constructor(nodeCount, rootNodeIndex, totalSize, maxJSObjectId) {
      /** @type {number} */
      this.nodeCount = nodeCount;
      /** @type {number} */
      this.rootNodeIndex = rootNodeIndex;
      /** @type {number} */
      this.totalSize = totalSize;
      /** @type {number} */
      this.maxJSObjectId = maxJSObjectId;
    }
  }

  /**
   * @unrestricted
   */
  class Statistics {
    constructor() {
      /** @type {number} */
      this.total;
      /** @type {number} */
      this.v8heap;
      /** @type {number} */
      this.native;
      /** @type {number} */
      this.code;
      /** @type {number} */
      this.jsArrays;
      /** @type {number} */
      this.strings;
      /** @type {number} */
      this.system;
    }
  }

  /**
   * @unrestricted
   */
  class Samples {
    /**
     * @param {!Array.<number>} timestamps
     * @param {!Array.<number>} lastAssignedIds
     * @param {!Array.<number>} sizes
     */
    constructor(timestamps, lastAssignedIds, sizes) {
      this.timestamps = timestamps;
      this.lastAssignedIds = lastAssignedIds;
      this.sizes = sizes;
    }
  }

  /**
   * @unrestricted
   */
  class Location {
    /**
     * @param {number} scriptId
     * @param {number} lineNumber
     * @param {number} columnNumber
     */
    constructor(scriptId, lineNumber, columnNumber) {
      this.scriptId = scriptId;
      this.lineNumber = lineNumber;
      this.columnNumber = columnNumber;
    }
  }

  /*
   * Copyright (C) 2013 Google Inc. All rights reserved.
   *
   * Redistribution and use in source and binary forms, with or without
   * modification, are permitted provided that the following conditions are
   * met:
   *
   *     * Redistributions of source code must retain the above copyright
   * notice, this list of conditions and the following disclaimer.
   *     * Redistributions in binary form must reproduce the above
   * copyright notice, this list of conditions and the following disclaimer
   * in the documentation and/or other materials provided with the
   * distribution.
   *     * Neither the name of Google Inc. nor the names of its
   * contributors may be used to endorse or promote products derived from
   * this software without specific prior written permission.
   *
   * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   */

  /**
   * @unrestricted
   */
  class AllocationProfile {
    constructor(profile, liveObjectStats) {
      this._strings = profile.strings;
      this._liveObjectStats = liveObjectStats;

      this._nextNodeId = 1;
      this._functionInfos = [];
      this._idToNode = {};
      this._idToTopDownNode = {};
      this._collapsedTopNodeIdToFunctionInfo = {};

      this._traceTops = null;

      this._buildFunctionAllocationInfos(profile);
      this._traceTree = this._buildAllocationTree(profile, liveObjectStats);
    }

    _buildFunctionAllocationInfos(profile) {
      const strings = this._strings;

      const functionInfoFields = profile.snapshot.meta.trace_function_info_fields;
      const functionNameOffset = functionInfoFields.indexOf('name');
      const scriptNameOffset = functionInfoFields.indexOf('script_name');
      const scriptIdOffset = functionInfoFields.indexOf('script_id');
      const lineOffset = functionInfoFields.indexOf('line');
      const columnOffset = functionInfoFields.indexOf('column');
      const functionInfoFieldCount = functionInfoFields.length;

      const rawInfos = profile.trace_function_infos;
      const infoLength = rawInfos.length;
      const functionInfos = this._functionInfos = new Array(infoLength / functionInfoFieldCount);
      let index = 0;
      for (let i = 0; i < infoLength; i += functionInfoFieldCount) {
        functionInfos[index++] = new FunctionAllocationInfo(
            strings[rawInfos[i + functionNameOffset]], strings[rawInfos[i + scriptNameOffset]],
            rawInfos[i + scriptIdOffset], rawInfos[i + lineOffset], rawInfos[i + columnOffset]);
      }
    }

    _buildAllocationTree(profile, liveObjectStats) {
      const traceTreeRaw = profile.trace_tree;
      const functionInfos = this._functionInfos;
      const idToTopDownNode = this._idToTopDownNode;

      const traceNodeFields = profile.snapshot.meta.trace_node_fields;
      const nodeIdOffset = traceNodeFields.indexOf('id');
      const functionInfoIndexOffset = traceNodeFields.indexOf('function_info_index');
      const allocationCountOffset = traceNodeFields.indexOf('count');
      const allocationSizeOffset = traceNodeFields.indexOf('size');
      const childrenOffset = traceNodeFields.indexOf('children');
      const nodeFieldCount = traceNodeFields.length;

      function traverseNode(rawNodeArray, nodeOffset, parent) {
        const functionInfo = functionInfos[rawNodeArray[nodeOffset + functionInfoIndexOffset]];
        const id = rawNodeArray[nodeOffset + nodeIdOffset];
        const stats = liveObjectStats[id];
        const liveCount = stats ? stats.count : 0;
        const liveSize = stats ? stats.size : 0;
        const result = new TopDownAllocationNode(
            id, functionInfo, rawNodeArray[nodeOffset + allocationCountOffset],
            rawNodeArray[nodeOffset + allocationSizeOffset], liveCount, liveSize, parent);
        idToTopDownNode[id] = result;
        functionInfo.addTraceTopNode(result);

        const rawChildren = rawNodeArray[nodeOffset + childrenOffset];
        for (let i = 0; i < rawChildren.length; i += nodeFieldCount) {
          result.children.push(traverseNode(rawChildren, i, result));
        }

        return result;
      }

      return traverseNode(traceTreeRaw, 0, null);
    }

    /**
     * @return {!Array.<!SerializedAllocationNode>}
     */
    serializeTraceTops() {
      if (this._traceTops) {
        return this._traceTops;
      }
      const result = this._traceTops = [];
      const functionInfos = this._functionInfos;
      for (let i = 0; i < functionInfos.length; i++) {
        const info = functionInfos[i];
        if (info.totalCount === 0) {
          continue;
        }
        const nodeId = this._nextNodeId++;
        const isRoot = i === 0;
        result.push(this._serializeNode(
            nodeId, info, info.totalCount, info.totalSize, info.totalLiveCount, info.totalLiveSize, !isRoot));
        this._collapsedTopNodeIdToFunctionInfo[nodeId] = info;
      }
      result.sort(function(a, b) {
        return b.size - a.size;
      });
      return result;
    }

    /**
     * @param {number} nodeId
     * @return {!AllocationNodeCallers}
     */
    serializeCallers(nodeId) {
      let node = this._ensureBottomUpNode(nodeId);
      const nodesWithSingleCaller = [];
      while (node.callers().length === 1) {
        node = node.callers()[0];
        nodesWithSingleCaller.push(this._serializeCaller(node));
      }

      const branchingCallers = [];
      const callers = node.callers();
      for (let i = 0; i < callers.length; i++) {
        branchingCallers.push(this._serializeCaller(callers[i]));
      }

      return new AllocationNodeCallers(nodesWithSingleCaller, branchingCallers);
    }

    /**
     * @param {number} traceNodeId
     * @return {!Array.<!AllocationStackFrame>}
     */
    serializeAllocationStack(traceNodeId) {
      let node = this._idToTopDownNode[traceNodeId];
      const result = [];
      while (node) {
        const functionInfo = node.functionInfo;
        result.push(new AllocationStackFrame(
            functionInfo.functionName, functionInfo.scriptName, functionInfo.scriptId, functionInfo.line,
            functionInfo.column));
        node = node.parent;
      }
      return result;
    }

    /**
     * @param {number} allocationNodeId
     * @return {!Array.<number>}
     */
    traceIds(allocationNodeId) {
      return this._ensureBottomUpNode(allocationNodeId).traceTopIds;
    }

    /**
     * @param {number} nodeId
     * @return {!BottomUpAllocationNode}
     */
    _ensureBottomUpNode(nodeId) {
      let node = this._idToNode[nodeId];
      if (!node) {
        const functionInfo = this._collapsedTopNodeIdToFunctionInfo[nodeId];
        node = functionInfo.bottomUpRoot();
        delete this._collapsedTopNodeIdToFunctionInfo[nodeId];
        this._idToNode[nodeId] = node;
      }
      return node;
    }

    /**
     * @param {!BottomUpAllocationNode} node
     * @return {!SerializedAllocationNode}
     */
    _serializeCaller(node) {
      const callerId = this._nextNodeId++;
      this._idToNode[callerId] = node;
      return this._serializeNode(
          callerId, node.functionInfo, node.allocationCount, node.allocationSize, node.liveCount, node.liveSize,
          node.hasCallers());
    }

    /**
     * @param {number} nodeId
     * @param {!FunctionAllocationInfo} functionInfo
     * @param {number} count
     * @param {number} size
     * @param {number} liveCount
     * @param {number} liveSize
     * @param {boolean} hasChildren
     * @return {!SerializedAllocationNode}
     */
    _serializeNode(nodeId, functionInfo, count, size, liveCount, liveSize, hasChildren) {
      return new SerializedAllocationNode(
          nodeId, functionInfo.functionName, functionInfo.scriptName, functionInfo.scriptId, functionInfo.line,
          functionInfo.column, count, size, liveCount, liveSize, hasChildren);
    }
  }

  /**
   * @unrestricted
   */
  class TopDownAllocationNode {
    /**
     * @param {number} id
     * @param {!FunctionAllocationInfo} functionInfo
     * @param {number} count
     * @param {number} size
     * @param {number} liveCount
     * @param {number} liveSize
     * @param {?TopDownAllocationNode} parent
     */
    constructor(id, functionInfo, count, size, liveCount, liveSize, parent) {
      this.id = id;
      this.functionInfo = functionInfo;
      this.allocationCount = count;
      this.allocationSize = size;
      this.liveCount = liveCount;
      this.liveSize = liveSize;
      this.parent = parent;
      this.children = [];
    }
  }

  /**
   * @unrestricted
   */
  class BottomUpAllocationNode {
    /**
     * @param {!FunctionAllocationInfo} functionInfo
     */
    constructor(functionInfo) {
      this.functionInfo = functionInfo;
      this.allocationCount = 0;
      this.allocationSize = 0;
      this.liveCount = 0;
      this.liveSize = 0;
      this.traceTopIds = [];
      this._callers = [];
    }

    /**
     * @param {!TopDownAllocationNode} traceNode
     * @return {!BottomUpAllocationNode}
     */
    addCaller(traceNode) {
      const functionInfo = traceNode.functionInfo;
      let result;
      for (let i = 0; i < this._callers.length; i++) {
        const caller = this._callers[i];
        if (caller.functionInfo === functionInfo) {
          result = caller;
          break;
        }
      }
      if (!result) {
        result = new BottomUpAllocationNode(functionInfo);
        this._callers.push(result);
      }
      return result;
    }

    /**
     * @return {!Array.<!BottomUpAllocationNode>}
     */
    callers() {
      return this._callers;
    }

    /**
     * @return {boolean}
     */
    hasCallers() {
      return this._callers.length > 0;
    }
  }

  /**
   * @unrestricted
   */
  class FunctionAllocationInfo {
    /**
     * @param {string} functionName
     * @param {string} scriptName
     * @param {number} scriptId
     * @param {number} line
     * @param {number} column
     */
    constructor(functionName, scriptName, scriptId, line, column) {
      this.functionName = functionName;
      this.scriptName = scriptName;
      this.scriptId = scriptId;
      this.line = line;
      this.column = column;
      this.totalCount = 0;
      this.totalSize = 0;
      this.totalLiveCount = 0;
      this.totalLiveSize = 0;
      this._traceTops = [];
    }

    /**
     * @param {!TopDownAllocationNode} node
     */
    addTraceTopNode(node) {
      if (node.allocationCount === 0) {
        return;
      }
      this._traceTops.push(node);
      this.totalCount += node.allocationCount;
      this.totalSize += node.allocationSize;
      this.totalLiveCount += node.liveCount;
      this.totalLiveSize += node.liveSize;
    }

    /**
     * @return {?BottomUpAllocationNode}
     */
    bottomUpRoot() {
      if (!this._traceTops.length) {
        return null;
      }
      if (!this._bottomUpTree) {
        this._buildAllocationTraceTree();
      }
      return this._bottomUpTree;
    }

    _buildAllocationTraceTree() {
      this._bottomUpTree = new BottomUpAllocationNode(this);

      for (let i = 0; i < this._traceTops.length; i++) {
        let node = this._traceTops[i];
        let bottomUpNode = this._bottomUpTree;
        const count = node.allocationCount;
        const size = node.allocationSize;
        const liveCount = node.liveCount;
        const liveSize = node.liveSize;
        const traceId = node.id;
        while (true) {
          bottomUpNode.allocationCount += count;
          bottomUpNode.allocationSize += size;
          bottomUpNode.liveCount += liveCount;
          bottomUpNode.liveSize += liveSize;
          bottomUpNode.traceTopIds.push(traceId);
          node = node.parent;
          if (node === null) {
            break;
          }

          bottomUpNode = bottomUpNode.addCaller(node);
        }
      }
    }
  }

  /*
   * Copyright (C) 2011 Google Inc. All rights reserved.
   *
   * Redistribution and use in source and binary forms, with or without
   * modification, are permitted provided that the following conditions are
   * met:
   *
   *     * Redistributions of source code must retain the above copyright
   * notice, this list of conditions and the following disclaimer.
   *     * Redistributions in binary form must reproduce the above
   * copyright notice, this list of conditions and the following disclaimer
   * in the documentation and/or other materials provided with the
   * distribution.
   *     * Neither the name of Google Inc. nor the names of its
   * contributors may be used to endorse or promote products derived from
   * this software without specific prior written permission.
   *
   * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   */

  /**
   * @implements {HeapSnapshotItem}
   * @unrestricted
   */
  class HeapSnapshotEdge {
    /**
     * @param {!HeapSnapshot} snapshot
     * @param {number=} edgeIndex
     */
    constructor(snapshot, edgeIndex) {
      this._snapshot = snapshot;
      this._edges = snapshot.containmentEdges;
      this.edgeIndex = edgeIndex || 0;
    }

    /**
     * @return {!HeapSnapshotEdge}
     */
    clone() {
      return new HeapSnapshotEdge(this._snapshot, this.edgeIndex);
    }

    /**
     * @return {boolean}
     */
    hasStringName() {
      throw new Error('Not implemented');
    }

    /**
     * @return {string}
     */
    name() {
      throw new Error('Not implemented');
    }

    /**
     * @return {!HeapSnapshotNode}
     */
    node() {
      return this._snapshot.createNode(this.nodeIndex());
    }

    /**
     * @return {number}
     */
    nodeIndex() {
      return this._edges[this.edgeIndex + this._snapshot._edgeToNodeOffset];
    }

    /**
     * @override
     * @return {string}
     */
    toString() {
      return 'HeapSnapshotEdge: ' + this.name();
    }

    /**
     * @return {string}
     */
    type() {
      return this._snapshot._edgeTypes[this.rawType()];
    }

    /**
     * @override
     * @return {number}
     */
    itemIndex() {
      return this.edgeIndex;
    }

    /**
     * @override
     * @return {!Edge}
     */
    serialize() {
      return new Edge(
          this.name(), this.node().serialize(), this.type(), this.edgeIndex);
    }

    /**
     * @protected
     * @return {number}
     */
    rawType() {
      return this._edges[this.edgeIndex + this._snapshot._edgeTypeOffset];
    }
  }

  /**
   * @implements {HeapSnapshotItemIndexProvider}
   * @unrestricted
   */
  class HeapSnapshotNodeIndexProvider {
    /**
     * @param {!HeapSnapshot} snapshot
     */
    constructor(snapshot) {
      this._node = snapshot.createNode();
    }

    /**
     * @override
     * @param {number} index
     * @return {!HeapSnapshotNode}
     */
    itemForIndex(index) {
      this._node.nodeIndex = index;
      return this._node;
    }
  }

  /**
   * @implements {HeapSnapshotItemIndexProvider}
   * @unrestricted
   */
  class HeapSnapshotEdgeIndexProvider {
    /**
     * @param {!HeapSnapshot} snapshot
     */
    constructor(snapshot) {
      this._edge = snapshot.createEdge(0);
    }

    /**
     * @override
     * @param {number} index
     * @return {!HeapSnapshotEdge}
     */
    itemForIndex(index) {
      this._edge.edgeIndex = index;
      return this._edge;
    }
  }

  /**
   * @implements {HeapSnapshotItemIndexProvider}
   * @unrestricted
   */
  class HeapSnapshotRetainerEdgeIndexProvider {
    /**
     * @param {!HeapSnapshot} snapshot
     */
    constructor(snapshot) {
      this._retainerEdge = snapshot.createRetainingEdge(0);
    }

    /**
     * @override
     * @param {number} index
     * @return {!HeapSnapshotRetainerEdge}
     */
    itemForIndex(index) {
      this._retainerEdge.setRetainerIndex(index);
      return this._retainerEdge;
    }
  }

  /**
   * @implements {HeapSnapshotItemIterator}
   * @unrestricted
   */
  class HeapSnapshotEdgeIterator {
    /**
     * @param {!HeapSnapshotNode} node
     */
    constructor(node) {
      this._sourceNode = node;
      this.edge = node._snapshot.createEdge(node.edgeIndexesStart());
    }

    /**
     * @override
     * @return {boolean}
     */
    hasNext() {
      return this.edge.edgeIndex < this._sourceNode.edgeIndexesEnd();
    }

    /**
     * @override
     * @return {!HeapSnapshotEdge}
     */
    item() {
      return this.edge;
    }

    /**
     * @override
     */
    next() {
      this.edge.edgeIndex += this.edge._snapshot._edgeFieldsCount;
    }
  }

  /**
   * @implements {HeapSnapshotItem}
   * @unrestricted
   */
  class HeapSnapshotRetainerEdge {
    /**
     * @param {!HeapSnapshot} snapshot
     * @param {number} retainerIndex
     */
    constructor(snapshot, retainerIndex) {
      this._snapshot = snapshot;
      this.setRetainerIndex(retainerIndex);
    }

    /**
     * @return {!HeapSnapshotRetainerEdge}
     */
    clone() {
      return new HeapSnapshotRetainerEdge(this._snapshot, this.retainerIndex());
    }

    /**
     * @return {boolean}
     */
    hasStringName() {
      return this._edge().hasStringName();
    }

    /**
     * @return {string}
     */
    name() {
      return this._edge().name();
    }

    /**
     * @return {!HeapSnapshotNode}
     */
    node() {
      return this._node();
    }

    /**
     * @return {number}
     */
    nodeIndex() {
      return this._retainingNodeIndex;
    }

    /**
     * @return {number}
     */
    retainerIndex() {
      return this._retainerIndex;
    }

    /**
     * @param {number} retainerIndex
     */
    setRetainerIndex(retainerIndex) {
      if (retainerIndex === this._retainerIndex) {
        return;
      }
      this._retainerIndex = retainerIndex;
      this._globalEdgeIndex = this._snapshot._retainingEdges[retainerIndex];
      this._retainingNodeIndex = this._snapshot._retainingNodes[retainerIndex];
      this._edgeInstance = null;
      this._nodeInstance = null;
    }

    /**
     * @param {number} edgeIndex
     */
    set edgeIndex(edgeIndex) {
      this.setRetainerIndex(edgeIndex);
    }

    _node() {
      if (!this._nodeInstance) {
        this._nodeInstance = this._snapshot.createNode(this._retainingNodeIndex);
      }
      return this._nodeInstance;
    }

    _edge() {
      if (!this._edgeInstance) {
        this._edgeInstance = this._snapshot.createEdge(this._globalEdgeIndex);
      }
      return this._edgeInstance;
    }

    /**
     * @override
     * @return {string}
     */
    toString() {
      return this._edge().toString();
    }

    /**
     * @override
     * @return {number}
     */
    itemIndex() {
      return this._retainerIndex;
    }

    /**
     * @override
     * @return {!Edge}
     */
    serialize() {
      return new Edge(
          this.name(), this.node().serialize(), this.type(), this._globalEdgeIndex);
    }

    /**
     * @return {string}
     */
    type() {
      return this._edge().type();
    }
  }

  /**
   * @implements {HeapSnapshotItemIterator}
   * @unrestricted
   */
  class HeapSnapshotRetainerEdgeIterator {
    /**
     * @param {!HeapSnapshotNode} retainedNode
     */
    constructor(retainedNode) {
      const snapshot = retainedNode._snapshot;
      const retainedNodeOrdinal = retainedNode.ordinal();
      const retainerIndex = snapshot._firstRetainerIndex[retainedNodeOrdinal];
      this._retainersEnd = snapshot._firstRetainerIndex[retainedNodeOrdinal + 1];
      this.retainer = snapshot.createRetainingEdge(retainerIndex);
    }

    /**
     * @override
     * @return {boolean}
     */
    hasNext() {
      return this.retainer.retainerIndex() < this._retainersEnd;
    }

    /**
     * @override
     * @return {!HeapSnapshotRetainerEdge}
     */
    item() {
      return this.retainer;
    }

    /**
     * @override
     */
    next() {
      this.retainer.setRetainerIndex(this.retainer.retainerIndex() + 1);
    }
  }

  /**
   * @implements {HeapSnapshotItem}
   * @unrestricted
   */
  class HeapSnapshotNode {
    /**
     * @param {!HeapSnapshot} snapshot
     * @param {number=} nodeIndex
     */
    constructor(snapshot, nodeIndex) {
      this._snapshot = snapshot;
      this.nodeIndex = nodeIndex || 0;
    }

    /**
     * @return {number}
     */
    distance() {
      return this._snapshot._nodeDistances[this.nodeIndex / this._snapshot._nodeFieldCount];
    }

    /**
     * @return {string}
     */
    className() {
      throw new Error('Not implemented');
    }

    /**
     * @return {number}
     */
    classIndex() {
      throw new Error('Not implemented');
    }

    /**
     * @return {number}
     */
    dominatorIndex() {
      const nodeFieldCount = this._snapshot._nodeFieldCount;
      return this._snapshot._dominatorsTree[this.nodeIndex / this._snapshot._nodeFieldCount] * nodeFieldCount;
    }

    /**
     * @return {!HeapSnapshotEdgeIterator}
     */
    edges() {
      return new HeapSnapshotEdgeIterator(this);
    }

    /**
     * @return {number}
     */
    edgesCount() {
      return (this.edgeIndexesEnd() - this.edgeIndexesStart()) / this._snapshot._edgeFieldsCount;
    }

    /**
     * @return {number}
     */
    id() {
      throw new Error('Not implemented');
    }

    /**
     * @return {boolean}
     */
    isRoot() {
      return this.nodeIndex === this._snapshot._rootNodeIndex;
    }

    /**
     * @return {string}
     */
    name() {
      return this._snapshot.strings[this._name()];
    }

    /**
     * @return {number}
     */
    retainedSize() {
      return this._snapshot._retainedSizes[this.ordinal()];
    }

    /**
     * @return {!HeapSnapshotRetainerEdgeIterator}
     */
    retainers() {
      return new HeapSnapshotRetainerEdgeIterator(this);
    }

    /**
     * @return {number}
     */
    retainersCount() {
      const snapshot = this._snapshot;
      const ordinal = this.ordinal();
      return snapshot._firstRetainerIndex[ordinal + 1] - snapshot._firstRetainerIndex[ordinal];
    }

    /**
     * @return {number}
     */
    selfSize() {
      const snapshot = this._snapshot;
      return snapshot.nodes[this.nodeIndex + snapshot._nodeSelfSizeOffset];
    }

    /**
     * @return {string}
     */
    type() {
      return this._snapshot._nodeTypes[this.rawType()];
    }

    /**
     * @return {number}
     */
    traceNodeId() {
      const snapshot = this._snapshot;
      return snapshot.nodes[this.nodeIndex + snapshot._nodeTraceNodeIdOffset];
    }

    /**
     * @override
     * @return {number}
     */
    itemIndex() {
      return this.nodeIndex;
    }

    /**
     * @override
     * @return {!Node}
     */
    serialize() {
      return new Node(
          this.id(), this.name(), this.distance(), this.nodeIndex, this.retainedSize(), this.selfSize(), this.type());
    }

    /**
     * @return {number}
     */
    _name() {
      const snapshot = this._snapshot;
      return snapshot.nodes[this.nodeIndex + snapshot._nodeNameOffset];
    }

    /**
     * @return {number}
     */
    edgeIndexesStart() {
      return this._snapshot._firstEdgeIndexes[this.ordinal()];
    }

    /**
     * @return {number}
     */
    edgeIndexesEnd() {
      return this._snapshot._firstEdgeIndexes[this.ordinal() + 1];
    }

    /**
     * @return {number}
     */
    ordinal() {
      return this.nodeIndex / this._snapshot._nodeFieldCount;
    }

    /**
     * @return {number}
     */
    _nextNodeIndex() {
      return this.nodeIndex + this._snapshot._nodeFieldCount;
    }

    /**
     * @protected
     * @return {number}
     */
    rawType() {
      const snapshot = this._snapshot;
      return snapshot.nodes[this.nodeIndex + snapshot._nodeTypeOffset];
    }
  }

  /**
   * @implements {HeapSnapshotItemIterator}
   * @unrestricted
   */
  class HeapSnapshotNodeIterator {
    /**
     * @param {!HeapSnapshotNode} node
     */
    constructor(node) {
      this.node = node;
      this._nodesLength = node._snapshot.nodes.length;
    }

    /**
     * @override
     * @return {boolean}
     */
    hasNext() {
      return this.node.nodeIndex < this._nodesLength;
    }

    /**
     * @override
     * @return {!HeapSnapshotNode}
     */
    item() {
      return this.node;
    }

    /**
     * @override
     */
    next() {
      this.node.nodeIndex = this.node._nextNodeIndex();
    }
  }

  /**
   * @implements {HeapSnapshotItemIterator}
   * @unrestricted
   */
  class HeapSnapshotIndexRangeIterator {
    /**
     * @param {!HeapSnapshotItemIndexProvider} itemProvider
     * @param {!Array.<number>|!Uint32Array} indexes
     */
    constructor(itemProvider, indexes) {
      this._itemProvider = itemProvider;
      this._indexes = indexes;
      this._position = 0;
    }

    /**
     * @override
     * @return {boolean}
     */
    hasNext() {
      return this._position < this._indexes.length;
    }

    /**
     * @override
     * @return {!HeapSnapshotItem}
     */
    item() {
      const index = this._indexes[this._position];
      return this._itemProvider.itemForIndex(index);
    }

    /**
     * @override
     */
    next() {
      ++this._position;
    }
  }

  /**
   * @implements {HeapSnapshotItemIterator}
   * @unrestricted
   */
  class HeapSnapshotFilteredIterator {
    /**
     * @param {!HeapSnapshotItemIterator} iterator
     * @param {function(!HeapSnapshotItem):boolean=} filter
     */
    constructor(iterator, filter) {
      this._iterator = iterator;
      this._filter = filter;
      this._skipFilteredItems();
    }

    /**
     * @override
     * @return {boolean}
     */
    hasNext() {
      return this._iterator.hasNext();
    }

    /**
     * @override
     * @return {!HeapSnapshotItem}
     */
    item() {
      return this._iterator.item();
    }

    /**
     * @override
     */
    next() {
      this._iterator.next();
      this._skipFilteredItems();
    }

    _skipFilteredItems() {
      while (this._iterator.hasNext() && !this._filter(this._iterator.item())) {
        this._iterator.next();
      }
    }
  }

  /**
   * @unrestricted
   */
  class HeapSnapshotProgress {
    /**
     * @param {!HeapSnapshotWorkerDispatcher=} dispatcher
     */
    constructor(dispatcher) {
      this._dispatcher = dispatcher;
    }

    /**
     * @param {string} status
     */
    updateStatus(status) {
      this._sendUpdateEvent(serializeUIString(status));
    }

    /**
     * @param {string} title
     * @param {number} value
     * @param {number} total
     */
    updateProgress(title, value, total) {
      const percentValue = ((total ? (value / total) : 0) * 100).toFixed(0);
      this._sendUpdateEvent(serializeUIString(title, [percentValue]));
    }

    /**
     * @param {string} error
     */
    reportProblem(error) {
      // May be undefined in tests.
      if (this._dispatcher) {
        this._dispatcher.sendEvent(HeapSnapshotProgressEvent.BrokenSnapshot, error);
      }
    }

    /**
     * @param {string} serializedText
     */
    _sendUpdateEvent(serializedText) {
      // May be undefined in tests.
      if (this._dispatcher) {
        this._dispatcher.sendEvent(HeapSnapshotProgressEvent.Update, serializedText);
      }
    }
  }

  /**
   * @unrestricted
   */
  class HeapSnapshotProblemReport {
    /**
     * @param {string} title
     */
    constructor(title) {
      this._errors = [title];
    }

    /**
     * @param {string} error
     */
    addError(error) {
      if (this._errors.length > 100) {
        return;
      }
      this._errors.push(error);
    }

    /**
     * @override
     * @return {string}
     */
    toString() {
      return this._errors.join('\n  ');
    }
  }

  /**
   * @unrestricted
   */
  class HeapSnapshot {
    /**
     * @param {!Object} profile
     * @param {!HeapSnapshotProgress} progress
     */
    constructor(profile, progress) {
      /** @type {!Uint32Array} */
      this.nodes = profile.nodes;
      /** @type {!Uint32Array} */
      this.containmentEdges = profile.edges;
      /** @type {!HeapSnapshotMetainfo} */
      this._metaNode = profile.snapshot.meta;
      /** @type {!Array.<number>} */
      this._rawSamples = profile.samples;
      /** @type {?Samples} */
      this._samples = null;
      /** @type {!Array.<string>} */
      this.strings = profile.strings;
      /** @type {!Array.<number>} */
      this._locations = profile.locations;
      this._progress = progress;

      this._noDistance = -5;
      this._rootNodeIndex = 0;
      if (profile.snapshot.root_index) {
        this._rootNodeIndex = profile.snapshot.root_index;
      }

      this._snapshotDiffs = {};
      this._aggregatesForDiff = null;
      this._aggregates = {};
      this._aggregatesSortedFlags = {};
      this._profile = profile;
    }

    /**
     * @protected
     */
    initialize() {
      const meta = this._metaNode;

      this._nodeTypeOffset = meta.node_fields.indexOf('type');
      this._nodeNameOffset = meta.node_fields.indexOf('name');
      this._nodeIdOffset = meta.node_fields.indexOf('id');
      this._nodeSelfSizeOffset = meta.node_fields.indexOf('self_size');
      this._nodeEdgeCountOffset = meta.node_fields.indexOf('edge_count');
      this._nodeTraceNodeIdOffset = meta.node_fields.indexOf('trace_node_id');
      this._nodeFieldCount = meta.node_fields.length;

      this._nodeTypes = meta.node_types[this._nodeTypeOffset];
      this._nodeArrayType = this._nodeTypes.indexOf('array');
      this._nodeHiddenType = this._nodeTypes.indexOf('hidden');
      this._nodeObjectType = this._nodeTypes.indexOf('object');
      this._nodeNativeType = this._nodeTypes.indexOf('native');
      this._nodeConsStringType = this._nodeTypes.indexOf('concatenated string');
      this._nodeSlicedStringType = this._nodeTypes.indexOf('sliced string');
      this._nodeCodeType = this._nodeTypes.indexOf('code');
      this._nodeSyntheticType = this._nodeTypes.indexOf('synthetic');

      this._edgeFieldsCount = meta.edge_fields.length;
      this._edgeTypeOffset = meta.edge_fields.indexOf('type');
      this._edgeNameOffset = meta.edge_fields.indexOf('name_or_index');
      this._edgeToNodeOffset = meta.edge_fields.indexOf('to_node');

      this._edgeTypes = meta.edge_types[this._edgeTypeOffset];
      this._edgeTypes.push('invisible');
      this._edgeElementType = this._edgeTypes.indexOf('element');
      this._edgeHiddenType = this._edgeTypes.indexOf('hidden');
      this._edgeInternalType = this._edgeTypes.indexOf('internal');
      this._edgeShortcutType = this._edgeTypes.indexOf('shortcut');
      this._edgeWeakType = this._edgeTypes.indexOf('weak');
      this._edgeInvisibleType = this._edgeTypes.indexOf('invisible');

      const location_fields = meta.location_fields || [];

      this._locationIndexOffset = location_fields.indexOf('object_index');
      this._locationScriptIdOffset = location_fields.indexOf('script_id');
      this._locationLineOffset = location_fields.indexOf('line');
      this._locationColumnOffset = location_fields.indexOf('column');
      this._locationFieldCount = location_fields.length;

      this.nodeCount = this.nodes.length / this._nodeFieldCount;
      this._edgeCount = this.containmentEdges.length / this._edgeFieldsCount;

      this._retainedSizes = new Float64Array(this.nodeCount);
      this._firstEdgeIndexes = new Uint32Array(this.nodeCount + 1);
      this._retainingNodes = new Uint32Array(this._edgeCount);
      this._retainingEdges = new Uint32Array(this._edgeCount);
      this._firstRetainerIndex = new Uint32Array(this.nodeCount + 1);
      this._nodeDistances = new Int32Array(this.nodeCount);
      this._firstDominatedNodeIndex = new Uint32Array(this.nodeCount + 1);
      this._dominatedNodes = new Uint32Array(this.nodeCount - 1);

      this._progress.updateStatus(ls`Building edge indexes…`);
      this._buildEdgeIndexes();
      this._progress.updateStatus(ls`Building retainers…`);
      this._buildRetainers();
      this._progress.updateStatus(ls`Calculating node flags…`);
      this.calculateFlags();
      this._progress.updateStatus(ls`Calculating distances…`);
      this.calculateDistances();
      this._progress.updateStatus(ls`Building postorder index…`);
      const result = this._buildPostOrderIndex();
      // Actually it is array that maps node ordinal number to dominator node ordinal number.
      this._progress.updateStatus(ls`Building dominator tree…`);
      this._dominatorsTree =
          this._buildDominatorTree(result.postOrderIndex2NodeOrdinal, result.nodeOrdinal2PostOrderIndex);
      this._progress.updateStatus(ls`Calculating retained sizes…`);
      this._calculateRetainedSizes(result.postOrderIndex2NodeOrdinal);
      this._progress.updateStatus(ls`Building dominated nodes…`);
      this._buildDominatedNodes();
      this._progress.updateStatus(ls`Calculating statistics…`);
      this.calculateStatistics();
      this._progress.updateStatus(ls`Calculating samples…`);
      this._buildSamples();
      this._progress.updateStatus(ls`Building locations…`);
      this._buildLocationMap();
      this._progress.updateStatus(ls`Finished processing.`);

      if (this._profile.snapshot.trace_function_count) {
        this._progress.updateStatus(ls`Building allocation statistics…`);
        const nodes = this.nodes;
        const nodesLength = nodes.length;
        const nodeFieldCount = this._nodeFieldCount;
        const node = this.rootNode();
        const liveObjects = {};
        for (let nodeIndex = 0; nodeIndex < nodesLength; nodeIndex += nodeFieldCount) {
          node.nodeIndex = nodeIndex;
          const traceNodeId = node.traceNodeId();
          let stats = liveObjects[traceNodeId];
          if (!stats) {
            liveObjects[traceNodeId] = stats = {count: 0, size: 0, ids: []};
          }
          stats.count++;
          stats.size += node.selfSize();
          stats.ids.push(node.id());
        }
        this._allocationProfile = new AllocationProfile(this._profile, liveObjects);
        this._progress.updateStatus(ls`Done`);
      }
    }

    _buildEdgeIndexes() {
      const nodes = this.nodes;
      const nodeCount = this.nodeCount;
      const firstEdgeIndexes = this._firstEdgeIndexes;
      const nodeFieldCount = this._nodeFieldCount;
      const edgeFieldsCount = this._edgeFieldsCount;
      const nodeEdgeCountOffset = this._nodeEdgeCountOffset;
      firstEdgeIndexes[nodeCount] = this.containmentEdges.length;
      for (let nodeOrdinal = 0, edgeIndex = 0; nodeOrdinal < nodeCount; ++nodeOrdinal) {
        firstEdgeIndexes[nodeOrdinal] = edgeIndex;
        edgeIndex += nodes[nodeOrdinal * nodeFieldCount + nodeEdgeCountOffset] * edgeFieldsCount;
      }
    }

    _buildRetainers() {
      const retainingNodes = this._retainingNodes;
      const retainingEdges = this._retainingEdges;
      // Index of the first retainer in the _retainingNodes and _retainingEdges
      // arrays. Addressed by retained node index.
      const firstRetainerIndex = this._firstRetainerIndex;

      const containmentEdges = this.containmentEdges;
      const edgeFieldsCount = this._edgeFieldsCount;
      const nodeFieldCount = this._nodeFieldCount;
      const edgeToNodeOffset = this._edgeToNodeOffset;
      const firstEdgeIndexes = this._firstEdgeIndexes;
      const nodeCount = this.nodeCount;

      for (let toNodeFieldIndex = edgeToNodeOffset, l = containmentEdges.length; toNodeFieldIndex < l;
           toNodeFieldIndex += edgeFieldsCount) {
        const toNodeIndex = containmentEdges[toNodeFieldIndex];
        if (toNodeIndex % nodeFieldCount) {
          throw new Error('Invalid toNodeIndex ' + toNodeIndex);
        }
        ++firstRetainerIndex[toNodeIndex / nodeFieldCount];
      }
      for (let i = 0, firstUnusedRetainerSlot = 0; i < nodeCount; i++) {
        const retainersCount = firstRetainerIndex[i];
        firstRetainerIndex[i] = firstUnusedRetainerSlot;
        retainingNodes[firstUnusedRetainerSlot] = retainersCount;
        firstUnusedRetainerSlot += retainersCount;
      }
      firstRetainerIndex[nodeCount] = retainingNodes.length;

      let nextNodeFirstEdgeIndex = firstEdgeIndexes[0];
      for (let srcNodeOrdinal = 0; srcNodeOrdinal < nodeCount; ++srcNodeOrdinal) {
        const firstEdgeIndex = nextNodeFirstEdgeIndex;
        nextNodeFirstEdgeIndex = firstEdgeIndexes[srcNodeOrdinal + 1];
        const srcNodeIndex = srcNodeOrdinal * nodeFieldCount;
        for (let edgeIndex = firstEdgeIndex; edgeIndex < nextNodeFirstEdgeIndex; edgeIndex += edgeFieldsCount) {
          const toNodeIndex = containmentEdges[edgeIndex + edgeToNodeOffset];
          if (toNodeIndex % nodeFieldCount) {
            throw new Error('Invalid toNodeIndex ' + toNodeIndex);
          }
          const firstRetainerSlotIndex = firstRetainerIndex[toNodeIndex / nodeFieldCount];
          const nextUnusedRetainerSlotIndex = firstRetainerSlotIndex + (--retainingNodes[firstRetainerSlotIndex]);
          retainingNodes[nextUnusedRetainerSlotIndex] = srcNodeIndex;
          retainingEdges[nextUnusedRetainerSlotIndex] = edgeIndex;
        }
      }
    }

    /**
     * @param {number=} nodeIndex
     */
    createNode(nodeIndex) {
      throw new Error('Not implemented');
    }

    /**
     * @param {number} edgeIndex
     * @return {!JSHeapSnapshotEdge}
     */
    createEdge(edgeIndex) {
      throw new Error('Not implemented');
    }

    /**
     * @param {number} retainerIndex
     * @return {!JSHeapSnapshotRetainerEdge}
     */
    createRetainingEdge(retainerIndex) {
      throw new Error('Not implemented');
    }

    /**
     * @return {!HeapSnapshotNodeIterator}
     */
    _allNodes() {
      return new HeapSnapshotNodeIterator(this.rootNode());
    }

    /**
     * @return {!HeapSnapshotNode}
     */
    rootNode() {
      return this.createNode(this._rootNodeIndex);
    }

    /**
     * @return {number}
     */
    get rootNodeIndex() {
      return this._rootNodeIndex;
    }

    /**
     * @return {number}
     */
    get totalSize() {
      return this.rootNode().retainedSize();
    }

    /**
     * @param {number} nodeIndex
     * @return {number}
     */
    _getDominatedIndex(nodeIndex) {
      if (nodeIndex % this._nodeFieldCount) {
        throw new Error('Invalid nodeIndex: ' + nodeIndex);
      }
      return this._firstDominatedNodeIndex[nodeIndex / this._nodeFieldCount];
    }

    /**
     * @param {!NodeFilter} nodeFilter
     * @return {undefined|function(!HeapSnapshotNode):boolean}
     */
    _createFilter(nodeFilter) {
      const minNodeId = nodeFilter.minNodeId;
      const maxNodeId = nodeFilter.maxNodeId;
      const allocationNodeId = nodeFilter.allocationNodeId;
      let filter;
      if (typeof allocationNodeId === 'number') {
        filter = this._createAllocationStackFilter(allocationNodeId);
        filter.key = 'AllocationNodeId: ' + allocationNodeId;
      } else if (typeof minNodeId === 'number' && typeof maxNodeId === 'number') {
        filter = this._createNodeIdFilter(minNodeId, maxNodeId);
        filter.key = 'NodeIdRange: ' + minNodeId + '..' + maxNodeId;
      }
      return filter;
    }

    /**
     * @param {!SearchConfig} searchConfig
     * @param {!NodeFilter} nodeFilter
     * @return {!Array.<number>}
     */
    search(searchConfig, nodeFilter) {
      const query = searchConfig.query;

      function filterString(matchedStringIndexes, string, index) {
        if (string.indexOf(query) !== -1) {
          matchedStringIndexes.add(index);
        }
        return matchedStringIndexes;
      }

      const regexp = searchConfig.isRegex ? new RegExp(query) : createPlainTextSearchRegex(query, 'i');
      function filterRegexp(matchedStringIndexes, string, index) {
        if (regexp.test(string)) {
          matchedStringIndexes.add(index);
        }
        return matchedStringIndexes;
      }

      const stringFilter = (searchConfig.isRegex || !searchConfig.caseSensitive) ? filterRegexp : filterString;
      const stringIndexes = this.strings.reduce(stringFilter, new Set());

      if (!stringIndexes.size) {
        return [];
      }

      const filter = this._createFilter(nodeFilter);
      const nodeIds = [];
      const nodesLength = this.nodes.length;
      const nodes = this.nodes;
      const nodeNameOffset = this._nodeNameOffset;
      const nodeIdOffset = this._nodeIdOffset;
      const nodeFieldCount = this._nodeFieldCount;
      const node = this.rootNode();

      for (let nodeIndex = 0; nodeIndex < nodesLength; nodeIndex += nodeFieldCount) {
        node.nodeIndex = nodeIndex;
        if (filter && !filter(node)) {
          continue;
        }
        if (stringIndexes.has(nodes[nodeIndex + nodeNameOffset])) {
          nodeIds.push(nodes[nodeIndex + nodeIdOffset]);
        }
      }
      return nodeIds;
    }

    /**
     * @param {!NodeFilter} nodeFilter
     * @return {!Object.<string, !Aggregate>}
     */
    aggregatesWithFilter(nodeFilter) {
      const filter = this._createFilter(nodeFilter);
      const key = filter ? filter.key : 'allObjects';
      return this.aggregates(false, key, filter);
    }

    /**
     * @param {number} minNodeId
     * @param {number} maxNodeId
     * @return {function(!HeapSnapshotNode):boolean}
     */
    _createNodeIdFilter(minNodeId, maxNodeId) {
      /**
       * @param {!HeapSnapshotNode} node
       * @return {boolean}
       */
      function nodeIdFilter(node) {
        const id = node.id();
        return id > minNodeId && id <= maxNodeId;
      }
      return nodeIdFilter;
    }

    /**
     * @param {number} bottomUpAllocationNodeId
     * @return {function(!HeapSnapshotNode):boolean|undefined}
     */
    _createAllocationStackFilter(bottomUpAllocationNodeId) {
      const traceIds = this._allocationProfile.traceIds(bottomUpAllocationNodeId);
      if (!traceIds.length) {
        return undefined;
      }
      const set = {};
      for (let i = 0; i < traceIds.length; i++) {
        set[traceIds[i]] = true;
      }
      /**
       * @param {!HeapSnapshotNode} node
       * @return {boolean}
       */
      function traceIdFilter(node) {
        return !!set[node.traceNodeId()];
      }
      return traceIdFilter;
    }

    /**
     * @param {boolean} sortedIndexes
     * @param {string=} key
     * @param {function(!HeapSnapshotNode):boolean=} filter
     * @return {!Object.<string, !Aggregate>}
     */
    aggregates(sortedIndexes, key, filter) {
      let aggregatesByClassName = key && this._aggregates[key];
      if (!aggregatesByClassName) {
        const aggregates = this._buildAggregates(filter);
        this._calculateClassesRetainedSize(aggregates.aggregatesByClassIndex, filter);
        aggregatesByClassName = aggregates.aggregatesByClassName;
        if (key) {
          this._aggregates[key] = aggregatesByClassName;
        }
      }

      if (sortedIndexes && (!key || !this._aggregatesSortedFlags[key])) {
        this._sortAggregateIndexes(aggregatesByClassName);
        if (key) {
          this._aggregatesSortedFlags[key] = sortedIndexes;
        }
      }
      return aggregatesByClassName;
    }

    /**
     * @return {!Array.<!SerializedAllocationNode>}
     */
    allocationTracesTops() {
      return this._allocationProfile.serializeTraceTops();
    }

    /**
     * @param {number} nodeId
     * @return {!AllocationNodeCallers}
     */
    allocationNodeCallers(nodeId) {
      return this._allocationProfile.serializeCallers(nodeId);
    }

    /**
     * @param {number} nodeIndex
     * @return {?Array.<!AllocationStackFrame>}
     */
    allocationStack(nodeIndex) {
      const node = this.createNode(nodeIndex);
      const allocationNodeId = node.traceNodeId();
      if (!allocationNodeId) {
        return null;
      }
      return this._allocationProfile.serializeAllocationStack(allocationNodeId);
    }

    /**
     * @return {!Object.<string, !AggregateForDiff>}
     */
    aggregatesForDiff() {
      if (this._aggregatesForDiff) {
        return this._aggregatesForDiff;
      }

      const aggregatesByClassName = this.aggregates(true, 'allObjects');
      this._aggregatesForDiff = {};

      const node = this.createNode();
      for (const className in aggregatesByClassName) {
        const aggregate = aggregatesByClassName[className];
        const indexes = aggregate.idxs;
        const ids = new Array(indexes.length);
        const selfSizes = new Array(indexes.length);
        for (let i = 0; i < indexes.length; i++) {
          node.nodeIndex = indexes[i];
          ids[i] = node.id();
          selfSizes[i] = node.selfSize();
        }

        this._aggregatesForDiff[className] = {indexes: indexes, ids: ids, selfSizes: selfSizes};
      }
      return this._aggregatesForDiff;
    }

    /**
     * @protected
     * @param {!HeapSnapshotNode} node
     * @return {boolean}
     */
    isUserRoot(node) {
      return true;
    }

    /**
     * @param {function(!HeapSnapshotNode,!HeapSnapshotEdge):boolean=} filter
     */
    calculateDistances(filter) {
      const nodeCount = this.nodeCount;
      const distances = this._nodeDistances;
      const noDistance = this._noDistance;
      for (let i = 0; i < nodeCount; ++i) {
        distances[i] = noDistance;
      }

      const nodesToVisit = new Uint32Array(this.nodeCount);
      let nodesToVisitLength = 0;

      // BFS for user root objects.
      for (let iter = this.rootNode().edges(); iter.hasNext(); iter.next()) {
        const node = iter.edge.node();
        if (this.isUserRoot(node)) {
          distances[node.ordinal()] = 1;
          nodesToVisit[nodesToVisitLength++] = node.nodeIndex;
        }
      }
      this._bfs(nodesToVisit, nodesToVisitLength, distances, filter);

      // BFS for objects not reached from user roots.
      distances[this.rootNode().ordinal()] =
          nodesToVisitLength > 0 ? baseSystemDistance : 0;
      nodesToVisit[0] = this.rootNode().nodeIndex;
      nodesToVisitLength = 1;
      this._bfs(nodesToVisit, nodesToVisitLength, distances, filter);
    }

    /**
     * @param {!Uint32Array} nodesToVisit
     * @param {number} nodesToVisitLength
     * @param {!Int32Array} distances
     * @param {function(!HeapSnapshotNode,!HeapSnapshotEdge):boolean=} filter
     */
    _bfs(nodesToVisit, nodesToVisitLength, distances, filter) {
      // Preload fields into local variables for better performance.
      const edgeFieldsCount = this._edgeFieldsCount;
      const nodeFieldCount = this._nodeFieldCount;
      const containmentEdges = this.containmentEdges;
      const firstEdgeIndexes = this._firstEdgeIndexes;
      const edgeToNodeOffset = this._edgeToNodeOffset;
      const edgeTypeOffset = this._edgeTypeOffset;
      const nodeCount = this.nodeCount;
      const edgeWeakType = this._edgeWeakType;
      const noDistance = this._noDistance;

      let index = 0;
      const edge = this.createEdge(0);
      const node = this.createNode(0);
      while (index < nodesToVisitLength) {
        const nodeIndex = nodesToVisit[index++];  // shift generates too much garbage.
        const nodeOrdinal = nodeIndex / nodeFieldCount;
        const distance = distances[nodeOrdinal] + 1;
        const firstEdgeIndex = firstEdgeIndexes[nodeOrdinal];
        const edgesEnd = firstEdgeIndexes[nodeOrdinal + 1];
        node.nodeIndex = nodeIndex;
        for (let edgeIndex = firstEdgeIndex; edgeIndex < edgesEnd; edgeIndex += edgeFieldsCount) {
          const edgeType = containmentEdges[edgeIndex + edgeTypeOffset];
          if (edgeType === edgeWeakType) {
            continue;
          }
          const childNodeIndex = containmentEdges[edgeIndex + edgeToNodeOffset];
          const childNodeOrdinal = childNodeIndex / nodeFieldCount;
          if (distances[childNodeOrdinal] !== noDistance) {
            continue;
          }
          edge.edgeIndex = edgeIndex;
          if (filter && !filter(node, edge)) {
            continue;
          }
          distances[childNodeOrdinal] = distance;
          nodesToVisit[nodesToVisitLength++] = childNodeIndex;
        }
      }
      if (nodesToVisitLength > nodeCount) {
        throw new Error(
            'BFS failed. Nodes to visit (' + nodesToVisitLength + ') is more than nodes count (' + nodeCount + ')');
      }
    }

    /**
     * @param {function(!HeapSnapshotNode):boolean=} filter
     * @return {!{aggregatesByClassName: !Object<string, !AggregatedInfo>,
     *     aggregatesByClassIndex: !Object<number, !AggregatedInfo>}}
     */
    _buildAggregates(filter) {
      const aggregates = {};
      const aggregatesByClassName = {};
      const classIndexes = [];
      const nodes = this.nodes;
      const nodesLength = nodes.length;
      const nodeNativeType = this._nodeNativeType;
      const nodeFieldCount = this._nodeFieldCount;
      const selfSizeOffset = this._nodeSelfSizeOffset;
      const nodeTypeOffset = this._nodeTypeOffset;
      const node = this.rootNode();
      const nodeDistances = this._nodeDistances;

      for (let nodeIndex = 0; nodeIndex < nodesLength; nodeIndex += nodeFieldCount) {
        node.nodeIndex = nodeIndex;
        if (filter && !filter(node)) {
          continue;
        }
        const selfSize = nodes[nodeIndex + selfSizeOffset];
        if (!selfSize && nodes[nodeIndex + nodeTypeOffset] !== nodeNativeType) {
          continue;
        }
        const classIndex = node.classIndex();
        const nodeOrdinal = nodeIndex / nodeFieldCount;
        const distance = nodeDistances[nodeOrdinal];
        if (!(classIndex in aggregates)) {
          const nodeType = node.type();
          const nameMatters = nodeType === 'object' || nodeType === 'native';
          const value = {
            count: 1,
            distance: distance,
            self: selfSize,
            maxRet: 0,
            type: nodeType,
            name: nameMatters ? node.name() : null,
            idxs: [nodeIndex]
          };
          aggregates[classIndex] = value;
          classIndexes.push(classIndex);
          aggregatesByClassName[node.className()] = value;
        } else {
          const clss = aggregates[classIndex];
          clss.distance = Math.min(clss.distance, distance);
          ++clss.count;
          clss.self += selfSize;
          clss.idxs.push(nodeIndex);
        }
      }

      // Shave off provisionally allocated space.
      for (let i = 0, l = classIndexes.length; i < l; ++i) {
        const classIndex = classIndexes[i];
        aggregates[classIndex].idxs = aggregates[classIndex].idxs.slice();
      }
      return {aggregatesByClassName: aggregatesByClassName, aggregatesByClassIndex: aggregates};
    }

    /**
     * @param {!Object<number, !AggregatedInfo>} aggregates
     * @param {function(!HeapSnapshotNode):boolean=} filter
     */
    _calculateClassesRetainedSize(aggregates, filter) {
      const rootNodeIndex = this._rootNodeIndex;
      const node = this.createNode(rootNodeIndex);
      const list = [rootNodeIndex];
      const sizes = [-1];
      const classes = [];
      const seenClassNameIndexes = {};
      const nodeFieldCount = this._nodeFieldCount;
      const nodeTypeOffset = this._nodeTypeOffset;
      const nodeNativeType = this._nodeNativeType;
      const dominatedNodes = this._dominatedNodes;
      const nodes = this.nodes;
      const firstDominatedNodeIndex = this._firstDominatedNodeIndex;

      while (list.length) {
        const nodeIndex = list.pop();
        node.nodeIndex = nodeIndex;
        let classIndex = node.classIndex();
        const seen = !!seenClassNameIndexes[classIndex];
        const nodeOrdinal = nodeIndex / nodeFieldCount;
        const dominatedIndexFrom = firstDominatedNodeIndex[nodeOrdinal];
        const dominatedIndexTo = firstDominatedNodeIndex[nodeOrdinal + 1];

        if (!seen && (!filter || filter(node)) &&
            (node.selfSize() || nodes[nodeIndex + nodeTypeOffset] === nodeNativeType)) {
          aggregates[classIndex].maxRet += node.retainedSize();
          if (dominatedIndexFrom !== dominatedIndexTo) {
            seenClassNameIndexes[classIndex] = true;
            sizes.push(list.length);
            classes.push(classIndex);
          }
        }
        for (let i = dominatedIndexFrom; i < dominatedIndexTo; i++) {
          list.push(dominatedNodes[i]);
        }

        const l = list.length;
        while (sizes[sizes.length - 1] === l) {
          sizes.pop();
          classIndex = classes.pop();
          seenClassNameIndexes[classIndex] = false;
        }
      }
    }

    /**
     * @param {!{aggregatesByClassName: !Object<string, !AggregatedInfo>, aggregatesByClassIndex: !Object<number, !AggregatedInfo>}} aggregates
     */
    _sortAggregateIndexes(aggregates) {
      const nodeA = this.createNode();
      const nodeB = this.createNode();
      for (const clss in aggregates) {
        aggregates[clss].idxs.sort((idxA, idxB) => {
          nodeA.nodeIndex = idxA;
          nodeB.nodeIndex = idxB;
          return nodeA.id() < nodeB.id() ? -1 : 1;
        });
      }
    }

    /**
     * The function checks is the edge should be considered during building
     * postorder iterator and dominator tree.
     *
     * @param {number} nodeIndex
     * @param {number} edgeType
     * @return {boolean}
     */
    _isEssentialEdge(nodeIndex, edgeType) {
      // Shortcuts at the root node have special meaning of marking user global objects.
      return edgeType !== this._edgeWeakType &&
          (edgeType !== this._edgeShortcutType || nodeIndex === this._rootNodeIndex);
    }

    _buildPostOrderIndex() {
      const nodeFieldCount = this._nodeFieldCount;
      const nodeCount = this.nodeCount;
      const rootNodeOrdinal = this._rootNodeIndex / nodeFieldCount;

      const edgeFieldsCount = this._edgeFieldsCount;
      const edgeTypeOffset = this._edgeTypeOffset;
      const edgeToNodeOffset = this._edgeToNodeOffset;
      const firstEdgeIndexes = this._firstEdgeIndexes;
      const containmentEdges = this.containmentEdges;

      const mapAndFlag = this.userObjectsMapAndFlag();
      const flags = mapAndFlag ? mapAndFlag.map : null;
      const flag = mapAndFlag ? mapAndFlag.flag : 0;

      const stackNodes = new Uint32Array(nodeCount);
      const stackCurrentEdge = new Uint32Array(nodeCount);
      const postOrderIndex2NodeOrdinal = new Uint32Array(nodeCount);
      const nodeOrdinal2PostOrderIndex = new Uint32Array(nodeCount);
      const visited = new Uint8Array(nodeCount);
      let postOrderIndex = 0;

      let stackTop = 0;
      stackNodes[0] = rootNodeOrdinal;
      stackCurrentEdge[0] = firstEdgeIndexes[rootNodeOrdinal];
      visited[rootNodeOrdinal] = 1;

      let iteration = 0;
      while (true) {
        ++iteration;
        while (stackTop >= 0) {
          const nodeOrdinal = stackNodes[stackTop];
          const edgeIndex = stackCurrentEdge[stackTop];
          const edgesEnd = firstEdgeIndexes[nodeOrdinal + 1];

          if (edgeIndex < edgesEnd) {
            stackCurrentEdge[stackTop] += edgeFieldsCount;
            const edgeType = containmentEdges[edgeIndex + edgeTypeOffset];
            if (!this._isEssentialEdge(nodeOrdinal * nodeFieldCount, edgeType)) {
              continue;
            }
            const childNodeIndex = containmentEdges[edgeIndex + edgeToNodeOffset];
            const childNodeOrdinal = childNodeIndex / nodeFieldCount;
            if (visited[childNodeOrdinal]) {
              continue;
            }
            const nodeFlag = !flags || (flags[nodeOrdinal] & flag);
            const childNodeFlag = !flags || (flags[childNodeOrdinal] & flag);
            // We are skipping the edges from non-page-owned nodes to page-owned nodes.
            // Otherwise the dominators for the objects that also were retained by debugger would be affected.
            if (nodeOrdinal !== rootNodeOrdinal && childNodeFlag && !nodeFlag) {
              continue;
            }
            ++stackTop;
            stackNodes[stackTop] = childNodeOrdinal;
            stackCurrentEdge[stackTop] = firstEdgeIndexes[childNodeOrdinal];
            visited[childNodeOrdinal] = 1;
          } else {
            // Done with all the node children
            nodeOrdinal2PostOrderIndex[nodeOrdinal] = postOrderIndex;
            postOrderIndex2NodeOrdinal[postOrderIndex++] = nodeOrdinal;
            --stackTop;
          }
        }

        if (postOrderIndex === nodeCount || iteration > 1) {
          break;
        }
        const errors = new HeapSnapshotProblemReport(`Heap snapshot: ${
          nodeCount - postOrderIndex} nodes are unreachable from the root. Following nodes have only weak retainers:`);
        const dumpNode = this.rootNode();
        // Remove root from the result (last node in the array) and put it at the bottom of the stack so that it is
        // visited after all orphan nodes and their subgraphs.
        --postOrderIndex;
        stackTop = 0;
        stackNodes[0] = rootNodeOrdinal;
        stackCurrentEdge[0] = firstEdgeIndexes[rootNodeOrdinal + 1];  // no need to reiterate its edges
        for (let i = 0; i < nodeCount; ++i) {
          if (visited[i] || !this._hasOnlyWeakRetainers(i)) {
            continue;
          }

          // Add all nodes that have only weak retainers to traverse their subgraphs.
          stackNodes[++stackTop] = i;
          stackCurrentEdge[stackTop] = firstEdgeIndexes[i];
          visited[i] = 1;

          dumpNode.nodeIndex = i * nodeFieldCount;
          const retainers = [];
          for (let it = dumpNode.retainers(); it.hasNext(); it.next()) {
            retainers.push(`${it.item().node().name()}@${it.item().node().id()}.${it.item().name()}`);
          }
          errors.addError(`${dumpNode.name()} @${dumpNode.id()}  weak retainers: ${retainers.join(', ')}`);
        }
        console.warn(errors.toString());
      }

      // If we already processed all orphan nodes that have only weak retainers and still have some orphans...
      if (postOrderIndex !== nodeCount) {
        const errors = new HeapSnapshotProblemReport(
            'Still found ' + (nodeCount - postOrderIndex) + ' unreachable nodes in heap snapshot:');
        const dumpNode = this.rootNode();
        // Remove root from the result (last node in the array) and put it at the bottom of the stack so that it is
        // visited after all orphan nodes and their subgraphs.
        --postOrderIndex;
        for (let i = 0; i < nodeCount; ++i) {
          if (visited[i]) {
            continue;
          }
          dumpNode.nodeIndex = i * nodeFieldCount;
          errors.addError(dumpNode.name() + ' @' + dumpNode.id());
          // Fix it by giving the node a postorder index anyway.
          nodeOrdinal2PostOrderIndex[i] = postOrderIndex;
          postOrderIndex2NodeOrdinal[postOrderIndex++] = i;
        }
        nodeOrdinal2PostOrderIndex[rootNodeOrdinal] = postOrderIndex;
        postOrderIndex2NodeOrdinal[postOrderIndex++] = rootNodeOrdinal;
        console.warn(errors.toString());
      }

      return {
        postOrderIndex2NodeOrdinal: postOrderIndex2NodeOrdinal,
        nodeOrdinal2PostOrderIndex: nodeOrdinal2PostOrderIndex
      };
    }

    /**
     * @param {number} nodeOrdinal
     * @return {boolean}
     */
    _hasOnlyWeakRetainers(nodeOrdinal) {
      const edgeTypeOffset = this._edgeTypeOffset;
      const edgeWeakType = this._edgeWeakType;
      const edgeShortcutType = this._edgeShortcutType;
      const containmentEdges = this.containmentEdges;
      const retainingEdges = this._retainingEdges;
      const beginRetainerIndex = this._firstRetainerIndex[nodeOrdinal];
      const endRetainerIndex = this._firstRetainerIndex[nodeOrdinal + 1];
      for (let retainerIndex = beginRetainerIndex; retainerIndex < endRetainerIndex; ++retainerIndex) {
        const retainerEdgeIndex = retainingEdges[retainerIndex];
        const retainerEdgeType = containmentEdges[retainerEdgeIndex + edgeTypeOffset];
        if (retainerEdgeType !== edgeWeakType && retainerEdgeType !== edgeShortcutType) {
          return false;
        }
      }
      return true;
    }

    // The algorithm is based on the article:
    // K. Cooper, T. Harvey and K. Kennedy "A Simple, Fast Dominance Algorithm"
    // Softw. Pract. Exper. 4 (2001), pp. 1-10.
    /**
     * @param {!Array.<number>} postOrderIndex2NodeOrdinal
     * @param {!Array.<number>} nodeOrdinal2PostOrderIndex
     */
    _buildDominatorTree(postOrderIndex2NodeOrdinal, nodeOrdinal2PostOrderIndex) {
      const nodeFieldCount = this._nodeFieldCount;
      const firstRetainerIndex = this._firstRetainerIndex;
      const retainingNodes = this._retainingNodes;
      const retainingEdges = this._retainingEdges;
      const edgeFieldsCount = this._edgeFieldsCount;
      const edgeTypeOffset = this._edgeTypeOffset;
      const edgeToNodeOffset = this._edgeToNodeOffset;
      const firstEdgeIndexes = this._firstEdgeIndexes;
      const containmentEdges = this.containmentEdges;
      const rootNodeIndex = this._rootNodeIndex;

      const mapAndFlag = this.userObjectsMapAndFlag();
      const flags = mapAndFlag ? mapAndFlag.map : null;
      const flag = mapAndFlag ? mapAndFlag.flag : 0;

      const nodesCount = postOrderIndex2NodeOrdinal.length;
      const rootPostOrderedIndex = nodesCount - 1;
      const noEntry = nodesCount;
      const dominators = new Uint32Array(nodesCount);
      for (let i = 0; i < rootPostOrderedIndex; ++i) {
        dominators[i] = noEntry;
      }
      dominators[rootPostOrderedIndex] = rootPostOrderedIndex;

      // The affected array is used to mark entries which dominators
      // have to be racalculated because of changes in their retainers.
      const affected = new Uint8Array(nodesCount);
      let nodeOrdinal;

      {  // Mark the root direct children as affected.
        nodeOrdinal = this._rootNodeIndex / nodeFieldCount;
        const endEdgeIndex = firstEdgeIndexes[nodeOrdinal + 1];
        for (let edgeIndex = firstEdgeIndexes[nodeOrdinal]; edgeIndex < endEdgeIndex; edgeIndex += edgeFieldsCount) {
          const edgeType = containmentEdges[edgeIndex + edgeTypeOffset];
          if (!this._isEssentialEdge(this._rootNodeIndex, edgeType)) {
            continue;
          }
          const childNodeOrdinal = containmentEdges[edgeIndex + edgeToNodeOffset] / nodeFieldCount;
          affected[nodeOrdinal2PostOrderIndex[childNodeOrdinal]] = 1;
        }
      }

      let changed = true;
      while (changed) {
        changed = false;
        for (let postOrderIndex = rootPostOrderedIndex - 1; postOrderIndex >= 0; --postOrderIndex) {
          if (affected[postOrderIndex] === 0) {
            continue;
          }
          affected[postOrderIndex] = 0;
          // If dominator of the entry has already been set to root,
          // then it can't propagate any further.
          if (dominators[postOrderIndex] === rootPostOrderedIndex) {
            continue;
          }
          nodeOrdinal = postOrderIndex2NodeOrdinal[postOrderIndex];
          const nodeFlag = !flags || (flags[nodeOrdinal] & flag);
          let newDominatorIndex = noEntry;
          const beginRetainerIndex = firstRetainerIndex[nodeOrdinal];
          const endRetainerIndex = firstRetainerIndex[nodeOrdinal + 1];
          let orphanNode = true;
          for (let retainerIndex = beginRetainerIndex; retainerIndex < endRetainerIndex; ++retainerIndex) {
            const retainerEdgeIndex = retainingEdges[retainerIndex];
            const retainerEdgeType = containmentEdges[retainerEdgeIndex + edgeTypeOffset];
            const retainerNodeIndex = retainingNodes[retainerIndex];
            if (!this._isEssentialEdge(retainerNodeIndex, retainerEdgeType)) {
              continue;
            }
            orphanNode = false;
            const retainerNodeOrdinal = retainerNodeIndex / nodeFieldCount;
            const retainerNodeFlag = !flags || (flags[retainerNodeOrdinal] & flag);
            // We are skipping the edges from non-page-owned nodes to page-owned nodes.
            // Otherwise the dominators for the objects that also were retained by debugger would be affected.
            if (retainerNodeIndex !== rootNodeIndex && nodeFlag && !retainerNodeFlag) {
              continue;
            }
            let retanerPostOrderIndex = nodeOrdinal2PostOrderIndex[retainerNodeOrdinal];
            if (dominators[retanerPostOrderIndex] !== noEntry) {
              if (newDominatorIndex === noEntry) {
                newDominatorIndex = retanerPostOrderIndex;
              } else {
                while (retanerPostOrderIndex !== newDominatorIndex) {
                  while (retanerPostOrderIndex < newDominatorIndex) {
                    retanerPostOrderIndex = dominators[retanerPostOrderIndex];
                  }
                  while (newDominatorIndex < retanerPostOrderIndex) {
                    newDominatorIndex = dominators[newDominatorIndex];
                  }
                }
              }
              // If idom has already reached the root, it doesn't make sense
              // to check other retainers.
              if (newDominatorIndex === rootPostOrderedIndex) {
                break;
              }
            }
          }
          // Make root dominator of orphans.
          if (orphanNode) {
            newDominatorIndex = rootPostOrderedIndex;
          }
          if (newDominatorIndex !== noEntry && dominators[postOrderIndex] !== newDominatorIndex) {
            dominators[postOrderIndex] = newDominatorIndex;
            changed = true;
            nodeOrdinal = postOrderIndex2NodeOrdinal[postOrderIndex];
            const beginEdgeToNodeFieldIndex = firstEdgeIndexes[nodeOrdinal] + edgeToNodeOffset;
            const endEdgeToNodeFieldIndex = firstEdgeIndexes[nodeOrdinal + 1];
            for (let toNodeFieldIndex = beginEdgeToNodeFieldIndex; toNodeFieldIndex < endEdgeToNodeFieldIndex;
                 toNodeFieldIndex += edgeFieldsCount) {
              const childNodeOrdinal = containmentEdges[toNodeFieldIndex] / nodeFieldCount;
              affected[nodeOrdinal2PostOrderIndex[childNodeOrdinal]] = 1;
            }
          }
        }
      }

      const dominatorsTree = new Uint32Array(nodesCount);
      for (let postOrderIndex = 0, l = dominators.length; postOrderIndex < l; ++postOrderIndex) {
        nodeOrdinal = postOrderIndex2NodeOrdinal[postOrderIndex];
        dominatorsTree[nodeOrdinal] = postOrderIndex2NodeOrdinal[dominators[postOrderIndex]];
      }
      return dominatorsTree;
    }

    /**
     * @param {!Array<number>} postOrderIndex2NodeOrdinal
     */
    _calculateRetainedSizes(postOrderIndex2NodeOrdinal) {
      const nodeCount = this.nodeCount;
      const nodes = this.nodes;
      const nodeSelfSizeOffset = this._nodeSelfSizeOffset;
      const nodeFieldCount = this._nodeFieldCount;
      const dominatorsTree = this._dominatorsTree;
      const retainedSizes = this._retainedSizes;

      for (let nodeOrdinal = 0; nodeOrdinal < nodeCount; ++nodeOrdinal) {
        retainedSizes[nodeOrdinal] = nodes[nodeOrdinal * nodeFieldCount + nodeSelfSizeOffset];
      }

      // Propagate retained sizes for each node excluding root.
      for (let postOrderIndex = 0; postOrderIndex < nodeCount - 1; ++postOrderIndex) {
        const nodeOrdinal = postOrderIndex2NodeOrdinal[postOrderIndex];
        const dominatorOrdinal = dominatorsTree[nodeOrdinal];
        retainedSizes[dominatorOrdinal] += retainedSizes[nodeOrdinal];
      }
    }

    _buildDominatedNodes() {
      // Builds up two arrays:
      //  - "dominatedNodes" is a continuous array, where each node owns an
      //    interval (can be empty) with corresponding dominated nodes.
      //  - "indexArray" is an array of indexes in the "dominatedNodes"
      //    with the same positions as in the _nodeIndex.
      const indexArray = this._firstDominatedNodeIndex;
      // All nodes except the root have dominators.
      const dominatedNodes = this._dominatedNodes;

      // Count the number of dominated nodes for each node. Skip the root (node at
      // index 0) as it is the only node that dominates itself.
      const nodeFieldCount = this._nodeFieldCount;
      const dominatorsTree = this._dominatorsTree;

      let fromNodeOrdinal = 0;
      let toNodeOrdinal = this.nodeCount;
      const rootNodeOrdinal = this._rootNodeIndex / nodeFieldCount;
      if (rootNodeOrdinal === fromNodeOrdinal) {
        fromNodeOrdinal = 1;
      } else if (rootNodeOrdinal === toNodeOrdinal - 1) {
        toNodeOrdinal = toNodeOrdinal - 1;
      } else {
        throw new Error('Root node is expected to be either first or last');
      }
      for (let nodeOrdinal = fromNodeOrdinal; nodeOrdinal < toNodeOrdinal; ++nodeOrdinal) {
        ++indexArray[dominatorsTree[nodeOrdinal]];
      }
      // Put in the first slot of each dominatedNodes slice the count of entries
      // that will be filled.
      let firstDominatedNodeIndex = 0;
      for (let i = 0, l = this.nodeCount; i < l; ++i) {
        const dominatedCount = dominatedNodes[firstDominatedNodeIndex] = indexArray[i];
        indexArray[i] = firstDominatedNodeIndex;
        firstDominatedNodeIndex += dominatedCount;
      }
      indexArray[this.nodeCount] = dominatedNodes.length;
      // Fill up the dominatedNodes array with indexes of dominated nodes. Skip the root (node at
      // index 0) as it is the only node that dominates itself.
      for (let nodeOrdinal = fromNodeOrdinal; nodeOrdinal < toNodeOrdinal; ++nodeOrdinal) {
        const dominatorOrdinal = dominatorsTree[nodeOrdinal];
        let dominatedRefIndex = indexArray[dominatorOrdinal];
        dominatedRefIndex += (--dominatedNodes[dominatedRefIndex]);
        dominatedNodes[dominatedRefIndex] = nodeOrdinal * nodeFieldCount;
      }
    }

    _buildSamples() {
      const samples = this._rawSamples;
      if (!samples || !samples.length) {
        return;
      }
      const sampleCount = samples.length / 2;
      const sizeForRange = new Array(sampleCount);
      const timestamps = new Array(sampleCount);
      const lastAssignedIds = new Array(sampleCount);

      const timestampOffset = this._metaNode.sample_fields.indexOf('timestamp_us');
      const lastAssignedIdOffset = this._metaNode.sample_fields.indexOf('last_assigned_id');
      for (let i = 0; i < sampleCount; i++) {
        sizeForRange[i] = 0;
        timestamps[i] = (samples[2 * i + timestampOffset]) / 1000;
        lastAssignedIds[i] = samples[2 * i + lastAssignedIdOffset];
      }

      const nodes = this.nodes;
      const nodesLength = nodes.length;
      const nodeFieldCount = this._nodeFieldCount;
      const node = this.rootNode();
      for (let nodeIndex = 0; nodeIndex < nodesLength; nodeIndex += nodeFieldCount) {
        node.nodeIndex = nodeIndex;

        const nodeId = node.id();
        // JS objects have odd ids, skip native objects.
        if (nodeId % 2 === 0) {
          continue;
        }
        const rangeIndex = lastAssignedIds.lowerBound(nodeId);
        if (rangeIndex === sampleCount) {
          // TODO: make heap profiler not allocate while taking snapshot
          continue;
        }
        sizeForRange[rangeIndex] += node.selfSize();
      }
      this._samples = new Samples(timestamps, lastAssignedIds, sizeForRange);
    }

    _buildLocationMap() {
      /** @type {!Map<number, !Location>} */
      const map = new Map();
      const locations = this._locations;

      for (let i = 0; i < locations.length; i += this._locationFieldCount) {
        const nodeIndex = locations[i + this._locationIndexOffset];
        const scriptId = locations[i + this._locationScriptIdOffset];
        const line = locations[i + this._locationLineOffset];
        const col = locations[i + this._locationColumnOffset];
        map.set(nodeIndex, new Location(scriptId, line, col));
      }

      this._locationMap = map;
    }

    /**
     * @param {number} nodeIndex
     * @return {?Location}
     */
    getLocation(nodeIndex) {
      return this._locationMap.get(nodeIndex) || null;
    }

    /**
     * @return {?Samples}
     */
    getSamples() {
      return this._samples;
    }

    /**
     * @protected
     */
    calculateFlags() {
      throw new Error('Not implemented');
    }

    /**
     * @protected
     */
    calculateStatistics() {
      throw new Error('Not implemented');
    }

    userObjectsMapAndFlag() {
      throw new Error('Not implemented');
    }

    /**
     * @param {string} baseSnapshotId
     * @param {!Object.<string, !AggregateForDiff>} baseSnapshotAggregates
     * @return {!Object.<string, !Diff>}
     */
    calculateSnapshotDiff(baseSnapshotId, baseSnapshotAggregates) {
      let snapshotDiff = this._snapshotDiffs[baseSnapshotId];
      if (snapshotDiff) {
        return snapshotDiff;
      }
      snapshotDiff = {};

      const aggregates = this.aggregates(true, 'allObjects');
      for (const className in baseSnapshotAggregates) {
        const baseAggregate = baseSnapshotAggregates[className];
        const diff = this._calculateDiffForClass(baseAggregate, aggregates[className]);
        if (diff) {
          snapshotDiff[className] = diff;
        }
      }
      const emptyBaseAggregate = new AggregateForDiff();
      for (const className in aggregates) {
        if (className in baseSnapshotAggregates) {
          continue;
        }
        snapshotDiff[className] = this._calculateDiffForClass(emptyBaseAggregate, aggregates[className]);
      }

      this._snapshotDiffs[baseSnapshotId] = snapshotDiff;
      return snapshotDiff;
    }

    /**
     * @param {!AggregateForDiff} baseAggregate
     * @param {!Aggregate} aggregate
     * @return {?Diff}
     */
    _calculateDiffForClass(baseAggregate, aggregate) {
      const baseIds = baseAggregate.ids;
      const baseIndexes = baseAggregate.indexes;
      const baseSelfSizes = baseAggregate.selfSizes;

      const indexes = aggregate ? aggregate.idxs : [];

      let i = 0;
      let j = 0;
      const l = baseIds.length;
      const m = indexes.length;
      const diff = new Diff();

      const nodeB = this.createNode(indexes[j]);
      while (i < l && j < m) {
        const nodeAId = baseIds[i];
        if (nodeAId < nodeB.id()) {
          diff.deletedIndexes.push(baseIndexes[i]);
          diff.removedCount++;
          diff.removedSize += baseSelfSizes[i];
          ++i;
        } else if (
            nodeAId >
            nodeB.id()) {  // Native nodes(e.g. dom groups) may have ids less than max JS object id in the base snapshot
          diff.addedIndexes.push(indexes[j]);
          diff.addedCount++;
          diff.addedSize += nodeB.selfSize();
          nodeB.nodeIndex = indexes[++j];
        } else {  // nodeAId === nodeB.id()
          ++i;
          nodeB.nodeIndex = indexes[++j];
        }
      }
      while (i < l) {
        diff.deletedIndexes.push(baseIndexes[i]);
        diff.removedCount++;
        diff.removedSize += baseSelfSizes[i];
        ++i;
      }
      while (j < m) {
        diff.addedIndexes.push(indexes[j]);
        diff.addedCount++;
        diff.addedSize += nodeB.selfSize();
        nodeB.nodeIndex = indexes[++j];
      }
      diff.countDelta = diff.addedCount - diff.removedCount;
      diff.sizeDelta = diff.addedSize - diff.removedSize;
      if (!diff.addedCount && !diff.removedCount) {
        return null;
      }
      return diff;
    }

    _nodeForSnapshotObjectId(snapshotObjectId) {
      for (let it = this._allNodes(); it.hasNext(); it.next()) {
        if (it.node.id() === snapshotObjectId) {
          return it.node;
        }
      }
      return null;
    }

    /**
     * @param {string} snapshotObjectId
     * @return {?string}
     */
    nodeClassName(snapshotObjectId) {
      const node = this._nodeForSnapshotObjectId(snapshotObjectId);
      if (node) {
        return node.className();
      }
      return null;
    }

    /**
     * @param {string} name
     * @return {!Array.<number>}
     */
    idsOfObjectsWithName(name) {
      const ids = [];
      for (let it = this._allNodes(); it.hasNext(); it.next()) {
        if (it.item().name() === name) {
          ids.push(it.item().id());
        }
      }
      return ids;
    }

    /**
     * @param {number} nodeIndex
     * @return {!HeapSnapshotEdgesProvider}
     */
    createEdgesProvider(nodeIndex) {
      const node = this.createNode(nodeIndex);
      const filter = this.containmentEdgesFilter();
      const indexProvider = new HeapSnapshotEdgeIndexProvider(this);
      return new HeapSnapshotEdgesProvider(this, filter, node.edges(), indexProvider);
    }

    /**
     * @param {number} nodeIndex
     * @param {?function(!HeapSnapshotEdge):boolean} filter
     * @return {!HeapSnapshotEdgesProvider}
     */
    createEdgesProviderForTest(nodeIndex, filter) {
      const node = this.createNode(nodeIndex);
      const indexProvider = new HeapSnapshotEdgeIndexProvider(this);
      return new HeapSnapshotEdgesProvider(this, filter, node.edges(), indexProvider);
    }

    /**
     * @return {?function(!HeapSnapshotEdge):boolean}
     */
    retainingEdgesFilter() {
      return null;
    }

    /**
     * @return {?function(!HeapSnapshotEdge):boolean}
     */
    containmentEdgesFilter() {
      return null;
    }

    /**
     * @param {number} nodeIndex
     * @return {!HeapSnapshotEdgesProvider}
     */
    createRetainingEdgesProvider(nodeIndex) {
      const node = this.createNode(nodeIndex);
      const filter = this.retainingEdgesFilter();
      const indexProvider = new HeapSnapshotRetainerEdgeIndexProvider(this);
      return new HeapSnapshotEdgesProvider(this, filter, node.retainers(), indexProvider);
    }

    /**
     * @param {string} baseSnapshotId
     * @param {string} className
     * @return {!HeapSnapshotNodesProvider}
     */
    createAddedNodesProvider(baseSnapshotId, className) {
      const snapshotDiff = this._snapshotDiffs[baseSnapshotId];
      const diffForClass = snapshotDiff[className];
      return new HeapSnapshotNodesProvider(this, diffForClass.addedIndexes);
    }

    /**
     * @param {!Array.<number>} nodeIndexes
     * @return {!HeapSnapshotNodesProvider}
     */
    createDeletedNodesProvider(nodeIndexes) {
      return new HeapSnapshotNodesProvider(this, nodeIndexes);
    }

    /**
     * @param {string} className
     * @param {!NodeFilter} nodeFilter
     * @return {!HeapSnapshotNodesProvider}
     */
    createNodesProviderForClass(className, nodeFilter) {
      return new HeapSnapshotNodesProvider(this, this.aggregatesWithFilter(nodeFilter)[className].idxs);
    }

    /**
     * @return {number}
     */
    _maxJsNodeId() {
      const nodeFieldCount = this._nodeFieldCount;
      const nodes = this.nodes;
      const nodesLength = nodes.length;
      let id = 0;
      for (let nodeIndex = this._nodeIdOffset; nodeIndex < nodesLength; nodeIndex += nodeFieldCount) {
        const nextId = nodes[nodeIndex];
        // JS objects have odd ids, skip native objects.
        if (nextId % 2 === 0) {
          continue;
        }
        if (id < nextId) {
          id = nextId;
        }
      }
      return id;
    }

    /**
     * @return {!StaticData}
     */
    updateStaticData() {
      return new StaticData(
          this.nodeCount, this._rootNodeIndex, this.totalSize, this._maxJsNodeId());
    }
  }

  /**
   * @unrestricted
   */
  class HeapSnapshotItemProvider {
    /**
     * @param {!HeapSnapshotItemIterator} iterator
     * @param {!HeapSnapshotItemIndexProvider} indexProvider
     */
    constructor(iterator, indexProvider) {
      this._iterator = iterator;
      this._indexProvider = indexProvider;
      this._isEmpty = !iterator.hasNext();
      /** @type {?Array.<number>} */
      this._iterationOrder = null;
      this._currentComparator = null;
      this._sortedPrefixLength = 0;
      this._sortedSuffixLength = 0;
    }

    _createIterationOrder() {
      if (this._iterationOrder) {
        return;
      }
      this._iterationOrder = [];
      for (let iterator = this._iterator; iterator.hasNext(); iterator.next()) {
        this._iterationOrder.push(iterator.item().itemIndex());
      }
    }

    /**
     * @return {boolean}
     */
    isEmpty() {
      return this._isEmpty;
    }

    /**
     * @param {number} begin
     * @param {number} end
     * @return {!ItemsRange}
     */
    serializeItemsRange(begin, end) {
      this._createIterationOrder();
      if (begin > end) {
        throw new Error('Start position > end position: ' + begin + ' > ' + end);
      }
      if (end > this._iterationOrder.length) {
        end = this._iterationOrder.length;
      }
      if (this._sortedPrefixLength < end && begin < this._iterationOrder.length - this._sortedSuffixLength) {
        this.sort(
            this._currentComparator, this._sortedPrefixLength, this._iterationOrder.length - 1 - this._sortedSuffixLength,
            begin, end - 1);
        if (begin <= this._sortedPrefixLength) {
          this._sortedPrefixLength = end;
        }
        if (end >= this._iterationOrder.length - this._sortedSuffixLength) {
          this._sortedSuffixLength = this._iterationOrder.length - begin;
        }
      }
      let position = begin;
      const count = end - begin;
      const result = new Array(count);
      for (let i = 0; i < count; ++i) {
        const itemIndex = this._iterationOrder[position++];
        const item = this._indexProvider.itemForIndex(itemIndex);
        result[i] = item.serialize();
      }
      return new ItemsRange(begin, end, this._iterationOrder.length, result);
    }

    sortAndRewind(comparator) {
      this._currentComparator = comparator;
      this._sortedPrefixLength = 0;
      this._sortedSuffixLength = 0;
    }
  }

  /**
   * @unrestricted
   */
  class HeapSnapshotEdgesProvider extends HeapSnapshotItemProvider {
    /**
     * @param {!HeapSnapshot} snapshot
     * @param {?function(!HeapSnapshotEdge):boolean} filter
     * @param {!HeapSnapshotEdgeIterator} edgesIter
     * @param {!HeapSnapshotItemIndexProvider} indexProvider
     */
    constructor(snapshot, filter, edgesIter, indexProvider) {
      const iter = filter ?
          new HeapSnapshotFilteredIterator(edgesIter, /** @type {function(!HeapSnapshotItem):boolean} */ (filter)) :
          edgesIter;
      super(iter, indexProvider);
      this.snapshot = snapshot;
    }

    /**
     * @param {!ComparatorConfig} comparator
     * @param {number} leftBound
     * @param {number} rightBound
     * @param {number} windowLeft
     * @param {number} windowRight
     */
    sort(comparator, leftBound, rightBound, windowLeft, windowRight) {
      const fieldName1 = comparator.fieldName1;
      const fieldName2 = comparator.fieldName2;
      const ascending1 = comparator.ascending1;
      const ascending2 = comparator.ascending2;

      const edgeA = /** @type {!HeapSnapshotEdge} */ (this._iterator.item()).clone();
      const edgeB = edgeA.clone();
      const nodeA = this.snapshot.createNode();
      const nodeB = this.snapshot.createNode();

      function compareEdgeFieldName(ascending, indexA, indexB) {
        edgeA.edgeIndex = indexA;
        edgeB.edgeIndex = indexB;
        if (edgeB.name() === '__proto__') {
          return -1;
        }
        if (edgeA.name() === '__proto__') {
          return 1;
        }
        const result = edgeA.hasStringName() === edgeB.hasStringName() ?
            (edgeA.name() < edgeB.name() ? -1 : (edgeA.name() > edgeB.name() ? 1 : 0)) :
            (edgeA.hasStringName() ? -1 : 1);
        return ascending ? result : -result;
      }

      function compareNodeField(fieldName, ascending, indexA, indexB) {
        edgeA.edgeIndex = indexA;
        nodeA.nodeIndex = edgeA.nodeIndex();
        const valueA = nodeA[fieldName]();

        edgeB.edgeIndex = indexB;
        nodeB.nodeIndex = edgeB.nodeIndex();
        const valueB = nodeB[fieldName]();

        const result = valueA < valueB ? -1 : (valueA > valueB ? 1 : 0);
        return ascending ? result : -result;
      }

      function compareEdgeAndNode(indexA, indexB) {
        let result = compareEdgeFieldName(ascending1, indexA, indexB);
        if (result === 0) {
          result = compareNodeField(fieldName2, ascending2, indexA, indexB);
        }
        if (result === 0) {
          return indexA - indexB;
        }
        return result;
      }

      function compareNodeAndEdge(indexA, indexB) {
        let result = compareNodeField(fieldName1, ascending1, indexA, indexB);
        if (result === 0) {
          result = compareEdgeFieldName(ascending2, indexA, indexB);
        }
        if (result === 0) {
          return indexA - indexB;
        }
        return result;
      }

      function compareNodeAndNode(indexA, indexB) {
        let result = compareNodeField(fieldName1, ascending1, indexA, indexB);
        if (result === 0) {
          result = compareNodeField(fieldName2, ascending2, indexA, indexB);
        }
        if (result === 0) {
          return indexA - indexB;
        }
        return result;
      }

      if (fieldName1 === '!edgeName') {
        this._iterationOrder.sortRange(compareEdgeAndNode, leftBound, rightBound, windowLeft, windowRight);
      } else if (fieldName2 === '!edgeName') {
        this._iterationOrder.sortRange(compareNodeAndEdge, leftBound, rightBound, windowLeft, windowRight);
      } else {
        this._iterationOrder.sortRange(compareNodeAndNode, leftBound, rightBound, windowLeft, windowRight);
      }
    }
  }

  /**
   * @unrestricted
   */
  class HeapSnapshotNodesProvider extends HeapSnapshotItemProvider {
    /**
     * @param {!HeapSnapshot} snapshot
     * @param {!Array<number>|!Uint32Array} nodeIndexes
     */
    constructor(snapshot, nodeIndexes) {
      const indexProvider = new HeapSnapshotNodeIndexProvider(snapshot);
      const it = new HeapSnapshotIndexRangeIterator(indexProvider, nodeIndexes);
      super(it, indexProvider);
      this.snapshot = snapshot;
    }

    /**
     * @param {string} snapshotObjectId
     * @return {number}
     */
    nodePosition(snapshotObjectId) {
      this._createIterationOrder();
      const node = this.snapshot.createNode();
      let i = 0;
      for (; i < this._iterationOrder.length; i++) {
        node.nodeIndex = this._iterationOrder[i];
        if (node.id() === snapshotObjectId) {
          break;
        }
      }
      if (i === this._iterationOrder.length) {
        return -1;
      }
      const targetNodeIndex = this._iterationOrder[i];
      let smallerCount = 0;
      const compare = this._buildCompareFunction(this._currentComparator);
      for (let i = 0; i < this._iterationOrder.length; i++) {
        if (compare(this._iterationOrder[i], targetNodeIndex) < 0) {
          ++smallerCount;
        }
      }
      return smallerCount;
    }

    /**
     * @return {function(number,number):number}
     */
    _buildCompareFunction(comparator) {
      const nodeA = this.snapshot.createNode();
      const nodeB = this.snapshot.createNode();
      const fieldAccessor1 = nodeA[comparator.fieldName1];
      const fieldAccessor2 = nodeA[comparator.fieldName2];
      const ascending1 = comparator.ascending1 ? 1 : -1;
      const ascending2 = comparator.ascending2 ? 1 : -1;

      /**
       * @param {function():*} fieldAccessor
       * @param {number} ascending
       * @return {number}
       */
      function sortByNodeField(fieldAccessor, ascending) {
        const valueA = fieldAccessor.call(nodeA);
        const valueB = fieldAccessor.call(nodeB);
        return valueA < valueB ? -ascending : (valueA > valueB ? ascending : 0);
      }

      /**
       * @param {number} indexA
       * @param {number} indexB
       * @return {number}
       */
      function sortByComparator(indexA, indexB) {
        nodeA.nodeIndex = indexA;
        nodeB.nodeIndex = indexB;
        let result = sortByNodeField(fieldAccessor1, ascending1);
        if (result === 0) {
          result = sortByNodeField(fieldAccessor2, ascending2);
        }
        return result || indexA - indexB;
      }

      return sortByComparator;
    }

    /**
     * @param {!ComparatorConfig} comparator
     * @param {number} leftBound
     * @param {number} rightBound
     * @param {number} windowLeft
     * @param {number} windowRight
     */
    sort(comparator, leftBound, rightBound, windowLeft, windowRight) {
      this._iterationOrder.sortRange(
          this._buildCompareFunction(comparator), leftBound, rightBound, windowLeft, windowRight);
    }
  }

  /**
   * @unrestricted
   */
  class JSHeapSnapshot extends HeapSnapshot {
    /**
     * @param {!Object} profile
     * @param {!HeapSnapshotProgress} progress
     */
    constructor(profile, progress) {
      super(profile, progress);
      this._nodeFlags = {
        // bit flags
        canBeQueried: 1,
        detachedDOMTreeNode: 2,
        pageObject: 4  // The idea is to track separately the objects owned by the page and the objects owned by debugger.
      };
      this._lazyStringCache = {};
      this.initialize();
    }

    /**
     * @override
     * @param {number=} nodeIndex
     * @return {!JSHeapSnapshotNode}
     */
    createNode(nodeIndex) {
      return new JSHeapSnapshotNode(this, nodeIndex === undefined ? -1 : nodeIndex);
    }

    /**
     * @override
     * @param {number} edgeIndex
     * @return {!JSHeapSnapshotEdge}
     */
    createEdge(edgeIndex) {
      return new JSHeapSnapshotEdge(this, edgeIndex);
    }

    /**
     * @override
     * @param {number} retainerIndex
     * @return {!JSHeapSnapshotRetainerEdge}
     */
    createRetainingEdge(retainerIndex) {
      return new JSHeapSnapshotRetainerEdge(this, retainerIndex);
    }

    /**
     * @override
     * @return {function(!HeapSnapshotEdge):boolean}
     */
    containmentEdgesFilter() {
      return edge => !edge.isInvisible();
    }

    /**
     * @override
     * @return {function(!HeapSnapshotEdge):boolean}
     */
    retainingEdgesFilter() {
      const containmentEdgesFilter = this.containmentEdgesFilter();
      function filter(edge) {
        return containmentEdgesFilter(edge) && !edge.node().isRoot() && !edge.isWeak();
      }
      return filter;
    }

    /**
     * @override
     */
    calculateFlags() {
      this._flags = new Uint32Array(this.nodeCount);
      this._markDetachedDOMTreeNodes();
      this._markQueriableHeapObjects();
      this._markPageOwnedNodes();
    }

    /**
     * @override
     */
    calculateDistances() {
      /**
       * @param {!HeapSnapshotNode} node
       * @param {!HeapSnapshotEdge} edge
       * @return {boolean}
       */
      function filter(node, edge) {
        if (node.isHidden()) {
          return edge.name() !== 'sloppy_function_map' || node.rawName() !== 'system / NativeContext';
        }
        if (node.isArray()) {
          // DescriptorArrays are fixed arrays used to hold instance descriptors.
          // The format of the these objects is:
          //   [0]: Number of descriptors
          //   [1]: Either Smi(0) if uninitialized, or a pointer to small fixed array:
          //          [0]: pointer to fixed array with enum cache
          //          [1]: either Smi(0) or pointer to fixed array with indices
          //   [i*3+2]: i-th key
          //   [i*3+3]: i-th type
          //   [i*3+4]: i-th descriptor
          // As long as maps may share descriptor arrays some of the descriptor
          // links may not be valid for all the maps. We just skip
          // all the descriptor links when calculating distances.
          // For more details see http://crbug.com/413608
          if (node.rawName() !== '(map descriptors)') {
            return true;
          }
          const index = edge.name();
          return index < 2 || (index % 3) !== 1;
        }
        return true;
      }
      super.calculateDistances(filter);
    }

    /**
     * @override
     * @protected
     * @param {!HeapSnapshotNode} node
     * @return {boolean}
     */
    isUserRoot(node) {
      return node.isUserRoot() || node.isDocumentDOMTreesRoot();
    }

    /**
     * @override
     * @return {?{map: !Uint32Array, flag: number}}
     */
    userObjectsMapAndFlag() {
      return {map: this._flags, flag: this._nodeFlags.pageObject};
    }

    /**
     * @param {!HeapSnapshotNode} node
     * @return {number}
     */
    _flagsOfNode(node) {
      return this._flags[node.nodeIndex / this._nodeFieldCount];
    }

    _markDetachedDOMTreeNodes() {
      const nodes = this.nodes;
      const nodesLength = nodes.length;
      const nodeFieldCount = this._nodeFieldCount;
      const nodeNativeType = this._nodeNativeType;
      const nodeTypeOffset = this._nodeTypeOffset;
      const flag = this._nodeFlags.detachedDOMTreeNode;
      const node = this.rootNode();
      for (let nodeIndex = 0, ordinal = 0; nodeIndex < nodesLength; nodeIndex += nodeFieldCount, ordinal++) {
        const nodeType = nodes[nodeIndex + nodeTypeOffset];
        if (nodeType !== nodeNativeType) {
          continue;
        }
        node.nodeIndex = nodeIndex;
        if (node.name().startsWith('Detached ')) {
          this._flags[ordinal] |= flag;
        }
      }
    }

    _markQueriableHeapObjects() {
      // Allow runtime properties query for objects accessible from Window objects
      // via regular properties, and for DOM wrappers. Trying to access random objects
      // can cause a crash due to insonsistent state of internal properties of wrappers.
      const flag = this._nodeFlags.canBeQueried;
      const hiddenEdgeType = this._edgeHiddenType;
      const internalEdgeType = this._edgeInternalType;
      const invisibleEdgeType = this._edgeInvisibleType;
      const weakEdgeType = this._edgeWeakType;
      const edgeToNodeOffset = this._edgeToNodeOffset;
      const edgeTypeOffset = this._edgeTypeOffset;
      const edgeFieldsCount = this._edgeFieldsCount;
      const containmentEdges = this.containmentEdges;
      const nodeFieldCount = this._nodeFieldCount;
      const firstEdgeIndexes = this._firstEdgeIndexes;

      const flags = this._flags;
      const list = [];

      for (let iter = this.rootNode().edges(); iter.hasNext(); iter.next()) {
        if (iter.edge.node().isUserRoot()) {
          list.push(iter.edge.node().nodeIndex / nodeFieldCount);
        }
      }

      while (list.length) {
        const nodeOrdinal = list.pop();
        if (flags[nodeOrdinal] & flag) {
          continue;
        }
        flags[nodeOrdinal] |= flag;
        const beginEdgeIndex = firstEdgeIndexes[nodeOrdinal];
        const endEdgeIndex = firstEdgeIndexes[nodeOrdinal + 1];
        for (let edgeIndex = beginEdgeIndex; edgeIndex < endEdgeIndex; edgeIndex += edgeFieldsCount) {
          const childNodeIndex = containmentEdges[edgeIndex + edgeToNodeOffset];
          const childNodeOrdinal = childNodeIndex / nodeFieldCount;
          if (flags[childNodeOrdinal] & flag) {
            continue;
          }
          const type = containmentEdges[edgeIndex + edgeTypeOffset];
          if (type === hiddenEdgeType || type === invisibleEdgeType || type === internalEdgeType ||
              type === weakEdgeType) {
            continue;
          }
          list.push(childNodeOrdinal);
        }
      }
    }

    _markPageOwnedNodes() {
      const edgeShortcutType = this._edgeShortcutType;
      const edgeElementType = this._edgeElementType;
      const edgeToNodeOffset = this._edgeToNodeOffset;
      const edgeTypeOffset = this._edgeTypeOffset;
      const edgeFieldsCount = this._edgeFieldsCount;
      const edgeWeakType = this._edgeWeakType;
      const firstEdgeIndexes = this._firstEdgeIndexes;
      const containmentEdges = this.containmentEdges;
      const nodeFieldCount = this._nodeFieldCount;
      const nodesCount = this.nodeCount;

      const flags = this._flags;
      const pageObjectFlag = this._nodeFlags.pageObject;

      const nodesToVisit = new Uint32Array(nodesCount);
      let nodesToVisitLength = 0;

      const rootNodeOrdinal = this._rootNodeIndex / nodeFieldCount;
      const node = this.rootNode();

      // Populate the entry points. They are Window objects and DOM Tree Roots.
      for (let edgeIndex = firstEdgeIndexes[rootNodeOrdinal], endEdgeIndex = firstEdgeIndexes[rootNodeOrdinal + 1];
           edgeIndex < endEdgeIndex; edgeIndex += edgeFieldsCount) {
        const edgeType = containmentEdges[edgeIndex + edgeTypeOffset];
        const nodeIndex = containmentEdges[edgeIndex + edgeToNodeOffset];
        if (edgeType === edgeElementType) {
          node.nodeIndex = nodeIndex;
          if (!node.isDocumentDOMTreesRoot()) {
            continue;
          }
        } else if (edgeType !== edgeShortcutType) {
          continue;
        }
        const nodeOrdinal = nodeIndex / nodeFieldCount;
        nodesToVisit[nodesToVisitLength++] = nodeOrdinal;
        flags[nodeOrdinal] |= pageObjectFlag;
      }

      // Mark everything reachable with the pageObject flag.
      while (nodesToVisitLength) {
        const nodeOrdinal = nodesToVisit[--nodesToVisitLength];
        const beginEdgeIndex = firstEdgeIndexes[nodeOrdinal];
        const endEdgeIndex = firstEdgeIndexes[nodeOrdinal + 1];
        for (let edgeIndex = beginEdgeIndex; edgeIndex < endEdgeIndex; edgeIndex += edgeFieldsCount) {
          const childNodeIndex = containmentEdges[edgeIndex + edgeToNodeOffset];
          const childNodeOrdinal = childNodeIndex / nodeFieldCount;
          if (flags[childNodeOrdinal] & pageObjectFlag) {
            continue;
          }
          const type = containmentEdges[edgeIndex + edgeTypeOffset];
          if (type === edgeWeakType) {
            continue;
          }
          nodesToVisit[nodesToVisitLength++] = childNodeOrdinal;
          flags[childNodeOrdinal] |= pageObjectFlag;
        }
      }
    }

    /**
     * @override
     */
    calculateStatistics() {
      const nodeFieldCount = this._nodeFieldCount;
      const nodes = this.nodes;
      const nodesLength = nodes.length;
      const nodeTypeOffset = this._nodeTypeOffset;
      const nodeSizeOffset = this._nodeSelfSizeOffset;
      const nodeNativeType = this._nodeNativeType;
      const nodeCodeType = this._nodeCodeType;
      const nodeConsStringType = this._nodeConsStringType;
      const nodeSlicedStringType = this._nodeSlicedStringType;
      const distances = this._nodeDistances;
      let sizeNative = 0;
      let sizeCode = 0;
      let sizeStrings = 0;
      let sizeJSArrays = 0;
      let sizeSystem = 0;
      const node = this.rootNode();
      for (let nodeIndex = 0; nodeIndex < nodesLength; nodeIndex += nodeFieldCount) {
        const nodeSize = nodes[nodeIndex + nodeSizeOffset];
        const ordinal = nodeIndex / nodeFieldCount;
        if (distances[ordinal] >= baseSystemDistance) {
          sizeSystem += nodeSize;
          continue;
        }
        const nodeType = nodes[nodeIndex + nodeTypeOffset];
        node.nodeIndex = nodeIndex;
        if (nodeType === nodeNativeType) {
          sizeNative += nodeSize;
        } else if (nodeType === nodeCodeType) {
          sizeCode += nodeSize;
        } else if (nodeType === nodeConsStringType || nodeType === nodeSlicedStringType || node.type() === 'string') {
          sizeStrings += nodeSize;
        } else if (node.name() === 'Array') {
          sizeJSArrays += this._calculateArraySize(node);
        }
      }
      this._statistics = new Statistics();
      this._statistics.total = this.totalSize;
      this._statistics.v8heap = this.totalSize - sizeNative;
      this._statistics.native = sizeNative;
      this._statistics.code = sizeCode;
      this._statistics.jsArrays = sizeJSArrays;
      this._statistics.strings = sizeStrings;
      this._statistics.system = sizeSystem;
    }

    /**
     * @param {!HeapSnapshotNode} node
     * @return {number}
     */
    _calculateArraySize(node) {
      let size = node.selfSize();
      const beginEdgeIndex = node.edgeIndexesStart();
      const endEdgeIndex = node.edgeIndexesEnd();
      const containmentEdges = this.containmentEdges;
      const strings = this.strings;
      const edgeToNodeOffset = this._edgeToNodeOffset;
      const edgeTypeOffset = this._edgeTypeOffset;
      const edgeNameOffset = this._edgeNameOffset;
      const edgeFieldsCount = this._edgeFieldsCount;
      const edgeInternalType = this._edgeInternalType;
      for (let edgeIndex = beginEdgeIndex; edgeIndex < endEdgeIndex; edgeIndex += edgeFieldsCount) {
        const edgeType = containmentEdges[edgeIndex + edgeTypeOffset];
        if (edgeType !== edgeInternalType) {
          continue;
        }
        const edgeName = strings[containmentEdges[edgeIndex + edgeNameOffset]];
        if (edgeName !== 'elements') {
          continue;
        }
        const elementsNodeIndex = containmentEdges[edgeIndex + edgeToNodeOffset];
        node.nodeIndex = elementsNodeIndex;
        if (node.retainersCount() === 1) {
          size += node.selfSize();
        }
        break;
      }
      return size;
    }

    /**
     * @return {!Statistics}
     */
    getStatistics() {
      return this._statistics;
    }
  }

  /**
   * @unrestricted
   */
  class JSHeapSnapshotNode extends HeapSnapshotNode {
    /**
     * @param {!JSHeapSnapshot} snapshot
     * @param {number=} nodeIndex
     */
    constructor(snapshot, nodeIndex) {
      super(snapshot, nodeIndex);
    }

    /**
     * @return {boolean}
     */
    canBeQueried() {
      const flags = this._snapshot._flagsOfNode(this);
      return !!(flags & this._snapshot._nodeFlags.canBeQueried);
    }

    /**
     * @return {string}
     */
    rawName() {
      return super.name();
    }

    /**
     * @override
     * @return {string}
     */
    name() {
      const snapshot = this._snapshot;
      if (this.rawType() === snapshot._nodeConsStringType) {
        let string = snapshot._lazyStringCache[this.nodeIndex];
        if (typeof string === 'undefined') {
          string = this._consStringName();
          snapshot._lazyStringCache[this.nodeIndex] = string;
        }
        return string;
      }
      return this.rawName();
    }

    /**
     * @return {string}
     */
    _consStringName() {
      const snapshot = this._snapshot;
      const consStringType = snapshot._nodeConsStringType;
      const edgeInternalType = snapshot._edgeInternalType;
      const edgeFieldsCount = snapshot._edgeFieldsCount;
      const edgeToNodeOffset = snapshot._edgeToNodeOffset;
      const edgeTypeOffset = snapshot._edgeTypeOffset;
      const edgeNameOffset = snapshot._edgeNameOffset;
      const strings = snapshot.strings;
      const edges = snapshot.containmentEdges;
      const firstEdgeIndexes = snapshot._firstEdgeIndexes;
      const nodeFieldCount = snapshot._nodeFieldCount;
      const nodeTypeOffset = snapshot._nodeTypeOffset;
      const nodeNameOffset = snapshot._nodeNameOffset;
      const nodes = snapshot.nodes;
      const nodesStack = [];
      nodesStack.push(this.nodeIndex);
      let name = '';

      while (nodesStack.length && name.length < 1024) {
        const nodeIndex = nodesStack.pop();
        if (nodes[nodeIndex + nodeTypeOffset] !== consStringType) {
          name += strings[nodes[nodeIndex + nodeNameOffset]];
          continue;
        }
        const nodeOrdinal = nodeIndex / nodeFieldCount;
        const beginEdgeIndex = firstEdgeIndexes[nodeOrdinal];
        const endEdgeIndex = firstEdgeIndexes[nodeOrdinal + 1];
        let firstNodeIndex = 0;
        let secondNodeIndex = 0;
        for (let edgeIndex = beginEdgeIndex; edgeIndex < endEdgeIndex && (!firstNodeIndex || !secondNodeIndex);
             edgeIndex += edgeFieldsCount) {
          const edgeType = edges[edgeIndex + edgeTypeOffset];
          if (edgeType === edgeInternalType) {
            const edgeName = strings[edges[edgeIndex + edgeNameOffset]];
            if (edgeName === 'first') {
              firstNodeIndex = edges[edgeIndex + edgeToNodeOffset];
            } else if (edgeName === 'second') {
              secondNodeIndex = edges[edgeIndex + edgeToNodeOffset];
            }
          }
        }
        nodesStack.push(secondNodeIndex);
        nodesStack.push(firstNodeIndex);
      }
      return name;
    }

    /**
     * @override
     * @return {string}
     */
    className() {
      const type = this.type();
      switch (type) {
        case 'hidden':
          return '(system)';
        case 'object':
        case 'native':
          return this.name();
        case 'code':
          return '(compiled code)';
        default:
          return '(' + type + ')';
      }
    }

    /**
     * @override
     * @return {number}
     */
    classIndex() {
      const snapshot = this._snapshot;
      const nodes = snapshot.nodes;
      const type = nodes[this.nodeIndex + snapshot._nodeTypeOffset];
      if (type === snapshot._nodeObjectType || type === snapshot._nodeNativeType) {
        return nodes[this.nodeIndex + snapshot._nodeNameOffset];
      }
      return -1 - type;
    }

    /**
     * @override
     * @return {number}
     */
    id() {
      const snapshot = this._snapshot;
      return snapshot.nodes[this.nodeIndex + snapshot._nodeIdOffset];
    }

    /**
     * @return {boolean}
     */
    isHidden() {
      return this.rawType() === this._snapshot._nodeHiddenType;
    }

    /**
     * @return {boolean}
     */
    isArray() {
      return this.rawType() === this._snapshot._nodeArrayType;
    }

    /**
     * @return {boolean}
     */
    isSynthetic() {
      return this.rawType() === this._snapshot._nodeSyntheticType;
    }

    /**
     * @return {boolean}
     */
    isUserRoot() {
      return !this.isSynthetic();
    }

    /**
     * @return {boolean}
     */
    isDocumentDOMTreesRoot() {
      return this.isSynthetic() && this.name() === '(Document DOM trees)';
    }

    /**
     * @override
     * @return {!Node}
     */
    serialize() {
      const result = super.serialize();
      const flags = this._snapshot._flagsOfNode(this);
      if (flags & this._snapshot._nodeFlags.canBeQueried) {
        result.canBeQueried = true;
      }
      if (flags & this._snapshot._nodeFlags.detachedDOMTreeNode) {
        result.detachedDOMTreeNode = true;
      }
      return result;
    }
  }

  /**
   * @unrestricted
   */
  class JSHeapSnapshotEdge extends HeapSnapshotEdge {
    /**
     * @param {!JSHeapSnapshot} snapshot
     * @param {number=} edgeIndex
     */
    constructor(snapshot, edgeIndex) {
      super(snapshot, edgeIndex);
    }

    /**
     * @override
     * @return {!JSHeapSnapshotEdge}
     */
    clone() {
      const snapshot = /** @type {!JSHeapSnapshot} */ (this._snapshot);
      return new JSHeapSnapshotEdge(snapshot, this.edgeIndex);
    }

    /**
     * @override
     * @return {boolean}
     */
    hasStringName() {
      if (!this.isShortcut()) {
        return this._hasStringName();
      }
      return isNaN(parseInt(this._name(), 10));
    }

    /**
     * @return {boolean}
     */
    isElement() {
      return this.rawType() === this._snapshot._edgeElementType;
    }

    /**
     * @return {boolean}
     */
    isHidden() {
      return this.rawType() === this._snapshot._edgeHiddenType;
    }

    /**
     * @return {boolean}
     */
    isWeak() {
      return this.rawType() === this._snapshot._edgeWeakType;
    }

    /**
     * @return {boolean}
     */
    isInternal() {
      return this.rawType() === this._snapshot._edgeInternalType;
    }

    /**
     * @return {boolean}
     */
    isInvisible() {
      return this.rawType() === this._snapshot._edgeInvisibleType;
    }

    /**
     * @return {boolean}
     */
    isShortcut() {
      return this.rawType() === this._snapshot._edgeShortcutType;
    }

    /**
     * @override
     * @return {string}
     */
    name() {
      const name = this._name();
      if (!this.isShortcut()) {
        return String(name);
      }
      const numName = parseInt(name, 10);
      return String(isNaN(numName) ? name : numName);
    }

    /**
     * @override
     * @return {string}
     */
    toString() {
      const name = this.name();
      switch (this.type()) {
        case 'context':
          return '->' + name;
        case 'element':
          return '[' + name + ']';
        case 'weak':
          return '[[' + name + ']]';
        case 'property':
          return name.indexOf(' ') === -1 ? '.' + name : '["' + name + '"]';
        case 'shortcut':
          if (typeof name === 'string') {
            return name.indexOf(' ') === -1 ? '.' + name : '["' + name + '"]';
          }
          return '[' + name + ']';
        case 'internal':
        case 'hidden':
        case 'invisible':
          return '{' + name + '}';
      }
      return '?' + name + '?';
    }

    /**
     * @return {boolean}
     */
    _hasStringName() {
      const type = this.rawType();
      const snapshot = this._snapshot;
      return type !== snapshot._edgeElementType && type !== snapshot._edgeHiddenType;
    }

    /**
     * @return {string|number}
     */
    _name() {
      return this._hasStringName() ? this._snapshot.strings[this._nameOrIndex()] : this._nameOrIndex();
    }

    /**
     * @return {number}
     */
    _nameOrIndex() {
      return this._edges[this.edgeIndex + this._snapshot._edgeNameOffset];
    }

    /**
     * @override
     * @return {number}
     */
    rawType() {
      return this._edges[this.edgeIndex + this._snapshot._edgeTypeOffset];
    }
  }

  /**
   * @unrestricted
   */
  class JSHeapSnapshotRetainerEdge extends HeapSnapshotRetainerEdge {
    /**
     * @param {!JSHeapSnapshot} snapshot
     * @param {number} retainerIndex
     */
    constructor(snapshot, retainerIndex) {
      super(snapshot, retainerIndex);
    }

    /**
     * @override
     * @return {!JSHeapSnapshotRetainerEdge}
     */
    clone() {
      const snapshot = /** @type {!JSHeapSnapshot} */ (this._snapshot);
      return new JSHeapSnapshotRetainerEdge(snapshot, this.retainerIndex());
    }

    /**
     * @return {boolean}
     */
    isHidden() {
      return this._edge().isHidden();
    }

    /**
     * @return {boolean}
     */
    isInternal() {
      return this._edge().isInternal();
    }

    /**
     * @return {boolean}
     */
    isInvisible() {
      return this._edge().isInvisible();
    }

    /**
     * @return {boolean}
     */
    isShortcut() {
      return this._edge().isShortcut();
    }

    /**
     * @return {boolean}
     */
    isWeak() {
      return this._edge().isWeak();
    }
  }

  (function disableLoggingForTest() {
    // Runtime doesn't exist because this file is loaded as a one-off
    // file in some inspector-protocol tests.
    if (self.Root && self.Root.Runtime && Root.Runtime.queryParam('test')) {
      console.warn = () => undefined;
    }
  })();

  /*
   * Copyright (C) 2012 Google Inc. All rights reserved.
   *
   * Redistribution and use in source and binary forms, with or without
   * modification, are permitted provided that the following conditions are
   * met:
   *
   *     * Redistributions of source code must retain the above copyright
   * notice, this list of conditions and the following disclaimer.
   *     * Redistributions in binary form must reproduce the above
   * copyright notice, this list of conditions and the following disclaimer
   * in the documentation and/or other materials provided with the
   * distribution.
   *     * Neither the name of Google Inc. nor the names of its
   * contributors may be used to endorse or promote products derived from
   * this software without specific prior written permission.
   *
   * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   */

  /**
   * @unrestricted
   */
  class HeapSnapshotLoader {
    /**
     * @param {!HeapSnapshotWorkerDispatcher} dispatcher
     */
    constructor(dispatcher) {
      this._reset();
      this._progress = new HeapSnapshotProgress(dispatcher);
      this._buffer = '';
      this._dataCallback = null;
      this._done = false;
      this._parseInput();
    }

    dispose() {
      this._reset();
    }

    _reset() {
      this._json = '';
      this._snapshot = {};
    }

    close() {
      this._done = true;
      if (this._dataCallback) {
        this._dataCallback('');
      }
    }

    /**
     * @return {!JSHeapSnapshot}
     */
    buildSnapshot() {
      this._progress.updateStatus(ls`Processing snapshot…`);
      const result = new JSHeapSnapshot(this._snapshot, this._progress);
      this._reset();
      return result;
    }

    _parseUintArray() {
      let index = 0;
      const char0 = '0'.charCodeAt(0);
      const char9 = '9'.charCodeAt(0);
      const closingBracket = ']'.charCodeAt(0);
      const length = this._json.length;
      while (true) {
        while (index < length) {
          const code = this._json.charCodeAt(index);
          if (char0 <= code && code <= char9) {
            break;
          } else if (code === closingBracket) {
            this._json = this._json.slice(index + 1);
            return false;
          }
          ++index;
        }
        if (index === length) {
          this._json = '';
          return true;
        }
        let nextNumber = 0;
        const startIndex = index;
        while (index < length) {
          const code = this._json.charCodeAt(index);
          if (char0 > code || code > char9) {
            break;
          }
          nextNumber *= 10;
          nextNumber += (code - char0);
          ++index;
        }
        if (index === length) {
          this._json = this._json.slice(startIndex);
          return true;
        }
        this._array[this._arrayIndex++] = nextNumber;
      }
    }

    _parseStringsArray() {
      this._progress.updateStatus(ls`Parsing strings…`);
      const closingBracketIndex = this._json.lastIndexOf(']');
      if (closingBracketIndex === -1) {
        throw new Error('Incomplete JSON');
      }
      this._json = this._json.slice(0, closingBracketIndex + 1);
      this._snapshot.strings = JSON.parse(this._json);
    }

    /**
     * @param {string} chunk
     */
    write(chunk) {
      this._buffer += chunk;
      if (!this._dataCallback) {
        return;
      }
      this._dataCallback(this._buffer);
      this._dataCallback = null;
      this._buffer = '';
    }

    /**
     * @return {!Promise<string>}
     */
    _fetchChunk() {
      return this._done ? Promise.resolve(this._buffer) : new Promise(r => {
        this._dataCallback = r;
      });
    }

    /**
     * @param {string} token
     * @param {number=} startIndex
     * @return {!Promise<number>}
     */
    async _findToken(token, startIndex) {
      while (true) {
        const pos = this._json.indexOf(token, startIndex || 0);
        if (pos !== -1) {
          return pos;
        }
        startIndex = this._json.length - token.length + 1;
        this._json += await this._fetchChunk();
      }
    }

    /**
     * @param {string} name
     * @param {string} title
     * @param {number=} length
     * @return {!Promise<!Uint32Array|!Array<number>>}
     */
    async _parseArray(name, title, length) {
      const nameIndex = await this._findToken(name);
      const bracketIndex = await this._findToken('[', nameIndex);
      this._json = this._json.slice(bracketIndex + 1);
      this._array = length ? new Uint32Array(length) : [];
      this._arrayIndex = 0;
      while (this._parseUintArray()) {
        this._progress.updateProgress(title, this._arrayIndex, this._array.length);
        this._json += await this._fetchChunk();
      }
      const result = this._array;
      this._array = null;
      return result;
    }

    async _parseInput() {
      const snapshotToken = '"snapshot"';
      const snapshotTokenIndex = await this._findToken(snapshotToken);
      if (snapshotTokenIndex === -1) {
        throw new Error('Snapshot token not found');
      }

      this._progress.updateStatus(ls`Loading snapshot info…`);
      const json = this._json.slice(snapshotTokenIndex + snapshotToken.length + 1);
      this._jsonTokenizer = new BalancedJSONTokenizer(metaJSON => {
        this._json = this._jsonTokenizer.remainder();
        this._jsonTokenizer = null;
        this._snapshot.snapshot = /** @type {!HeapSnapshotHeader} */ (JSON.parse(metaJSON));
      });
      this._jsonTokenizer.write(json);
      while (this._jsonTokenizer) {
        this._jsonTokenizer.write(await this._fetchChunk());
      }

      this._snapshot.nodes = await this._parseArray(
          '"nodes"', ls`Loading nodes… %d%%`,
          this._snapshot.snapshot.meta.node_fields.length * this._snapshot.snapshot.node_count);

      this._snapshot.edges = await this._parseArray(
          '"edges"', ls`Loading edges… %d%%`,
          this._snapshot.snapshot.meta.edge_fields.length * this._snapshot.snapshot.edge_count);

      if (this._snapshot.snapshot.trace_function_count) {
        this._snapshot.trace_function_infos = await this._parseArray(
            '"trace_function_infos"', ls`Loading allocation traces… %d%%`,
            this._snapshot.snapshot.meta.trace_function_info_fields.length *
                this._snapshot.snapshot.trace_function_count);

        const thisTokenEndIndex = await this._findToken(':');
        const nextTokenIndex = await this._findToken('"', thisTokenEndIndex);
        const openBracketIndex = this._json.indexOf('[');
        const closeBracketIndex = this._json.lastIndexOf(']', nextTokenIndex);
        this._snapshot.trace_tree = JSON.parse(this._json.substring(openBracketIndex, closeBracketIndex + 1));
        this._json = this._json.slice(closeBracketIndex + 1);
      }

      if (this._snapshot.snapshot.meta.sample_fields) {
        this._snapshot.samples = await this._parseArray('"samples"', ls`Loading samples…`);
      }

      if (this._snapshot.snapshot.meta['location_fields']) {
        this._snapshot.locations = await this._parseArray('"locations"', ls`Loading locations…`);
      } else {
        this._snapshot.locations = [];
      }

      this._progress.updateStatus(ls`Loading strings…`);
      const stringsTokenIndex = await this._findToken('"strings"');
      const bracketIndex = await this._findToken('[', stringsTokenIndex);
      this._json = this._json.slice(bracketIndex);
      while (!this._done) {
        this._json += await this._fetchChunk();
      }
      this._parseStringsArray();
    }
  }

  exports.HeapSnapshotLoader = HeapSnapshotLoader;

  return exports;

}({}));

self.HeapSnapshotWorker = Object.assign(self.HeapSnapshotWorker || {}, HeapSnapshotLoader);
