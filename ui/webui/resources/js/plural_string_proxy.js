// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used to get a pluralized string.
 */

// clang-format off
import {addSingletonGetter, sendWithPromise} from './cr.m.js';
// clang-format on

/** @interface */
export class PluralStringProxy {
  /**
   * Obtains a pluralized string for |messageName| with |itemCount| items.
   * @param {!string} messageName The name of the message.
   * @param {!number} itemCount The number of items.
   * @return {!Promise<string>} Promise resolved with the appropriate plural
   *     string for |messageName| with |itemCount| items.
   */
  getPluralString(messageName, itemCount) {}

  /**
   * Fetches both plural strings, concatenated to one string with a comma.
   * @param {!string} messageName1 The name of the first message.
   * @param {!number} itemCount1 The number of items in the first message.
   * @param {!string} messageName2 The name of the second message.
   * @param {!number} itemCount2 The number of items in the second message.
   * @return {!Promise<string>} Promise resolved with the appropriate plural
   *     strings for both messages, concatenated with a comma+whitespace in
   *     between them.
   */
  getPluralStringTupleWithComma(
      messageName1, itemCount1, messageName2, itemCount2) {}

  /**
   * Fetches both plural strings, concatenated to one string with periods.
   * @param {!string} messageName1 The name of the first message.
   * @param {!number} itemCount1 The number of items in the first message.
   * @param {!string} messageName2 The name of the second message.
   * @param {!number} itemCount2 The number of items in the second message.
   * @return {!Promise<string>} Promise resolved with the appropriate plural
   *     strings for both messages, concatenated with a period+whitespace after
   *     the first message, and a period after the second message.
   */
  getPluralStringTupleWithPeriods(
      messageName1, itemCount1, messageName2, itemCount2) {}
}

/** @implements {PluralStringProxy} */
export class PluralStringProxyImpl {
  /** @override */
  getPluralString(messageName, itemCount) {
    return sendWithPromise('getPluralString', messageName, itemCount);
  }

  /** @override */
  getPluralStringTupleWithComma(
      messageName1, itemCount1, messageName2, itemCount2) {
    return sendWithPromise(
        'getPluralStringTupleWithComma', messageName1, itemCount1, messageName2,
        itemCount2);
  }

  /** @override */
  getPluralStringTupleWithPeriods(
      messageName1, itemCount1, messageName2, itemCount2) {
    return sendWithPromise(
        'getPluralStringTupleWithPeriods', messageName1, itemCount1,
        messageName2, itemCount2);
  }
}

addSingletonGetter(PluralStringProxyImpl);
